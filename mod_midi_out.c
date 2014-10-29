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

#include <jack/midiport.h>

#define MOD_NAME "midi_out"

typedef struct _Data Data;

struct _Data {
	Tjost_Module *module;
	void *port_buf;
};

static int
_midi(osc_time_t time, const char *path, const char *fmt, osc_data_t *buf, size_t size, void *dat)
{
	Data *data = dat;

	jack_midi_event_t ev;
	uint8_t m [3];
	uint8_t *M;

	osc_data_t *ptr = buf;
	const char *type;
	for(type=fmt; *type!='\0'; type++)
		switch(*type)
		{
			case OSC_MIDI:
				ptr = osc_get_midi(ptr, &M);
				m[0] = M[1];
				m[1] = M[2];
				m[2] = M[3];

				if(jack_midi_max_event_size(data->port_buf) < 3)
					tjost_host_message_push(data->module->host, MOD_NAME": %s", "buffer overflow");
				else
					jack_midi_event_write(data->port_buf, time, m, 3);
				break;

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

int
process_out(jack_nframes_t nframes, void *arg)
{
	static Data data;

	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	jack_port_t *port = module->dat;

	if(!port)
		return 0;

	jack_nframes_t last = jack_last_frame_time(host->client);

	void *port_buf = jack_port_get_buffer(port, nframes);
	jack_midi_clear_buffer(port_buf);

	data.port_buf = port_buf;
	data.module = module;

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

		osc_dispatch_method(tev->time - last, tev->buf, tev->size, methods, NULL, NULL, &data);

		module->queue = eina_inlist_remove(module->queue, EINA_INLIST_GET(tev));
		tjost_free(host, tev);
	}

	return 0;
}

int
add(Tjost_Module *module)
{
	Tjost_Host *host = module->host;
	lua_State *L = host->L;
	jack_port_t *port = NULL;

	lua_getfield(L, 1, "port");
	const char *portn = luaL_optstring(L, -1, "midi.out");
	lua_pop(L, 1);

	if(!(port = jack_port_register(module->host->client, portn, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0)))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not register jack port");

	module->dat = port;
	module->type = TJOST_MODULE_OUTPUT;

	return 0;
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
