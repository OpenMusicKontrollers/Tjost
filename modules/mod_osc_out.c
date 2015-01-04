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
#include <jack_osc.h>

#define MOD_NAME "osc_out"

int
process_out(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	jack_port_t *port = module->dat;

	if(!port)
		return 0;

	jack_nframes_t last = jack_last_frame_time(host->client);

	void *port_buf = jack_port_get_buffer(port, nframes);
	jack_osc_clear_buffer(port_buf);

	// handle events
	Eina_Inlist *l;
	Tjost_Event *tev;
	EINA_INLIST_FOREACH_SAFE(module->queue, l, tev)
	{
		if(tev->time >= last + nframes)
			break;
		else if(tev->time == 0) // immediate execution
			tev->time = last;
		else if(tev->time < last)
		{
			tjost_host_message_push(host, MOD_NAME": %s %i", "late event", tev->time - last);
			tev->time = last;
		}

		if(jack_osc_max_event_size(port_buf) < tev->size)
			tjost_host_message_push(host, MOD_NAME": %s", "buffer overflow");
		else
			jack_osc_event_write(port_buf, tev->time - last, tev->buf, tev->size);

		module->queue = eina_inlist_remove(module->queue, EINA_INLIST_GET(tev));
		tjost_free(host, tev);
	}

	return 0;
}

int
add(Tjost_Module *module, int argc, const char **argv)
{
	Tjost_Host *host = module->host;
	lua_State *L = host->L;
	jack_port_t *port = NULL;

	lua_getfield(L, 1, "port");
	const char *portn = luaL_optstring(L, -1, "osc_out");
	lua_pop(L, 1);

	if(!(port = jack_port_register(module->host->client, portn, JACK_DEFAULT_OSC_TYPE, JackPortIsOutput, 0)))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not register jack port");

#ifdef HAS_METADATA_API
	lua_getfield(L, 1, "pretty");
	const char *pretty = luaL_optstring(L, -1, "OSC Output");
	lua_pop(L, 1);

	jack_uuid_t uuid = jack_port_uuid(port);
	if(jack_set_property(module->host->client, uuid, JACK_METADATA_PRETTY_NAME, pretty, "text/plain"))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not set prettyname");
	if(jack_set_property(module->host->client, uuid, JACKEY_EVENT_TYPES, JACK_EVENT_TYPE__OSC, "text/plain"))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not set OSC event type");
#endif // HAS_METADATA_API

	module->dat = port;
	module->type = TJOST_MODULE_OUTPUT;

	return 0;
}

void
del(Tjost_Module *module)
{
	jack_port_t *port = module->dat;

	if(port)
	{
#ifdef HAS_METADATA_API
		jack_uuid_t uuid = jack_port_uuid(port);
		jack_remove_property(module->host->client, uuid, JACK_METADATA_PRETTY_NAME);
		jack_remove_property(module->host->client, uuid, JACKEY_EVENT_TYPES);
#endif
		jack_port_unregister(module->host->client, port);
	}
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
