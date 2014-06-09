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

#include <mod_net.h>

#define MOD_NAME "net"

void
mod_net_sync(uv_timer_t *handle)
{
	Tjost_Module *module = handle->data;
	Mod_Net *net = module->dat;

	clock_gettime(CLOCK_REALTIME, &net->sync_osc);
	net->sync_osc.tv_sec += JAN_1970;

	net->sync_jack = jack_get_time();
}

static jack_nframes_t
_update_tstamp(Tjost_Module *module, uint64_t tstamp)
{
	Mod_Net *net = module->dat;

	double diff; // time difference of OSC timestamp to current wall clock time (s)

	if(tstamp == 1ULL)
		return 0; // immediate execution

	uint32_t tstamp_sec = tstamp >> 32;
	uint32_t tstamp_frac = tstamp & 0xffffffff;

	if(tstamp_sec >= net->sync_osc.tv_sec)
		diff = tstamp_sec - net->sync_osc.tv_sec;
	else
		diff = -(net->sync_osc.tv_sec - tstamp_sec);
	diff += tstamp_frac * SLICE;
	diff -= net->sync_osc.tv_nsec * 1e-9;

	jack_time_t future = net->sync_jack + diff*1e6; // us
	return jack_time_to_frames(module->host->client, future);
}

static void
_inject_message(Tjost_Module *module, jack_nframes_t tstamp, osc_data_t *buf, size_t len)
{
	Mod_Net *net = module->dat;

	if(osc_message_check(buf, len))
	{
		Tjost_Event tev;
		tev.module = module;
		tev.time = tstamp;
		tev.size = len;

		if(jack_ringbuffer_write_space(net->rb_in) < sizeof(Tjost_Event) + len)
			fprintf(stderr, MOD_NAME": ringbuffer overflow\n");
		else
		{
			if(jack_ringbuffer_write(net->rb_in, (const char *)&tev, sizeof(Tjost_Event)) != sizeof(Tjost_Event))
				fprintf(stderr, MOD_NAME": ringbuffer write 1 error\n");
			if(jack_ringbuffer_write(net->rb_in, (const char *)buf, len) != len)
				fprintf(stderr, MOD_NAME": ringbuffer write 2 error\n");
		}
	}
	else
		fprintf(stderr, MOD_NAME": rx OSC message invalid\n");
}

// inject whole bundle as-is
static void
_inject_bundle(Tjost_Module *module, osc_data_t *buf, size_t len)
{
	Mod_Net *net = module->dat;
	
	if(osc_bundle_check(buf, len))
	{
		uint64_t timetag = ntohll(*(uint64_t *)(buf + 8));
		jack_nframes_t tstamp = _update_tstamp(module, timetag);

		Tjost_Event tev;
		tev.module = module;
		tev.time = tstamp;
		tev.size = len;

		if(jack_ringbuffer_write_space(net->rb_in) < sizeof(Tjost_Event) + len)
			fprintf(stderr, MOD_NAME": ringbuffer overflow\n");
		else
		{
			if(jack_ringbuffer_write(net->rb_in, (const char *)&tev, sizeof(Tjost_Event)) != sizeof(Tjost_Event))
				fprintf(stderr, MOD_NAME": ringbuffer write 1 error\n");
			if(jack_ringbuffer_write(net->rb_in, (const char *)buf, len) != len)
				fprintf(stderr, MOD_NAME": ringbuffer write 2 error\n");
		}
	}
	else
		fprintf(stderr, MOD_NAME": rx OSC bundle invalid\n");
}

// extract nested bundles with non-matching timestamps
static void
_unroll_partial(Tjost_Module *module, osc_data_t *buf, size_t len)
{
	if(strncmp((char *)buf, bundle_str, 8)) // bundle header valid?
		return;

	osc_data_t *end = buf + len;
	osc_data_t *ptr = buf;

	uint64_t timetag = ntohll(*(uint64_t *)(buf + 8));
	jack_nframes_t tstamp = _update_tstamp(module, timetag);

	int has_messages = 0;
	int has_nested_bundles = 0;

	ptr = buf + 16; // skip bundle header
	while(ptr < end)
	{
		int32_t *size = (int32_t *)ptr;
		int32_t hsize = htonl(*size);
		ptr += sizeof(int32_t);

		char c = *(char *)ptr;
		switch(c)
		{
			case '#':
				has_nested_bundles = 1;
				_unroll_partial(module, ptr, hsize);
				// ignore for now, messages are handled first
				break;
			case '/':
				has_messages = 1;
				// ignore for now
				break;
			default:
				fprintf(stderr, MOD_NAME": not an OSC bundle item '%c'\n", c);
				return;
		}

		ptr += hsize;
	}

	if(!has_nested_bundles)
	{
		if(has_messages)
			_inject_bundle(module, buf, len);
		return;
	}

	if(!has_messages)
		return; // discard empty bundles

	// repack bundle with messages only, ignoring nested bundles
	ptr = buf + 16; // skip bundle header
	osc_data_t *dst = ptr;
	if(has_messages)
		while(ptr < end)
		{
			int32_t *size = (int32_t *)ptr;
			int32_t hsize = htonl(*size);
			ptr += sizeof(int32_t);

			char c = *(char *)ptr;
			if(c == '/')
			{
				memmove(dst, ptr - sizeof(int32_t), sizeof(int32_t) + hsize);
				dst += hsize;
			}

			ptr += hsize;
		}

	size_t nlen = dst - buf; 
	_inject_bundle(module, buf, nlen);
}

