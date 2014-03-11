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

#ifndef _WIN32 // POSIX only
#	include <pthread.h>
#else
#	include <windows.h>
#endif

#include <tjost.h>

#include <netaddr.h>

typedef struct _Data Data;

struct _Data {
	jack_ringbuffer_t *rb;

	NetAddr_UDP_Responder src;

	uv_timer_t sync;
	jack_time_t sync_jack;
	struct timespec sync_osc;

	uv_loop_t *loop;
	uv_thread_t thread;
	uv_async_t quit;

#ifndef _WIN32 // POSIX only
	struct sched_param schedp;
#else
	int mcss_sched_priority;
#endif
};

static const char * bundle_str = "#bundle";
#define JAN_1970 (uint32_t)0x83aa7e80
static double slice;

static void
_sync(uv_timer_t *handle, int status)
{
	Tjost_Module *module = handle->data;
	Data *dat = module->dat;

	clock_gettime(CLOCK_REALTIME, &dat->sync_osc);
	dat->sync_osc.tv_sec += JAN_1970;

	dat->sync_jack = jack_get_time();
}

static jack_nframes_t
_update_tstamp(Tjost_Module *module, uint64_t tstamp)
{
	Data *dat = module->dat;

	double diff; // time difference of OSC timestamp to current wall clock time (s)

	if(tstamp == 1ULL)
		return 0; // immediate execution

	int64_t tstamp_sec = tstamp >> 32;
	uint32_t tstamp_frac = tstamp & 0xffffffff;

	diff = tstamp_sec - dat->sync_osc.tv_sec;
	diff += tstamp_frac * slice;
	diff -= dat->sync_osc.tv_nsec * 1e-9;

	jack_time_t future = dat->sync_jack + diff*1e6; // us
	return jack_time_to_frames(module->host->client, future);
}

static void
_handle_message(Tjost_Module *module, jack_nframes_t tstamp, uint8_t *buf, size_t len)
{
	Data *dat = module->dat;

	if(jack_osc_message_check(buf, len))
	{
		Tjost_Event tev;
		tev.module = module;
		tev.time = tstamp;
		tev.size = len;

		if(jack_ringbuffer_write_space(dat->rb) < sizeof(Tjost_Event) + len)
			fprintf(stderr, "net_in: ringbuffer overflow\n");
		else
		{
			if(jack_ringbuffer_write(dat->rb, (const char *)&tev, sizeof(Tjost_Event)) != sizeof(Tjost_Event))
				fprintf(stderr, "net_in: ringbuffer write 1 error\n");
			if(jack_ringbuffer_write(dat->rb, (const char *)buf, len) != len)
				fprintf(stderr, "net_in: ringbuffer write 2 error\n");
		}
	}
	else
		fprintf(stderr, "rx OSC message invalid\n");
}

static void
_handle_bundle(Tjost_Module *module, uint8_t *buf, size_t len)
{
	if(strncmp((char *)buf, bundle_str, 8)) // bundle header valid?
		return;

	uint8_t *end = buf + len;
	uint8_t *ptr;

	uint64_t timetag = ntohll(*(uint64_t *)(buf + 8));
	jack_nframes_t tstamp = _update_tstamp(module, timetag);

	int has_nested_bundles = 0;

	ptr = buf + 16; // skip bundle header
	while(ptr < end)
	{
		int32_t *size = (int32_t *)ptr;
		int32_t hsize = htonl(*size);
		ptr += 4;

		switch(*ptr)
		{
			case '#':
				has_nested_bundles = 1;
				// ignore for now, messages are handled first
				break;
			case '/':
				_handle_message(module, tstamp, ptr, hsize);
				break;
			default:
				fprintf(stderr, "not an OSC bundle item '%c'\n", *ptr);
				return;
		}

		ptr += hsize;
	}

	if(!has_nested_bundles)
		return;

	ptr = buf + 16; // skip bundle header
	while(ptr < end)
	{
		int32_t *size = (int32_t *)ptr;
		int32_t hsize = htonl(*size);
		ptr += 4;

		if(*ptr == '#')
			_handle_bundle(module, ptr, hsize);

		ptr += hsize;
	}
}

static void
_netaddr_cb(NetAddr_UDP_Responder *netaddr, uint8_t *buf, size_t len, void *data)
{
	Tjost_Module *module = data;

	// check and insert messages into sorted list
	switch(*buf)
	{
		case '#':
			_handle_bundle(module, buf, len);
			break;
		case '/':
			_handle_message(module, 0, buf, len);
			break;
		default:
			fprintf(stderr, "not an OSC packet\n");
			break;
	}
}

int
process(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	Tjost_Event tev;
	while(jack_ringbuffer_read_space(dat->rb) >= sizeof(Tjost_Event))
	{
		if(jack_ringbuffer_peek(dat->rb, (char *)&tev, sizeof(Tjost_Event)) != sizeof(Tjost_Event))
			tjost_host_message_push(host, "net_in: %s", "ringbuffer peek error");

		if(jack_ringbuffer_read_space(dat->rb) >= sizeof(Tjost_Event) + tev.size)
		{
			jack_ringbuffer_read_advance(dat->rb, sizeof(Tjost_Event));

			uint8_t *bf = tjost_host_schedule_inline(host, module, tev.time, tev.size);
			if(jack_ringbuffer_read(dat->rb, (char *)bf, tev.size) != tev.size)
				tjost_host_message_push(host, "net_in: %s", "ringbuffer read error");
		}
		else
			break;
	}

	return 0;
}

static void
_quit(uv_async_t *handle, int status)
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

	uv_run(dat->loop, UV_RUN_DEFAULT);
}

void
add(Tjost_Module *module, int argc, const char **argv)
{
	Data *dat = tjost_alloc(module->host, sizeof(Data));

	dat->loop = uv_loop_new();

	if(!(dat->rb = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		fprintf(stderr, "could not initialize ringbuffer\n");

	uv_async_init(dat->loop, &dat->quit, _quit);

	dat->sync.data = module;
	uv_timer_init(dat->loop, &dat->sync);
	uv_timer_start(&dat->sync, _sync, 0, 1000); // ms

	if(netaddr_udp_responder_init(&dat->src, dat->loop, argv[0], _netaddr_cb, module))
		fprintf(stderr, "could not initialize socket\n");

	if(argv[1])
#ifndef _WIN32 // POSIX only
		dat->schedp.sched_priority = atoi(argv[1]);
#else
		dat->mcss_sched_priority = atoi(argv[1]);
#endif

	module->dat = dat;
	module->type = TJOST_MODULE_INPUT;

	uv_thread_create(&dat->thread, _thread, dat);
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	uv_async_send(&dat->quit);
	uv_thread_join(&dat->thread);

	netaddr_udp_responder_deinit(&dat->src);

	uv_timer_stop(&dat->sync);
	uv_close((uv_handle_t *)&dat->quit, NULL);

	uv_loop_delete(dat->loop);

	if(dat->rb)
		jack_ringbuffer_free(dat->rb);

	tjost_free(module->host, dat);
}

Eina_Bool
init()
{
	/*
	struct timespec res;
	clock_getres(CLOCK_REALTIME, &res);
	printf("clock resolution: %ld %ld\n", res.tv_sec, res.tv_nsec);
	*/

	slice = pow(2.f, -32.f); // smallest NTP time slice
	return EINA_TRUE;
}

void
deinit()
{
}

EINA_MODULE_INIT(init);
EINA_MODULE_SHUTDOWN(deinit);
