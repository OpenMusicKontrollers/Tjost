/*
 * Copyright (c) 2015 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

#include <tjost.h>

#include <jackey.h>

#include <mod_cv.h>

#define MOD_NAME "cv_in"

#define ALLOC_LEN 12

typedef struct _Data Data;

struct _Data {
	jack_port_t *port;
	jack_default_audio_sample_t value;
};

int
process_in(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	if(!dat->port)
		return 0;

	jack_nframes_t last = jack_last_frame_time(host->client);
	jack_default_audio_sample_t *port_buf = jack_port_get_buffer(dat->port, nframes);

	jack_nframes_t i;
	for(i=0; i<nframes; i++)
		if(port_buf[i] != dat->value)
		{
			dat->value = port_buf[i];

			osc_data_t *ptr = tjost_host_schedule_inline(host, module, last + i, ALLOC_LEN);
			osc_data_t *end = ptr + ALLOC_LEN;

			ptr = osc_set_path(ptr, end, CV_PATH);
			ptr = osc_set_fmt(ptr, end, CV_FMT);
			ptr = osc_set_float(ptr, end, dat->value);
		}

	return 0;
}

int
add(Tjost_Module *module)
{
	Tjost_Host *host = module->host;
	lua_State *L = host->L;
	Data *dat = tjost_alloc(module->host, sizeof(Data));
	memset(dat, 0, sizeof(Data));

	lua_getfield(L, 1, "port");
	const char *portn = luaL_optstring(L, -1, "cv_in");
	lua_pop(L, 1);

	if(!(dat->port = jack_port_register(module->host->client, portn, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0)))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not register jack port");

#ifdef HAS_METADATA_API
	lua_getfield(L, 1, "pretty");
	const char *pretty = luaL_optstring(L, -1, "Control Input");
	lua_pop(L, 1);

	jack_uuid_t uuid = jack_port_uuid(dat->port);
	if(jack_set_property(module->host->client, uuid, JACK_METADATA_PRETTY_NAME, pretty, "text/plain"))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not set prettyname");
	if(jack_set_property(module->host->client, uuid, JACKEY_SIGNAL_TYPE, "CV", "text/plain"))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not set CV event type");
#endif // HAS_METADATA_API

	module->dat = dat;

	module->type = TJOST_MODULE_INPUT;

	return 0;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	if(dat->port)
	{
#ifdef HAS_METADATA_API
		jack_uuid_t uuid = jack_port_uuid(dat->port);
		jack_remove_property(module->host->client, uuid, JACK_METADATA_PRETTY_NAME);
		jack_remove_property(module->host->client, uuid, JACKEY_SIGNAL_TYPE);
#endif
		jack_port_unregister(module->host->client, dat->port);
	}

	tjost_free(module->host, dat);
}

Eina_Bool
init()
{
	return EINA_TRUE;
}

void
deinit()
{
}

EINA_MODULE_INIT(init);
EINA_MODULE_SHUTDOWN(deinit);