// fully unroll bundle into single messages
static void
_unroll_full(Tjost_Module *module, osc_data_t *buf, size_t len)
{
	if(strncmp((char *)buf, bundle_str, 8)) // bundle header valid?
		return;

	osc_data_t *end = buf + len;
	osc_data_t *ptr = buf;

	uint64_t timetag = ntohll(*(uint64_t *)(buf + 8));
	jack_nframes_t tstamp = _update_tstamp(module, timetag);

	int has_nested_bundles = 0;

	ptr = buf + 16; // skip bundle header
	while(ptr < end)
	{
		int32_t *size = (int32_t *)ptr;
		int32_t hsize = htonl(*size);
		ptr += sizeof(int32_t);

		char c = *(char *)ptr;
		switch(c)
		{
			case '#':
				has_nested_bundles = 1;
				// ignore for now, messages are handled first
				break;
			case '/':
				_inject_message(module, tstamp, ptr, hsize);
				break;
			default:
				fprintf(stderr, MOD_NAME": not an OSC bundle item '%c'\n", c);
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
		ptr += sizeof(int32_t);

		char c = *(char *)ptr;
		if(c == '#')
			_unroll_full(module, ptr, hsize);

		ptr += hsize;
	}
}

void
mod_net_recv_cb(osc_data_t *buf, size_t len, void *data)
{
	Tjost_Module *module = data;
	Mod_Net *net = module->dat;

	// check and insert messages into sorted list
	char c = *(char *)buf;
	switch(c)
	{
		case '#':
			switch(net->unroll)
			{
				case UNROLL_NONE:
					_inject_bundle(module, buf, len);
					break;
				case UNROLL_PARTIAL:
					_unroll_partial(module, buf, len);
					break;
				case UNROLL_FULL:
					_unroll_full(module, buf, len);
					break;
			}
			break;
		case '/':
			_inject_message(module, 0, buf, len);
			break;
		default:
			fprintf(stderr, MOD_NAME": not an OSC packet\n");
			break;
	}
}

static void _next(Tjost_Module *module);

static void
_advance(size_t len, void *arg)
{
	Tjost_Module *module = arg;
	Mod_Net *net = module->dat;

	jack_ringbuffer_read_advance(net->rb_out, len);
	_next(module);
}

static void
_next(Tjost_Module *module)
{
	Mod_Net *net = module->dat;

	Tjost_Event tev;
	if(jack_ringbuffer_read_space(net->rb_out) >= sizeof(Tjost_Event))
	{
		jack_ringbuffer_peek(net->rb_out, (char *)&tev, sizeof(Tjost_Event));
		if(jack_ringbuffer_read_space(net->rb_out) >= sizeof(Tjost_Event) + tev.size)
		{
			jack_ringbuffer_read_advance(net->rb_out, sizeof(Tjost_Event));

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
			sec = htonl(sec);
			frac = htonl(frac);
		
			char ch;
			jack_ringbuffer_peek(net->rb_out, &ch, 1);
			switch(ch)
			{
				case '#':
				{
					static int32_t psize;
					psize = size;
					psize = ntohl(psize);

					uv_buf_t msg [3];
					msg[0].base = (char *)&psize;
					msg[0].len = sizeof(int32_t);

					jack_ringbuffer_data_t vec [2];
					jack_ringbuffer_get_read_vector(net->rb_out, vec);

					if(size <= vec[0].len)
					{
						//FIXME rewrite bundle timestamp
						msg[1].base = vec[0].buf;
						msg[1].len = size;
						
						assert((uintptr_t)msg[1].base % sizeof(uint32_t) == 0);

						msg[2].len = 0;
					}
					else // size > vec[0].len;
					{
						msg[1].base = vec[0].buf;
						msg[1].len = vec[0].len;

						assert((uintptr_t)msg[1].base % sizeof(uint32_t) == 0);

						assert(size - vec[0].len <= vec[1].len);
						msg[2].base = vec[1].buf;
						msg[2].len = size - vec[0].len;
						
						assert((uintptr_t)msg[2].base % sizeof(uint32_t) == 0);
					}

					switch(net->type)
					{
						case SOCKET_UDP:
							netaddr_udp_sender_send(&net->handle.udp_tx, &msg[1], msg[2].len > 0 ? 2 : 1, size, _advance, module);
							break;
						case SOCKET_TCP:
							netaddr_tcp_endpoint_send(&net->handle.tcp, &msg[0], msg[2].len > 0 ? 3 : 2, size, _advance, module);
							break;
					}
					break;
				}
				case '/':
				{
					uint32_t nsize = htonl(size);

					static uint8_t header [20];
					memcpy(header, bundle_str, 8);
					memcpy(header+8, &sec, 4);
					memcpy(header+12, &frac, 4);
					memcpy(header+16, &nsize, 4);
				
					static int32_t psize;
					psize = size + sizeof(header); // packet size for TCP preamble
					psize = ntohl(psize);

					uv_buf_t msg [4];
					msg[0].base = (char *)&psize;
					msg[0].len = sizeof(int32_t);

					msg[1].base = (char *)header;
					msg[1].len = sizeof(header);

					jack_ringbuffer_data_t vec [2];
					jack_ringbuffer_get_read_vector(net->rb_out, vec);

					if(size <= vec[0].len)
					{
						msg[2].base = vec[0].buf;
						msg[2].len = size;
					
						assert((uintptr_t)msg[2].base % sizeof(uint32_t) == 0);

						msg[3].len = 0;
					}
					else // size > vec[0].len
					{
						msg[2].base = vec[0].buf;
						msg[2].len = vec[0].len;
						
						assert((uintptr_t)msg[2].base % sizeof(uint32_t) == 0);

						assert(size - vec[0].len <= vec[1].len);
						msg[3].base = vec[1].buf;
						msg[3].len = size - vec[0].len;
						
						assert((uintptr_t)msg[3].base % sizeof(uint32_t) == 0);
					}

					switch(net->type)
					{
						case SOCKET_UDP:
							netaddr_udp_sender_send(&net->handle.udp_tx, &msg[1], msg[3].len > 0 ? 3 : 2, size, _advance, module);
							break;
						case SOCKET_TCP:
							netaddr_tcp_endpoint_send(&net->handle.tcp, &msg[0], msg[3].len > 0 ? 4 : 3, size, _advance, module);
							break;
					}
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

	_next(module);
}

int
mod_net_process_in(Tjost_Module *module, jack_nframes_t nframes)
{
	Tjost_Host *host = module->host;
	Mod_Net *net = module->dat;

	Tjost_Event tev;
	while(jack_ringbuffer_read_space(net->rb_in) >= sizeof(Tjost_Event))
	{
		if(jack_ringbuffer_peek(net->rb_in, (char *)&tev, sizeof(Tjost_Event)) != sizeof(Tjost_Event))
			tjost_host_message_push(host, MOD_NAME": %s", "ringbuffer peek error");

		if(jack_ringbuffer_read_space(net->rb_in) >= sizeof(Tjost_Event) + tev.size)
		{
			jack_ringbuffer_read_advance(net->rb_in, sizeof(Tjost_Event));

			osc_data_t *bf = tjost_host_schedule_inline(host, module, tev.time, tev.size);
			if(jack_ringbuffer_read(net->rb_in, (char *)bf, tev.size) != tev.size)
				tjost_host_message_push(host, MOD_NAME": %s", "ringbuffer read error");
		}
		else
			break;
	}

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

		if(jack_ringbuffer_write_space(net->rb_out) < sizeof(Tjost_Event) + tev->size)
			tjost_host_message_push(host, MOD_NAME": %s", "ringbuffer overflow");
		else
		{
			jack_ringbuffer_write(net->rb_out, (const char *)tev, sizeof(Tjost_Event));
			jack_ringbuffer_write(net->rb_out, (const char *)tev->buf, tev->size);
		}

		module->queue = eina_inlist_remove(module->queue, EINA_INLIST_GET(tev));
		tjost_free(host, tev);
	}

	if(count > 0)
	{
		int err;
		if((err = uv_async_send(&net->asio)))
			fprintf(stderr, MOD_NAME": %s\n", uv_err_name(err));
	}

	return 0;
}
