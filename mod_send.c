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

#include <ctype.h>

#include <tjost.h>

typedef struct _Data Data;

struct _Data {
	jack_ringbuffer_t *rb;

	uv_tty_t recv_client;
};

static uint8_t buffer [TJOST_BUF_SIZE] __attribute__((aligned (8)));
static uint8_t buf2 [TJOST_BUF_SIZE] __attribute__((aligned (8)));

static void
_tty_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	Tjost_Module *module = handle->data;

	buf->base = (char *)buffer;
	buf->len = TJOST_BUF_SIZE;
}

static void
_tty_recv_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	Tjost_Module *module = stream->data;
	Data *dat = module->dat;

	if(nread > 0)
	{
		char *s = buf->base;

		uint8_t *ptr = buf2;
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
		if(!jack_osc_check_path(path))
		{
			fprintf(stderr, "invalid path: %s\n", path);
			return;
		}
		ptr = jack_osc_set_path(ptr, path);

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
		if(!jack_osc_check_fmt(fmt, 0))
		{
			fprintf(stderr, "invalid format: %s\n", fmt);
			return;
		}
		ptr = jack_osc_set_fmt(ptr, fmt);

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
					fprintf(stderr, "argument '%c' missing\n", *type);

				// skip whitespace
				while( (end < s+nread) && isspace(*end))
					end++;
			}

			switch(*type)
			{
				case 'i':
				{
					int32_t i;
					if(sscanf(cur, "%"SCNi32, &i))
						ptr = jack_osc_set_int32(ptr, i);
					else
						fprintf(stderr, "type mismatch at '%c'\n", *type);
					break;
				}
				case 'f':
				{
					float f;
					if(sscanf(cur, "%f", &f))
						ptr = jack_osc_set_float(ptr, f);
					else
						fprintf(stderr, "type mismatch at '%c'\n", *type);
					break;
				}
				case 's':
				{
					char *s = cur;
					ptr = jack_osc_set_string(ptr, s);
					break;
				}
				case 'b':
				{
					int32_t size = strlen(cur) / 2;
					uint8_t *payload = calloc(size, sizeof(uint8_t));
					int i;
					for(i=0; i<size; i++)
						if(!sscanf(cur+i*2, "%02"SCNx8, payload+i))
							fprintf(stderr, "type mismatch at '%c'\n", *type);
					ptr = jack_osc_set_blob(ptr, size, payload);
					free(payload);
					break;
				}

				case 'T':
				case 'F':
				case 'N':
				case 'I':
					break;

				case 'h':
				{
					int64_t h;
					if(sscanf(cur, "%"SCNi64, &h))
						ptr = jack_osc_set_int64(ptr, h);
					else
						fprintf(stderr, "type mismatch at '%c'\n", *type);
					break;
				}
				case 'd':
				{
					double d;
					if(sscanf(cur, "%lf", &d))
						ptr = jack_osc_set_double(ptr, d);
					else
						fprintf(stderr, "type mismatch at '%c'\n", *type);
					break;
				}
				case 't':
				{
					uint64_t t;
					uint32_t sec, frac;
					if(sscanf(cur, "%"SCNx32".%"SCNx32, &sec, &frac))
					{
						t = (((uint64_t)sec<<32)) | frac;
						ptr = jack_osc_set_timetag(ptr, t);
					}
					else
						fprintf(stderr, "type mismatch at '%c'\n", *type);
					break;
				}

				case 'S':
				{
					char *S = cur;
					ptr = jack_osc_set_symbol(ptr, S);
					break;
				}
				case 'c':
				{
					char c = *cur;
					ptr = jack_osc_set_char(ptr, c);
					break;
				}
				case 'm':
				{
					uint8_t m [4];
					if(sscanf(cur, "%02"SCNx8"%02"SCNx8"%02"SCNx8"%02"SCNx8, m, m+1, m+2, m+3))
						ptr = jack_osc_set_midi(ptr, m);
					else
						fprintf(stderr, "type mismatch at '%c'\n", *type);
					break;
				}

				default:
					break;
			}
		}

		uint32_t len = ptr - buf2;

		Tjost_Event tev;
		tev.time = 0; // immediate execution
		tev.size = len;
		if(jack_osc_message_check(buf2, tev.size))
		{
			if(jack_ringbuffer_write_space(dat->rb) < sizeof(Tjost_Event) + tev.size)
				fprintf(stderr, "send: ringbuffer overflow\n");
			else
			{
				if(jack_ringbuffer_write(dat->rb, (const char *)&tev, sizeof(Tjost_Event)) != sizeof(Tjost_Event))
					fprintf(stderr, "send: ringbuffer write 1 error\n");
				if(jack_ringbuffer_write(dat->rb, (const char *)buf2, len) != len)
					fprintf(stderr, "send: ringbuffer write 2 error\n");
			}
		}
		else
			fprintf(stderr, "rx OSC message invalid\n");
	}
	else if (nread < 0)
	{
		int err;
		if((err = uv_read_stop((uv_stream_t *)&dat->recv_client)))
			fprintf(stderr, "send: %s\n", uv_err_name(err));
		uv_close((uv_handle_t *)&dat->recv_client, NULL);
		fprintf(stderr, "%s\n", uv_err_name(nread));
	}
}

int
process_in(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	Tjost_Event tev;
	while(jack_ringbuffer_read_space(dat->rb) >= sizeof(Tjost_Event))
	{
		if(jack_ringbuffer_peek(dat->rb, (char *)&tev, sizeof(Tjost_Event)) != sizeof(Tjost_Event))
			tjost_host_message_push(host, "send: %s", "ringbuffer peek error");

		if(jack_ringbuffer_read_space(dat->rb) >= sizeof(Tjost_Event) + tev.size)
		{
			jack_ringbuffer_read_advance(dat->rb, sizeof(Tjost_Event));

			uint8_t *bf = tjost_host_schedule_inline(host, module, tev.time, tev.size);
			if(jack_ringbuffer_read(dat->rb, (char *)bf, tev.size) != tev.size)
				tjost_host_message_push(host, "send: %s", "ringbuffer read error");
		}
		else
			break;
	}

	return 0;
}

void
add(Tjost_Module *module, int argc, const char **argv)
{
	Data *dat = tjost_alloc(module->host, sizeof(Data));

	if(!(dat->rb = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		fprintf(stderr, "could not initialize ringbuffer\n");
	
	uv_loop_t *loop = uv_default_loop();

	dat->recv_client.data = module;
	int err;
	if((err = uv_tty_init(loop, &dat->recv_client, 0, 1)))
		fprintf(stderr, "send: %s\n", uv_err_name(err));
	if((err = uv_read_start((uv_stream_t *)&dat->recv_client, _tty_alloc, _tty_recv_cb)))
		fprintf(stderr, "send: %s\n", uv_err_name(err));

	module->dat = dat;
	module->type = TJOST_MODULE_INPUT;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	uv_close((uv_handle_t *)&dat->recv_client, NULL);

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
