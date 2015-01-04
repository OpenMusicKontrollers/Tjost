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

#include <ctype.h>

#include <tjost.h>

#include <osc_stream.h>

#define MOD_NAME "serial"

#define SLICE (double)0x0.00000001p0 // smallest NTP time slice

typedef struct _Data Data;

struct _Data {
	Tjost_Pipe pipe_rx;
	Tjost_Pipe pipe_tx;
	
	osc_unroll_mode_t unroll;
	jack_nframes_t tstamp;

	osc_stream_t stream;
	
	jack_time_t sync_jack;
	struct timespec sync_osc;

	osc_data_t buf [TJOST_BUF_SIZE];
};

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
_inject_message(osc_data_t *buf, size_t len, void *dat)
{
	Tjost_Module *module = dat;
	Data *net = module->dat;

	if(tjost_pipe_produce(&net->pipe_rx, module, net->tstamp, len, buf))
		fprintf(stderr, MOD_NAME": tjost_pipe_produce error\n");
}

// inject whole bundle as-is
static void
_inject_bundle(osc_data_t *buf, size_t len, void *dat)
{
	Tjost_Module *module = dat;
	Data *net = module->dat;
	
	uint64_t timetag = be64toh(*(uint64_t *)(buf + 8));
	_inject_stamp(timetag, dat);
	
	if(tjost_pipe_produce(&net->pipe_rx, module, net->tstamp, len, buf))
		fprintf(stderr, MOD_NAME": tjost_pipe_produce error\n");
}

static osc_unroll_inject_t inject = {
	.stamp = _inject_stamp,
	.message = _inject_message,
	.bundle = _inject_bundle
};

static void
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

	if(tjost_pipe_consume(&dat->pipe_rx, _rx_alloc, _rx_sched, NULL))
		tjost_host_message_push(host, MOD_NAME": %s", "tjost_pipe_consume error");

	return 0;
}

static osc_data_t *
_tx_alloc(Tjost_Event *tev, void *arg)
{
	Tjost_Module *module = tev->module;
	Data *dat = module->dat;

	return dat->buf;
}

static int
_tx_sched(Tjost_Event *tev, osc_data_t *buf, void *arg)
{
	Tjost_Module *module = tev->module;
	Data *dat = module->dat;

	osc_stream_send(&dat->stream, buf, tev->size);
	
	return 0; // reload
}

static void
_send_cb(osc_stream_t *stream, size_t len, void *data)
{
	Tjost_Module *module = data;
	Data *dat = module->dat;
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

		//tev->time -= last; // time relative to current period
		if(tjost_pipe_produce(&dat->pipe_tx, module, tev->time, tev->size, tev->buf))
			tjost_host_message_push(host, MOD_NAME": %s", "tjost_pipe_produce error");

		module->queue = eina_inlist_remove(module->queue, EINA_INLIST_GET(tev));
		tjost_free(host, tev);
	}

	if(count > 0)
	{
		if(tjost_pipe_flush(&dat->pipe_tx))
			tjost_host_message_push(host, MOD_NAME": %s", "tjost_pipe_flush error");
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
	
	uv_loop_t *loop = uv_default_loop();

	if(tjost_pipe_init(&dat->pipe_tx))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize pipe_tx");
	if(tjost_pipe_listen_start(&dat->pipe_tx, loop, _tx_alloc, _tx_sched, NULL))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize pipe_tx");
	if(tjost_pipe_init(&dat->pipe_rx))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize pipe_rx");

	if(osc_stream_init(loop, &dat->stream, uri, _recv_cb, _send_cb, module))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize pipe");

	if(!strcmp(unroll, "none"))
		dat->unroll = OSC_UNROLL_MODE_NONE;
	else if(!strcmp(unroll, "partial"))
		dat->unroll = OSC_UNROLL_MODE_PARTIAL;
	else if(!strcmp(unroll, "full"))
		dat->unroll = OSC_UNROLL_MODE_FULL;

	module->dat = dat;
	module->type = TJOST_MODULE_IN_OUT;

	return 0;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	osc_stream_deinit(&dat->stream);

	tjost_pipe_listen_stop(&dat->pipe_tx);
	tjost_pipe_deinit(&dat->pipe_tx);
	tjost_pipe_deinit(&dat->pipe_rx);

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
