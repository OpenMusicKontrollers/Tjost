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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sched.h>
#include <assert.h>
#include <malloc.h>
#include <sys/mman.h>

#include <tlsf.h>

#include <tjost.h>

static void
_err(const char *msg)
{
	fprintf(stderr, "libev main loop error: %s\n", msg);
	ev_break(EV_DEFAULT, EVBREAK_ALL);
}

static void
_sig(struct ev_loop *loop, struct ev_signal *w, int revents)
{
	ev_break(loop, EVBREAK_ALL);
}

static void
_quit(struct ev_loop *loop, struct ev_async *w, int revents)
{
	ev_break(loop, EVBREAK_ALL);
}

static void
_msg(struct ev_loop *loop, struct ev_async *w, int revents)
{
	static char str [1024]; // TODO how big?

	Tjost_Host *host = w->data;

	while(tjost_host_message_pull(host, str))
		fprintf(stderr, "MESSAGE: %s\n", str);
}

static void
_shutdown(void *arg)
{
	Tjost_Host *host = arg;
	ev_async_send(EV_DEFAULT, &host->quit);
}

static void *
_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
	Tjost_Host *host = ud;
	(void)osize;  // not used

	if(nsize == 0) {
		tjost_free(host, ptr);
		return NULL;
	}
	else
		return tjost_realloc(host, nsize, ptr);
}

static int
_tjost_schedule_sort(const void *dat1, const void *dat2)
{
	const Eina_Inlist *l1 = (const Eina_Inlist *)dat1;
	const Eina_Inlist *l2 = (const Eina_Inlist *)dat2;

	Tjost_Event *ev1 = EINA_INLIST_CONTAINER_GET(l1, Tjost_Event);
	Tjost_Event *ev2 = EINA_INLIST_CONTAINER_GET(l2, Tjost_Event);

	return ev1->time <= ev2->time ? -1 : 1;
}

void
tjost_host_schedule(Tjost_Host *host, Tjost_Module *module, jack_nframes_t time, size_t len, void *buf)
{
	Tjost_Event *tev = tjost_alloc(host, sizeof(Tjost_Event) + len);

	tev->time = time;
	tev->size = len;
	tev->module = module; // source module
	memcpy(tev->buf, buf, len);

	host->queue = eina_inlist_sorted_insert(host->queue, EINA_INLIST_GET(tev), _tjost_schedule_sort);
}

uint8_t *
tjost_host_schedule_inline(Tjost_Host *host, Tjost_Module *module, jack_nframes_t time, size_t len)
{
	Tjost_Event *tev = tjost_alloc(host, sizeof(Tjost_Event) + len);

	tev->time = time;
	tev->size = len;
	tev->module = module; // source module

	host->queue = eina_inlist_sorted_insert(host->queue, EINA_INLIST_GET(tev), _tjost_schedule_sort);
	return tev->buf;
}

void
tjost_module_schedule(Tjost_Module *module, jack_nframes_t time, size_t len, void *buf)
{
	Tjost_Event *tev = tjost_alloc(module->host, sizeof(Tjost_Event) + len);

	tev->time = time;
	tev->size = len;
	tev->module = NULL;
	memcpy(tev->buf, buf, len);

	module->queue = eina_inlist_sorted_insert(module->queue, EINA_INLIST_GET(tev), _tjost_schedule_sort);
}

void
tjost_host_message_push(Tjost_Host *host, const char *fmt, ...)
{
	static char str [1024]; //TODO how big?
	va_list argv;
	va_start(argv, fmt);
	vsprintf(str, fmt, argv);
	va_end(argv);

	size_t size = strlen(str) + 1;

	if(jack_ringbuffer_write_space(host->rb_msg) < sizeof(size_t) + size)
		; //FIXME
	else
	{
		jack_ringbuffer_write(host->rb_msg, (const char *)&size, sizeof(size_t));
		jack_ringbuffer_write(host->rb_msg, str, size);
	}

	ev_async_send(EV_DEFAULT, &host->msg);
}

int
tjost_host_message_pull(Tjost_Host *host, char *str)
{
	if(jack_ringbuffer_read_space(host->rb_msg) >= sizeof(size_t))
	{
		size_t size;
		jack_ringbuffer_peek(host->rb_msg, (char *)&size, sizeof(size_t));
		if(jack_ringbuffer_read_space(host->rb_msg) >= sizeof(size_t) + size)
		{
			jack_ringbuffer_read_advance(host->rb_msg, sizeof(size_t));
			jack_ringbuffer_read(host->rb_msg, str, size);
			return 1;
		}
	}

	return 0;
}
	
static const size_t area_size = 0x1000000UL; // 16MB
//static const size_t area_size = 0x10000UL; // 64KB

