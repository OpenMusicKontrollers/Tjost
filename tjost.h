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

#ifndef _TJOST_H_
#define _TJOST_H_

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

// Two segregate fit real time memory allocator
#include <tlsf.h>

// JACKified OpenSoundControl
#include <osc.h>

#include <tjost_config.h>

#include <jack/ringbuffer.h>
#ifdef HAS_METADATA_API
#	include <jack/metadata.h>
#	include <jack/uuid.h>
#endif // HAS_METADATA_API

typedef struct _Tjost_Midi Tjost_Midi;
typedef struct _Tjost_Blob Tjost_Blob;
typedef struct _Tjost_Bundle Tjost_Bundle;
typedef struct _Tjost_Event Tjost_Event;
typedef struct _Tjost_Module Tjost_Module;
typedef struct _Tjost_Child Tjost_Child;
typedef struct _Tjost_Mem_Chunk Tjost_Mem_Chunk;
typedef struct _Tjost_Host Tjost_Host;

typedef void (*Tjost_Module_Add_Cb)(Tjost_Module *module, int argc, const char **argv);
typedef void (*Tjost_Module_Del_Cb)(Tjost_Module *module);

#define TJOST_MODULE_INPUT	0b001
#define TJOST_MODULE_OUTPUT 0b010
#define TJOST_MODULE_IN_OUT (TJOST_MODULE_INPUT | TJOST_MODULE_OUTPUT)
#define TJOST_MODULE_UPLINK 0b100

#define TJOST_MODULE_BROADCAST NULL
#define TJOST_BUF_SIZE (0x4000)
#define TJOST_RINGBUF_SIZE (0x10000)

struct _Tjost_Midi {
	uint8_t buf[4];
};

struct _Tjost_Blob {
	int32_t size;
	uint8_t buf[0];
};

struct _Tjost_Bundle {
	EINA_INLIST;

	jack_nframes_t time;
	osc_data_t *ptr;
};

struct _Tjost_Event {
	EINA_INLIST;

	Tjost_Module *module; // destination

	jack_nframes_t time;
	size_t size;
	osc_data_t buf [0];
};

struct _Tjost_Module {
	EINA_INLIST;

	Tjost_Module_Add_Cb add;
	Tjost_Module_Del_Cb del;
	JackProcessCallback process_in;
	JackProcessCallback process_out;

	int type;
	Tjost_Host *host;
	void *dat;

	Eina_Inlist *queue; // module output event queue
	Eina_Inlist *children; // child modules for direct mode
	int has_lua_callback;

	osc_data_t buffer [TJOST_BUF_SIZE];
	osc_data_t *buf_ptr;
	osc_data_t *itm;
	Eina_Inlist *bndls;
	int nest;
};

struct _Tjost_Child {
	EINA_INLIST;

	Tjost_Module *module;
};

struct _Tjost_Mem_Chunk {
	EINA_INLIST;

	size_t size;
	void *area;
	pool_t pool;
};

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
	Eina_Inlist *rtmem_chunks;
	size_t rtmem_sum;
	int rtmem_flag;
	tlsf_t tlsf;

	Eina_Array *arr; // modules

	Eina_Inlist *modules; // input module instances
	Eina_Inlist *uplinks; // uplink module instances

	Eina_Inlist *queue; // host event queue
};

// in tjost.c
void *tjost_alloc(Tjost_Host *host, size_t len);
void *tjost_realloc(Tjost_Host *host, size_t len, void *buf);
void tjost_free(Tjost_Host *host, void *buf);

void tjost_host_schedule(Tjost_Host *host, Tjost_Module *module, jack_nframes_t time, size_t len, void *buf);
osc_data_t *tjost_host_schedule_inline(Tjost_Host *host, Tjost_Module *module, jack_nframes_t time, size_t len);
void tjost_module_schedule(Tjost_Module *module, jack_nframes_t time, size_t len, void *buf);

void tjost_host_message_push(Tjost_Host *host, const char *fmt, ...);
int tjost_host_message_pull(Tjost_Host *host, char *str);

// in tjost_lua.c
void tjost_lua_deserialize(Tjost_Event *tev);
extern const luaL_Reg tjost_input_mt [];
extern const luaL_Reg tjost_output_mt [];
extern const luaL_Reg tjost_in_out_mt [];
extern const luaL_Reg tjost_uplink_mt [];
extern const luaL_Reg tjost_globals [];
extern const luaL_Reg tjost_blob_mt [];
extern const luaL_Reg tjost_midi_mt [];

// in tjost_uplink.c
void tjost_uplink_tx_drain(uv_async_t *handle);
void tjost_uplink_tx_push(Tjost_Host *host, Tjost_Event *tev);
void tjost_uplink_rx_drain(Tjost_Host *host, int ignore);

void tjost_client_registration(const char *name, int state, void *arg);
void tjost_port_registration(jack_port_id_t id, int state, void *arg);
void tjost_port_connect(jack_port_id_t id_a, jack_port_id_t id_b, int state, void *arg);
void tjost_port_rename(jack_port_id_t port, const char *old_name, const char *new_name, void *arg);
int tjost_graph_order(void *arg);
#ifdef HAS_METADATA_API
void tjost_property_change(jack_uuid_t uuid, const char *key, jack_property_change_t change, void *arg);
#endif // HAS_METADATA_API

#ifdef __cplusplus
}
#endif

#endif
