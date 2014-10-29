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

#include <jackey.h>
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
	const char *portn = luaL_optstring(L, -1, "osc_in");
	lua_pop(L, 1);

	if(!(port = jack_port_register(module->host->client, portn, JACK_DEFAULT_OSC_TYPE, JackPortIsInput, 0)))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not register jack port");

#ifdef HAS_METADATA_API
	lua_getfield(L, 1, "pretty");
	const char *pretty = luaL_optstring(L, -1, "OSC Input");
	lua_pop(L, 1);

	jack_uuid_t uuid = jack_port_uuid(port);
	if(jack_set_property(module->host->client, uuid, JACK_METADATA_PRETTY_NAME, pretty, "text/plain"))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not set prettyname");
	if(jack_set_property(module->host->client, uuid, JACKEY_EVENT_TYPES, JACK_EVENT_TYPE__OSC, "text/plain"))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not set OSC event type");
#endif // HAS_METADATA_API

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
#ifdef HAS_METADATA_API
		jack_uuid_t uuid = jack_port_uuid(port);
		jack_remove_property(module->host->client, uuid, JACK_METADATA_PRETTY_NAME);
		jack_remove_property(module->host->client, uuid, JACKEY_EVENT_TYPES);
#endif
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
