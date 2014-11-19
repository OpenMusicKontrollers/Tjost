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

#include <ctype.h>

#include <tjost.h>

#include <osc_stream.h>

#define MOD_NAME "serial"

#define SLICE (double)0x0.00000001p0 // smallest NTP time slice

typedef struct _Data Data;

struct _Data {
	jack_ringbuffer_t *rb_in;
	jack_ringbuffer_t *rb_out;
	
	osc_unroll_mode_t unroll;
	jack_nframes_t tstamp;

	osc_stream_t stream;

	uv_async_t asio;
	
	jack_time_t sync_jack;
	struct timespec sync_osc;

	osc_data_t buf [TJOST_BUF_SIZE];
};

static void
_inject_stamp(uint64_t tstamp, void *dat)
{
	Tjost_Module *module = dat;
	Data *net = module->dat;

	double diff; // time difference of OSC timestamp to current wall clock time (s)

	if(tstamp == 1ULL)
	{
		net->tstamp = 0; // immediate execution
		return;
	}

	net->tstamp = 0;
	return; //FIXME synchronize

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
_rx_push(jack_ringbuffer_t *rb, jack_nframes_t time, osc_data_t *buf, size_t size)
{
	Tjost_Event tev;
	tev.time = time;
	tev.size = size;

	if(jack_ringbuffer_write_space(rb) < sizeof(Tjost_Event) + tev.size)
		fprintf(stderr, MOD_NAME": ringbuffer overflow\n");
	else
	{
		if(jack_ringbuffer_write(rb, (const char *)&tev, sizeof(Tjost_Event)) != sizeof(Tjost_Event))
			fprintf(stderr, MOD_NAME": ringbuffer write 1 error\n");
		if(jack_ringbuffer_write(rb, (const char *)buf, tev.size) != tev.size)
			fprintf(stderr, MOD_NAME": ringbuffer write 2 error\n");
	}
}

static void
_inject_message(osc_data_t *buf, size_t len, void *dat)
{
	Tjost_Module *module = dat;
	Data *net = module->dat;

	if(osc_check_message(buf, len))
		_rx_push(net->rb_in, net->tstamp, buf, len);
	else
		fprintf(stderr, MOD_NAME": rx OSC message invalid\n");
}

// inject whole bundle as-is
static void
_inject_bundle(osc_data_t *buf, size_t len, void *dat)
{
	Tjost_Module *module = dat;
	Data *net = module->dat;
	
	if(osc_check_bundle(buf, len))
	{
		uint64_t timetag = be64toh(*(uint64_t *)(buf + 8));
		_inject_stamp(timetag, dat);
		
		_rx_push(net->rb_in, net->tstamp, buf, len);
	}
	else
		fprintf(stderr, MOD_NAME": rx OSC bundle invalid\n");
}

static osc_unroll_inject_t inject = {
	.stamp = _inject_stamp,
	.message = _inject_message,
	.bundle = _inject_bundle
};

static
void
_recv_cb(osc_stream_t *stream, osc_data_t *buf, size_t size, void *data)
{
	Tjost_Module *module = data;
	Data *dat = module->dat;

	if(!osc_unroll_packet(buf, size, dat->unroll, &inject, module))
		fprintf(stderr, MOD_NAME": OSC packet unroll failed\n");
}

int
process_in(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	Tjost_Event tev;
	while(jack_ringbuffer_read_space(dat->rb_in) >= sizeof(Tjost_Event))
	{
		if(jack_ringbuffer_peek(dat->rb_in, (char *)&tev, sizeof(Tjost_Event)) != sizeof(Tjost_Event))
			tjost_host_message_push(host, MOD_NAME": %s", "ringbuffer peek error");

		if(jack_ringbuffer_read_space(dat->rb_in) >= sizeof(Tjost_Event) + tev.size)
		{
			jack_ringbuffer_read_advance(dat->rb_in, sizeof(Tjost_Event));

			osc_data_t *bf = tjost_host_schedule_inline(host, module, tev.time, tev.size);
			if(jack_ringbuffer_read(dat->rb_in, (char *)bf, tev.size) != tev.size)
				tjost_host_message_push(host, MOD_NAME": %s", "ringbuffer read error");
		}
		else
			break;
	}

	return 0;
}

static void
_send_cb(osc_stream_t *stream, size_t len, void *data)
{
	Tjost_Module *module = data;
	Data *dat = module->dat;
}

static void
_asio(uv_async_t *handle)
{
	Tjost_Module *module = handle->data;
	Data *dat = module->dat;

	Tjost_Event tev;
	while(jack_ringbuffer_read_space(dat->rb_out) >= sizeof(Tjost_Event))
	{
		jack_ringbuffer_peek(dat->rb_out, (char *)&tev, sizeof(Tjost_Event));
		if(jack_ringbuffer_read_space(dat->rb_out) >= sizeof(Tjost_Event) + tev.size)
		{
			jack_ringbuffer_read_advance(dat->rb_out, sizeof(Tjost_Event));

			jack_ringbuffer_data_t vec [2];
			jack_ringbuffer_get_read_vector(dat->rb_out, vec);

			osc_data_t *buffer;
			if(vec[0].len >= tev.size)
				buffer = (osc_data_t *)vec[0].buf;
			else
			{
				buffer = dat->buf;
				jack_ringbuffer_read(dat->rb_out, (char *)buffer, tev.size);
			}

			osc_stream_send(&dat->stream, buffer, tev.size);

			if(vec[0].len >= tev.size)
				jack_ringbuffer_read_advance(dat->rb_out, tev.size);
		}
		else
			break;
	}
}

int
process_out(jack_nframes_t nframes, void *arg)
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
		else if(tev->time == 0) // immediate execution
			tev->time = last;
		else if(tev->time < last)
		{
			tjost_host_message_push(host, MOD_NAME": %s %i", "late event", tev->time - last);
			tev->time = last;
		}

		if(jack_ringbuffer_write_space(dat->rb_out) < sizeof(Tjost_Event) + tev->size)
			tjost_host_message_push(host, MOD_NAME": %s", "ringbuffer overflow");
		else
		{
			//tev->time -= last; // time relative to current period
			jack_ringbuffer_write(dat->rb_out, (const char *)tev, sizeof(Tjost_Event));
			jack_ringbuffer_write(dat->rb_out, (const char *)tev->buf, tev->size);
		}

		module->queue = eina_inlist_remove(module->queue, EINA_INLIST_GET(tev));
		tjost_free(host, tev);
	}

	if(count > 0)
	{
		int err;
		if((err = uv_async_send(&dat->asio)))
			tjost_host_message_push(host, MOD_NAME": %s", uv_err_name(err));
	}

	return 0;
}

