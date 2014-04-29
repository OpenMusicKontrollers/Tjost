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

#define MOD_NAME "raw_out"

typedef struct _Data Data;

struct _Data {
	snd_rawmidi_t *dev;
	jack_ringbuffer_t *rb;
	uv_async_t asio;
	jack_osc_data_t buffer [TJOST_BUF_SIZE] __attribute__((aligned (8)));
};

static int
_midi(jack_nframes_t time, const char *path, const char *fmt, jack_osc_data_t *buf, void *arg)
{
	Tjost_Module *module = arg;
	Data *dat = module->dat;

	jack_midi_event_t ev;
	uint8_t m [3];
	uint8_t *M;

	jack_osc_data_t *ptr = buf;
	const char *type;
	for(type=fmt; *type!='\0'; type++)
		switch(*type)
		{
			case 'm':
				ptr = jack_osc_get_midi(ptr, &M);
				m[0] = M[0] | M[1];
				m[1] = M[2];
				m[2] = M[3];
			
				if(snd_rawmidi_write(dat->dev, m, 3) != 3)
					fprintf(stderr, MOD_NAME": writing MIDI failed\n");
				if(snd_rawmidi_drain(dat->dev))
					fprintf(stderr, MOD_NAME": draining MIDI failed\n");
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
			
			jack_ringbuffer_data_t vec [2];
			jack_ringbuffer_get_read_vector(dat->rb, vec);
			
			jack_osc_data_t *buffer;
			if(vec[0].len >= tev.size)
				buffer = (jack_osc_data_t *)vec[0].buf;
			else
			{
				buffer = dat->buffer;
				jack_ringbuffer_read(dat->rb, (char *)buffer, tev.size);
			}
			
			assert((uintptr_t)buffer % sizeof(jack_osc_data_t) == 0);

			jack_osc_method_dispatch(tev.time, buffer, tev.size, methods, module);
			
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

void
add(Tjost_Module *module, int argc, const char **argv)
{
	Data *dat = tjost_alloc(module->host, sizeof(Data));

	const char *device = "virtual";
	if( (argc > 0) && argv[0] )
		device = argv[0];
	if(snd_rawmidi_open(NULL, &dat->dev, device, 0))
		fprintf(stderr, MOD_NAME": opening MIDI failed\n");

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
	
	if(snd_rawmidi_drain(dat->dev))
		fprintf(stderr, MOD_NAME": draining MIDI failed\n");
	if(snd_rawmidi_close(dat->dev))
		fprintf(stderr, MOD_NAME": closing MIDI failed\n");

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
