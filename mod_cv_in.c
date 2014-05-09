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

#include <mod_cv.h>

#define MOD_NAME "cv_in"

int
process_in(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	jack_port_t *port = module->dat;

	if(!port)
		return 0;

	jack_nframes_t last = jack_last_frame_time(host->client);

	jack_default_audio_sample_t *port_buf = jack_port_get_buffer(port, nframes);

	int32_t size = nframes * sizeof(jack_default_audio_sample_t);

	void *payload;

	size_t len = jack_osc_strlen(CV_PATH)
						 + jack_osc_fmtlen(CV_FMT) + 1
						 + 2 * sizeof(int32_t)
						 + round_to_four_bytes(size);
	jack_osc_data_t *ptr = tjost_host_schedule_inline(host, module, last, len);
	ptr = jack_osc_set_path(ptr, CV_PATH);
	ptr = jack_osc_set_fmt(ptr, CV_FMT);
	ptr = jack_osc_set_int32(ptr, last);
	ptr = jack_osc_set_blob_inline(ptr, size, &payload);

	jack_default_audio_sample_t *load = payload;
	jack_nframes_t i;
	for(i=0; i<nframes; i++)
		load[i] = port_buf[i]; //FIXME htonl

	return 0;
}

void
add(Tjost_Module *module, int argc, const char **argv)
{
	jack_port_t *port = NULL;

	if(!(port = jack_port_register(module->host->client, argv[0], JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0)))
		fprintf(stderr, MOD_NAME": could not register jack port\n");
	jack_uuid_t uuid = jack_port_uuid(port);
	jack_set_property(module->host->client, uuid, JACKEY_SIGNAL_TYPE, "CV", NULL);

	module->dat = port;

	module->type = TJOST_MODULE_INPUT;
}

void
del(Tjost_Module *module)
{
	jack_port_t *port = module->dat;

	if(port)
	{
		jack_uuid_t uuid = jack_port_uuid(port);
		jack_remove_property(module->host->client, uuid, JACKEY_SIGNAL_TYPE);
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
