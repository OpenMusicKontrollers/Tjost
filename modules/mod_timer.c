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

#include <tjost.h>

#define MOD_NAME "timer"

typedef struct _Data Data;

struct _Data {
	Tjost_Pipe pipe;
	uv_timer_t timer;
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
_timeup(uv_timer_t *handle)
{
	Tjost_Module *module = handle->data;
	Data *dat = module->dat;

	const size_t len = 12;
	osc_data_t buf [len];
	osc_data_t *ptr = buf;
	osc_data_t *end = ptr + len;

	ptr = osc_set_path(ptr, end, "/timeup");
	ptr = osc_set_fmt(ptr, end, "");

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

	lua_getfield(L, 1, "delay");
	const int msec = luaL_optnumber(L, -1, 1.f) * 1000;
	lua_pop(L, 1);
	
	if(tjost_pipe_init(&dat->pipe))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize tjost pipe");

	uv_loop_t *loop = uv_default_loop();

	int err;
	dat->timer.data = module;
	if((err = uv_timer_init(loop, &dat->timer)))
		MOD_ADD_ERR(module->host, MOD_NAME, uv_err_name(err));
	if((err = uv_timer_start(&dat->timer, _timeup, 0, msec))) // ms
		MOD_ADD_ERR(module->host, MOD_NAME, uv_err_name(err));

	module->dat = dat;
	module->type = TJOST_MODULE_INPUT;

	return 0;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	int err;
	if((err = uv_timer_stop(&dat->timer)))
		fprintf(stderr, MOD_NAME": %s\n", uv_err_name(err));

	tjost_pipe_deinit(&dat->pipe);

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
