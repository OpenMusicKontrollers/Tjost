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

#include <alsa/asoundlib.h>

#define MOD_NAME "seq_in"

typedef struct _Data Data;

struct _Data {
	snd_seq_t *seq;
	snd_midi_event_t *trans;
	int port;
	int queue;

	Tjost_Pipe pipe;
	uv_poll_t poll;
};

static osc_data_t *
_alloc(Tjost_Event *tev, void *arg)
{
	Tjost_Module *module = tev->module;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	return tjost_host_schedule_inline(host, module, tev->time, tev->size);
}

static int
_sched(Tjost_Event *tev, osc_data_t *buf, void *arg)
{
	return 0; // reload
}

int
process_in(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;
	
	if(tjost_pipe_consume(&dat->pipe, _alloc, _sched, NULL))
		tjost_host_message_push(host, MOD_NAME": %s", "tjost_pipe_consume error");

	return 0;
}

static void
_poll(uv_poll_t *handle, int status, int events)
{
	Tjost_Module *module = handle->data;
	Data *dat = module->dat;

	if(status < 0)
	{
		fprintf(stderr, MOD_NAME": %s\n", uv_err_name(status));
		return;
	}

	uint8_t m [4] = {0x0};
	snd_seq_event_t *sev;

	do
	{
		snd_seq_event_input(dat->seq, &sev);
		if(snd_midi_event_decode(dat->trans, &m[1], 0x10, sev) == 3)
		{
			const size_t len = 16;
			osc_data_t buf [len];
			osc_data_t *ptr = buf;
			osc_data_t *end = ptr + len;

			ptr = osc_set_path(ptr, end, "/midi");
			ptr = osc_set_fmt(ptr, end, "m");
			ptr = osc_set_midi(ptr, end, m);

			if(ptr && osc_check_message(buf, len))
			{
				if(tjost_pipe_produce(&dat->pipe, module, 0, len, buf))
					fprintf(stderr, MOD_NAME": tjost_pipe_produce error\n");
			}
			else
				fprintf(stderr, MOD_NAME": rx OSC message invalid\n");
		}
		else
			fprintf(stderr, MOD_NAME": event decode failed\n");
		if(snd_seq_free_event(sev))
			fprintf(stderr, MOD_NAME": event free failed\n");
	} while(snd_seq_event_input_pending(dat->seq, 0) > 0);
}

int
add(Tjost_Module *module)
{
	Tjost_Host *host = module->host;
	lua_State *L = host->L;
	Data *dat = tjost_alloc(module->host, sizeof(Data));
	memset(dat, 0, sizeof(Data));

	lua_getfield(L, 1, "device");
	const char *device = luaL_optstring(L, -1, "seq");
	lua_pop(L, 1);
	
	if(snd_seq_open(&dat->seq, "hw", SND_SEQ_OPEN_INPUT, 0))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not open sequencer");
	if(snd_seq_set_client_name(dat->seq, jack_get_client_name(module->host->client)))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not set name");
	dat->port = snd_seq_create_simple_port(dat->seq, device,
		SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
		SND_SEQ_PORT_TYPE_HARDWARE);
	if(dat->port < 0)
		MOD_ADD_ERR(module->host, MOD_NAME, "could not create port");
	dat->queue = snd_seq_alloc_queue(dat->seq);
	if(dat->queue < 0)
		MOD_ADD_ERR(module->host, MOD_NAME, "could not allocate queue");
	snd_seq_start_queue(dat->seq, dat->queue, NULL);

	if(snd_midi_event_new(0x10, &dat->trans))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not create event");
	snd_midi_event_init(dat->trans);
	snd_midi_event_reset_decode(dat->trans);
	snd_midi_event_no_status(dat->trans, 1);

	short events = POLLIN;
	struct pollfd pfds;
	int count = snd_seq_poll_descriptors_count(dat->seq, events); //TODO check count
	if(snd_seq_poll_descriptors(dat->seq, &pfds, 1, events) != 1)
		MOD_ADD_ERR(module->host, MOD_NAME, "could not get poll descriptors");

	if(tjost_pipe_init(&dat->pipe))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize tjost pipe");

	uv_loop_t *loop = uv_default_loop();

	int err;
	dat->poll.data = module;
	if((err = uv_poll_init(loop, &dat->poll, pfds.fd)))
		MOD_ADD_ERR(module->host, MOD_NAME, uv_err_name(err));
	if((err = uv_poll_start(&dat->poll, UV_READABLE, _poll)))
		MOD_ADD_ERR(module->host, MOD_NAME, uv_err_name(err));

	module->dat = dat;
	module->type = TJOST_MODULE_INPUT;

	return 0;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	uv_poll_stop(&dat->poll);

	tjost_pipe_deinit(&dat->pipe);

	snd_midi_event_free(dat->trans);
	
	if(snd_seq_drain_output(dat->seq))
		fprintf(stderr, MOD_NAME": draining output failed\n");
	snd_seq_stop_queue(dat->seq, dat->queue, NULL);
	if(snd_seq_free_queue(dat->seq, dat->queue))
		fprintf(stderr, MOD_NAME": freeing queue failed\n");
	if(snd_seq_close(dat->seq))
		fprintf(stderr, MOD_NAME": close sequencer failed\n");

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
