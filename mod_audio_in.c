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

#define MOD_NAME "audio_in"

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
			size = nframes * sizeof(uint8_t);
			break;
		case SAMPLE_TYPE_INT8:
			size = nframes * sizeof(int8_t);
			break;
		case SAMPLE_TYPE_UINT12:
			size = nframes * sizeof(uint16_t);
			break;
		case SAMPLE_TYPE_INT12:
			size = nframes * sizeof(int16_t);
			break;
		case SAMPLE_TYPE_UINT16:
			size = nframes * sizeof(uint16_t);
			break;
		case SAMPLE_TYPE_INT16:
			size = nframes * sizeof(int16_t);
			break;
		case SAMPLE_TYPE_UINT24:
			size = nframes * sizeof(uint32_t);
			break;
		case SAMPLE_TYPE_INT24:
			size = nframes * sizeof(int32_t);
			break;
		case SAMPLE_TYPE_UINT32:
			size = nframes * sizeof(uint32_t);
			break;
		case SAMPLE_TYPE_INT32:
			size = nframes * sizeof(int32_t);
			break;
		case SAMPLE_TYPE_FLOAT:
			size = nframes * sizeof(float);
			break;
		case SAMPLE_TYPE_DOUBLE:
			size = nframes * sizeof(double);
			break;
		default:
			//TODO warn
			break;
	}

	void *payload;

	size_t len = osc_strlen(AUDIO_PATH)
						 + osc_fmtlen(AUDIO_FMT)
						 + 3 * sizeof(int32_t)
						 + osc_padded_size(size);
	osc_data_t *ptr = tjost_host_schedule_inline(host, module, last, len);
	osc_data_t *end = ptr + len;
	ptr = osc_set_path(ptr, end, AUDIO_PATH);
	ptr = osc_set_fmt(ptr, end, AUDIO_FMT);
	ptr = osc_set_int32(ptr, end, last);
	ptr = osc_set_int32(ptr, end, sample_type);
	ptr = osc_set_blob_inline(ptr, end, size, &payload);

	switch(sample_type)
	{
		case SAMPLE_TYPE_UINT8:
		{
			uint8_t *load = payload;
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
				load[i] = port_buf[i] * 0xffU;
			break;
		}
		case SAMPLE_TYPE_INT8:
		{
			int8_t *load = payload;
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
				load[i] = port_buf[i] * 0x7f;
			break;
		}

		case SAMPLE_TYPE_UINT12:
		{
			uint16_t *load = payload;
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = port_buf[i] * 0xfffU;
				load[i] = htobe16(load[i]); // in-place byte-swapping
			}
			break;
		}
		case SAMPLE_TYPE_INT12:
		{
			int16_t *load = payload;
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = port_buf[i] * 0x7ff;
				load[i] = htobe16(load[i]); // in-place byte-swapping
			}
			break;
		}

		case SAMPLE_TYPE_UINT16:
		{
			uint16_t *load = payload;
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = port_buf[i] * 0xffffU;
				load[i] = htobe16(load[i]); // in-place byte-swapping
			}
			break;
		}
		case SAMPLE_TYPE_INT16:
		{
			int16_t *load = payload;
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = port_buf[i] * 0x7fff;
				load[i] = htobe16(load[i]); // in-place byte-swapping
			}
			break;
		}

		case SAMPLE_TYPE_UINT24:
		{
			uint32_t *load = payload;
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = port_buf[i] * 0xffffffUL;
				load[i] = htobe32(load[i]); // in-place byte-swapping
			}
			break;
		}
		case SAMPLE_TYPE_INT24:
		{
			int32_t *load = payload;
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = port_buf[i] * 0x7fffffL;
				load[i] = htobe32(load[i]); // in-place byte-swapping
			}
			break;
		}

		case SAMPLE_TYPE_UINT32:
		{
			uint32_t *load = payload;
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = port_buf[i] * 0xffffffffUL;
				load[i] = htobe32(load[i]); // in-place byte-swapping
			}
			break;
		}
		case SAMPLE_TYPE_INT32:
		{
			int32_t *load = payload;
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = port_buf[i] * 0x7fffffffL;
				load[i] = htobe32(load[i]); // in-place byte-swapping
			}
			break;
		}

		case SAMPLE_TYPE_FLOAT:
		{
			float *load = payload;
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = (float)port_buf[i];
				//uint32_t *s = (uint32_t *)&load[i];
				//*s = htobe32(*s); // in-place byte-swapping
			}
			break;
		}
		case SAMPLE_TYPE_DOUBLE:
		{
			double *load = payload;
			jack_nframes_t i;
			for(i=0; i<nframes; i++)
			{
				load[i] = (double)port_buf[i];
				uint64_t *s = (uint64_t *)&load[i];
				*s = htobe64(*s); // in-place byte-swapping
			}
			break;
		}
		default:
			break; //TODO warn
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
	const char *portn = luaL_optstring(L, -1, "audio.in");
	lua_pop(L, 1);

	if(!(port = jack_port_register(module->host->client, portn, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0)))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not register jack port");

	module->dat = port;

	module->type = TJOST_MODULE_INPUT;

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
