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

#include <mod_net.h>

#define MOD_NAME "net"

static const char *resolve_msg = "/resolve\0\0\0\0,\0\0\0";

void
mod_net_sync(uv_timer_t *handle)
{
	Tjost_Module *module = handle->data;
	Mod_Net *net = module->dat;

	clock_gettime(CLOCK_REALTIME, &net->sync_osc);
	net->sync_osc.tv_sec += JAN_1970;

	net->sync_jack = jack_get_time();
}

static osc_data_t *
_rx_alloc(Tjost_Event *tev, void *arg)
{
	Tjost_Module *module = tev->module;
	Tjost_Host *host = module->host;
			
	return tjost_host_schedule_inline(host, module, tev->time, tev->size);
}

static int
_rx_sched(Tjost_Event *tev, osc_data_t *buf, void *arg)
{
	return 0; // reload
}

static void
_inject_stamp(uint64_t tstamp, void *dat)
{
	Tjost_Module *module = dat;
	Mod_Net *net = module->dat;

	double diff; // time difference of OSC timestamp to current wall clock time (s)

	if(tstamp == OSC_IMMEDIATE)
	{
		net->tstamp = 0; // immediate execution
		return;
	}

	uint32_t tstamp_sec = tstamp >> 32;
	uint32_t tstamp_frac = tstamp & 0xffffffff;

	if(tstamp_sec >= net->sync_osc.tv_sec)
		diff = tstamp_sec - net->sync_osc.tv_sec;
	else
		diff = -(net->sync_osc.tv_sec - tstamp_sec);
	diff += tstamp_frac * SLICE;
	diff -= net->sync_osc.tv_nsec * 1e-9;

	jack_time_t future = net->sync_jack + diff*1e6; // us
	net->tstamp = jack_time_to_frames(module->host->client, future);
}

static void
_inject_message(osc_data_t *buf, size_t len, void *dat)
{
	Tjost_Module *module = dat;
	Mod_Net *net = module->dat;

	jack_nframes_t tstamp = net->tstamp;

	if(tjost_pipe_produce(&net->pipe_rx, module, tstamp, len, buf))
		fprintf(stderr, MOD_NAME": tjost_pipe_produce error\n");
}

// inject whole bundle as-is
static void
_inject_bundle(osc_data_t *buf, size_t len, void *dat)
{
	Tjost_Module *module = dat;
	Mod_Net *net = module->dat;
	
	uint64_t timetag = be64toh(*(uint64_t *)(buf + 8));
	_inject_stamp(timetag, dat);
	jack_nframes_t tstamp = net->tstamp;

	if(tjost_pipe_produce(&net->pipe_rx, module, tstamp, len, buf))
		fprintf(stderr, MOD_NAME": tjost_pipe_produce error\n");
}

static osc_unroll_inject_t inject = {
	.stamp = _inject_stamp,
	.message = _inject_message,
	.bundle = _inject_bundle
};

void
mod_net_recv_cb(osc_stream_t *stream, osc_data_t *buf, size_t len, void *data)
{
	Tjost_Module *module = data;
	Mod_Net *net = module->dat;

	if(!osc_unroll_packet(buf, len, net->unroll, &inject, data))
		fprintf(stderr, MOD_NAME": OSC packet not valid\n");
}

static void _next(Tjost_Module *module);

void
mod_net_send_cb(osc_stream_t *stream, size_t len, void *data)
{
	Tjost_Module *module = data;
	Mod_Net *net = module->dat;

	jack_ringbuffer_read_advance(net->rb_tx, len);
	_next(module);
}