int
add(Tjost_Module *module)
{
	Tjost_Host *host = module->host;
	lua_State *L = host->L;
	Data *dat = tjost_alloc(module->host, sizeof(Data));
	memset(dat, 0, sizeof(Data));

	lua_getfield(L, 1, "uri");
	const char *uri = luaL_optstring(L, -1, NULL);
	lua_pop(L, 1);

	lua_getfield(L, 1, "unroll");
	const char *unroll = luaL_optstring(L, -1, "full");
	lua_pop(L, 1);

	if(!(dat->rb_in = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize ringbuffer");
	if(!(dat->rb_out = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize ringbuffer");
	
	uv_loop_t *loop = uv_default_loop();

	if(osc_stream_init(loop, &dat->stream, uri, _recv_cb, _send_cb, module))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize pipe");

	if(!strcmp(unroll, "none"))
		dat->unroll = OSC_UNROLL_MODE_NONE;
	else if(!strcmp(unroll, "partial"))
		dat->unroll = OSC_UNROLL_MODE_PARTIAL;
	else if(!strcmp(unroll, "full"))
		dat->unroll = OSC_UNROLL_MODE_FULL;

	int err;
	dat->asio.data = module;
	if((err = uv_async_init(loop, &dat->asio, _asio)))
		MOD_ADD_ERR(module->host, MOD_NAME, uv_err_name(err));

	module->dat = dat;
	module->type = TJOST_MODULE_IN_OUT;

	return 0;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	uv_close((uv_handle_t *)&dat->asio, NULL);
	osc_stream_deinit(&dat->stream);

	if(dat->rb_in)
		jack_ringbuffer_free(dat->rb_in);
	if(dat->rb_out)
		jack_ringbuffer_free(dat->rb_out);

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
