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

#include <string.h>

#include <mod_osc.h>
#include <tjost_config.h>

#ifdef HAS_METADATA_API
#	include <jack/metadata.h>
#	include <jackey.h>
#	include <jack_osc.h>
#endif // HAS_METADATA_API

int
osc_mark_port(jack_client_t *client, jack_port_t *port)
{
#ifdef HAS_METADATA_API
	jack_uuid_t uuid = jack_port_uuid(port);
	return jack_set_property(client, uuid, JACKEY_EVENT_TYPES, JACK_EVENT_TYPE__OSC, "text/plain");
#else
	return 0;
#endif // HAS_METADATA_API
}

int
osc_unmark_port(jack_client_t *client, jack_port_t *port)
{
#ifdef HAS_METADATA_API
	jack_uuid_t uuid = jack_port_uuid(port);
	return jack_remove_property(client, uuid, JACKEY_EVENT_TYPES);
#else
	return 0;
#endif // HAS_METADATA_API
}

int
osc_is_marked_port(jack_port_t *port)
{
#ifdef HAS_METADATA_API
	jack_uuid_t uuid = jack_port_uuid(port);
	char *value = NULL;
	char *type = NULL;

	int marked = 0;

	if( (jack_get_property(uuid, JACKEY_EVENT_TYPES, &value, &type) == 0) &&
			(strstr(value, JACK_EVENT_TYPE__OSC) != NULL) )
		marked = 1;
	if(value)
		jack_free(value);
	if(type)
		jack_free(type);
	return marked;
#else
	return 0;
#endif // HAS_METADATA_API
}
