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

#include <alsa/asoundlib.h>

#define MOD_NAME "seq_out"

typedef struct _Data Data;

struct _Data {
	snd_seq_t *seq;
	snd_midi_event_t *trans;
	int port;
	int queue;

	Tjost_Pipe pipe;
	osc_data_t buffer [TJOST_BUF_SIZE];
};

static int
_midi(osc_time_t time, const char *path, const char *fmt, osc_data_t *buf, size_t size, void *arg)
{
	Tjost_Module *module = arg;
	Data *dat = module->dat;

	uint8_t m [3];
	uint8_t *M;

	osc_data_t *ptr = buf;
	const char *type;
	for(type=fmt; *type!='\0'; type++)
		switch(*type)
		{
			case OSC_MIDI:
			{
				ptr = osc_get_midi(ptr, &M);
				m[0] = M[1];
				m[1] = M[2];
				m[2] = M[3];
	
				snd_seq_event_t sev;
				snd_seq_ev_clear(&sev);
				if(snd_midi_event_encode(dat->trans, m, 3, &sev) != 3)
					fprintf(stderr, MOD_NAME": encode event failed\n");
			
				// relative timestamp
				struct snd_seq_real_time rtime = {
					.tv_sec = 0,
					.tv_nsec = (float)time * 1e9 / module->host->srate
				}; // TODO check for overflow

				// schedule midi
				snd_seq_ev_set_source(&sev, dat->port);
				snd_seq_ev_set_subs(&sev); // set broadcasting to subscribers
				snd_seq_ev_schedule_real(&sev, dat->queue, 1, &rtime); // relative scheduling
				snd_seq_event_output(dat->seq, &sev);
				snd_seq_drain_output(dat->seq);
			}
			default:
				ptr = osc_skip(*type, ptr);
				break;
		}

	return 1;
}

static osc_method_t methods [] = {
	{NULL, NULL, _midi},
	{NULL, NULL, NULL}
};

static osc_data_t *
_alloc(Tjost_Event *tev, void *arg)
{
	Tjost_Module *module = tev->module;
	Data *dat = module->dat;

	return dat->buffer;
}

static int
_sched(Tjost_Event *tev, osc_data_t *buf, void *arg)
{
	Tjost_Module *module = tev->module;

	osc_dispatch_method(tev->time, buf, tev->size, methods, NULL, NULL, module);

	return 0; // reload
}

int
process_out(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Data *dat = module->dat;
	Tjost_Host *host = module->host;

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

		tev->time -= last; // time relative to current period
		if(tjost_pipe_produce(&dat->pipe, module, tev->time, tev->size, tev->buf))
			tjost_host_message_push(host, MOD_NAME": %s", "ringbuffer overflow");

		module->queue = eina_inlist_remove(module->queue, EINA_INLIST_GET(tev));
		tjost_free(host, tev);
	}

	if(count > 0)
	{
		if(tjost_pipe_flush(&dat->pipe))
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

	lua_getfield(L, 1, "device");
	const char *device = luaL_optstring(L, -1, "seq");
	lua_pop(L, 1);

	if(snd_seq_open(&dat->seq, "hw", SND_SEQ_OPEN_OUTPUT, 0))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not create sequencer");
	if(snd_seq_set_client_name(dat->seq, jack_get_client_name(module->host->client)))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not set name");
	dat->port = snd_seq_create_simple_port(dat->seq, device,
		SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
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
	
	uv_loop_t *loop = uv_default_loop();

	if(tjost_pipe_init(&dat->pipe))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize tjost pipe");
	tjost_pipe_listen_start(&dat->pipe, loop, _alloc, _sched, NULL);

	module->dat = dat;
	module->type = TJOST_MODULE_OUTPUT;

	return 0;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	tjost_pipe_listen_stop(&dat->pipe);
	tjost_pipe_deinit(&dat->pipe);

	snd_midi_event_free(dat->trans);
	
	if(snd_seq_drain_output(dat->seq))
		fprintf(stderr, MOD_NAME": draining queue failed\n");
	snd_seq_stop_queue(dat->seq, dat->queue, NULL);
	if(snd_seq_free_queue(dat->seq, dat->queue))
		fprintf(stderr, MOD_NAME": freeing queue failed\n");
	if(snd_seq_close(dat->seq))
		fprintf(stderr, MOD_NAME": closing sequencer failed\n");

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
