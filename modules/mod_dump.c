/*
 * Copyright (c) 2015 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

#include <assert.h>

#include <tjost.h>

#define MOD_NAME "dump"

typedef struct _Data Data;

struct _Data {
	Tjost_Pipe pipe;
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
			case OSC_INT32:
			{
				int32_t i;
				ptr = osc_get_int32(ptr, &i);
				printf(" %"PRIi32, i);
				break;
			}
			case OSC_FLOAT:
			{
				float f;
				ptr = osc_get_float(ptr, &f);
				printf(" %f", f);
				break;
			}
			case OSC_STRING:
			{
				const char *s;
				ptr = osc_get_string(ptr, &s);
				printf(" '%s'", s);
				break;
			}
			case OSC_BLOB:
			{
				osc_blob_t b;
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

			case OSC_INT64:
			{
				int64_t h;
				ptr = osc_get_int64(ptr, &h);
					printf(" %"PRIi64, h);
				break;
			}
			case OSC_DOUBLE:
			{
				double d;
				ptr = osc_get_double(ptr, &d);
				printf(" %f", d);
				break;
			}
			case OSC_TIMETAG:
			{
				uint64_t t;
				ptr = osc_get_timetag(ptr, &t);
				uint32_t sec = t >> 32;
				uint32_t frac = t & 0xFFFFFFFF;
				printf(" %08"PRIX32".%08"PRIX32, sec, frac);
				break;
			}
			case OSC_SYMBOL:
			{
				const char *S;
				ptr = osc_get_string(ptr, &S);
				printf(" '%s'", S);
				break;
			}
			case OSC_CHAR:
			{
				char c;
				ptr = osc_get_char(ptr, &c);
				printf(" %c", c);
				break;
			}
			case OSC_MIDI:
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

	uint64_t timetag = be64toh(*(uint64_t *)(ptr + 8));

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
		int32_t hsize = be32toh(*size);
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
			if(osc_check_bundle(buffer, size))
				_serialize_bundle(buffer, size, 0, verbose);
			else
				fprintf(stderr, MOD_NAME": tx OSC bundle invalid\n");
			break;
		case '/':
			if(osc_check_message(buffer, size))
				_serialize_message(buffer, size, verbose);
			else
				fprintf(stderr, MOD_NAME": tx OSC message invalid\n");
			break;
		default:
			fprintf(stderr, MOD_NAME": tx OSC packet invalid\n");
			break;
	}
}

static osc_data_t *
_alloc(Tjost_Event *tev, void *arg)
{
	Data *dat = tev->module->dat;

	return dat->buffer;
}

static int
_sched(Tjost_Event *tev, osc_data_t *buf, void *arg)
{
	Data *dat = tev->module->dat;

	_serialize_packet(tev->time, buf, tev->size, dat->verbose);

	return 0; // reload
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

		//tev->time -= last; // time relative to current period
		if(tjost_pipe_produce(&dat->pipe, module, tev->time, tev->size, tev->buf))
			tjost_host_message_push(host, MOD_NAME": %s", "tjost_pipe_produce error");

		module->queue = eina_inlist_remove(module->queue, EINA_INLIST_GET(tev));
		tjost_free(host, tev);
	}

	if(count > 0)
	{
		if(tjost_pipe_flush(&dat->pipe))
			tjost_host_message_push(host, MOD_NAME": %s", "tjost_pipe_flush error");
	}

	return 0;
}

int
add(Tjost_Module *module)
{
	Tjost_Host *host = module->host;
	lua_State *L = host->L;
	Data *dat = tjost_alloc(module->host, sizeof(Data));
	memset(dat, 0, sizeof(Data));

	uv_loop_t *loop = uv_default_loop();

	lua_getfield(L, 1, "verbose");
	dat->verbose = luaL_optint(L, -1, 0) ? 1 : 0;
	lua_pop(L, 1);

	if(tjost_pipe_init(&dat->pipe))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize tjost pipe");
	if(tjost_pipe_listen_start(&dat->pipe, loop, _alloc, _sched, NULL))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not start listening in tjost pipe");

	module->dat = dat;
	module->type = TJOST_MODULE_OUTPUT;

	return 0;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	tjost_pipe_listen_stop(&dat->pipe);
	tjost_pipe_deinit(&dat->pipe);

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
