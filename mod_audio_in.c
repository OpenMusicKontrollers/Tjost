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

static uint8_t buffer [TJOST_BUF_SIZE] __attribute__((aligned (8)));
static uint8_t payload [TJOST_BUF_SIZE] __attribute__((aligned (4)));
static const char *audio_path = "/audio";
static const char *audio_fmt = "iib";
	
static Sample_Type sample_type = SAMPLE_TYPE_FLOAT; //TODO make this configurable

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

	size_t size; 

	switch(sample_type)
	{
		case SAMPLE_TYPE_UINT8:
		{
			uint8_t *load = (uint8_t *)payload;
			size = nframes * sizeof(uint8_t);
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
				load[i] = port_buf[i] * 0xffU;
			break;
		}
		case SAMPLE_TYPE_INT8:
		{
			int8_t *load = (int8_t *)payload;
			size = nframes * sizeof(int8_t);
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
				load[i] = port_buf[i] * 0x7f;
			break;
		}

		case SAMPLE_TYPE_UINT12:
		{
			uint16_t *load = (uint16_t *)payload;
			size = nframes * sizeof(uint16_t);
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = port_buf[i] * 0xfffU;
				load[i] = htons(load[i]); // in-place byte-swapping
			}
			break;
		}
		case SAMPLE_TYPE_INT12:
		{
			int16_t *load = (int16_t *)payload;
			size = nframes * sizeof(int16_t);
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = port_buf[i] * 0x7ff;
				load[i] = htons(load[i]); // in-place byte-swapping
			}
			break;
		}

		case SAMPLE_TYPE_UINT16:
		{
			uint16_t *load = (uint16_t *)payload;
			size = nframes * sizeof(uint16_t);
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = port_buf[i] * 0xffffU;
				load[i] = htons(load[i]); // in-place byte-swapping
			}
			break;
		}
		case SAMPLE_TYPE_INT16:
		{
			int16_t *load = (int16_t *)payload;
			size = nframes * sizeof(int16_t);
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = port_buf[i] * 0x7fff;
				load[i] = htons(load[i]); // in-place byte-swapping
			}
			break;
		}

		case SAMPLE_TYPE_UINT24:
		{
			uint32_t *load = (uint32_t *)payload;
			size = nframes * sizeof(uint32_t);
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = port_buf[i] * 0xffffffUL;
				load[i] = htonl(load[i]); // in-place byte-swapping
			}
			break;
		}
		case SAMPLE_TYPE_INT24:
		{
			int32_t *load = (int32_t *)payload;
			size = nframes * sizeof(int32_t);
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = port_buf[i] * 0x7fffffL;
				load[i] = htonl(load[i]); // in-place byte-swapping
			}
			break;
		}

		case SAMPLE_TYPE_UINT32:
		{
			uint32_t *load = (uint32_t *)payload;
			size = nframes * sizeof(uint32_t);
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = port_buf[i] * 0xffffffffUL;
				load[i] = htonl(load[i]); // in-place byte-swapping
			}
			break;
		}
		case SAMPLE_TYPE_INT32:
		{
			int32_t *load = (int32_t *)payload;
			size = nframes * sizeof(int32_t);
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = port_buf[i] * 0x7fffffffL;
				load[i] = htonl(load[i]); // in-place byte-swapping
			}
			break;
		}

		case SAMPLE_TYPE_FLOAT:
		{
			float *load = (float *)payload;
			size = nframes * sizeof(float);
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = (float)port_buf[i];
				uint32_t *s = (uint32_t *)&load[i];
				*s = htonl(*s); // in-place byte-swapping
			}
			break;
		}
		case SAMPLE_TYPE_DOUBLE:
		{
			double *load = (double *)payload;
			size = nframes * sizeof(double);
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = (double)port_buf[i];
				uint64_t *s = (uint64_t *)&load[i];
				*s = htonll(*s); // in-place byte-swapping
			}
			break;
		}
		default:
			break; //TODO warn
	}

	uint8_t *ptr = buffer;
	ptr = jack_osc_set_path(ptr, audio_path);
	ptr = jack_osc_set_fmt(ptr, audio_fmt),
	ptr = jack_osc_set_int32(ptr, last);
	ptr = jack_osc_set_int32(ptr, sample_type);
	ptr = jack_osc_set_blob(ptr, size, (uint8_t *)payload);
	
	size_t len = ptr - buffer;
	tjost_host_schedule(module->host, module, last, len, buffer);

	return 0;
}

void
add(Tjost_Module *module, int argc, const char **argv)
{
	jack_port_t *port = NULL;

	if(!(port = jack_port_register(module->host->client, argv[0], JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0)))
		fprintf(stderr, "could not register jack port\n");

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
