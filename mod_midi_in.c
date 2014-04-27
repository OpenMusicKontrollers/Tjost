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

#define MOD_NAME "midi_in"

static const char *midi_path = "/midi";

int
process_in(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	jack_port_t *port = module->dat;

	if(!port)
		return 0;

	jack_nframes_t last = jack_last_frame_time(host->client);

	void *port_buf = jack_port_get_buffer(port, nframes);

	jack_midi_event_t mev;
	jack_nframes_t last_time;

	uint32_t n = jack_midi_get_event_count(port_buf);
	uint32_t last_i = 0;
	uint32_t i;
	for(i=0; i<n; i++)
	{
		jack_midi_event_get(&mev, port_buf, i);

		if(i == 0)
		{
			last_i = i;
			last_time = mev.time;
		}

		if( (mev.time > last_time) || (i == (n-1)) ) // if new event timestamp or last event
		{
			if(i == (n-1))
				i++;

			uint32_t N = i-last_i;

			size_t len = jack_osc_strlen(midi_path) + (quads(N+2) + N)*sizeof(uint32_t);
			jack_osc_data_t *ptr = tjost_host_schedule_inline(module->host, module, mev.time + last, len);

			ptr = jack_osc_set_path(ptr, midi_path);

			char *fmt = (char *)ptr;
			char *fmt_ptr = fmt;
			*fmt_ptr++ = ',';
			memset(fmt_ptr, 'm', N);
			fmt_ptr += N;
			*fmt_ptr++ = '\0';
			while( (fmt_ptr-fmt) % 4)
				*fmt_ptr++ = '\0';

			ptr = (jack_osc_data_t *)fmt_ptr;

			uint32_t j;
			for(j=last_i; j<i; j++)
			{
				jack_midi_event_get(&mev, port_buf, j);

				uint8_t m[4];

				m[0] = mev.buffer[0] & 0x0f; // extract channel nibble
				m[1] = mev.buffer[0] & 0xf0; // extract status nibble
				m[2] = mev.buffer[1];
				m[3] = mev.buffer[2];

				ptr = jack_osc_set_midi(ptr, m);
			}
		
			last_i = i;
			last_time = mev.time;
		}
		else // ev.time == last_time
			; // do nothing
	}

	return 0;
}

void
add(Tjost_Module *module, int argc, const char **argv)
{
	jack_port_t *port = NULL;

	if(!(port = jack_port_register(module->host->client, argv[0], JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0)))
		fprintf(stderr, MOD_NAME": could not register jack port\n");

	module->dat = port;

	module->type = TJOST_MODULE_INPUT;
}

void
del(Tjost_Module *module)
{
	jack_port_t *port = module->dat;

	if(port)
		jack_port_unregister(module->host->client, port);
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
