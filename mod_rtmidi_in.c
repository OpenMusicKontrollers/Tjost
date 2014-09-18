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

#include <rtmidi_c.h>

#define MOD_NAME "seq_in"

typedef struct _Data Data;

struct _Data {
	RtMidiC_In *dev;
	RtMidiC_Callback cb;

	jack_ringbuffer_t *rb;
};

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
			tjost_host_message_push(host, MOD_NAME": %s", "ringbuffer peek error");

		if(jack_ringbuffer_read_space(dat->rb) >= sizeof(Tjost_Event) + tev.size)
		{
			jack_ringbuffer_read_advance(dat->rb, sizeof(Tjost_Event));

			osc_data_t *bf = tjost_host_schedule_inline(host, module, tev.time, tev.size);
			if(jack_ringbuffer_read(dat->rb, (char *)bf, tev.size) != tev.size)
				tjost_host_message_push(host, MOD_NAME": %s", "ringbuffer read error");
		}
		else
			break;
	}

	return 0;
}

static void
_rtmidic_cb(double timestamp, size_t len, uint8_t* message, void **cb)
{
	Data *dat = (Data *)((uint8_t *)cb - offsetof(Data, cb));

	// TODO assert(len <= 4)
	uint8_t m [4];
	m[0] = message[0] & 0x0f;
	m[1] = message[0] & 0xf0;
	m[2] = message[1];
	m[3] = message[2];

	osc_data_t buf [20];
	osc_data_t *ptr = buf;
	ptr = osc_set_path(ptr, "/midi");
	ptr = osc_set_fmt(ptr, "m");
	ptr = osc_set_midi(ptr, m);
	size_t size = ptr - buf;

	Tjost_Event tev;
	tev.time = 0; // immediate execution
	tev.size = size;
	if(osc_message_check(buf, tev.size))
	{
		if(jack_ringbuffer_write_space(dat->rb) < sizeof(Tjost_Event) + tev.size)
			fprintf(stderr, MOD_NAME": ringbuffer overflow\n");
		else
		{
			if(jack_ringbuffer_write(dat->rb, (const char *)&tev, sizeof(Tjost_Event)) != sizeof(Tjost_Event))
				fprintf(stderr, MOD_NAME": ringbuffer write 1 error\n");
			if(jack_ringbuffer_write(dat->rb, (const char *)buf, size) != size)
				fprintf(stderr, MOD_NAME": ringbuffer write 2 error\n");
		}
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
	const char *device = luaL_optstring(L, -1, "seq");
	lua_pop(L, 1);

	lua_getfield(L, 1, "api");
	RtMidiC_API api = luaL_optint(L, -1, RTMIDIC_API_UNSPECIFIED);
	lua_pop(L, 1);

	dat->cb = _rtmidic_cb;
	
	if(!(dat->dev = rtmidic_in_new(api, jack_get_client_name(module->host->client))))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not create device");
	if(rtmidic_in_virtual_port_open(dat->dev, device))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not open virtual port");
	if(rtmidic_in_callback_set(dat->dev, &dat->cb))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not set callback");

	if(!(dat->rb = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		MOD_ADD_ERR(module->host, MOD_NAME, "could not initilize ringbuffer");
	
	module->dat = dat;
	module->type = TJOST_MODULE_INPUT;

	return 0;
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	if(dat->rb)
		jack_ringbuffer_free(dat->rb);

	if(rtmidic_in_callback_unset(dat->dev))
		fprintf(stderr, MOD_NAME": could not unset callback\n");
	if(rtmidic_in_port_close(dat->dev))
		fprintf(stderr, MOD_NAME": could not close port\n");
	if(rtmidic_in_free(dat->dev))
		fprintf(stderr, MOD_NAME": could not free device\n");

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