static void
tjost_request_memory(struct ev_loop *loop, struct ev_async *w, int revents)
{
	Tjost_Host *host = w->data;
	void *area;

	printf("requesting new memory chunk\n");

	//FIXME munmap, munlock
  if(!(area = mmap(NULL, area_size, PROT_READ|PROT_WRITE, MAP_32BIT|MAP_PRIVATE|MAP_ANONYMOUS|MAP_LOCKED, -1, 0)))
	{
		fprintf(stderr, "tjost_request_memory: could not allocate RT memory chunk\n");
		host->rtmem_sum -= area_size;
	}
	else
	{
		mlock(area, area_size);

		if(jack_ringbuffer_write_space(host->rb_rtmem) < sizeof(uintptr_t))
			fprintf(stderr, "tjost_request_memory: ring buffer overflow");
		else
			jack_ringbuffer_write(host->rb_rtmem, (const char *)&area, sizeof(uintptr_t));
	}
}

static void
tjost_add_memory(Tjost_Host *host)
{
	void *area;

	while(jack_ringbuffer_read_space(host->rb_rtmem) >= sizeof(uintptr_t))
	{
		jack_ringbuffer_read(host->rb_rtmem, (char *)&area, sizeof(uintptr_t));
		add_new_area(area, area_size, host->pool);
	}
}

void *
tjost_alloc(Tjost_Host *host, size_t len)
{
	void *data = NULL;
	
	if(!(data = malloc_ex(len, host->pool)))
		tjost_host_message_push(host, "tjost_alloc: out of memory");

	size_t used = get_used_size(host->pool);
	if(used > host->rtmem_sum/2) //TODO make this configurable
	{
		ev_async_send(EV_DEFAULT, &host->rtmem);
		host->rtmem_sum += area_size;
	}

	/*
	tjost_host_message_push(host, "tjost_alloc: used size: %f",
		(float)used / host->rtmem_sum);
	*/

	return data;
}

void *
tjost_realloc(Tjost_Host *host, size_t len, void *buf)
{
	void *data = NULL;

	if(!(data =realloc_ex(buf, len, host->pool)))
		tjost_host_message_push(host, "tjost_realloc: out of memory");

	size_t used = get_used_size(host->pool);
	if(used > host->rtmem_sum/2) //TODO make this configurable
	{
		ev_async_send(EV_DEFAULT, &host->rtmem);
		host->rtmem_sum += area_size;
	}

	return data;
}

void
tjost_free(Tjost_Host *host, void *buf)
{
	free_ex(buf, host->pool);
}

static int
_process_indirect(jack_nframes_t nframes, void *arg)
{
	Tjost_Host *host = arg;
	Tjost_Module *module;

	jack_nframes_t last = jack_last_frame_time(host->client);

	// extend memory if requested
	tjost_add_memory(host);

	// receive from uplink ringbuffer
	tjost_uplink_rx_drain(host);

	// receive on all inputs
	EINA_INLIST_FOREACH(host->inputs, module)
		module->process(nframes, module);

	// handle main queue events
	Eina_Inlist *l;
	Tjost_Event *tev;
	EINA_INLIST_FOREACH_SAFE(host->queue, l, tev)
	{
		if(tev->time >= last + nframes)
			break;

		if(tev->time == 0) // immediate execution
			tev->time = last;

		if(tev->time >= last)
		{
			if(tev->module == TJOST_MODULE_BROADCAST)
				tjost_lua_deserialize_broadcast(tev, host->uplinks);
			else
				tjost_lua_deserialize_unicast(tev);
		}
		else
			tjost_host_message_push(host, "main loop: ignoring out-of-order event %u %u",
				last, tev->time);

		host->queue = eina_inlist_remove(host->queue, EINA_INLIST_GET(tev));
		tjost_free(host, tev);
	}

	// send on all outputs
	EINA_INLIST_FOREACH(host->outputs, module)
		module->process(nframes, module);

	// send on all uplinks
	EINA_INLIST_FOREACH(host->uplinks, module)
		module->process(nframes, module);

	// write uplink events to rinbbuffer
	ev_async_send(EV_DEFAULT, &host->uplink_tx);

	// run garbage collection step
	lua_gc(host->L, LUA_GCSTEP, 0);
	
	return 0;
}

