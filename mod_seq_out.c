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
	osc_data_t buffer [TJOST_BUF_SIZE] __attribute__((aligned (8)));
};

static int
_midi(jack_nframes_t time, const char *path, const char *fmt, osc_data_t *buf, void *arg)
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
			case 'm':
			{
				ptr = osc_get_midi(ptr, &M);
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
				ptr = osc_skip(*type, ptr);
				break;
		}

	return 1;
}

static OSC_Method methods [] = {
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
			
			jack_ringbuffer_data_t vec [2];
			jack_ringbuffer_get_read_vector(dat->rb, vec);
			
			osc_data_t *buffer;
			if(vec[0].len >= tev.size)
				buffer = (osc_data_t *)vec[0].buf;
			else
			{
				buffer = dat->buffer;
				jack_ringbuffer_read(dat->rb, (char *)buffer, tev.size);
			}
			
			assert((uintptr_t)buffer % sizeof(uint32_t) == 0);

			osc_method_dispatch(tev.time, buffer, tev.size, methods, NULL, NULL, module);
			
			if(vec[0].len >= tev.size)
				jack_ringbuffer_read_advance(dat->rb, tev.size);
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
		else if(tev->time == 0) // immediate execution
			tev->time = last;
		else if(tev->time < last)
		{
			tjost_host_message_push(host, MOD_NAME": %s %i", "late event", tev->time - last);
			tev->time = last;
		}

		if(jack_ringbuffer_write_space(dat->rb) < sizeof(Tjost_Event) + tev->size)
			tjost_host_message_push(host, MOD_NAME": %s", "ringbuffer overflow");
		else
		{
			tev->time -= last; // time relative to current period
			jack_ringbuffer_write(dat->rb, (const char *)tev, sizeof(Tjost_Event));
			jack_ringbuffer_write(dat->rb, (const char *)tev->buf, tev->size);
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

	if(!(dat->rb = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize ringbuffer");
	
	uv_loop_t *loop = uv_default_loop();

	dat->asio.data = module;
	int err;
	if((err = uv_async_init(loop, &dat->asio, _asio)))
		MOD_ADD_ERR(module->host, MOD_NAME, uv_err_name(err));

	module->dat = dat;
	module->type = TJOST_MODULE_OUTPUT;

	return 0;
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
