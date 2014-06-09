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
	osc_data_t buffer [TJOST_BUF_SIZE];
	int verbose;
};

static void
_serialize_message(osc_data_t *buffer, size_t len, int verbose)
{
	const char *path;
	const char *fmt;
	osc_data_t *ptr = buffer;
	ptr = osc_get_path(ptr, &path);
	ptr = osc_get_fmt(ptr, &fmt);
	printf(" %s %s", path, fmt+1);
	const char *type;
	for(type=fmt+1; *type; type++)
		switch(*type)
		{
			case 'i':
			{
				int32_t i;
				ptr = osc_get_int32(ptr, &i);
				printf(" %"PRIi32, i);
				break;
			}
			case 'f':
			{
				float f;
				ptr = osc_get_float(ptr, &f);
				printf(" %f", f);
				break;
			}
			case 's':
			{
				const char *s;
				ptr = osc_get_string(ptr, &s);
				printf(" '%s'", s);
				break;
			}
			case 'b':
			{
				OSC_Blob b;
				ptr = osc_get_blob(ptr, &b);
				printf(" (%"PRIi32")", b.size);
				if(verbose)
				{
					printf("[");
					int32_t i;
					uint8_t *bytes = b.payload;
					for(i=0; i<b.size; i++)
						printf("%02"PRIX8",", bytes[i]);
					printf("\b]");
				}
				break;
			}

			case 'h':
			{
				int64_t h;
				ptr = osc_get_int64(ptr, &h);
					printf(" %"PRIi64, h);
				break;
			}
			case 'd':
			{
				double d;
				ptr = osc_get_double(ptr, &d);
				printf(" %f", d);
				break;
			}
			case 't':
			{
				uint64_t t;
				ptr = osc_get_timetag(ptr, &t);
				uint32_t sec = t >> 32;
				uint32_t frac = t & 0xFFFFFFFF;
				printf(" %08"PRIX32".%08"PRIX32, sec, frac);
				break;
			}
			case 'S':
			{
				const char *S;
				ptr = osc_get_string(ptr, &S);
				printf(" '%s'", S);
				break;
			}
			case 'c':
			{
				char c;
				ptr = osc_get_char(ptr, &c);
				printf(" %c", c);
				break;
			}
			case 'm':
			{
				uint8_t *m;
				ptr = osc_get_midi(ptr, &m);
				printf(" [%02"PRIX8",%02"PRIX8",%02"PRIX8",%02"PRIX8"]", m[0], m[1], m[2], m[3]);
				break;
			}

			default:
				ptr = osc_skip(*type, ptr);
				break;
		}
	printf("\n");
}

static void
_serialize_bundle(osc_data_t *buffer, size_t len, int indent, int verbose)
{
	osc_data_t *end = buffer + len;
	osc_data_t *ptr = buffer;

	uint64_t timetag = ntohll(*(uint64_t *)(ptr + 8));

	uint32_t sec = timetag >> 32;
	uint32_t frac = timetag & 0xFFFFFFFF;
	int i;
	printf(" #bundle %08"PRIX32".%08"PRIX32"\n", sec, frac);
	for(i=0; i<indent; i++) printf("\t");
	printf("[\n");

	ptr += 16; // skip bundle header
	while(ptr < end)
	{
		int32_t *size = (int32_t *)ptr;
		int32_t hsize = htonl(*size);
		ptr += sizeof(int32_t);

		char c = *(char *)ptr;
		switch(c)
		{
			case '#':
				for(i=0; i<indent+1; i++) printf("\t");
				printf("%"PRIi32":", hsize);
				_serialize_bundle(ptr, hsize, indent+1, verbose);
				break;
			case '/':
				for(i=0; i<indent+1; i++) printf("\t");
				printf("%"PRIi32":", hsize);
				_serialize_message(ptr, hsize, verbose);
				break;
			default:
				fprintf(stderr, MOD_NAME": not an OSC bundle item '%c'\n", c);
				return;
		}

		ptr += hsize;
	}

	for(i=0; i<indent; i++) printf("\t");
	printf("]\n");
}

static void
_serialize_packet(jack_nframes_t time, osc_data_t *buffer, size_t size, int verbose)
{
	printf("%08"PRIX32" %4zu:", time, size);

	switch(*buffer)
	{
		case '#':
			if(osc_bundle_check(buffer, size))
				_serialize_bundle(buffer, size, 0, verbose);
			else
				fprintf(stderr, MOD_NAME": tx OSC bundle invalid\n");
			break;
		case '/':
			if(osc_message_check(buffer, size))
				_serialize_message(buffer, size, verbose);
			else
				fprintf(stderr, MOD_NAME": tx OSC message invalid\n");
			break;
		default:
			fprintf(stderr, MOD_NAME": tx OSC packet invalid\n");
			break;
	}
}

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

			osc_data_t *buffer;
			if(vec[0].len >= tev.size)
				buffer = (osc_data_t *)vec[0].buf;
			else
			{
				buffer = dat->buffer;
				jack_ringbuffer_read(dat->rb, (char *)buffer, tev.size);
			}

			assert((uintptr_t)buffer % sizeof(uint32_t) == 0);

			_serialize_packet(tev.time, buffer, tev.size, dat->verbose);

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

	uv_loop_t *loop = uv_default_loop();

	if( (argc > 0) && argv[0])
		dat->verbose = !strcmp(argv[0], "verbose") ? 1 : 0;
	else
		dat->verbose = 0;

	if(!(dat->rb = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		fprintf(stderr, MOD_NAME": could not initialize ringbuffer\n");

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
