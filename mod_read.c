/*
 * Copyright (c) 2014 Hanspeter Portner (dev@open-music-kontrollers.ch)
 * 
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * 
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 
 *     1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 * 
 *     2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 * 
 *     3. This notice may not be removed or altered from any source
 *     distribution.
 */

#include <assert.h>

#include <tjost.h>

#define MOD_NAME "read"

typedef struct _Data Data;

struct _Data {
	Tjost_Pipe pipe;
	uv_async_t asio;

	FILE *f;
	jack_nframes_t offset;
	osc_data_t buffer [TJOST_BUF_SIZE];
};

static osc_data_t *
_alloc(Tjost_Event *tev, void *arg)
{
	Tjost_Module *module = tev->module;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	return tjost_host_schedule_inline(host, module, tev->time + dat->offset, tev->size);
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

	if(dat->offset == 0) //TODO add way to reset this
		dat->offset = jack_last_frame_time(host->client);

	if(tjost_pipe_consume(&dat->pipe, _alloc, _sched, NULL))
		tjost_host_message_push(host, MOD_NAME": %s", "tjost_pipe_consume error");

	int err;
	if((err = uv_async_send(&dat->asio)))
		tjost_host_message_push(host, MOD_NAME": %s", uv_err_name(err));

	return 0;
}

static void
_asio(uv_async_t *handle)
{
	Tjost_Module *module = handle->data;
	Data *dat = module->dat;
	
	while(!feof(dat->f) && (tjost_pipe_space(&dat->pipe) > TJOST_BUF_SIZE))
	{
		uint32_t ntime;
		uint32_t nsize;
		fread(&ntime, sizeof(uint32_t), 1, dat->f);
		ntime = ntohl(ntime);
		fread(&nsize, sizeof(uint32_t), 1, dat->f);
		nsize = ntohl(nsize);
		fread(dat->buffer, sizeof(osc_data_t), nsize, dat->f);
		
		Tjost_Event tev;
		tev.time = ntime;
		tev.size = nsize;

		if(osc_check_message(dat->buffer, tev.size))
		{
			if(tjost_pipe_produce(&dat->pipe, module, ntime, nsize, dat->buffer))
				fprintf(stderr, MOD_NAME": tjost_pipe_produce error\n");
		}
		else
			fprintf(stderr, MOD_NAME": rx OSC message invalid\n");
	}
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
		if(!(dat->f = fopen(path, "rb")))
			MOD_ADD_ERR(module->host, MOD_NAME, "could not open file handle");
	}
	else
		dat->f = stdin;

	char head [8];
	uint32_t nsrate;
	fread(head, sizeof(char), 8, dat->f);
	fread(&nsrate, sizeof(uint32_t), 1, dat->f);
	nsrate = ntohl(nsrate);
	if(strncmp(head, "tjostbin", 8))
		MOD_ADD_ERR(module->host, MOD_NAME, "not an OSC binary file");

	dat->asio.data = module;
	int err;
	if((err = uv_async_init(loop, &dat->asio, _asio)))
		MOD_ADD_ERR(module->host, MOD_NAME, uv_err_name(err));

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
	uv_close((uv_handle_t *)&dat->asio, NULL);

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
