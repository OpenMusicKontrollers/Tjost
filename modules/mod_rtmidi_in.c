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

#include <rtmidi_c.h>

#define MOD_NAME "seq_in"

typedef struct _Data Data;

struct _Data {
	RtMidiC_In *dev;
	RtMidiC_Callback cb;

	Tjost_Pipe pipe;
};

static osc_data_t *
_alloc(Tjost_Event *tev, void *arg)
{
	Tjost_Module *module = tev->module;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	return tjost_host_schedule_inline(host, module, tev->time, tev->size);
}

static int
_sched(Tjost_Event *tev, osc_data_t *buf, void *arg)
{
	return 0; // reload
}

int
process_in(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	if(tjost_pipe_consume(&dat->pipe, _alloc, _sched, NULL))
		tjost_host_message_push(host, MOD_NAME": %s", "tjost_pipe_consume error");

	return 0;
}

static void
_rtmidic_cb(double timestamp, size_t size, uint8_t* message, void **cb)
{
	Data *dat = (Data *)((uint8_t *)cb - offsetof(Data, cb));
	Tjost_Module *module = (Tjost_Module *)((uint8_t *)dat - offsetof(Tjost_Module, dat));

	// TODO assert(size <= 4)
	uint8_t m [4] = {0x0};
	m[1] = message[0];
	m[2] = message[1];
	m[3] = message[2];

	const size_t len = 16;
	osc_data_t buf [len];
	osc_data_t *ptr = buf;
	osc_data_t *end = ptr + len;

	ptr = osc_set_path(ptr, end, "/midi");
	ptr = osc_set_fmt(ptr, end, "m");
	ptr = osc_set_midi(ptr, end, m);

	if(ptr && osc_check_message(buf, len))
	{
		if(tjost_pipe_produce(&dat->pipe, module, 0, len, buf))
			fprintf(stderr, MOD_NAME": tjost_pipe_produce error\n");
	}
	else
		fprintf(stderr, MOD_NAME": rx OSC message invalid\n");
}

int
add(Tjost_Module *module)
{
	Tjost_Host *host = module->host;
	lua_State *L = host->L;
	Data *dat = tjost_alloc(module->host, sizeof(Data));
	memset(dat, 0, sizeof(Data));

	lua_getfield(L, 1, "device");
	const char *device = luaL_optstring(L, -1, "seq");
	lua_pop(L, 1);

	lua_getfield(L, 1, "api");
	RtMidiC_API api = luaL_optint(L, -1, RTMIDIC_API_UNSPECIFIED);
	lua_pop(L, 1);

	dat->cb = _rtmidic_cb;
	
	if(!(dat->dev = rtmidic_in_new(api, jack_get_client_name(module->host->client))))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not create device");
	if(rtmidic_in_virtual_port_open(dat->dev, device))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not open virtual port");
	if(rtmidic_in_callback_set(dat->dev, &dat->cb))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not set callback");

	if(tjost_pipe_init(&dat->pipe))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize tjost pipe");
	
	module->dat = dat;
	module->type = TJOST_MODULE_INPUT;

	return 0;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	tjost_pipe_deinit(&dat->pipe);

	if(rtmidic_in_callback_unset(dat->dev))
		fprintf(stderr, MOD_NAME": could not unset callback\n");
	if(rtmidic_in_port_close(dat->dev))
		fprintf(stderr, MOD_NAME": could not close port\n");
	if(rtmidic_in_free(dat->dev))
		fprintf(stderr, MOD_NAME": could not free device\n");

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
