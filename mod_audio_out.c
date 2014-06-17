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
#include <mod_audio.h>

#define MOD_NAME "audio_out"

typedef struct _Data Data;

struct _Data {
	Tjost_Module *module;
	void *port_buf;
};

static int
_audio(jack_nframes_t time, const char *path, const char *fmt, osc_data_t *buf, void *dat)
{
	Data *data = dat;

	OSC_Blob b;
	jack_nframes_t last;
	int32_t sample_type;

	osc_data_t *ptr = buf;
	ptr = osc_get_int32(ptr, (int32_t *)&last); //TODO needs to be checked
	ptr = osc_get_int32(ptr, &sample_type);
	ptr = osc_get_blob(ptr, &b);

	jack_default_audio_sample_t *port_buf_out = data->port_buf;

	switch(sample_type)
	{
		case SAMPLE_TYPE_UINT8:
		{
			uint8_t *load = b.payload;
			int i;
			for(i=0; i<b.size/sizeof(uint8_t); i++)
				port_buf_out[i] = (jack_default_audio_sample_t)load[i] / 0xffU;
			break;
		}
		case SAMPLE_TYPE_INT8:
		{
			int8_t *load = b.payload;
			int i;
			for(i=0; i<b.size/sizeof(int8_t); i++)
				port_buf_out[i] = (jack_default_audio_sample_t)load[i] / 0x7f;
			break;
		}

		case SAMPLE_TYPE_UINT12:
		{
			uint16_t *load = b.payload;
			int i;
			for(i=0; i<b.size/sizeof(uint16_t); i++)
			{
				load[i] = ntohs(load[i]); // in-place byte-swapping
				port_buf_out[i] = (jack_default_audio_sample_t)load[i] / 0xfffU;
			}
			break;
		}
		case SAMPLE_TYPE_INT12:
		{
			int16_t *load = b.payload;
			int i;
			for(i=0; i<b.size/sizeof(int16_t); i++)
			{
				load[i] = ntohs(load[i]); // in-place byte-swapping
				port_buf_out[i] = (jack_default_audio_sample_t)load[i] / 0x7ff;
			}
			break;
		}

		case SAMPLE_TYPE_UINT16:
		{
			uint16_t *load = b.payload;
			int i;
			for(i=0; i<b.size/sizeof(uint16_t); i++)
			{
				load[i] = ntohs(load[i]); // in-place byte-swapping
				port_buf_out[i] = (jack_default_audio_sample_t)load[i] / 0xffffU;
			}
			break;
		}
		case SAMPLE_TYPE_INT16:
		{
			int16_t *load = b.payload;
			int i;
			for(i=0; i<b.size/sizeof(int16_t); i++)
			{
				load[i] = ntohs(load[i]); // in-place byte-swapping
				port_buf_out[i] = (jack_default_audio_sample_t)load[i] / 0x7fff;
			}
			break;
		}

		case SAMPLE_TYPE_UINT24:
		{
			uint32_t *load = b.payload;
			int i;
			for(i=0; i<b.size/sizeof(uint32_t); i++)
			{
				load[i] = ntohl(load[i]); // in-place byte-swapping
				port_buf_out[i] = (jack_default_audio_sample_t)load[i] / 0xffffffUL;
			}
			break;
		}
		case SAMPLE_TYPE_INT24:
		{
			int32_t *load = b.payload;
			int i;
			for(i=0; i<b.size/sizeof(int32_t); i++)
			{
				load[i] = ntohl(load[i]); // in-place byte-swapping
				port_buf_out[i] = (jack_default_audio_sample_t)load[i] / 0x7fffffL;
			}
			break;
		}

		case SAMPLE_TYPE_UINT32:
		{
			uint32_t *load = b.payload;
			int i;
			for(i=0; i<b.size/sizeof(uint32_t); i++)
			{
				load[i] = ntohl(load[i]); // in-place byte-swapping
				port_buf_out[i] = (jack_default_audio_sample_t)load[i] / 0xffffffffUL;
			}
			break;
		}
		case SAMPLE_TYPE_INT32:
		{
			int32_t *load = b.payload;
			int i;
			for(i=0; i<b.size/sizeof(int32_t); i++)
			{
				load[i] = ntohl(load[i]); // in-place byte-swapping
				port_buf_out[i] = (jack_default_audio_sample_t)load[i] / 0x7fffffffL;
			}
			break;
		}

		case SAMPLE_TYPE_FLOAT:
		{
			float *load = b.payload;
			int i;
			for(i=0; i<b.size/sizeof(float); i++)
			{
				//uint32_t *s = (uint32_t *)&load[i];
				//*s = ntohl(*s); // in-place byte-swapping
				port_buf_out[i] = (jack_default_audio_sample_t)load[i];
			}
			break;
		}
		case SAMPLE_TYPE_DOUBLE:
		{
			double *load = b.payload;
			int i;
			for(i=0; i<b.size/sizeof(float); i++)
			{
				uint64_t *s = (uint64_t *)&load[i];
				*s = ntohll(*s); // in-place byte-swapping
				port_buf_out[i] = (jack_default_audio_sample_t)load[i];
			}
			break;
		}
		default:
			break; //TODO warn
	}

	return 1;
}

static OSC_Method methods [] = {
	{AUDIO_PATH, AUDIO_FMT, _audio},
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

		osc_method_dispatch(tev->time - last, tev->buf, tev->size, methods, NULL, NULL, &data);

		module->queue = eina_inlist_remove(module->queue, EINA_INLIST_GET(tev));
		tjost_free(host, tev);
	}

	return 0;
}

int
add(Tjost_Module *module, int argc, const char **argv)
{
	jack_port_t *port = NULL;

	if(!(port = jack_port_register(module->host->client, argv[0], JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0)))
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
