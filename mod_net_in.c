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

#include <math.h>
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

// TCP is bidirectional
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

void
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
		fprintf(stderr, "AvSetMmThreadCharacteristics error: %d\n", GetLastError());
	else if(!AvSetMmThreadPriority(task, dat->mcss_sched_priority))
		fprintf(stderr, "AvSetMmThreadPriority error: %d\n", GetLastError());

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

void
add(Tjost_Module *module, int argc, const char **argv)
{
	Data *dat = tjost_alloc(module->host, sizeof(Data));

	if(!(dat->net.rb_out = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		fprintf(stderr, MOD_NAME": could not initialize ringbuffer\n");
	if(!(dat->net.rb_in = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		fprintf(stderr, MOD_NAME": could not initialize ringbuffer\n");

	int err;
	if((err = uv_loop_init(&dat->loop)))
		fprintf(stderr, MOD_NAME": uv_loop_init failed\n");

	if((err = uv_async_init(&dat->loop, &dat->quit, _quit)))
		fprintf(stderr, MOD_NAME": %s\n", uv_err_name(err));

	dat->net.asio.data = module;
	if((err = uv_async_init(&dat->loop, &dat->net.asio, mod_net_asio)))
		fprintf(stderr, MOD_NAME": %s\n", uv_err_name(err));

	dat->net.sync.data = module;
	if((err = uv_timer_init(&dat->loop, &dat->net.sync)))
		fprintf(stderr, MOD_NAME": %s\n", uv_err_name(err));
	if((err = uv_timer_start(&dat->net.sync, mod_net_sync, 0, 1000))) // ms
		fprintf(stderr, MOD_NAME": %s\n", uv_err_name(err));

	if(!strncmp(argv[0], "osc.udp://", 10) || !strncmp(argv[0], "osc.udp4://", 11) || !strncmp(argv[0], "osc.udp6://", 11))
		dat->net.type = SOCKET_UDP;
	else if(!strncmp(argv[0], "osc.tcp://", 10) || !strncmp(argv[0], "osc.tcp4://", 11) || !strncmp(argv[0], "osc.tcp6://", 11) || !strncmp(argv[0], "osc.slip.tcp://", 15) || !strncmp(argv[0], "osc.slip.tcp4://", 16) || !strncmp(argv[0], "osc.slip.tcp6://", 16))
		dat->net.type = SOCKET_TCP;
	else
		fprintf(stderr, MOD_NAME": unknown protocol '%s'\n", argv[0]);

	module->dat = dat;

	switch(dat->net.type)
	{
		case SOCKET_UDP:
			if(netaddr_udp_responder_init(&dat->net.handle.udp_rx, &dat->loop, argv[0], mod_net_recv_cb, module))
				fprintf(stderr, MOD_NAME": could not initialize socket\n");
			module->type = TJOST_MODULE_INPUT;
			break;
		case SOCKET_TCP:
			if(netaddr_tcp_endpoint_init(&dat->net.handle.tcp, NETADDR_TCP_RESPONDER, &dat->loop, argv[0], mod_net_recv_cb, module))
				fprintf(stderr, MOD_NAME": could not initialize socket\n");
			module->type = TJOST_MODULE_IN_OUT;
			break;
	}

	if( (argc > 1) && argv[1])
#ifndef _WIN32 // POSIX only
		dat->schedp.sched_priority = atoi(argv[1]);
#else
		dat->mcss_sched_priority = atoi(argv[1]);
#endif

	dat->net.unroll = UNROLL_NONE;
	if( (argc > 2) && argv[2])
	{
		if(!strcmp(argv[2], "none"))
			dat->net.unroll = UNROLL_NONE;
		else if(!strcmp(argv[2], "partial"))
			dat->net.unroll = UNROLL_PARTIAL;
		else if(!strcmp(argv[2], "full"))
			dat->net.unroll = UNROLL_FULL;
	}

	if((err = uv_thread_create(&dat->thread, _thread, dat)))
		fprintf(stderr, MOD_NAME": %s\n", uv_err_name(err));
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

	switch(dat->net.type)
	{
		case SOCKET_UDP:
			netaddr_udp_responder_deinit(&dat->net.handle.udp_rx);
			break;
		case SOCKET_TCP:
			netaddr_tcp_endpoint_deinit(&dat->net.handle.tcp);
			break;
	}

	if((err = uv_timer_stop(&dat->net.sync)))
		fprintf(stderr, MOD_NAME": %s\n", uv_err_name(err));
	uv_close((uv_handle_t *)&dat->quit, NULL);
	uv_close((uv_handle_t *)&dat->net.asio, NULL);

	uv_loop_close(&dat->loop);

	if(dat->net.rb_out)
		jack_ringbuffer_free(dat->net.rb_out);
	if(dat->net.rb_in)
		jack_ringbuffer_free(dat->net.rb_in);

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
