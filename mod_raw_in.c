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

#define MOD_NAME "raw_in"

typedef struct _Data Data;

struct _Data {
	snd_rawmidi_t *dev;
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

			uint8_t *bf = tjost_host_schedule_inline(host, module, tev.time, tev.size);
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
	if(snd_rawmidi_read(dat->dev, &m[1], 3) != 3)
		fprintf(stderr, MOD_NAME": reading MIDI failed\n");
	m[0] = m[1] & 0x0f;
	m[1] = m[1] & 0xf0;

	uint8_t buf [20];
	uint8_t *ptr = buf;
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

void
add(Tjost_Module *module, int argc, const char **argv)
{
	Data *dat = tjost_alloc(module->host, sizeof(Data));
	
	const char *device = "virtual";
	if( (argc > 0) && argv[0] )
		device = argv[0];
	if(snd_rawmidi_open(&dat->dev, NULL, device, 0))
		fprintf(stderr, MOD_NAME": opening MIDI failed\n");

	struct pollfd pfds;
	int count = snd_rawmidi_poll_descriptors_count(dat->dev); //TODO check count
	if(snd_rawmidi_poll_descriptors(dat->dev, &pfds, 1) != 1)
		fprintf(stderr, MOD_NAME": polling MIDI descriptors failed\n");

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

	if(snd_rawmidi_drop(dat->dev))
		fprintf(stderr, MOD_NAME": dropping MIDI failed\n");
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
