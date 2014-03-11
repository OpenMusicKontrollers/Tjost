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

#ifndef __TJOST_H
#define __TJOST_H

#ifdef __cplusplus
extern "C" {
#endif

// libuv for the event system
#include <uv.h>

// Lua as embedded scripting language
#define LUA_COMPAT_ALL
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

// eina for module and list handling
#include <Eina.h>

// JACKified OpenSoundControl
#include <jack_osc.h>

typedef struct _Tjost_Time Tjost_Time;
typedef struct _Tjost_Event Tjost_Event;
typedef struct _Tjost_Module Tjost_Module;
typedef struct _Tjost_Host Tjost_Host;

typedef void (*Tjost_Module_Add_Cb)(Tjost_Module *module, int argc, const char **argv);
typedef void (*Tjost_Module_Del_Cb)(Tjost_Module *module);

typedef enum _Tjost_Module_Type {
	TJOST_MODULE_INPUT, TJOST_MODULE_OUTPUT, TJOST_MODULE_UPLINK
} Tjost_Module_Type;

struct _Tjost_Time {
	jack_nframes_t current_frames;
	jack_nframes_t nframes;
	jack_time_t current_usecs;
	jack_time_t next_usecs;
	float periods_usecs;
};

struct _Tjost_Event {
	EINA_INLIST;

	Tjost_Module *module; // destination

	jack_nframes_t time;
	size_t size;
	uint8_t buf [0];
};

struct _Tjost_Module {
	EINA_INLIST;

	Tjost_Module_Add_Cb add;
	Tjost_Module_Del_Cb del;
	JackProcessCallback process;

	Tjost_Module_Type type;
	Tjost_Host *host;
	void *dat;

	Eina_Inlist *queue; // module event queue
};

#define TJOST_MODULE_BROADCAST NULL
#define TJOST_BUF_SIZE 0x2000
#define TJOST_RINGBUF_SIZE 0x8000

struct _Tjost_Host {
	jack_client_t *client;

	jack_nframes_t srate;

	lua_State *L;

	uv_signal_t sigterm, sigquit, sigint;
	uv_async_t quit;

	jack_ringbuffer_t *rb_msg;
	uv_async_t msg;

	jack_ringbuffer_t *rb_uplink_tx;
	jack_ringbuffer_t *rb_uplink_rx;
	uv_async_t uplink_tx;

	jack_ringbuffer_t *rb_rtmem;
	uv_async_t rtmem;
	size_t rtmem_sum;

	Eina_Array *arr; // modules

	Eina_Inlist *inputs; // input module instances
	Eina_Inlist *outputs; // outputs module instances
	Eina_Inlist *uplinks; // uplink module instances

	Eina_Inlist *queue; // host event queue

	uint8_t *pool; // realtime memory pool
	Eina_Mempool *mempool;
};

// in tjost.c
void *tjost_alloc(Tjost_Host *host, size_t len);
void *tjost_realloc(Tjost_Host *host, size_t len, void *buf);
void tjost_free(Tjost_Host *host, void *buf);

void tjost_host_schedule(Tjost_Host *host, Tjost_Module *module, jack_nframes_t time, size_t len, void *buf);
uint8_t *tjost_host_schedule_inline(Tjost_Host *host, Tjost_Module *module, jack_nframes_t time, size_t len);
void tjost_module_schedule(Tjost_Module *module, jack_nframes_t time, size_t len, void *buf);

void tjost_host_message_push(Tjost_Host *host, const char *fmt, ...);
int tjost_host_message_pull(Tjost_Host *host, char *str);

// in tjost_lua.c
void tjost_lua_deserialize_unicast(Tjost_Event *tev);
void tjost_lua_deserialize_broadcast(Tjost_Event *tev, Eina_Inlist *modules);
extern const luaL_Reg tjost_input_mt [];
extern const luaL_Reg tjost_output_mt [];
extern const luaL_Reg tjost_uplink_mt [];
extern const luaL_Reg tjost_globals [];

// in tjost_uplink.c
void tjost_uplink_tx_drain(uv_async_t *handle, int status);
void tjost_uplink_tx_push(Tjost_Host *host, Tjost_Event *tev);
void tjost_uplink_rx_drain(Tjost_Host *host);

void tjost_client_registration(const char *name, int state, void *arg);
void tjost_port_registration(jack_port_id_t id, int state, void *arg);
void tjost_port_connect(jack_port_id_t id_a, jack_port_id_t id_b, int state, void *arg);
int tjost_port_rename(jack_port_id_t port, const char *old_name, const char *new_name, void *arg);
int tjost_graph_order(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* __TJOST_H */
