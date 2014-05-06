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

#define MOD_NAME "seq_in"

typedef struct _Data Data;

struct _Data {
	snd_seq_t *seq;
	snd_midi_event_t *trans;
	int port;
	int queue;
	jack_ringbuffer_t *rb;
	uv_poll_t poll;
};

int
process_in(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	Tjost_Event tev;
	while(jack_ringbuffer_read_space(dat->rb) >= sizeof(Tjost_Event))
	{
		if(jack_ringbuffer_peek(dat->rb, (char *)&tev, sizeof(Tjost_Event)) != sizeof(Tjost_Event))
			tjost_host_message_push(host, MOD_NAME": %s", "ringbuffer peek error");

		if(jack_ringbuffer_read_space(dat->rb) >= sizeof(Tjost_Event) + tev.size)
		{
			jack_ringbuffer_read_advance(dat->rb, sizeof(Tjost_Event));

			jack_osc_data_t *bf = tjost_host_schedule_inline(host, module, tev.time, tev.size);
			if(jack_ringbuffer_read(dat->rb, (char *)bf, tev.size) != tev.size)
				tjost_host_message_push(host, MOD_NAME": %s", "ringbuffer read error");
		}
		else
			break;
	}

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

	uint8_t m [4];
	snd_seq_event_t *sev;

	do
	{
		snd_seq_event_input(dat->seq, &sev);
		if(snd_midi_event_decode(dat->trans, &m[1], 0x10, sev) == 3)
		{
			m[0] = m[1] & 0x0f;
			m[1] = m[1] & 0xf0;

			jack_osc_data_t buf [20];
			jack_osc_data_t *ptr = buf;
			ptr = jack_osc_set_path(ptr, "/midi");
			ptr = jack_osc_set_fmt(ptr, "m");
			ptr = jack_osc_set_midi(ptr, m);
			size_t len = ptr - buf;

			Tjost_Event tev;
			tev.time = 0; // immediate execution
			tev.size = len;
			if(jack_osc_message_check(buf, tev.size))
			{
				if(jack_ringbuffer_write_space(dat->rb) < sizeof(Tjost_Event) + tev.size)
					fprintf(stderr, MOD_NAME": ringbuffer overflow\n");
				else
				{
					if(jack_ringbuffer_write(dat->rb, (const char *)&tev, sizeof(Tjost_Event)) != sizeof(Tjost_Event))
						fprintf(stderr, MOD_NAME": ringbuffer write 1 error\n");
					if(jack_ringbuffer_write(dat->rb, (const char *)buf, len) != len)
						fprintf(stderr, MOD_NAME": ringbuffer write 2 error\n");
				}
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

void
add(Tjost_Module *module, int argc, const char **argv)
{
	Data *dat = tjost_alloc(module->host, sizeof(Data));
	
	const char *device = "midi.seq";
	if( (argc > 0) && argv[0] )
		device = argv[0];
	if(snd_seq_open(&dat->seq, "hw", SND_SEQ_OPEN_INPUT, 0))
		fprintf(stderr, MOD_NAME": opening sequencer failed\n");
	if(snd_seq_set_client_name(dat->seq, jack_get_client_name(module->host->client)))
		fprintf(stderr, MOD_NAME": setting name failed\n");
	dat->port = snd_seq_create_simple_port(dat->seq, device,
		SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
		SND_SEQ_PORT_TYPE_HARDWARE);
	if(dat->port < 0)
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

	short events = POLLIN;
	struct pollfd pfds;
	int count = snd_seq_poll_descriptors_count(dat->seq, events); //TODO check count
	if(snd_seq_poll_descriptors(dat->seq, &pfds, 1, events) != 1)
		fprintf(stderr, MOD_NAME": getting poll descriptors failed\n");

	if(!(dat->rb = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		fprintf(stderr, MOD_NAME": could not initialize ringbuffer\n");
	
	uv_loop_t *loop = uv_default_loop();

	int err;
	dat->poll.data = module;
	if((err = uv_poll_init(loop, &dat->poll, pfds.fd)))
		fprintf(stderr, MOD_NAME": %s\n", uv_err_name(err));
	if((err = uv_poll_start(&dat->poll, UV_READABLE, _poll)))
		fprintf(stderr, MOD_NAME": %s\n", uv_err_name(err));

	module->dat = dat;
	module->type = TJOST_MODULE_INPUT;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	uv_poll_stop(&dat->poll);

	if(dat->rb)
		jack_ringbuffer_free(dat->rb);

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
