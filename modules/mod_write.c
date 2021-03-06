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

#define MOD_NAME "write"

typedef struct _Data Data;

struct _Data {
	Tjost_Pipe pipe;

	FILE *f;
	jack_nframes_t offset;
	osc_data_t buffer [TJOST_BUF_SIZE];
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
	Data *dat = module->dat;

	uint32_t ntime = tev->time;
	uint32_t nsize = tev->size;

	ntime = htonl(ntime);
	nsize = htonl(nsize);

	fwrite(&ntime, sizeof(uint32_t), 1, dat->f);
	fwrite(&nsize, sizeof(uint32_t), 1, dat->f);
	fwrite(buf, tev->size, 1, dat->f);

	return 0; // reload
}

int
process_out(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	jack_nframes_t last = jack_last_frame_time(host->client);

	if(dat->offset == 0) //TODO add way to reset this
		dat->offset = last;

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

		tev->time -= dat->offset; // relative time to first written event
		if(tjost_pipe_produce(&dat->pipe, module, tev->time, tev->size, tev->buf))
			tjost_host_message_push(host, MOD_NAME": %s", "tjost_pipe_produce error");

		module->queue = eina_inlist_remove(module->queue, EINA_INLIST_GET(tev));
		tjost_free(host, tev);
	}

	if(count > 0)
	{
		tjost_pipe_flush(&dat->pipe);
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

	uv_loop_t *loop = uv_default_loop();

	lua_getfield(L, 1, "path");
	const char *path = luaL_optstring(L, -1, NULL);
	lua_pop(L, 1);

	if(path)
	{
		if(!(dat->f = fopen(path, "wb")))
			MOD_ADD_ERR(module->host, MOD_NAME, "could not open file handle");
	}
	else
		dat->f = stdout;

	const char *head = "tjostbin";
	uint32_t nsrate = htonl(module->host->srate);
	fwrite(head, sizeof(char), 8, dat->f);
	fwrite(&nsrate, sizeof(uint32_t), 1, dat->f);
	fflush(dat->f);

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

	if( dat->f && (dat->f != stdout) )
	{
		fflush(dat->f);
		fclose(dat->f);
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