static int
_process_direct(jack_nframes_t nframes, void *arg)
{
	Tjost_Host *host = arg;
	Tjost_Module *module;

	jack_nframes_t last = jack_last_frame_time(host->client);

	// extend memory if requested
	tjost_add_memory(host);

	// receive from uplink ringbuffer
	tjost_uplink_rx_drain(host);

	// receive on all inputs
	EINA_INLIST_FOREACH(host->inputs, module)
		module->process(nframes, module);

	// handle main queue events
	Eina_Inlist *l;
	Tjost_Event *tev;
	EINA_INLIST_FOREACH_SAFE(host->queue, l, tev)
	{

		EINA_INLIST_FOREACH(host->outputs, module)
			tjost_module_schedule(module, tev->time, tev->size, tev->buf);

		host->queue = eina_inlist_remove(host->queue, EINA_INLIST_GET(tev));
		tjost_free(host, tev);
	}

	// send on all outputs
	EINA_INLIST_FOREACH(host->outputs, module)
		module->process(nframes, module);

	// send on all uplinks
	EINA_INLIST_FOREACH(host->uplinks, module)
		module->process(nframes, module);

	// write uplink events to rinbbuffer
	ev_async_send(EV_DEFAULT, &host->uplink_tx);
	
	return 0;
}

