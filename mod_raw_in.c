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

#include <alsa/asoundlib.h>

#define MOD_NAME "raw_in"

typedef struct _Data Data;

struct _Data {
	snd_rawmidi_t *dev;

	Tjost_Pipe pipe;
	uv_poll_t poll;
};

static osc_data_t *
_alloc(jack_nframes_t timestamp, size_t len, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	return tjost_host_schedule_inline(host, module, timestamp, len);
}

static int
_sched(jack_nframes_t timestamp, osc_data_t *buf, size_t len, void *arg)
{
	return 0; // reload
}

int
process_in(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	Tjost_Host *host = module->host;
	Data *dat = module->dat;

	if(tjost_pipe_consume(&dat->pipe, _alloc, _sched, module))
		tjost_host_message_push(host, MOD_NAME": %s", "tjost_pipe_consume error");

	return 0;
}

static void
_poll(uv_poll_t *handle, int status, int events)
{
	Tjost_Module *module = handle->data;
	Data *dat = module->dat;

	if(status < 0)
	{
		fprintf(stderr, MOD_NAME": %s\n", uv_err_name(status));
		return;
	}

	uint8_t m [4] = {0x0};
	if(snd_rawmidi_read(dat->dev, &m[1], 3) != 3)
		fprintf(stderr, MOD_NAME": reading MIDI failed\n");

	osc_data_t buf [20];
	osc_data_t *ptr = buf;
	ptr = osc_set_path(ptr, "/midi");
	ptr = osc_set_fmt(ptr, "m");
	ptr = osc_set_midi(ptr, m);
	size_t len = ptr - buf;

	if(osc_message_check(buf, len))
	{
		if(tjost_pipe_produce(&dat->pipe, 0, len, buf))
			fprintf(stderr, MOD_NAME": tjost_pipe_produce error\n");
	}
	else
		fprintf(stderr, MOD_NAME": rx OSC message invalid\n");
}

int
add(Tjost_Module *module)
{
	Tjost_Host *host = module->host;
	lua_State *L = host->L;
	Data *dat = tjost_alloc(module->host, sizeof(Data));
	memset(dat, 0, sizeof(Data));

	lua_getfield(L, 1, "device");
	const char *device = luaL_optstring(L, -1, "virtual");
	lua_pop(L, 1);
	
	if(snd_rawmidi_open(&dat->dev, NULL, device, 0))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not open MIDI device");

	struct pollfd pfds;
	int count = snd_rawmidi_poll_descriptors_count(dat->dev); //TODO check count
	if(snd_rawmidi_poll_descriptors(dat->dev, &pfds, 1) != 1)
		MOD_ADD_ERR(module->host, MOD_NAME, "could not poll MIDI descriptors");

	uv_loop_t *loop = uv_default_loop();

	if(tjost_pipe_init(&dat->pipe))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initialize tjost pipe");

	int err;
	dat->poll.data = module;
	if((err = uv_poll_init(loop, &dat->poll, pfds.fd)))
		MOD_ADD_ERR(module->host, MOD_NAME, uv_err_name(err));
	if((err = uv_poll_start(&dat->poll, UV_READABLE, _poll)))
		MOD_ADD_ERR(module->host, MOD_NAME, uv_err_name(err));

	module->dat = dat;
	module->type = TJOST_MODULE_INPUT;

	return 0;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	uv_poll_stop(&dat->poll);

	tjost_pipe_deinit(&dat->pipe);

	if(snd_rawmidi_drop(dat->dev))
		fprintf(stderr, MOD_NAME": dropping MIDI failed\n");
	if(snd_rawmidi_close(dat->dev))
		fprintf(stderr, MOD_NAME": closing MIDI failed\n");

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
