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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sched.h>
#include <assert.h>
#include <malloc.h>
#include <sys/mman.h>

#include <tjost.h>

static void _tjost_deinit(Tjost_Host *host);

static void
_sig(uv_signal_t *handle, int signum)
{
	Tjost_Host *host = handle->data;
	_tjost_deinit(host);
}

static void
_quit(uv_async_t *handle)
{

	Tjost_Host *host = handle->data;
	_tjost_deinit(host);
}

static void
_msg(uv_async_t *handle)
{
	static char str [1024]; // TODO how big?

	Tjost_Host *host = handle->data;

	while(tjost_host_message_pull(host, str))
		fprintf(stderr, "MESSAGE: %s\n", str);
}

static void
_shutdown(void *arg)
{
	Tjost_Host *host = arg;

	int err;
	if((err = uv_async_send(&host->quit)))
		fprintf(stderr, "_shutdown: %s\n", uv_err_name(err));
}

static void *
_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
	Tjost_Host *host = ud;
	(void)osize;

	if(nsize == 0) {
		if(ptr)
			tjost_free(host, ptr);
		return NULL;
	}
	else {
		if(ptr)
			return tjost_realloc(host, nsize, ptr);
		else
			return tjost_alloc(host, nsize);
	}
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

osc_data_t *
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
		; //FIXME report error
	else
	{
		jack_ringbuffer_write(host->rb_msg, (const char *)&size, sizeof(size_t));
		jack_ringbuffer_write(host->rb_msg, str, size);
	}

	int err;
	if((err = uv_async_send(&host->msg)))
		; //FIXME report error
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
	
//#define area_size 0x100000UL // 1MB
#define area_size 0x1000000UL // 16MB

static Tjost_Mem_Chunk *
tjost_map_memory_chunk(size_t size)
{
	void *area = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_32BIT|MAP_PRIVATE|MAP_ANONYMOUS|MAP_LOCKED, -1, 0);

	if(area)
	{
		Tjost_Mem_Chunk *chunk = calloc(1, sizeof(Tjost_Mem_Chunk));

		chunk->size = size;
		chunk->area = area;
		return chunk;
	}
	else
		return NULL;
}

static void
tjost_unmap_memory_chunk(Tjost_Mem_Chunk *chunk)
{
	if(chunk)
	{
		//munmap(chunk->area, chunk->size); // done automatically at process end
		free(chunk);
	}
}

static void
tjost_request_memory(uv_async_t *handle)
{
	Tjost_Host *host = handle->data;
	Tjost_Mem_Chunk *chunk;

	if(!(chunk = tjost_map_memory_chunk(area_size)))
		fprintf(stderr, "tjost_request_memory: could not allocate RT memory chunk\n");
	else
	{
		if(jack_ringbuffer_write_space(host->rb_rtmem) < sizeof(uintptr_t))
			fprintf(stderr, "tjost_request_memory: ring buffer overflow");
		else
			jack_ringbuffer_write(host->rb_rtmem, (const char *)&chunk, sizeof(uintptr_t));
	}
}

static size_t used = 0;

static void
tjost_add_memory(Tjost_Host *host)
{
	if(jack_ringbuffer_read_space(host->rb_rtmem) >= sizeof(uintptr_t))
	{
		Tjost_Mem_Chunk *chunk;

		jack_ringbuffer_read(host->rb_rtmem, (char *)&chunk, sizeof(uintptr_t));

		chunk->pool = tlsf_add_pool(host->tlsf, chunk->area, chunk->size);
		host->rtmem_chunks = eina_inlist_prepend(host->rtmem_chunks, EINA_INLIST_GET(chunk));
		host->rtmem_sum += chunk->size;
		host->rtmem_flag = 0;
		
		tjost_host_message_push(host, "Rt memory extended to: 0x%x bytes", host->rtmem_sum);
	}
}

static void
tjost_free_memory(Tjost_Host *host)
{
	Eina_Inlist *l;
	Tjost_Mem_Chunk *chunk;
	EINA_INLIST_FOREACH_SAFE(host->rtmem_chunks, l, chunk)
	{
		tlsf_remove_pool(host->tlsf, chunk->pool);
		host->rtmem_sum -= chunk->size;
		host->rtmem_chunks = eina_inlist_remove(host->rtmem_chunks, EINA_INLIST_GET(chunk));
		tjost_unmap_memory_chunk(chunk);
	}
}

