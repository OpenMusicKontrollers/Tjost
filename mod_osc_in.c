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
#include <mod_osc.h>

#include <jack_osc.h>

#define MOD_NAME "osc_in"

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

	jack_osc_event_t jev;
	uint32_t i;
	for(i=0; i<jack_osc_get_event_count(port_buf); i++)
	{
		jack_osc_event_get(&jev, port_buf, i);
		tjost_host_schedule(module->host, module, jev.time + last, jev.size, jev.buffer);
	}

	return 0;
}

int
add(Tjost_Module *module, int argc, const char **argv)
{
	Tjost_Host *host = module->host;
	lua_State *L = host->L;
	jack_port_t *port = NULL;

	lua_getfield(L, 1, "port");
	const char *portn = luaL_optstring(L, -1, "osc.in");
	lua_pop(L, 1);

	if(!(port = jack_port_register(module->host->client, portn, JACK_DEFAULT_OSC_TYPE, JackPortIsInput, 0)))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not register jack port");

	if(osc_mark_port(module->host->client, port))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not set event type");

	module->dat = port;
	module->type = TJOST_MODULE_INPUT;

	return 0;
}

void
del(Tjost_Module *module)
{
	jack_port_t *port = module->dat;

	if(port)
	{
		osc_unmark_port(module->host->client, port);
		jack_port_unregister(module->host->client, port);
	}
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
