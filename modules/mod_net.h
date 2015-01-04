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

#ifndef _TJOST_MOD_NET_H_
#define _TJOST_MOD_NET_H_

#include <tjost.h>

#include <osc_stream.h>

#define JAN_1970 (uint32_t)0x83aa7e80
#define SLICE (double)0x0.00000001p0 // smallest NTP time slice
#define NSEC_PER_NTP_SLICE (1e-9 / SLICE)

#define bundle_str "#bundle"
	
typedef struct _Mod_Net	Mod_Net;
typedef enum _Unroll_Type {UNROLL_NONE, UNROLL_PARTIAL, UNROLL_FULL} Unroll_Type;

struct _Mod_Net {
	jack_ringbuffer_t *rb_tx;
	uv_async_t asio;
	Tjost_Pipe pipe_rx;

	osc_unroll_mode_t unroll;
	jack_nframes_t tstamp;

	osc_stream_t stream;

	uv_timer_t sync;
	jack_time_t sync_jack;
	struct timespec sync_osc;
	
	uint32_t delay_sec;
	uint32_t delay_nsec;
};

void mod_net_sync(uv_timer_t *handle);
void mod_net_asio(uv_async_t *handle);

void mod_net_recv_cb(osc_stream_t *stream, osc_data_t *buf, size_t len, void *data);
void mod_net_send_cb(osc_stream_t *stream, size_t len, void *data);

int mod_net_process_in(Tjost_Module *module, jack_nframes_t);
int mod_net_process_out(Tjost_Module *module, jack_nframes_t nframes);

#endif