void *
tjost_alloc(Tjost_Host *host, size_t len)
{
	void *data = NULL;
	
	if(!(data = tlsf_malloc(host->tlsf, len)))
		tjost_host_message_push(host, "tjost_alloc: out of memory");

	used += tlsf_block_size(data);

	if( (host->rtmem_flag == 0) && (used > host->rtmem_sum/2) ) //TODO make this configurable
	{
		host->rtmem_flag = 1;
		uv_async_send(&host->rtmem);
	}

	return data;
}

void *
tjost_realloc(Tjost_Host *host, size_t len, void *buf)
{
	void *data = NULL;
	
	used -= tlsf_block_size(buf);

	if(!(data =tlsf_realloc(host->tlsf, buf, len)))
		tjost_host_message_push(host, "tjost_realloc: out of memory");
	
	used += tlsf_block_size(data);

	if( (host->rtmem_flag == 0) && (used > host->rtmem_sum/2) ) //TODO make this configurable
	{
		host->rtmem_flag = 1;
		uv_async_send(&host->rtmem);
	}

	return data;
}

void
tjost_free(Tjost_Host *host, void *buf)
{
	used -= tlsf_block_size(buf);
	tlsf_free(host->tlsf, buf);
}

static int
_process(jack_nframes_t nframes, void *arg)
{
	Tjost_Host *host = arg;
	Tjost_Module *module;
	Tjost_Child *child;
	Tjost_Module *uplink;

	jack_nframes_t last = jack_last_frame_time(host->client);

	// extend memory if requested
	tjost_add_memory(host);

	// receive from uplink ringbuffer
	tjost_uplink_rx_drain(host, 0);

	// reset uplink TX message counter
	host->pipe_uplink_tx_count = 0;

	// receive on all inputs
	EINA_INLIST_FOREACH(host->modules, module)
		if(module->type & TJOST_MODULE_INPUT)
			module->process_in(nframes, module);

	// handle main queue events
	Eina_Inlist *l;
	Tjost_Event *tev;
	EINA_INLIST_FOREACH_SAFE(host->queue, l, tev)
	{
		if(tev->time >= last + nframes)
			break;
		else if(tev->time == 0) // immediate execution
			tev->time = last;
		else if(tev->time < last)
		{
			tjost_host_message_push(host, "main loop: %s %i", "late event", tev->time - last);
			tev->time = last;
		}

		if(tev->module == TJOST_MODULE_BROADCAST) // is uplink message
		{
			EINA_INLIST_FOREACH(host->uplinks, uplink)
			{
				// send to all children modules
				EINA_INLIST_FOREACH(uplink->children, child)
					tjost_module_schedule(child->module, tev->time, tev->size, tev->buf);

				// serialize to Lua callback function
				if(uplink->has_lua_callback)
				{
					tev->module = uplink;
					tjost_lua_deserialize(tev);
				}
			}
		}
		else // != TJOST_MODULE_BROADCAST
		{
			// send to all children modules
			EINA_INLIST_FOREACH(tev->module->children, child)
				tjost_module_schedule(child->module, tev->time, tev->size, tev->buf);

			// serialize to Lua callback function
			if(tev->module->has_lua_callback)
			{
				//tjost_host_message_push(host, "main loop: Lua logic for %p", tev->module);
				tjost_lua_deserialize(tev);
			}
		}

		host->queue = eina_inlist_remove(host->queue, EINA_INLIST_GET(tev));
		tjost_free(host, tev);
	}

	// send on all outputs
	EINA_INLIST_FOREACH(host->modules, module)
		if(module->type & TJOST_MODULE_OUTPUT)
			module->process_out(nframes, module);

	// send on all uplinks
	EINA_INLIST_FOREACH(host->uplinks, module)
		module->process_out(nframes, module);

	// write uplink events to rinbbuffer
	if(host->pipe_uplink_tx_count > 0)
		if(tjost_pipe_flush(&host->pipe_uplink_tx))
			tjost_host_message_push(host, "tjost_pipe_flush failed");

	// run garbage collection step
	lua_gc(host->L, LUA_GCSTEP, 0); //TODO check if needed
	
	return 0;
}

#define FAIL(...) \
{ \
	fprintf(stderr, "FAIL: "__VA_ARGS__); \
	return -1; \
}

