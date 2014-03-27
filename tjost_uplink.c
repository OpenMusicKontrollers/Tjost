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

static uint8_t buf_rx [TJOST_BUF_SIZE] __attribute__((aligned (8)));
static uint8_t buf_tx [TJOST_BUF_SIZE] __attribute__((aligned (8)));

// non real time
static void
tjost_uplink_rx_push(Tjost_Host *host, Tjost_Module *module, uint8_t *buf, size_t size)
{
	Tjost_Event tev;
	tev.module = module;
	tev.time = 0; // schedule for immediate execution
	tev.size = size;

	if(jack_ringbuffer_write_space(host->rb_uplink_rx) < sizeof(Tjost_Event) + tev.size)
		fprintf(stderr, "Tjost uplink buffer overflow");
	else
	{
		jack_ringbuffer_write(host->rb_uplink_rx, (const char *)&tev, sizeof(Tjost_Event));
		jack_ringbuffer_write(host->rb_uplink_rx, (const char *)buf, tev.size);
	}
}

static int
_connect(jack_nframes_t time, const char *path, const char *fmt, uint8_t *buf, void *dat)
{
	Tjost_Event *tev = dat;
	Tjost_Module *module = tev->module;
	Tjost_Host *host = module->host;

	const char *port_a, *port_b;
	uint8_t *ptr = buf;

	ptr = jack_osc_get_string(ptr, &port_a);
	ptr = jack_osc_get_string(ptr, &port_b);

	jack_connect(host->client, port_a, port_b);

	return 1;
}

static int
_disconnect(jack_nframes_t time, const char *path, const char *fmt, uint8_t *buf, void *dat)
{
	Tjost_Event *tev = dat;
	Tjost_Module *module = tev->module;
	Tjost_Host *host = module->host;

	const char *port_a, *port_b;
	uint8_t *ptr = buf;

	ptr = jack_osc_get_string(ptr, &port_a);
	ptr = jack_osc_get_string(ptr, &port_b);

	jack_disconnect(host->client, port_a, port_b);

	return 1;
}

static int
_ports(jack_nframes_t time, const char *path, const char *fmt, uint8_t *buf, void *dat)
{
	Tjost_Event *tev = dat;
	Tjost_Module *module = tev->module;
	Tjost_Host *host = module->host;

	const char **ports = jack_get_ports(host->client, NULL, NULL, 0);

	if(ports)
	{
		const char **name;
		uint8_t *ptr = buf_rx;

		ptr = jack_osc_set_path(ptr, "/jack/ports");

		uint8_t *fmt = ptr;
		*ptr++ = ',';
		for(name=ports; *name; name++)
			*ptr++ = 's';
		*ptr++ = '\0';
		while( ((ptr - fmt) % 4) )
			*ptr++ = '\0';

		for(name=ports; *name; name++)
			ptr = jack_osc_set_string(ptr, *name);

		jack_free(ports);
		tjost_uplink_rx_push(host, module, buf_rx, ptr-buf_rx);
	}
	else
	{
		uint8_t *ptr = buf_rx;
		ptr = jack_osc_set_path(ptr, "/jack/ports");
		ptr = jack_osc_set_fmt(ptr, "");
		tjost_uplink_rx_push(host, module, buf_rx, ptr-buf_rx);
	}

	return 1;
}

static int
_connections(jack_nframes_t time, const char *path, const char *fmt, uint8_t *buf, void *dat)
{
	Tjost_Event *tev = dat;
	Tjost_Module *module = tev->module;
	Tjost_Host *host = module->host;

	uint8_t *ptr = buf;
	const char *port_name;

	ptr = jack_osc_get_string(ptr, &port_name);

	jack_port_t *port = jack_port_by_name(host->client, port_name);

	const char **connections = jack_port_get_all_connections(host->client, port);

	if(connections)
	{
		const char **name;
		uint8_t *ptr = buf_rx;

		ptr = jack_osc_set_path(ptr, "/jack/connections");

		uint8_t *fmt = ptr;
		*ptr++ = ',';
		*ptr++ = 's';
		for(name=connections; *name; name++)
			*ptr++ = 's';
		*ptr++ = '\0';
		while( ((ptr - fmt) % 4) )
			*ptr++ = '\0';

		ptr = jack_osc_set_string(ptr, port_name);
		for(name=connections; *name; name++)
			ptr = jack_osc_set_string(ptr, *name);

		jack_free(connections);
		tjost_uplink_rx_push(host, module, buf_rx, ptr-buf_rx);
	}
	else
	{
		uint8_t *ptr = buf_rx;
		ptr = jack_osc_set_path(ptr, "/jack/connections");
		ptr = jack_osc_set_fmt(ptr, "s");
		ptr = jack_osc_set_string(ptr, port_name);
		tjost_uplink_rx_push(host, module, buf_rx, ptr-buf_rx);
	}

	return 1;
}

static Jack_OSC_Method methods [] = {
	{"/jack/connect", "ss", _connect},
	{"/jack/disconnect", "ss", _disconnect},

	{"/jack/ports", "", _ports},
	{"/jack/connections", "s", _connections},

	{NULL, NULL, NULL}
};

