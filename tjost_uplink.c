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

#include <tjost.h>

static osc_data_t buf_rx [TJOST_BUF_SIZE];

// non real time
static void
tjost_uplink_rx_push(Tjost_Host *host, Tjost_Module *module, osc_data_t *buf, size_t size)
{
	if(tjost_pipe_produce(&host->pipe_uplink_rx, module, 0, size, buf))
		fprintf(stderr, "tjost_uplink_rx_push: tjost_pipe_produce error\n");
}

static int
_connect(osc_time_t time, const char *path, const char *fmt, osc_data_t *buf, size_t size, void *dat)
{
	Tjost_Event *tev = dat;
	Tjost_Module *module = tev->module;
	Tjost_Host *host = module->host;

	const char *port_a, *port_b;
	osc_data_t *ptr = buf;

	ptr = osc_get_string(ptr, &port_a);
	ptr = osc_get_string(ptr, &port_b);

	jack_connect(host->client, port_a, port_b);

	return 1;
}

static int
_disconnect(osc_time_t time, const char *path, const char *fmt, osc_data_t *buf, size_t size, void *dat)
{
	Tjost_Event *tev = dat;
	Tjost_Module *module = tev->module;
	Tjost_Host *host = module->host;

	const char *port_a, *port_b;
	osc_data_t *ptr = buf;

	ptr = osc_get_string(ptr, &port_a);
	ptr = osc_get_string(ptr, &port_b);

	jack_disconnect(host->client, port_a, port_b);

	return 1;
}

static int
_ports(osc_time_t time, const char *path, const char *fmt, osc_data_t *buf, size_t size, void *dat)
{
	Tjost_Event *tev = dat;
	Tjost_Module *module = tev->module;
	Tjost_Host *host = module->host;

	const char **ports = jack_get_ports(host->client, NULL, NULL, 0);

	if(ports)
	{
		const char **name;
		osc_data_t *ptr = buf_rx;
		osc_data_t *end = ptr + TJOST_BUF_SIZE;

		ptr = osc_set_path(ptr, end, "/jack/ports");

		char *fmt = (char *)ptr;
		char *fmt_ptr = fmt;
		*fmt_ptr++ = ',';
		for(name=ports; *name; name++)
			*fmt_ptr++ = 's';
		*fmt_ptr++ = '\0';
		while( ((fmt_ptr - fmt) % 4) )
			*fmt_ptr++ = '\0';

		ptr = (osc_data_t *)fmt_ptr;

		for(name=ports; *name; name++)
			ptr = osc_set_string(ptr, end, *name);

		jack_free(ports);
		size_t len = ptr-buf_rx;
		if(ptr)
			tjost_uplink_rx_push(host, module, buf_rx, len);
	}
	else
	{
		osc_data_t *ptr = buf_rx;
		osc_data_t *end = ptr + TJOST_BUF_SIZE;

		ptr = osc_set_path(ptr, end, "/jack/ports");
		ptr = osc_set_fmt(ptr, end, "");
		size_t len = ptr-buf_rx;
		if(ptr)
			tjost_uplink_rx_push(host, module, buf_rx, len);
	}

	return 1;
}

static int
_connections(osc_time_t time, const char *path, const char *fmt, osc_data_t *buf, size_t size, void *dat)
{
	Tjost_Event *tev = dat;
	Tjost_Module *module = tev->module;
	Tjost_Host *host = module->host;

	osc_data_t *ptr = buf;
	const char *port_name;

	ptr = osc_get_string(ptr, &port_name);

	jack_port_t *port = jack_port_by_name(host->client, port_name);

	const char **connections = jack_port_get_all_connections(host->client, port);

	if(connections)
	{
		const char **name;
		osc_data_t *ptr = buf_rx;
		osc_data_t *end = ptr + TJOST_BUF_SIZE;

		ptr = osc_set_path(ptr, end, "/jack/connections");

		char *fmt = (char *)ptr;
		char *fmt_ptr = fmt;
		*fmt_ptr++ = ',';
		*fmt_ptr++ = 's';
		for(name=connections; *name; name++)
			*fmt_ptr++ = 's';
		*fmt_ptr++ = '\0';
		while( ((fmt_ptr - fmt) % 4) )
			*fmt_ptr++ = '\0';

		ptr = (osc_data_t *)fmt_ptr;

		ptr = osc_set_string(ptr, end, port_name);
		for(name=connections; *name; name++)
			ptr = osc_set_string(ptr, end, *name);

		jack_free(connections);
		size_t len = ptr-buf_rx;
		if(ptr)
			tjost_uplink_rx_push(host, module, buf_rx, len);
	}
	else
	{
		osc_data_t *ptr = buf_rx;
		osc_data_t *end = ptr + TJOST_BUF_SIZE;
		ptr = osc_set_path(ptr, end, "/jack/connections");
		ptr = osc_set_fmt(ptr, end, "s");

		ptr = osc_set_string(ptr, end, port_name);
		size_t len = ptr-buf_rx;
		if(ptr)
			tjost_uplink_rx_push(host, module, buf_rx, len);
	}

	return 1;
}

static osc_method_t methods [] = {
	{"/jack/connect", "ss", _connect},
	{"/jack/disconnect", "ss", _disconnect},

	{"/jack/ports", "", _ports},
	{"/jack/connections", "s", _connections},

	{NULL, NULL, NULL}
};

// non real time
osc_data_t *
tjost_uplink_tx_drain_alloc(Tjost_Event *tev, void *arg)
{
	static osc_data_t buf_tx [TJOST_BUF_SIZE];

	return buf_tx;
}

