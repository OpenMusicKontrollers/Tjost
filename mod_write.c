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

#define MOD_NAME "write"

typedef struct _Data Data;

struct _Data {
	jack_ringbuffer_t *rb;
	uv_async_t asio;
	osc_data_t buffer [TJOST_BUF_SIZE];
	FILE *f;
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

			osc_data_t *buffer;
			if(vec[0].len >= tev.size)
				buffer = (osc_data_t *)vec[0].buf;
			else
			{
				buffer = dat->buffer;
				jack_ringbuffer_read(dat->rb, (char *)buffer, tev.size);
			}

			assert((uintptr_t)buffer % sizeof(uint32_t) == 0);

			{
				uint32_t ntime = tev.time;
				uint32_t nsize = tev.size;

				ntime = htonl(ntime);
				nsize = htonl(nsize);

				fwrite(&ntime, sizeof(uint32_t), 1, dat->f);
				fwrite(&nsize, sizeof(uint32_t), 1, dat->f);
				fwrite(buffer, tev.size, 1, dat->f);
			}

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

int
add(Tjost_Module *module)
{
	Tjost_Host *host = module->host;
	lua_State *L = host->L;
	Data *dat = tjost_alloc(module->host, sizeof(Data));
	memset(dat, 0, sizeof(Data));

	uv_loop_t *loop = uv_default_loop();

	lua_getfield(L, 1, "path");
	const char *path = luaL_optstring(L, -1, NULL);
	lua_pop(L, 1);

	if(path)
	{
		if(!(dat->f = fopen(path, "wb")))
			MOD_ADD_ERR(module->host, MOD_NAME, "could not open file handle");
	}
	else
		dat->f = stdout;

	const char *head = "tjostbin";
	uint32_t nsrate = htonl(module->host->srate);
	fwrite(head, sizeof(char), 8, dat->f);
	fwrite(&nsrate, sizeof(uint32_t), 1, dat->f);
	fflush(dat->f);

	if(!(dat->rb = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize ringbuffer");

	dat->asio.data = module;
	int err;
	if((err = uv_async_init(loop, &dat->asio, _asio)))
		MOD_ADD_ERR(module->host, MOD_NAME, uv_err_name(err));

	module->dat = dat;
	module->type = TJOST_MODULE_OUTPUT;

	return 0;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	uv_close((uv_handle_t *)&dat->asio, NULL);

	if( dat->f && (dat->f != stdout) )
	{
		fflush(dat->f);
		fclose(dat->f);
	}

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
