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
	jack_ringbuffer_t *rb;
	uv_async_t asio;
};

static uint8_t buffer [TJOST_BUF_SIZE] __attribute__((aligned (8)));

static int
_midi(jack_nframes_t time, const char *path, const char *fmt, uint8_t *buf, void *arg)
{
	Tjost_Module *module = arg;
	Data *dat = module->dat;

	uint8_t m [3];
	uint8_t *M;

	uint8_t *ptr = buf;
	const char *type;
	for(type=fmt; *type!='\0'; type++)
		switch(*type)
		{
			case 'm':
			{
				ptr = jack_osc_get_midi(ptr, &M);
				m[0] = M[0] | M[1];
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
				ptr = jack_osc_skip(*type, ptr);
				break;
		}

	return 1;
}

static Jack_OSC_Method methods [] = {
	{NULL, NULL, _midi},
	{NULL, NULL, NULL}
};

static void
_asio(uv_async_t *handle)
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
			jack_ringbuffer_read(dat->rb, (char *)buffer, tev.size);

			jack_osc_method_dispatch(tev.time, buffer, tev.size, methods, module);
		}
		else
			break;
	}
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

		if(tev->time == 0) // immediate execution
			tev->time = last;

		if(tev->time >= last)
		{
			if(jack_ringbuffer_write_space(dat->rb) < sizeof(Tjost_Event) + tev->size)
				tjost_host_message_push(host, MOD_NAME": %s", "ringbuffer overflow");
			else
			{
				tev->time -= last; // time relative to current period
				jack_ringbuffer_write(dat->rb, (const char *)tev, sizeof(Tjost_Event));
				jack_ringbuffer_write(dat->rb, (const char *)tev->buf, tev->size);
			}
		}
		else
			tjost_host_message_push(host, MOD_NAME": %s", "ignoring out-of-order event");

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

void
add(Tjost_Module *module, int argc, const char **argv)
{
	Data *dat = tjost_alloc(module->host, sizeof(Data));

	const char *device = "midi.seq";
	if( (argc > 0) && argv[0] )
		device = argv[0];
	if(snd_seq_open(&dat->seq, "hw", SND_SEQ_OPEN_OUTPUT, 0))
		fprintf(stderr, MOD_NAME": creating sequencer failed\n");
	if(snd_seq_set_client_name(dat->seq, jack_get_client_name(module->host->client)))
		fprintf(stderr, MOD_NAME": setting name failed\n");
	dat->port = snd_seq_create_simple_port(dat->seq, device,
		SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
		SND_SEQ_PORT_TYPE_HARDWARE);
	if(!dat->port < 0)
		fprintf(stderr, MOD_NAME": creating port failed\n");
	dat->queue = snd_seq_alloc_queue(dat->seq);
	if(dat->queue < 0)
		fprintf(stderr, MOD_NAME": allocating queue failed\n");
	snd_seq_start_queue(dat->seq, dat->queue, NULL);

	if(snd_midi_event_new(0x10, &dat->trans))
		fprintf(stderr, MOD_NAME": creating event failed\n");
	snd_midi_event_init(dat->trans);
	snd_midi_event_reset_decode(dat->trans);
	snd_midi_event_no_status(dat->trans, 1);

	if(!(dat->rb = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		fprintf(stderr, MOD_NAME": could not initialize ringbuffer\n");
	
	uv_loop_t *loop = uv_default_loop();

	dat->asio.data = module;
	int err;
	if((err = uv_async_init(loop, &dat->asio, _asio)))
		fprintf(stderr, MOD_NAME": %s\n", uv_err_name(err));

	module->dat = dat;
	module->type = TJOST_MODULE_OUTPUT;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	uv_close((uv_handle_t *)&dat->asio, NULL);

	if(dat->rb)
		jack_ringbuffer_free(dat->rb);

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
