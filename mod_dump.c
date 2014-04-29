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

#include <assert.h>

#include <tjost.h>

#define MOD_NAME "dump"

typedef struct _Data Data;

struct _Data {
	jack_ringbuffer_t *rb;
	uv_async_t asio;
	jack_osc_data_t buffer [TJOST_BUF_SIZE] __attribute__((aligned (8)));
};
	

static void
_asio(uv_async_t *handle)
{
	Tjost_Module *module = handle->data;
	Data *dat = module->dat;

	Tjost_Event tev;
	while(jack_ringbuffer_read_space(dat->rb) >= sizeof(Tjost_Event))
	{
		jack_ringbuffer_peek(dat->rb, (char *)&tev, sizeof(Tjost_Event));
		if(jack_ringbuffer_read_space(dat->rb) >= sizeof(Tjost_Event) + tev.size)
		{
			jack_ringbuffer_read_advance(dat->rb, sizeof(Tjost_Event));

			jack_ringbuffer_data_t vec [2];
			jack_ringbuffer_get_read_vector(dat->rb, vec);

			jack_osc_data_t *buffer;
			if(vec[0].len >= tev.size)
				buffer = (jack_osc_data_t *)vec[0].buf;
			else
			{
				buffer = dat->buffer;
				jack_ringbuffer_read(dat->rb, (char *)buffer, tev.size);
			}

			assert((uintptr_t)buffer % sizeof(jack_osc_data_t) == 0);

			if(jack_osc_message_check(buffer, tev.size))
			{
				const char *path;
				const char *fmt;
				jack_osc_data_t *ptr = buffer;
				ptr = jack_osc_get_path(ptr, &path);
				ptr = jack_osc_get_fmt(ptr, &fmt);
				printf("%08"PRIX32" %4zu: %s %s", tev.time, tev.size, path, fmt+1);
				const char *type;
				for(type=fmt+1; *type; type++)
					switch(*type)
					{
						case 'i':
						{
							int32_t i;
							ptr = jack_osc_get_int32(ptr, &i);
							printf(" %"PRIi32, i);
							break;
						}
						case 'f':
						{
							float f;
							ptr = jack_osc_get_float(ptr, &f);
							printf(" %f", f);
							break;
						}
						case 's':
						{
							const char *s;
							ptr = jack_osc_get_string(ptr, &s);
							printf(" '%s'", s);
							break;
						}
						case 'b':
						{
							Jack_OSC_Blob b;
							ptr = jack_osc_get_blob(ptr, &b);
							printf(" (%"PRIi32")", b.size);
#if 0 //TODO
							{
								printf("[");
								int32_t i;
								uint8_t *bytes = b.payload;
								for(i=0; i<b.size; i++)
									printf("%02"PRIX8",", bytes[i]);
								printf("\b]");
							}
#endif
							break;
						}

						case 'h':
						{
							int64_t h;
							ptr = jack_osc_get_int64(ptr, &h);
								printf(" %"PRIi64, h);
							break;
						}
						case 'd':
						{
							double d;
							ptr = jack_osc_get_double(ptr, &d);
							printf(" %f", d);
							break;
						}
						case 't':
						{
							uint64_t t;
							ptr = jack_osc_get_timetag(ptr, &t);
							uint32_t sec = t >> 32;
							uint32_t frac = t & 0xFFFFFFFF;
							printf(" %08"PRIX32".%08"PRIX32, sec, frac);
							break;
						}
						case 'S':
						{
							const char *S;
							ptr = jack_osc_get_string(ptr, &S);
							printf(" '%s'", S);
							break;
						}
						case 'c':
						{
							char c;
							ptr = jack_osc_get_char(ptr, &c);
							printf(" %c", c);
							break;
						}
						case 'm':
						{
							uint8_t *m;
							ptr = jack_osc_get_midi(ptr, &m);
							printf(" [%02"PRIX8",%02"PRIX8",%02"PRIX8",%02"PRIX8"]", m[0], m[1], m[2], m[3]);
							break;
						}

						default:
							ptr = jack_osc_skip(*type, ptr);
							break;
					}
				printf("\n");

			}
			else
				fprintf(stderr, MOD_NAME": tx OSC message invalid\n");

			if(vec[0].len >= tev.size)
				jack_ringbuffer_read_advance(dat->rb, tev.size);
		}
		else
			break;
	}
}

int
process_out(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	jack_nframes_t last = jack_last_frame_time(host->client);

	unsigned int count = eina_inlist_count(module->queue);

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

		if(jack_ringbuffer_write_space(dat->rb) < sizeof(Tjost_Event) + tev->size)
			tjost_host_message_push(host, MOD_NAME": %s", "ringbuffer overflow");
		else
		{
			//tev->time -= last; // time relative to current period
			jack_ringbuffer_write(dat->rb, (const char *)tev, sizeof(Tjost_Event));
			jack_ringbuffer_write(dat->rb, (const char *)tev->buf, tev->size);
		}

		module->queue = eina_inlist_remove(module->queue, EINA_INLIST_GET(tev));
		tjost_free(host, tev);
	}

	if(count > 0)
	{
		int err;
		if((err = uv_async_send(&dat->asio)))
			tjost_host_message_push(host, MOD_NAME": %s", uv_err_name(err));
	}

	return 0;
}

void
add(Tjost_Module *module, int argc, const char **argv)
{
	Data *dat = tjost_alloc(module->host, sizeof(Data));

	if(!(dat->rb = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		fprintf(stderr, MOD_NAME": could not initialize ringbuffer\n");
	
	uv_loop_t *loop = uv_default_loop();

	dat->asio.data = module;
	int err;
	if((err = uv_async_init(loop, &dat->asio, _asio)))
		fprintf(stderr, MOD_NAME": %s\n", uv_err_name(err));

	module->dat = dat;
	module->type = TJOST_MODULE_OUTPUT;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	uv_close((uv_handle_t *)&dat->asio, NULL);

	if(dat->rb)
		jack_ringbuffer_free(dat->rb);

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
