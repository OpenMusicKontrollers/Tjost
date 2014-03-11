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

#include <netaddr.h>

typedef struct _Data Data;

struct _Data {
	jack_ringbuffer_t *rb;

	NetAddr_UDP_Sender snk;

	uv_timer_t sync;
	jack_time_t sync_jack;
	struct timespec sync_osc;
	int32_t delay_sec;
	uint32_t delay_nsec;
	
	uv_async_t asio;
};

static uint8_t buffer [TJOST_BUF_SIZE] __attribute__((aligned (8)));
static char * bundle_str = "#bundle";
#define JAN_1970 (uint32_t)0x83aa7e80
#define NSEC_PER_NTP_SLICE 4.2950f

static void
_sync(uv_timer_t *handle, int status)
{
	Tjost_Module *module = handle->data;
	Data *dat = module->dat;

	clock_gettime(CLOCK_REALTIME, &dat->sync_osc);
	dat->sync_osc.tv_sec += JAN_1970;

	dat->sync_jack = jack_get_time();
}

static void
_asio(uv_async_t *handle, int status)
{
	Tjost_Module *module = handle->data;
	Data *dat = module->dat;

	Tjost_Event tev;
	while(jack_ringbuffer_read_space(dat->rb) >= sizeof(Tjost_Event))
	{
		jack_ringbuffer_peek(dat->rb, (char *)&tev, sizeof(Tjost_Event));
		if(jack_ringbuffer_read_space(dat->rb) >= sizeof(Tjost_Event) + tev.size)
		{
			jack_ringbuffer_read_advance(dat->rb, sizeof(Tjost_Event));

			jack_time_t usecs = jack_frames_to_time(module->host->client, tev.time) - dat->sync_jack;
		
			uint32_t size = tev.size;
			uint32_t sec = dat->sync_osc.tv_sec + JAN_1970 + dat->delay_sec;
			uint64_t nsec = dat->sync_osc.tv_nsec + usecs*1e3 + dat->delay_nsec;
			while(nsec > 1e9)
			{
				sec += 1;
				nsec -= 1e9;
			}
			uint32_t frac = nsec * NSEC_PER_NTP_SLICE;
			sec = htonl(sec);
			frac = htonl(frac);
			size = htonl(size);
			memcpy(buffer, bundle_str, 8);
			memcpy(buffer+8, &sec, 4);
			memcpy(buffer+12, &frac, 4);
			memcpy(buffer+16, &size, 4);

			jack_ringbuffer_read(dat->rb, (char *)(buffer+20), tev.size);

			if(jack_osc_message_check(buffer+20, tev.size))
				netaddr_udp_sender_send(&dat->snk, buffer, tev.size+20);
			else
				fprintf(stderr, "tx OSC message invalid\n");
		}
		else
			break;
	}
}

int
process(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	jack_nframes_t last = jack_last_frame_time(host->client);

	// handle events
	Eina_Inlist *l;
	Tjost_Event *tev;
	EINA_INLIST_FOREACH_SAFE(module->queue, l, tev)
	{
		if(tev->time >= last + nframes)
			break;

		if(tev->time == 0) // immediate execution
			tev->time = last;

		if(tev->time >= last)
		{
			if(jack_ringbuffer_write_space(dat->rb) < sizeof(Tjost_Event) + tev->size)
				tjost_host_message_push(host, "net_out: %s", "ringbuffer overflow");
			else
			{
				//tev->time -= last; // time relative to current period
				jack_ringbuffer_write(dat->rb, (const char *)tev, sizeof(Tjost_Event));
				jack_ringbuffer_write(dat->rb, (const char *)tev->buf, tev->size);
			}
		}
		else
			tjost_host_message_push(host, "mod_net_out: %s", "ignoring out-of-order event");

		module->queue = eina_inlist_remove(module->queue, EINA_INLIST_GET(tev));
		tjost_free(host, tev);
	}

	uv_async_send(&dat->asio);

	return 0;
}

void
add(Tjost_Module *module, int argc, const char **argv)
{
	Data *dat = tjost_alloc(module->host, sizeof(Data));

	uv_loop_t *loop = uv_default_loop();

	if(netaddr_udp_sender_init(&dat->snk, loop, argv[0]))
		fprintf(stderr, "could not initialize socket\n");
	if(!(dat->rb = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		fprintf(stderr, "could not initialize ringbuffer\n");

	dat->asio.data = module;
	uv_async_init(loop, &dat->asio, _asio);

	dat->sync.data = module;
	uv_timer_init(loop, &dat->sync);
	uv_timer_start(&dat->sync, _sync, 0, 1000); // ms

	module->dat = dat;
	module->type = TJOST_MODULE_OUTPUT;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	uv_timer_stop(&dat->sync);
	uv_close((uv_handle_t *)&dat->asio, NULL);

	if(dat->rb)
		jack_ringbuffer_free(dat->rb);

	netaddr_udp_sender_deinit(&dat->snk);

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