static void
_next(Tjost_Module *module)
{
	Mod_Net *net = module->dat;

	Tjost_Event tev;
	if(jack_ringbuffer_read_space(net->rb_tx) >= sizeof(Tjost_Event))
	{
		jack_ringbuffer_peek(net->rb_tx, (char *)&tev, sizeof(Tjost_Event));
		if(jack_ringbuffer_read_space(net->rb_tx) >= sizeof(Tjost_Event) + tev.size)
		{
			jack_ringbuffer_read_advance(net->rb_tx, sizeof(Tjost_Event));

			jack_time_t usecs = jack_frames_to_time(module->host->client, tev.time) - net->sync_jack;

			uint32_t size = tev.size;
			uint32_t sec;
			uint32_t frac;
			if(net->delay_sec || net->delay_nsec)
			{
				sec = net->sync_osc.tv_sec + net->delay_sec;
				uint64_t nsec = net->sync_osc.tv_nsec + usecs*1e3 + net->delay_nsec;
				while(nsec > 1e9)
				{
					sec += 1;
					nsec -= 1e9;
				}
				frac = nsec * NSEC_PER_NTP_SLICE;
			}
			else
			{
				// immediate execution
				sec = 0UL;
				frac = 1UL;
			}
			sec = htobe32(sec);
			frac = htobe32(frac);
		
			char ch;
			jack_ringbuffer_peek(net->rb_tx, &ch, 1);
			switch(ch)
			{
				case '#':
				{
					uv_buf_t msg [2];
					jack_ringbuffer_data_t vec [2];
					jack_ringbuffer_get_read_vector(net->rb_tx, vec);

					if(size <= vec[0].len)
					{
						//FIXME rewrite bundle timestamp
						msg[0].base = vec[0].buf;
						msg[0].len = size;
					
						msg[1].base = NULL;
						msg[1].len = 0;
					}
					else // size > vec[0].len;
					{
						msg[0].base = vec[0].buf;
						msg[0].len = vec[0].len;

						msg[1].base = vec[1].buf;
						msg[1].len = size - vec[0].len;
					}

					osc_stream_send2(&net->stream, msg, msg[1].len > 0 ? 2 : 1);

					break;
				}
				case '/':
				{
					uv_buf_t msg [2];
					jack_ringbuffer_data_t vec [2];
					jack_ringbuffer_get_read_vector(net->rb_tx, vec);

					if(size <= vec[0].len)
					{
						msg[0].base = vec[0].buf;
						msg[0].len = size;
				
						msg[1].base = NULL;
						msg[1].len = 0;
					}
					else // size > vec[0].len
					{
						msg[0].base = vec[0].buf;
						msg[0].len = vec[0].len;
						
						msg[1].base = vec[1].buf;
						msg[1].len = size - vec[0].len;
					}

					osc_stream_send2(&net->stream, msg, msg[1].len > 0 ? 2 : 1);

					break;
				}
				default:
					break; //TODO report error
			}
		}
	}
}

void
mod_net_asio(uv_async_t *handle)
{
	Tjost_Module *module = handle->data;

	// start sending loop
	_next(module);
}

int
mod_net_process_in(Tjost_Module *module, jack_nframes_t nframes)
{
	Tjost_Host *host = module->host;
	Mod_Net *net = module->dat;

	if(tjost_pipe_consume(&net->pipe_rx, _rx_alloc, _rx_sched, NULL))
		tjost_host_message_push(host, MOD_NAME": %s", "tjost_pipe_consume error");

	return 0;
}

int
mod_net_process_out(Tjost_Module *module, jack_nframes_t nframes)
{
	Tjost_Host *host = module->host;
	Mod_Net *net = module->dat;

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

		if(jack_ringbuffer_write_space(net->rb_tx) < sizeof(Tjost_Event) + tev->size)
			tjost_host_message_push(host, MOD_NAME": %s", "ringbuffer overflow");
		else
		{
			jack_ringbuffer_write(net->rb_tx, (const char *)tev, sizeof(Tjost_Event));
			jack_ringbuffer_write(net->rb_tx, (const char *)tev->buf, tev->size);
		}

		module->queue = eina_inlist_remove(module->queue, EINA_INLIST_GET(tev));
		tjost_free(host, tev);
	}

	if(count > 0)
	{
		int err;
		if((err = uv_async_send(&net->asio)))
			tjost_host_message_push(host, MOD_NAME": %s", uv_err_name(err));
	}

	return 0;
}