int
tjost_uplink_tx_drain_sched(Tjost_Event *tev, osc_data_t *buf, void *arg)
{
	osc_dispatch_method(tev->time, buf, tev->size, methods, NULL, NULL, tev);

	return 0;
}

// real time
void
tjost_uplink_tx_push(Tjost_Host *host, Tjost_Event *tev)
{
	host->pipe_uplink_tx_count++;
	if(tjost_pipe_produce(&host->pipe_uplink_tx, tev->module, tev->time, tev->size, tev->buf))
		tjost_host_message_push(tev->module->host, "tjost_uplink_tx_push: tjost_pipe_produce failed");
}

// real time
static osc_data_t *
_tjost_uplink_rx_drain_alloc(Tjost_Event *tev, void *arg)
{
	Tjost_Host *host = arg;
	return tjost_host_schedule_inline(host, tev->module, tev->time, tev->size);
}

// real time
static int
_tjost_uplink_rx_drain_sched(Tjost_Event *tev, osc_data_t *buf, void *arg)
{
	return 0;
}

// real time
void
tjost_uplink_rx_drain(Tjost_Host *host, int ignore)
{
	if(tjost_pipe_consume(&host->pipe_uplink_rx,
			_tjost_uplink_rx_drain_alloc, _tjost_uplink_rx_drain_sched, host))
		tjost_host_message_push(host, "tjost_uplink_rx_drain: tjost_pipe_consume failed");
}

// non real time
void
tjost_client_registration(const char *name, int state, void *arg)
{
	Tjost_Host *host = arg;
	osc_data_t *ptr;

	size_t len;
	if(state)
		ptr = osc_set_vararg(buf_rx, buf_rx + TJOST_BUF_SIZE, "/jack/client/registration", "sT", name);
	else
		ptr = osc_set_vararg(buf_rx, buf_rx + TJOST_BUF_SIZE, "/jack/client/registration", "sF", name);
	if(ptr)
		tjost_uplink_rx_push(host, TJOST_MODULE_BROADCAST, buf_rx, ptr-buf_rx);
}

// non real time
void
tjost_port_registration(jack_port_id_t id, int state, void *arg)
{
	Tjost_Host *host = arg;
	osc_data_t *ptr;

	const jack_port_t *port = jack_port_by_id(host->client, id);
	if(port) //TODO can be (null), is this a bug in jack1?
	{
		const char *name = jack_port_name(port);

		size_t len;
		if(state)
			ptr = osc_set_vararg(buf_rx, buf_rx + TJOST_BUF_SIZE, "/jack/port/registration", "sT", name);
		else
			ptr = osc_set_vararg(buf_rx, buf_rx + TJOST_BUF_SIZE, "/jack/port/registration", "sF", name);
		if(ptr)
			tjost_uplink_rx_push(host, TJOST_MODULE_BROADCAST, buf_rx, ptr-buf_rx);
	}
}

// non real time
void
tjost_port_connect(jack_port_id_t id_a, jack_port_id_t id_b, int state, void *arg)
{
	Tjost_Host *host = arg;
	osc_data_t *ptr;

	const jack_port_t *port_a = jack_port_by_id(host->client, id_a);
	const jack_port_t *port_b = jack_port_by_id(host->client, id_b);
	const char *name_a = jack_port_name(port_a);
	const char *name_b = jack_port_name(port_b);

	size_t len;
	if(state)
		ptr = osc_set_vararg(buf_rx, buf_rx + TJOST_BUF_SIZE, "/jack/port/connect", "ssT", name_a, name_b);
	else
		ptr = osc_set_vararg(buf_rx, buf_rx + TJOST_BUF_SIZE, "/jack/port/connect", "ssF", name_a, name_b);
	if(ptr)
		tjost_uplink_rx_push(host, TJOST_MODULE_BROADCAST, buf_rx, ptr-buf_rx);
}

// non real time
void
tjost_port_rename(jack_port_id_t port, const char *old_name, const char *new_name, void *arg)
{
	Tjost_Host *host = arg;
	osc_data_t *ptr;

	ptr = osc_set_vararg(buf_rx, buf_rx + TJOST_BUF_SIZE, "/jack/graph/rename", "iss", port, old_name, new_name);
	if(ptr)
		tjost_uplink_rx_push(host, TJOST_MODULE_BROADCAST, buf_rx, ptr-buf_rx);
}

// non real time
int
tjost_graph_order(void *arg)
{
	Tjost_Host *host = arg;
	osc_data_t *ptr;

	ptr = osc_set_vararg(buf_rx, buf_rx + TJOST_BUF_SIZE, "/jack/graph/order", "");
	if(ptr)
		tjost_uplink_rx_push(host, TJOST_MODULE_BROADCAST, buf_rx, ptr-buf_rx);

	return 0;
}

#ifdef HAS_METADATA_API
#	include <jackey.h>
// non real time
void
tjost_property_change(jack_uuid_t uuid, const char *key, jack_property_change_t change, void *arg)
{
	Tjost_Host *host = arg;

	if( (change == PropertyDeleted) && !strcmp(key, JACKEY_EVENT_TYPES))
	{
		/* FIXME check whether this is one of our ports and reset event port type
		fprintf(stderr, "WARNING: metadata for event port type '%s' has been deleted\n", key);
		if(jack_set_property(host->client, uuid, JACK_METADATA_EVENT_KEY, JACK_METADATA_EVENT_TYPE_OSC, NULL))
			fprintf(stderr, "could not set event type\n");
		*/
	}
}
#endif // HAS_METADATA_API
