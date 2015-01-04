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

#include <sched.h>

#define MOD_NAME "net_in"

#ifndef _WIN32 // POSIX only
#	include <pthread.h>
#else
#	include <windows.h>
#endif

#include <tjost.h>

#include <mod_net.h>

typedef struct _Data Data;

struct _Data {
	Mod_Net net;

	uv_loop_t loop;
	uv_thread_t thread;
	uv_async_t quit;

#ifndef _WIN32 // POSIX only
	struct sched_param schedp;
#else
	int mcss_sched_priority;
#endif
};

int
process_in(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	return mod_net_process_in(module, nframes);
}

int
process_out(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	return mod_net_process_out(module, nframes);
}

static void
_quit(uv_async_t *handle)
{
	uv_stop(handle->loop);
}

static void
_thread(void *arg)
{
	Data *dat = arg;

#ifndef _WIN32 // POSIX only
	if(dat->schedp.sched_priority)
		pthread_setschedparam(dat->thread, SCHED_RR, &dat->schedp);
#else
	// Multimedia Class Scheduler Service
	DWORD dummy = 0;
	HANDLE task = AvSetMmThreadCharacteristics("Games", &dummy);
	if(!task)
		fprintf(stderr, "AvSetMmThreadCharacteristics error: %d\n", GetLastError()); //FIXME tjost_message
	else if(!AvSetMmThreadPriority(task, dat->mcss_sched_priority))
		fprintf(stderr, "AvSetMmThreadPriority error: %d\n", GetLastError()); //FIXME tjost_message

	/*
	Audio
	Capture
	Distribution
	Games
	Playback
	Pro Audio
	Window Manager
	*/

	/*
	AVRT_PRIORITY_CRITICAL (2)
	AVRT_PRIORITY_HIGH (1)
	AVRT_PRIORITY_LOW (-1)
	AVRT_PRIORITY_NORMAL (0)
	*/
#endif

	uv_run(&dat->loop, UV_RUN_DEFAULT);
}

int
add(Tjost_Module *module)
{
	Tjost_Host *host = module->host;
	lua_State *L = host->L;

	lua_getfield(L, 1, "uri");
	const char *uri = luaL_optstring(L, -1, NULL);
	lua_pop(L, 1);
	
	lua_getfield(L, 1, "rtprio");
	const int rtprio = luaL_optint(L, -1, 0);
	lua_pop(L, 1);
	
	lua_getfield(L, 1, "unroll");
	const char *unroll = luaL_optstring(L, -1, "full");
	lua_pop(L, 1);
	
	lua_getfield(L, 1, "offset");
	const float offset = luaL_optnumber(L, -1, 0.f);
	lua_pop(L, 1);

	Data *dat = tjost_alloc(module->host, sizeof(Data));
	memset(dat, 0, sizeof(Data));

	if(!(dat->net.rb_tx = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize ringbuffer");
	if(tjost_pipe_init(&dat->net.pipe_rx))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize pipe_rx");

	int err;
	if((err = uv_loop_init(&dat->loop)))
		MOD_ADD_ERR(module->host, MOD_NAME, "uv_loop_init failed");

	if((err = uv_async_init(&dat->loop, &dat->quit, _quit)))
		MOD_ADD_ERR(module->host, MOD_NAME, uv_err_name(err));

	dat->net.asio.data = module;
	if((err = uv_async_init(&dat->loop, &dat->net.asio, mod_net_asio)))
		MOD_ADD_ERR(module->host, MOD_NAME, uv_err_name(err));

	dat->net.sync.data = module;
	if((err = uv_timer_init(&dat->loop, &dat->net.sync)))
		MOD_ADD_ERR(module->host, MOD_NAME, uv_err_name(err));
	if((err = uv_timer_start(&dat->net.sync, mod_net_sync, 0, 1000))) // ms
		MOD_ADD_ERR(module->host, MOD_NAME, uv_err_name(err));

	if(osc_stream_init(&dat->loop, &dat->net.stream, uri, mod_net_recv_cb, mod_net_send_cb, module))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize socket");

	module->dat = dat;
	module->type = TJOST_MODULE_IN_OUT;

#ifndef _WIN32 // POSIX only
	dat->schedp.sched_priority = rtprio;
#else
	dat->mcss_sched_priority = rtprio;
#endif

	if(!strcmp(unroll, "none"))
		dat->net.unroll = OSC_UNROLL_MODE_NONE;
	else if(!strcmp(unroll, "partial"))
		dat->net.unroll = OSC_UNROLL_MODE_PARTIAL;
	else if(!strcmp(unroll, "full"))
		dat->net.unroll = OSC_UNROLL_MODE_FULL;
	else
		; //TODO warn

	if(offset > 0.f)
	{
		dat->net.delay_sec = (uint32_t)offset;
		dat->net.delay_nsec = (offset - dat->net.delay_sec) * 1e9;
	}
	else
	{
		dat->net.delay_sec = 0UL;
		dat->net.delay_nsec = 0UL;
	}

	if((err = uv_thread_create(&dat->thread, _thread, dat)))
		MOD_ADD_ERR(module->host, MOD_NAME, uv_err_name(err));

	return 0;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	int err;
	if((err = uv_async_send(&dat->quit)))
		fprintf(stderr, MOD_NAME": %s\n", uv_err_name(err));
	if((err = uv_thread_join(&dat->thread)))
		fprintf(stderr, MOD_NAME": %s\n", uv_err_name(err));

	osc_stream_deinit(&dat->net.stream);

	if((err = uv_timer_stop(&dat->net.sync)))
		fprintf(stderr, MOD_NAME": %s\n", uv_err_name(err));
	uv_close((uv_handle_t *)&dat->quit, NULL);
	uv_close((uv_handle_t *)&dat->net.asio, NULL);

	uv_loop_close(&dat->loop);

	if(dat->net.rb_tx)
		jack_ringbuffer_free(dat->net.rb_tx);
	tjost_pipe_deinit(&dat->net.pipe_rx);

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