static int
_tjost_init(uv_loop_t *loop, Tjost_Host *host, int argc, const char **argv)
{
	// init Non Session Management
	const char *id = tjost_nsm_init(argc, argv);
	
	// init memory pool
	Tjost_Mem_Chunk *chunk;
	if(!(chunk = tjost_map_memory_chunk(area_size)))
		FAIL("could not allocate RT memory chunk\n");
	if(!(host->tlsf = tlsf_create_with_pool(chunk->area, chunk->size)))
		FAIL("could not initialize TLSF memory pool\n");
	chunk->pool = tlsf_get_pool(host->tlsf);
	host->rtmem_chunks = eina_inlist_prepend(host->rtmem_chunks, EINA_INLIST_GET(chunk));
	host->rtmem_sum = area_size;
	
	// init jack
	host->server_name = NULL; //FIXME
	jack_options_t options = host->server_name ? JackNullOption | JackServerName : JackNullOption;
	jack_status_t status;
	if(!(host->client = jack_client_open(id, options, &status, host->server_name)))
		FAIL("could not open client\n");
	if(jack_set_process_callback(host->client, _process, host))
		FAIL("could not set process callback\n");
	jack_on_shutdown(host->client, _shutdown, host);

	if(jack_set_client_registration_callback(host->client, tjost_client_registration, host))
		FAIL("could not set client_registration callback\n");
	if(jack_set_port_registration_callback(host->client, tjost_port_registration, host))
		FAIL("could not set port_registration callback\n");
	if(jack_set_port_connect_callback(host->client, tjost_port_connect, host))
		FAIL("could not set port_connect callback\n");
	//if(jack_set_port_rename_callback(host->client, tjost_port_rename, host))
	//	FAIL("could not set port_rename callback\n");
	if(jack_set_graph_order_callback(host->client, tjost_graph_order, host))
		FAIL("could not set graph_order callback\n");
#ifdef HAS_METADATA_API
	jack_uuid_parse(jack_get_uuid_for_client_name(host->client, jack_get_client_name(host->client)), &host->uuid);
	if(jack_set_property(host->client, host->uuid, JACK_METADATA_PRETTY_NAME, "Tjost", "text/plain"))
		FAIL("could not set client pretty name\n");

	if(jack_set_property_change_callback(host->client, tjost_property_change, host))
		FAIL("could not set property_change callback\n");
#endif // HAS_METADATA_API

	// init pipes
	if(tjost_pipe_init(&host->pipe_uplink_rx))
		FAIL("could not initialize uplink RX pipe\n");
	if(tjost_pipe_init(&host->pipe_uplink_tx))
		FAIL("could not initialize uplink TX pipe\n");
	if(tjost_pipe_listen_start(&host->pipe_uplink_tx, loop,
			tjost_uplink_tx_drain_alloc, tjost_uplink_tx_drain_sched, NULL))
		FAIL("could not initialize listening on uplink RX pipe\n");

	// init message ringbuffer
	if(!(host->rb_msg = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		FAIL("could not initialize ringbuffer\n");
	// init realtime memory ringbuffer
	if(!(host->rb_rtmem = jack_ringbuffer_create(sizeof(Tjost_Mem_Chunk)*2+1)))
		FAIL("could not initialize ringbuffer\n");

	host->srate = jack_get_sample_rate(host->client);

	// init and load modules
	host->mod_path = NULL; //FIXME
	host->arr = eina_module_list_get(NULL, "/usr/local/lib/tjost", EINA_FALSE, NULL, NULL);
	if(host->arr)
		eina_module_list_load(host->arr);

	// init libuv
	int err;
	host->sigint.data = host;
	if((err = uv_signal_init(loop, &host->sigint)))
		FAIL("uv error: %s\n", uv_err_name(err));
	if((err = uv_signal_start(&host->sigint, _sig, SIGINT)))
		FAIL("uv error: %s\n", uv_err_name(err));

	host->sigterm.data = host;
	if((err = uv_signal_init(loop, &host->sigterm)))
		FAIL("uv error: %s\n", uv_err_name(err));
	if((err = uv_signal_start(&host->sigterm, _sig, SIGTERM)))
		FAIL("uv error: %s\n", uv_err_name(err));

	host->sigquit.data = host;
	if((err = uv_signal_init(loop, &host->sigquit)))
		FAIL("uv error: %s\n", uv_err_name(err));
	if((err = uv_signal_start(&host->sigquit, _sig, SIGQUIT)))
		FAIL("uv error: %s\n", uv_err_name(err));

	host->quit.data = host;
	if((err = uv_async_init(loop, &host->quit, _quit)))
		FAIL("uv error: %s\n", uv_err_name(err));

	host->msg.data = host;
	if((err = uv_async_init(loop, &host->msg, _msg)))
		FAIL("uv error: %s\n", uv_err_name(err));

	host->rtmem.data = host;
	if((err = uv_async_init(loop, &host->rtmem, tjost_request_memory)))
		FAIL("uv error: %s\n", uv_err_name(err));

	char *sep = strrchr(argv[1], '/');
	if(sep)
	{
		char *path = strndup(argv[1], sep - argv[1]); // extract 'path' from 'path/file'
		char *file = strdup(sep + 1); // extract 'file' from 'path/file'

		uv_chdir(path); // change current working directory to 'path'
		free(path);

		strcpy((char *)argv[1], file); // overwrite 'path/file' with 'file'
		free(file);
	}

	// init Lua
	if(!(host->L = lua_newstate(_alloc, host)))
		FAIL("could not initialize Lua\n");
	tjost_lua_init(host, argc, argv);

	// load file
	if(argv[1] && luaL_dofile(host->L, argv[1]))
		FAIL("error loading file: %s\n", lua_tostring(host->L, -1));
	lua_gc(host->L, LUA_GCSTOP, 0); // disable automatic garbage collection

	tjost_lua_deregister(host);
	
	// activate JACK
	if(jack_activate(host->client))
		FAIL("could not activate jack client\n");

	return 0;
}

static void
_tjost_deinit(Tjost_Host *host)
{
	// deactivate jack
	if(host->client)
		jack_deactivate(host->client);

	// deinit Lua
	tjost_lua_deinit(host);

	// deinit libuv
	uv_close((uv_handle_t *)&host->rtmem, NULL);
	uv_close((uv_handle_t *)&host->msg, NULL);
	uv_close((uv_handle_t *)&host->quit, NULL);

	int err;
	if((err = uv_signal_stop(&host->sigint)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));
	if((err = uv_signal_stop(&host->sigterm)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));
	if((err = uv_signal_stop(&host->sigquit)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));

	// drain main queue
	Eina_Inlist *l;
	Tjost_Event *tev;
	EINA_INLIST_FOREACH_SAFE(host->queue, l, tev)
	{
		host->queue = eina_inlist_remove(host->queue, EINA_INLIST_GET(tev));
		tjost_free(host, tev);
	}

	// unload, deinit and free modules
	if(host->arr)
	{
		eina_module_list_unload(host->arr);
		eina_module_list_free(host->arr);
		host->arr = NULL;
	}

	// free module path string
	if(host->mod_path)
	{
		free(host->mod_path);
		host->mod_path = NULL;
	}

	// deinit ringbuffers
	if(host->rb_rtmem)
	{
		jack_ringbuffer_free(host->rb_rtmem);
		host->rb_rtmem = NULL;
	}
	if(host->rb_msg)
	{
		jack_ringbuffer_free(host->rb_msg);
		host->rb_msg = NULL;
	}

	// deinit pipes
	tjost_pipe_listen_stop(&host->pipe_uplink_tx);
	tjost_pipe_deinit(&host->pipe_uplink_tx);
	tjost_pipe_deinit(&host->pipe_uplink_rx);

	// close jack
	if(host->client)
	{
#ifdef HAS_METADATA_API
		jack_remove_property(host->client, host->uuid, JACK_METADATA_PRETTY_NAME);
#endif
		jack_client_close(host->client);
		host->client = NULL;
	}

	// free server name string
	if(host->server_name)
	{
		free(host->server_name);
		host->server_name = NULL;
	}

	// deinit Rt memory pool
	if(host->tlsf)
	{
		tjost_free_memory(host);
		tlsf_destroy(host->tlsf);
		host->tlsf = NULL;
	}

	// deinit Non Session Management
	tjost_nsm_deinit();
}

int
main(int argc, const char **argv)
{
	static Tjost_Host host;

	// init eina
	eina_init();

	// get default loop
	uv_loop_t *loop = uv_default_loop();

	if(_tjost_init(loop, &host, argc, argv)) // init Tjost
	{
		fprintf(stderr, "_tjost_init failed\n");

		// deinit Tjost
		_tjost_deinit(&host);
		return -1;
	}
	else
		uv_run(loop, UV_RUN_DEFAULT); // run event loop

	// deinit eina
	eina_shutdown();

	return 0;
}
