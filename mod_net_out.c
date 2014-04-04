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
#include <mod_net.h>

typedef struct _Data Data;

struct _Data {
	Mod_Net net;
};

// TCP is bidirectional
int
process_in(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	return mod_net_process_in(module, nframes);
}

int
process_out(jack_nframes_t nframes, void *arg)
{
	Tjost_Module *module = arg;
	return mod_net_process_out(module, nframes);
}

void
add(Tjost_Module *module, int argc, const char **argv)
{
	Data *dat = tjost_alloc(module->host, sizeof(Data));

	uv_loop_t *loop = uv_default_loop();

	if(!strncmp(argv[0], "osc.udp://", 10))
		dat->net.type = SOCKET_UDP;
	else if(!strncmp(argv[0], "osc.tcp://", 10))
		dat->net.type = SOCKET_TCP;
	else
		; //FIXME error

	if( (argc > 1) && argv[1])
	{
		float del = atof(argv[1]);
		dat->net.delay_sec = (uint32_t)del;
		dat->net.delay_nsec = (del - dat->net.delay_sec) * 1e9;
	}
	else
	{
		dat->net.delay_sec = 0UL;
		dat->net.delay_nsec = 0UL;
	}

	module->dat = dat;

	switch(dat->net.type)
	{
		case SOCKET_UDP:
			if(netaddr_udp_sender_init(&dat->net.handle.udp_tx, loop, argv[0])) //TODO close?
				fprintf(stderr, "could not initialize socket\n");
			module->type = TJOST_MODULE_OUTPUT;
			break;
		case SOCKET_TCP:
			if(netaddr_tcp_endpoint_init(&dat->net.handle.tcp, NETADDR_TCP_SENDER, loop, argv[0], mod_net_recv_cb, module)) //TODO close?
				fprintf(stderr, "could not initialize socket\n");
			module->type = TJOST_MODULE_IN_OUT;
			break;
	}

	if(!(dat->net.rb_out = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		fprintf(stderr, "could not initialize ringbuffer\n");
	if(!(dat->net.rb_in = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		fprintf(stderr, "could not initialize ringbuffer\n");

	int err;
	dat->net.asio.data = module;
	if((err = uv_async_init(loop, &dat->net.asio, mod_net_asio)))
		fprintf(stderr, "mod_net_out: %s\n", uv_err_name(err));

	dat->net.sync.data = module;
	if((err = uv_timer_init(loop, &dat->net.sync)))
		fprintf(stderr, "mod_net_out: %s\n", uv_err_name(err));
	if((err = uv_timer_start(&dat->net.sync, mod_net_sync, 0, 1000))) // ms
		fprintf(stderr, "mod_net_out: %s\n", uv_err_name(err));
}

void
del(Tjost_Module *module)
{
	Data *dat = module->dat;

	int err;
	if((err = uv_timer_stop(&dat->net.sync)))
		fprintf(stderr, "mod_net_out: %s\n", uv_err_name(err));
	uv_close((uv_handle_t *)&dat->net.asio, NULL);
	
	if(dat->net.rb_out)
		jack_ringbuffer_free(dat->net.rb_out);
	if(dat->net.rb_in)
		jack_ringbuffer_free(dat->net.rb_in);

	switch(dat->net.type)
	{
		case SOCKET_UDP:
			netaddr_udp_sender_deinit(&dat->net.handle.udp_tx);
			break;
		case SOCKET_TCP:
			netaddr_tcp_endpoint_deinit(&dat->net.handle.tcp);
			break;
	}

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