int
main(int argc, const char **argv)
{
	static Tjost_Host host;
	char *mod_path = NULL;
	char *server_name = NULL;

#define FAIL(...) \
{ \
	fprintf(stderr, "FAIL: "__VA_ARGS__); \
	goto cleanup; \
}

	int indirect;
	if(!strcmp(argv[1], "-i"))
		indirect = 1;
	else if (!strcmp(argv[1], "-d"))
		indirect = 0;
	else
		FAIL("choose whether to run in (-i)indirect or (-d)irect mode\n");

	// init eina
	eina_init();

	// init memory pool
  if(!(host.pool = mmap(NULL, area_size, PROT_READ|PROT_WRITE, MAP_32BIT|MAP_PRIVATE|MAP_ANONYMOUS|MAP_LOCKED, -1, 0)))
		FAIL("could not allocate RT memory chunk\n");
	mlock(host.pool, area_size);
	host.rtmem_sum = area_size;
	
	if(init_memory_pool(area_size, host.pool) == -1)
		FAIL("could not initialize rt event memory pool\n");
	
	// init jack
	jack_options_t options = server_name ? JackNullOption | JackServerName : JackNullOption;
	jack_status_t status;
	if(!(host.client = jack_client_open("Tjost", options, &status, server_name)))
		FAIL("could not open client\n");
	if(indirect)
	{
		if(jack_set_process_callback(host.client, _process_indirect, &host))
			FAIL("could not set process callback\n");
	}
	else // direct
	{
		if(jack_set_process_callback(host.client, _process_direct, &host))
			FAIL("could not set process callback\n");
	}
	jack_on_shutdown(host.client, _shutdown, &host);

	if(jack_set_client_registration_callback(host.client, tjost_client_registration, &host))
		FAIL("could not set client_registration callback\n");
	if(jack_set_port_registration_callback(host.client, tjost_port_registration, &host))
		FAIL("could not set port_registration callback\n");
	if(jack_set_port_connect_callback(host.client, tjost_port_connect, &host))
		FAIL("could not set port_connect callback\n");
	if(jack_set_port_rename_callback(host.client, tjost_port_rename, &host))
		FAIL("could not set port_rename callback\n");
	if(jack_set_graph_order_callback(host.client, tjost_graph_order, &host))
		FAIL("could not set graph_order callback\n");

	// init message ringbuffer
	if(!(host.rb_msg = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		FAIL("could not initialize ringbuffer\n");

	// init uplink ringbuffer
	if(!(host.rb_uplink_rx = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		FAIL("could not initialize ringbuffer\n");
	if(!(host.rb_uplink_tx = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		FAIL("could not initialize ringbuffer\n");

	// init realtime memory ringbuffer
	if(!(host.rb_rtmem = jack_ringbuffer_create(sizeof(uintptr_t)*4+1)))
		FAIL("could not initialize ringbuffer\n");

	host.srate = jack_get_sample_rate(host.client);

	// init and load modules
	host.arr = eina_module_list_get(NULL, "/usr/local/lib/tjost", EINA_FALSE, NULL, NULL);
	if(host.arr)
		eina_module_list_load(host.arr);

	// init Lua
	if(!(host.L = lua_newstate(_alloc, &host)))
		FAIL("could not initialize Lua\n");
	luaL_openlibs(host.L);

#if LUA_VERSION_NUM == 502
	lua_pushglobaltable(host.L);
#elif LUA_VERSION_NUM == 501
	lua_pushvalue(host.L, LUA_GLOBALSINDEX);
#endif
	lua_pushlightuserdata(host.L, &host);
	luaL_openlib(host.L, NULL, tjost_globals, 1);
	lua_pop(host.L, 1); // _G

	// register metatables
	luaL_newmetatable(host.L, "Tjost_Input"); // mt
	luaL_register(host.L, NULL, tjost_input_mt);
	lua_pushvalue(host.L, -1);
	lua_setfield(host.L, -2, "__index");
	lua_pop(host.L, 1); // mt

	luaL_newmetatable(host.L, "Tjost_Output"); // mt
	luaL_register(host.L, NULL, tjost_output_mt);
	lua_pushvalue(host.L, -1);
	lua_setfield(host.L, -2, "__index");
	lua_pop(host.L, 1); // mt

	luaL_newmetatable(host.L, "Tjost_Uplink"); // mt
	luaL_register(host.L, NULL, tjost_uplink_mt);
	lua_pushvalue(host.L, -1);
	lua_setfield(host.L, -2, "__index");
	lua_pop(host.L, 1); // mt

	lua_getglobal(host.L, "package");
	lua_getfield(host.L, -1, "cpath");
	lua_pushstring(host.L, ";/usr/local/lib/tjost/lua/?.so");
	lua_concat(host.L, 2);
	lua_setfield(host.L, -2, "cpath");
	lua_pop(host.L, 1); // package

	// push command line arguments
	lua_createtable(host.L, argc, 0);
	int i;
	for(i=0; i<argc; i++) {
		lua_pushstring(host.L, argv[i]);
		lua_rawseti(host.L, -2, i+1);
	}
	lua_setglobal(host.L, "argv");

	// load file
	if(luaL_dofile(host.L, argv[2]))
		FAIL("error loading file: %s\n", lua_tostring(host.L, -1));
	lua_gc(host.L, LUA_GCSTOP, 0); // disable automatic garbage collection

	// activate JACK
	if(jack_activate(host.client))
		FAIL("could not activate jack client\n");

	// init libev
	ev_signal_init(&host.sigterm, _sig, SIGTERM);
	ev_signal_init(&host.sigquit, _sig, SIGQUIT);
	ev_signal_init(&host.sigint, _sig, SIGINT);
	ev_signal_start(EV_DEFAULT, &host.sigterm);
	ev_signal_start(EV_DEFAULT, &host.sigquit);
	ev_signal_start(EV_DEFAULT, &host.sigint);

	ev_async_init(&host.quit, _quit);
	ev_async_start(EV_DEFAULT, &host.quit);

	host.msg.data = &host;
	ev_async_init(&host.msg, _msg);
	ev_async_start(EV_DEFAULT, &host.msg);

	host.uplink_tx.data = &host;
	ev_async_init(&host.uplink_tx, tjost_uplink_tx_drain);
	ev_async_start(EV_DEFAULT, &host.uplink_tx);

	host.rtmem.data = &host;
	ev_async_init(&host.rtmem, tjost_request_memory);
	ev_async_start(EV_DEFAULT, &host.rtmem);

	ev_set_syserr_cb(_err);

	// run libev
	ev_run(EV_DEFAULT, EVRUN_NOWAIT);
	ev_run(EV_DEFAULT, 0);

cleanup:
	fprintf(stderr, "cleaning up\n");

	// deinit libev
	ev_async_stop(EV_DEFAULT, &host.rtmem);
	ev_async_stop(EV_DEFAULT, &host.uplink_tx);
	ev_async_stop(EV_DEFAULT, &host.msg);
	ev_async_stop(EV_DEFAULT, &host.quit);

	ev_signal_stop(EV_DEFAULT, &host.sigterm);
	ev_signal_stop(EV_DEFAULT, &host.sigquit);
	ev_signal_stop(EV_DEFAULT, &host.sigint);
	
	if(host.client)
		jack_deactivate(host.client);

	// deinit Lua
	if(host.L)
		lua_close(host.L);

	// drain queue
	Eina_Inlist *itm;
	EINA_INLIST_FREE(host.queue, itm)
	{
		Tjost_Event *tev = EINA_INLIST_CONTAINER_GET(itm, Tjost_Event);
		host.queue = eina_inlist_remove(host.queue, itm);
		tjost_free(&host, tev);
	}

	// unload, deinit and free modules
	if(host.arr)
	{
		eina_module_list_unload(host.arr);
		eina_module_list_free(host.arr);
	}

	// deinit jack
	if(host.client)
		jack_client_close(host.client);

	// deinit message ringbuffer
	if(host.rb_msg)
		jack_ringbuffer_free(host.rb_msg);

	// deinit message ringbuffer
	if(host.rb_uplink_rx)
		jack_ringbuffer_free(host.rb_uplink_rx);
	if(host.rb_uplink_tx)
		jack_ringbuffer_free(host.rb_uplink_tx);

	// free server name string
	if(server_name)
		free(server_name);

	// free module path string
	if(mod_path)
		free(mod_path);

	// deinit Rt memory pool
	if(host.pool)
	{
		destroy_memory_pool(host.pool);
		munlock(host.pool, area_size);
		munmap(host.pool, area_size);
	}

	// deinit eina
	eina_shutdown();

	return 0;
}
