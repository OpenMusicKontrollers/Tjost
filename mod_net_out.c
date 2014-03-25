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

typedef enum _Socket_Type {SOCKET_UDP, SOCKET_TCP} Socket_Type;
typedef struct _Data Data;

struct _Data {
	jack_ringbuffer_t *rb;

	Socket_Type type;
	union {
		NetAddr_UDP_Sender udp;
		NetAddr_TCP_Sender tcp;
	} snk;

	uv_timer_t sync;
	jack_time_t sync_jack;
	struct timespec sync_osc;
	uint32_t delay_sec;
	uint32_t delay_nsec;
	
	uv_async_t asio;
};

static const char *bundle_str = "#bundle";
#define JAN_1970 (uint32_t)0x83aa7e80
#define NSEC_PER_NTP_SLICE (1e-9 / 0x0.00000001p0)

static void
_sync(uv_timer_t *handle, int status)
{
	Tjost_Module *module = handle->data;
	Data *dat = module->dat;

	clock_gettime(CLOCK_REALTIME, &dat->sync_osc);
	dat->sync_osc.tv_sec += JAN_1970;

	dat->sync_jack = jack_get_time();
}

static void _next(Tjost_Module *module);

static void
_advance(size_t len, void *arg)
{
	Tjost_Module *module = arg;
	Data *dat = module->dat;

	jack_ringbuffer_read_advance(dat->rb, len);
	_next(module);
}

static void
_next(Tjost_Module *module)
{
	Data *dat = module->dat;

	Tjost_Event tev;
	if(jack_ringbuffer_read_space(dat->rb) >= sizeof(Tjost_Event))
	{
		jack_ringbuffer_peek(dat->rb, (char *)&tev, sizeof(Tjost_Event));
		if(jack_ringbuffer_read_space(dat->rb) >= sizeof(Tjost_Event) + tev.size)
		{
			jack_ringbuffer_read_advance(dat->rb, sizeof(Tjost_Event));

			jack_time_t usecs = jack_frames_to_time(module->host->client, tev.time) - dat->sync_jack;

			uint32_t size = tev.size;
			uint32_t sec = dat->sync_osc.tv_sec + dat->delay_sec;
			uint64_t nsec = dat->sync_osc.tv_nsec + usecs*1e3 + dat->delay_nsec;
			while(nsec > 1e9)
			{
				sec += 1;
				nsec -= 1e9;
			}
			uint32_t frac = nsec * NSEC_PER_NTP_SLICE;
			sec = htonl(sec);
			frac = htonl(frac);
			uint32_t nsize = htonl(size);

			static uint8_t header [20];
			memcpy(header, bundle_str, 8);
			memcpy(header+8, &sec, 4);
			memcpy(header+12, &frac, 4);
			memcpy(header+16, &nsize, 4);
			
			uv_buf_t msg [3];
			msg[0].base = (char *)header;
			msg[0].len = 20;

			jack_ringbuffer_data_t vec [2];
			jack_ringbuffer_get_read_vector(dat->rb, vec);

			if(size <= vec[0].len)
			{
				msg[1].base = vec[0].buf;
				msg[1].len = size;

				msg[2].len = 0;
			}
			else // size > vec[0].len //FIXME check for vec[1].len
			{
				msg[1].base = vec[0].buf;
				msg[1].len = vec[0].len;

				msg[2].base = vec[1].buf;
				msg[2].len = size - vec[0].len;
			}

			//if(jack_osc_message_check(buffer+20, tev.size)) //FIXME
				switch(dat->type)
				{
					case SOCKET_UDP:
						netaddr_udp_sender_send(&dat->snk.udp, msg, msg[2].len > 0 ? 3 : 2, _advance, module);
						break;
					case SOCKET_TCP:
						netaddr_tcp_sender_send(&dat->snk.tcp, msg, msg[2].len > 0 ? 3 : 2, _advance, module);
						break;
				}
			//else
			//	fprintf(stderr, "tx OSC message invalid\n");

		}
	}
}

static void
_asio(uv_async_t *handle, int status)
{
	Tjost_Module *module = handle->data;

	_next(module);
}

int
process(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	jack_nframes_t last = jack_last_frame_time(host->client);

	unsigned int count = eina_inlist_count(module->queue);

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

	if(count > 0)
		uv_async_send(&dat->asio);

	return 0;
}

void
add(Tjost_Module *module, int argc, const char **argv)
{
	Data *dat = tjost_alloc(module->host, sizeof(Data));

	dat->delay_sec = 0;
	//dat->delay_nsec = 1e6; // 1 ms
	dat->delay_nsec = 7e5; // 700 us

	uv_loop_t *loop = uv_default_loop();

	if(!strncmp(argv[0], "osc.udp://", 10))
		dat->type = SOCKET_UDP;
	else if(!strncmp(argv[0], "osc.tcp://", 10))
		dat->type = SOCKET_TCP;
	else
		; //FIXME error

	switch(dat->type)
	{
		case SOCKET_UDP:
			if(netaddr_udp_sender_init(&dat->snk.udp, loop, argv[0])) //TODO close?
				fprintf(stderr, "could not initialize socket\n");
			break;
		case SOCKET_TCP:
			if(netaddr_tcp_sender_init(&dat->snk.tcp, loop, argv[0])) //TODO close?
				fprintf(stderr, "could not initialize socket\n");
			break;
	}
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

	switch(dat->type)
	{
		case SOCKET_UDP:
			netaddr_udp_sender_deinit(&dat->snk.udp);
			break;
		case SOCKET_TCP:
			netaddr_tcp_sender_deinit(&dat->snk.tcp);
			break;
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
