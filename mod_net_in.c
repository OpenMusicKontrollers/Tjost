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
#include <pthread.h>
#include <sched.h>

#include <tjost.h>

#include <netaddr.h>

typedef struct _Data Data;

struct _Data {
	jack_ringbuffer_t *rb;

	NetAddr src;

	ev_timer sync;
	jack_time_t sync_jack;
	struct timespec sync_osc;

	struct ev_loop *loop;
	pthread_t thread;
	ev_async quit;
	struct sched_param schedp;
};

static const char * bundle_str = "#bundle";
#define JAN_1970 (uint32_t)0x83aa7e80
static double slice;

static void
_sync(struct ev_loop *loop, struct ev_timer *w, int revents)
{
	Tjost_Module *module = w->data;
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
_netaddr_cb(NetAddr *netaddr, uint8_t *buf, size_t len, void *data)
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
_quit(struct ev_loop *loop, struct ev_async *w, int revents)
{
	ev_break(loop, EVBREAK_ALL);
}

void *
_thread(void *arg)
{
	Data *dat = arg;

	if(dat->schedp.sched_priority)
		pthread_setschedparam(dat->thread, SCHED_RR, &dat->schedp);

	ev_run(dat->loop, EVRUN_NOWAIT);
	ev_run(dat->loop, 0);

	return NULL;
}

void
add(Tjost_Module *module, int argc, const char **argv)
{
	Data *dat = tjost_alloc(module->host, sizeof(Data));

	dat->loop = ev_loop_new(0);

	if(!(dat->rb = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		fprintf(stderr, "could not initialize ringbuffer\n");

	ev_async_init(&dat->quit, _quit);
	ev_async_start(dat->loop, &dat->quit);

	dat->sync.data = module;
	ev_timer_init(&dat->sync, _sync, 0.f, 1.f);
	ev_timer_start(dat->loop, &dat->sync);

	if(netaddr_udp_init(&dat->src, argv[0]))
		fprintf(stderr, "could not initialize socket\n");
	netaddr_udp_listen(&dat->src, dat->loop, _netaddr_cb, module);

	if(argv[1])
		dat->schedp.sched_priority = atoi(argv[1]);

	module->dat = dat;
	module->type = TJOST_MODULE_INPUT;

	pthread_create(&dat->thread, NULL, _thread, dat);
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	ev_async_send(dat->loop, &dat->quit);
	pthread_join(dat->thread, NULL);

	netaddr_udp_unlisten(&dat->src, dat->loop);
	netaddr_udp_deinit(&dat->src);

	ev_timer_stop(dat->loop, &dat->sync);
	ev_async_stop(dat->loop, &dat->quit);

	ev_loop_destroy(dat->loop);

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