// non real time
void
tjost_uplink_tx_drain(uv_async_t *handle, int status)
{
	Tjost_Host *host = handle->data;

	// Rx
	Tjost_Event tev;
	while(jack_ringbuffer_read_space(host->rb_uplink_tx) >= sizeof(Tjost_Event))
	{
		jack_ringbuffer_peek(host->rb_uplink_tx, (char *)&tev, sizeof(Tjost_Event));
		if(jack_ringbuffer_read_space(host->rb_uplink_tx) >= sizeof(Tjost_Event) + tev.size)
		{
			jack_ringbuffer_read_advance(host->rb_uplink_tx, sizeof(Tjost_Event));
			jack_ringbuffer_read(host->rb_uplink_tx, (char *)buf_tx, tev.size);

			jack_osc_method_dispatch(tev.time, buf_tx, tev.size, methods, &tev);
		}
		else
			break;
	}
}

void
tjost_uplink_tx_push(Tjost_Host *host, Tjost_Event *tev)
{
	if(jack_ringbuffer_write_space(host->rb_uplink_tx) < sizeof(Tjost_Event) + tev->size)
		tjost_host_message_push(host, "uplink: %s", "ringbuffer overflow");
	else
	{
		jack_ringbuffer_write(host->rb_uplink_tx, (const char *)tev, sizeof(Tjost_Event));
		jack_ringbuffer_write(host->rb_uplink_tx, (const char *)tev->buf, tev->size);
	}
}

// real time
void
tjost_uplink_rx_drain(Tjost_Host *host, int ignore)
{
	Tjost_Event tev;
	while(jack_ringbuffer_read_space(host->rb_uplink_rx) >= sizeof(Tjost_Event))
	{
		jack_ringbuffer_peek(host->rb_uplink_rx, (char *)&tev, sizeof(Tjost_Event));
		if(jack_ringbuffer_read_space(host->rb_uplink_rx) >= sizeof(Tjost_Event) + tev.size)
		{
			jack_ringbuffer_read_advance(host->rb_uplink_rx, sizeof(Tjost_Event));

			if(ignore)
				jack_ringbuffer_read_advance(host->rb_uplink_rx, tev.size);
			else
			{
				uint8_t *bf = tjost_host_schedule_inline(host, tev.module, tev.time, tev.size);
				jack_ringbuffer_read(host->rb_uplink_rx, (char *)bf, tev.size);
			}
		}
		else
			break;
	}
}

// non real time
void
tjost_client_registration(const char *name, int state, void *arg)
{
	Tjost_Host *host = arg;

	size_t len;
	if(state)
		len = jack_osc_vararg_set(buf_rx, "/jack/client/registration", "sT", name);
	else
		len = jack_osc_vararg_set(buf_rx, "/jack/client/registration", "sF", name);
	tjost_uplink_rx_push(host, TJOST_MODULE_BROADCAST, buf_rx, len);
}

// non real time
void
tjost_port_registration(jack_port_id_t id, int state, void *arg)
{
	Tjost_Host *host = arg;

	const jack_port_t *port = jack_port_by_id(host->client, id);
	const char *name = jack_port_name(port);

	size_t len;
	if(state)
		len = jack_osc_vararg_set(buf_rx, "/jack/port/registration", "sT", name);
	else
		len = jack_osc_vararg_set(buf_rx, "/jack/port/registration", "sF", name);
	tjost_uplink_rx_push(host, TJOST_MODULE_BROADCAST, buf_rx, len);
}

// non real time
void
tjost_port_connect(jack_port_id_t id_a, jack_port_id_t id_b, int state, void *arg)
{
	Tjost_Host *host = arg;

	const jack_port_t *port_a = jack_port_by_id(host->client, id_a);
	const jack_port_t *port_b = jack_port_by_id(host->client, id_b);
	const char *name_a = jack_port_name(port_a);
	const char *name_b = jack_port_name(port_b);

	size_t len;
	if(state)
		len = jack_osc_vararg_set(buf_rx, "/jack/port/connect", "ssT", name_a, name_b);
	else
		len = jack_osc_vararg_set(buf_rx, "/jack/port/connect", "ssF", name_a, name_b);
	tjost_uplink_rx_push(host, TJOST_MODULE_BROADCAST, buf_rx, len);
}

// non real time
int
tjost_port_rename(jack_port_id_t port, const char *old_name, const char *new_name, void *arg)
{
	Tjost_Host *host = arg;

	size_t len = jack_osc_vararg_set(buf_rx, "/jack/graph/rename", "iss", port, old_name, new_name);
	tjost_uplink_rx_push(host, TJOST_MODULE_BROADCAST, buf_rx, len);

	return 0;
}

// non real time
int
tjost_graph_order(void *arg)
{
	Tjost_Host *host = arg;

	size_t len = jack_osc_vararg_set(buf_rx, "/jack/graph/order", "");
	tjost_uplink_rx_push(host, TJOST_MODULE_BROADCAST, buf_rx, len);

	return 0;
}
