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

#include <assert.h>

#include <tjost.h>
#include <rtmidi_c.h>

#define MOD_NAME "rtmidi_out"

typedef struct _Data Data;

struct _Data {
	RtMidiC_Out *dev;

	Tjost_Pipe pipe;
	osc_data_t buffer [TJOST_BUF_SIZE] __attribute__((aligned (8)));
};

static int
_midi(osc_time_t time, const char *path, const char *fmt, osc_data_t *buf, size_t size, void *arg)
{
	Tjost_Module *module = arg;
	Data *dat = module->dat;

	uint8_t m [3];
	uint8_t *M;

	osc_data_t *ptr = buf;
	const char *type;
	for(type=fmt; *type!='\0'; type++)
		switch(*type)
		{
			case OSC_MIDI:
			{
				ptr = osc_get_midi(ptr, &M);
				m[0] = M[1];
				m[1] = M[2];
				m[2] = M[3];

				if(rtmidic_out_send_message(dat->dev, 3, m))
					fprintf(stderr, MOD_NAME": encode event failed\n");
			}
			default:
				ptr = osc_skip(*type, ptr);
				break;
		}

	return 1;
}

static osc_method_t methods [] = {
	{NULL, NULL, _midi},
	{NULL, NULL, NULL}
};

static osc_data_t *
_alloc(Tjost_Event *tev, void *arg)
{
	Tjost_Module *module = tev->module;
	Data *dat = module->dat;

	return dat->buffer;
}

static int
_sched(Tjost_Event *tev, osc_data_t *buf, void *arg)
{
	Tjost_Module *module = tev->module;

	osc_dispatch_method(tev->time, buf, tev->size, methods, NULL, NULL, module);

	return 0; // reload
}

int
process_out(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Data *dat = module->dat;
	Tjost_Host *host = module->host;

	jack_nframes_t last = jack_last_frame_time(host->client);

	unsigned int count = eina_inlist_count(module->queue);

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

		tev->time -= last; // time relative to current period
		if(tjost_pipe_produce(&dat->pipe, module, tev->time, tev->size, tev->buf))
			tjost_host_message_push(host, MOD_NAME": %s", "tjost_pipe_produce error");

		module->queue = eina_inlist_remove(module->queue, EINA_INLIST_GET(tev));
		tjost_free(host, tev);
	}

	if(count > 0)
	{
		if(tjost_pipe_flush(&dat->pipe))
			tjost_host_message_push(host, MOD_NAME": %s", "tjost_pipe_flush error");
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

	lua_getfield(L, 1, "device");
	const char *device = luaL_optstring(L, -1, "seq");
	lua_pop(L, 1);

	lua_getfield(L, 1, "api");
	RtMidiC_API api = luaL_optint(L, -1, RTMIDIC_API_UNSPECIFIED);
	lua_pop(L, 1);

	if(!(dat->dev = rtmidic_out_new(api, jack_get_client_name(module->host->client))))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not create device");
	if(rtmidic_out_virtual_port_open(dat->dev, device))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not open virtual port");

	uv_loop_t *loop = uv_default_loop();

	if(tjost_pipe_init(&dat->pipe))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize tjost pipe");
	tjost_pipe_listen_start(&dat->pipe, loop, _alloc, _sched, NULL);

	module->dat = dat;
	module->type = TJOST_MODULE_OUTPUT;

	return 0;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	tjost_pipe_listen_stop(&dat->pipe);
	tjost_pipe_deinit(&dat->pipe);

	if(rtmidic_out_port_close(dat->dev))
		fprintf(stderr, MOD_NAME": could not close port\n");
	if(rtmidic_out_free(dat->dev))
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
