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

#define MOD_NAME "read"

typedef struct _Data Data;

struct _Data {
	jack_ringbuffer_t *rb;
	uv_async_t asio;
	osc_data_t buffer [TJOST_BUF_SIZE];
	FILE *f;
	jack_nframes_t offset;
};

int
process_in(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	if(dat->offset == 0) //TODO add way to reset this
		dat->offset = jack_last_frame_time(host->client);

	Tjost_Event tev;
	while(jack_ringbuffer_read_space(dat->rb) >= sizeof(Tjost_Event))
	{
		if(jack_ringbuffer_peek(dat->rb, (char *)&tev, sizeof(Tjost_Event)) != sizeof(Tjost_Event))
			tjost_host_message_push(host, MOD_NAME": %s", "ringbuffer peek error");

		if(jack_ringbuffer_read_space(dat->rb) >= sizeof(Tjost_Event) + tev.size)
		{
			jack_ringbuffer_read_advance(dat->rb, sizeof(Tjost_Event));

			tev.time += dat->offset;
			osc_data_t *bf = tjost_host_schedule_inline(host, module, tev.time, tev.size);
			if(jack_ringbuffer_read(dat->rb, (char *)bf, tev.size) != tev.size)
				tjost_host_message_push(host, MOD_NAME": %s", "ringbuffer read error");
		}
		else
			break;
	}

	int err;
	if((err = uv_async_send(&dat->asio)))
		tjost_host_message_push(host, MOD_NAME": %s", uv_err_name(err));

	return 0;
}

static void
_asio(uv_async_t *handle)
{
	Tjost_Module *module = handle->data;
	Data *dat = module->dat;
	
	while(!feof(dat->f) && (jack_ringbuffer_write_space(dat->rb) > TJOST_BUF_SIZE) )
	{
		uint32_t ntime;
		uint32_t nsize;
		fread(&ntime, sizeof(uint32_t), 1, dat->f);
		ntime = ntohl(ntime);
		fread(&nsize, sizeof(uint32_t), 1, dat->f);
		nsize = ntohl(nsize);
		fread(dat->buffer, sizeof(osc_data_t), nsize, dat->f);
		
		Tjost_Event tev;
		tev.time = ntime;
		tev.size = nsize;

		if(osc_message_check(dat->buffer, tev.size))
		{
			if(jack_ringbuffer_write_space(dat->rb) < sizeof(Tjost_Event) + tev.size)
				fprintf(stderr, MOD_NAME": ringbuffer overflow\n");
			else
			{
				if(jack_ringbuffer_write(dat->rb, (const char *)&tev, sizeof(Tjost_Event)) != sizeof(Tjost_Event))
					fprintf(stderr, MOD_NAME": ringbuffer write 1 error\n");
				if(jack_ringbuffer_write(dat->rb, (const char *)dat->buffer, tev.size) != tev.size)
					fprintf(stderr, MOD_NAME": ringbuffer write 2 error\n");
			}
		}
		else
			fprintf(stderr, MOD_NAME": rx OSC message invalid\n");
	}
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
		if(!(dat->f = fopen(path, "rb")))
			MOD_ADD_ERR(module->host, MOD_NAME, "could not open file handle");
	}
	else
		dat->f = stdin;

	char head [8];
	uint32_t nsrate;
	fread(head, sizeof(char), 8, dat->f);
	fread(&nsrate, sizeof(uint32_t), 1, dat->f);
	nsrate = ntohl(nsrate);
	if(strncmp(head, "tjostbin", 8))
		MOD_ADD_ERR(module->host, MOD_NAME, "not an OSC binary file");

	if(!(dat->rb = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize ringbuffer");

	dat->asio.data = module;
	int err;
	if((err = uv_async_init(loop, &dat->asio, _asio)))
		MOD_ADD_ERR(module->host, MOD_NAME, uv_err_name(err));

	module->dat = dat;
	module->type = TJOST_MODULE_INPUT;

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
