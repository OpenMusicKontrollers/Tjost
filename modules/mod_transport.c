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

#include <tjost.h>

#include <jack/transport.h>

#define MOD_NAME "transport"

static const char *state_path [] = {
	[JackTransportStopped]	= "/transport/stopped",
	[JackTransportRolling]	= "/transport/rolling",
	[JackTransportLooping]	= "/transport/looping",
	[JackTransportStarting]	= "/transport/starting"
};

static const size_t len = 44;

typedef struct _Data Data;

struct _Data {
	jack_transport_state_t state;
	int32_t bar;
	int32_t tick;
	int32_t beat;
	jack_nframes_t offset;
};

int
process_in(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	jack_nframes_t last = jack_last_frame_time(host->client);

	jack_position_t pos;
	jack_transport_state_t state = jack_transport_query(host->client, &pos);

	if(  (state != dat->state)
		|| (pos.bar != dat->bar)
		|| (pos.beat != dat->beat)
		|| (pos.tick != dat->tick)
		|| (pos.bbt_offset != dat->offset) )
	{
		osc_data_t *ptr = tjost_host_schedule_inline(module->host, module, last, len);
		osc_data_t *end = ptr + len;

		ptr = osc_set_path(ptr, end, state_path[state]);
		ptr = osc_set_fmt(ptr, end, "iiii");

		if(pos.valid & JackPositionBBT)
		{
			ptr = osc_set_int32(ptr, end, pos.bar);
			ptr = osc_set_int32(ptr, end, pos.beat);
			ptr = osc_set_int32(ptr, end, pos.tick);
		}
		else
		{
			ptr = osc_set_int32(ptr, end, 0);
			ptr = osc_set_int32(ptr, end, 0);
			ptr = osc_set_int32(ptr, end, 0);
		}

		if(pos.valid & JackBBTFrameOffset)
			ptr = osc_set_int32(ptr, end, (int32_t)pos.bbt_offset);
		else
			ptr = osc_set_int32(ptr, end, 0);


		dat->state = state;
		dat->bar = pos.bar;
		dat->beat = pos.beat;
		dat->tick = pos.tick;
		dat->offset = pos.bbt_offset;
	}

	return 0;
}

int
add(Tjost_Module *module, int argc, const char **argv)
{
	Tjost_Host *host = module->host;
	lua_State *L = host->L;

	module->dat = tjost_alloc(host, sizeof(Data));
	module->type = TJOST_MODULE_INPUT;

	return 0;
}

void
del(Tjost_Module *module)
{
	Tjost_Host *host = module->host;

	tjost_free(host, module->dat);
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
