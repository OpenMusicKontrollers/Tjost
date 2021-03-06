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

#include <ctype.h>

#include <tjost.h>

#define MOD_NAME "send"

typedef struct _Data Data;

struct _Data {
	Tjost_Pipe pipe;

	uv_tty_t recv_client;
	char line [1024];
	osc_data_t buffer [TJOST_BUF_SIZE];
};

static void
_tty_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	Tjost_Module *module = handle->data;
	Data *dat = module->dat;

	buf->base = dat->line;
	buf->len = sizeof(dat->line);
}

static void
_tty_recv_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	Tjost_Module *module = stream->data;
	Data *dat = module->dat;

	if(nread > 0)
	{
		char *s = buf->base;

		osc_data_t *ptr = dat->buffer;
		osc_data_t *endptr = ptr + TJOST_BUF_SIZE;
		char *cur;
		char *end;
		char *path;
		char *fmt;

		cur = s;
		end = cur;
		while(isprint(*end) && (*end != ' '))
			end++;
		*end++ = '\0';
		path = cur;
		if(!osc_check_path(path))
		{
			fprintf(stderr, MOD_NAME": invalid path: %s\n", path);
			return;
		}
		ptr = osc_set_path(ptr, endptr, path);

		// skip whitespace
		while( (end < s+nread) && isspace(*end))
			end++;
	
		if(end < s + nread)
		{
			cur = end;
			end = cur;
			while(isprint(*end) && (*end != ' '))
				end++;
			*end++ = '\0';
			fmt = cur;
		}
		else
			fmt = "";
		if(!osc_check_fmt(fmt, 0))
		{
			fprintf(stderr, MOD_NAME": invalid format: %s\n", fmt);
			return;
		}
		ptr = osc_set_fmt(ptr, endptr, fmt);

		// skip whitespace
		while( (end < s+nread) && isspace(*end))
			end++;

		char *type;
		for(type=fmt; *type; type++)
		{
			if( (*type!='T') && (*type!='F') && (*type!='N') && (*type!='I') )
			{
				if(end < s + nread)
				{
					cur = end;
					end = cur;
					while(isprint(*end) && (*end != ' '))
						end++;
					*end++ = '\0';
				}
				else
					fprintf(stderr, MOD_NAME": argument '%c' missing\n", *type);

				// skip whitespace
				while( (end < s+nread) && isspace(*end))
					end++;
			}

			switch(*type)
			{
				case OSC_INT32:
				{
					int32_t i;
					if(sscanf(cur, "%"SCNi32, &i))
						ptr = osc_set_int32(ptr, endptr, i);
					else
						fprintf(stderr, MOD_NAME": type mismatch at '%c'\n", *type);
					break;
				}
				case OSC_FLOAT:
				{
					float f;
					if(sscanf(cur, "%f", &f))
						ptr = osc_set_float(ptr, endptr, f);
					else
						fprintf(stderr, MOD_NAME": type mismatch at '%c'\n", *type);
					break;
				}
				case OSC_STRING:
				{
					char *s = cur;
					ptr = osc_set_string(ptr, endptr, s);
					break;
				}
				case OSC_BLOB:
				{
					int32_t size = strlen(cur) / 2;
					uint8_t *payload = calloc(size, sizeof(uint8_t));
					int i;
					for(i=0; i<size; i++)
						if(!sscanf(cur+i*2, "%02"SCNx8, payload+i))
							fprintf(stderr, MOD_NAME": type mismatch at '%c'\n", *type);
					ptr = osc_set_blob(ptr, endptr, size, payload);
					free(payload);
					break;
				}

				case OSC_TRUE:
				case OSC_FALSE:
				case OSC_NIL:
				case OSC_BANG:
					break;

				case OSC_INT64:
				{
					int64_t h;
					if(sscanf(cur, "%"SCNi64, &h))
						ptr = osc_set_int64(ptr, endptr, h);
					else
						fprintf(stderr, MOD_NAME": type mismatch at '%c'\n", *type);
					break;
				}
				case OSC_DOUBLE:
				{
					double d;
					if(sscanf(cur, "%lf", &d))
						ptr = osc_set_double(ptr, endptr, d);
					else
						fprintf(stderr, MOD_NAME": type mismatch at '%c'\n", *type);
					break;
				}
				case OSC_TIMETAG:
				{
					uint64_t t;
					uint32_t sec, frac;
					if(sscanf(cur, "%"SCNx32".%"SCNx32, &sec, &frac))
					{
						t = (((uint64_t)sec<<32)) | frac;
						ptr = osc_set_timetag(ptr, endptr, t);
					}
					else
						fprintf(stderr, MOD_NAME": type mismatch at '%c'\n", *type);
					break;
				}

				case OSC_SYMBOL:
				{
					char *S = cur;
					ptr = osc_set_symbol(ptr, endptr, S);
					break;
				}
				case OSC_CHAR:
				{
					char c = *cur;
					ptr = osc_set_char(ptr, endptr, c);
					break;
				}
				case OSC_MIDI:
				{
					uint8_t m [4];
					if(sscanf(cur, "%02"SCNx8"%02"SCNx8"%02"SCNx8"%02"SCNx8, m, m+1, m+2, m+3))
						ptr = osc_set_midi(ptr, endptr, m);
					else
						fprintf(stderr, MOD_NAME": type mismatch at '%c'\n", *type);
					break;
				}

				default:
					break;
			}
		}

		size_t len = ptr - dat->buffer;
		if(ptr && osc_check_message(dat->buffer, len))
		{
			if(tjost_pipe_produce(&dat->pipe, module, 0, len, dat->buffer))
				fprintf(stderr, MOD_NAME": tjost_pipe_produce error\n");
		}
		else
			fprintf(stderr, MOD_NAME": rx OSC message invalid\n");
	}
	else if (nread < 0)
	{
		int err;
		if((err = uv_read_stop((uv_stream_t *)&dat->recv_client)))
			fprintf(stderr, MOD_NAME": %s\n", uv_err_name(err));
		uv_close((uv_handle_t *)&dat->recv_client, NULL);
		fprintf(stderr, MOD_NAME": %s\n", uv_err_name(nread));
	}
}

static osc_data_t *
_alloc(Tjost_Event *tev, void *arg)
{
	Tjost_Module *module = tev->module;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	return tjost_host_schedule_inline(host, module, tev->time, tev->size);
}

static int
_sched(Tjost_Event *tev, osc_data_t *buf, void *arg)
{
	return 0; // reload
}

int
process_in(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	if(tjost_pipe_consume(&dat->pipe, _alloc, _sched, NULL))
		tjost_host_message_push(host, MOD_NAME": %s", "tjost_pipe_consume error");

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

	if(tjost_pipe_init(&dat->pipe))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize tjost pipe");

	dat->recv_client.data = module;
	int err;
	if((err = uv_tty_init(loop, &dat->recv_client, 0, 1)))
		MOD_ADD_ERR(module->host, MOD_NAME, uv_err_name(err));
	if((err = uv_read_start((uv_stream_t *)&dat->recv_client, _tty_alloc, _tty_recv_cb)))
		MOD_ADD_ERR(module->host, MOD_NAME, uv_err_name(err));

	module->dat = dat;
	module->type = TJOST_MODULE_INPUT;

	return 0;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	uv_close((uv_handle_t *)&dat->recv_client, NULL);
	
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
