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

#include <netaddr.h>

static const char *connect_msg = "/connect\0\0\0\0,\0\0\0";
static const char *disconnect_msg = "/disconnect\0,\0\0\0";

static void
_tcp_prefix_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	NetAddr_TCP_Endpoint *netaddr = handle->data;

	buf->base = (char *)netaddr->recv.buf;
	buf->len = netaddr->recv.nchunk < TJOST_BUF_SIZE ? netaddr->recv.nchunk : TJOST_BUF_SIZE;
}

static void
_tcp_prefix_recv_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	NetAddr_TCP_Endpoint *netaddr = stream->data;

	if(nread > 0)
	{
		if(nread == sizeof(int32_t))
			netaddr->recv.nchunk = ntohl(*(int32_t *)buf->base);
		else if(nread == netaddr->recv.nchunk)
		{
			netaddr->recv.cb((osc_data_t *)buf->base, nread, netaddr->recv.dat);
			netaddr->recv.nchunk = sizeof(int32_t);
		}
		else // nread != sizeof(int32_t) && nread != nchunk
		{
			//FIXME what should we do here?
			netaddr->recv.nchunk = sizeof(int32_t);
			fprintf(stderr, "_tcp_prefix_recv_cb: TCP packet size not matching\n");
		}
	}
	else if (nread < 0)
	{
		int err;
		if((err = uv_read_stop(stream)))
			fprintf(stderr, "uv_read_stop: %s\n", uv_err_name(err));
		uv_close((uv_handle_t *)stream, NULL);
		netaddr->streams = eina_list_remove(netaddr->streams, stream);
		free(stream);
		fprintf(stderr, "_tcp_prefix_recv_cb: %s\n", uv_err_name(nread));
	}
	else // nread == 0
		;
}

static void
_tcp_slip_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	NetAddr_TCP_Endpoint *netaddr = handle->data;

	buf->base = (char *)netaddr->recv.buf;
	buf->base += netaddr->recv.nchunk; // is there remaining chunk from last call?
	buf->len = TJOST_BUF_SIZE - netaddr->recv.nchunk;
}

static void
_tcp_slip_recv_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	NetAddr_TCP_Endpoint *netaddr = stream->data;

	if(nread > 0)
	{
		uint8_t *ptr = netaddr->recv.buf;
		nread += netaddr->recv.nchunk; // is there remaining chunk from last call?
		size_t size;
		size_t parsed;
		while( (parsed = slip_decode(ptr, nread, &size)) && (nread > 0) )
		{
			netaddr->recv.cb((osc_data_t *)ptr, size, netaddr->recv.dat);
			ptr += parsed;
			nread -= parsed;
		}
		if(nread > 0) // is there remaining chunk for next call?
		{
			memmove(netaddr->recv.buf, ptr, nread);
			netaddr->recv.nchunk = nread;
		}
		else
			netaddr->recv.nchunk = 0;
	}
	else if (nread < 0)
	{
		int err;
		if((err = uv_read_stop(stream)))
			fprintf(stderr, "uv_read_stop: %s\n", uv_err_name(err));
		uv_close((uv_handle_t *)stream, NULL);
		netaddr->streams = eina_list_remove(netaddr->streams, stream);
		free(stream);
		fprintf(stderr, "_tcp_slip_recv_cb: %s\n", uv_err_name(nread));
		if(nread == UV_EOF)
			netaddr->recv.cb((osc_data_t *)disconnect_msg, 16, netaddr->recv.dat);
	}
	else // nread == 0
		;
}

static void
_tcp_send_cb(uv_write_t *req, int status)
{
	uv_stream_t *stream = req->handle;
	NetAddr_TCP_Endpoint *netaddr = stream->data;

	netaddr->count--;

	if(!status)
	{
		if(!netaddr->count)
			netaddr->send.cb(netaddr->send.len, netaddr->send.dat);
	}
	else
		fprintf(stderr, "_tcp_send_cb: %s\n", uv_err_name(status));
}

static void
_responder_connect(uv_stream_t *responder, int status)
{
	if(status)
	{
		fprintf(stderr, "_responder_connect: %s\n", uv_err_name(status));
		return;
	}

	NetAddr_TCP_Endpoint *netaddr = responder->data;

	fprintf(stderr, "connect to sender\n");
	netaddr->recv.cb((osc_data_t *)connect_msg, 16, netaddr->recv.dat);

	uv_tcp_t *stream = calloc(1, sizeof(uv_tcp_t));
	stream->data = netaddr;
	netaddr->streams = eina_list_append(netaddr->streams, stream);

	uv_write_t *req = calloc(1, sizeof(uv_write_t));
	netaddr->reqs = eina_list_append(netaddr->reqs, req);

	int err;
	if((err = uv_tcp_init(responder->loop, stream)))
	{
		fprintf(stderr, "uv_tcp_init: %s\n", uv_err_name(err));
		return;
	}
	if((err = uv_tcp_nodelay(stream, 1))) // disable Nagle's algo
	{
		fprintf(stderr, "uv_tcp_nodelay: %s\n", uv_err_name(err));
		return;
	}
	if((err = uv_tcp_keepalive(stream, 1, 5))) // keepalive after 5 seconds
	{
		fprintf(stderr, "uv_tcp_keepalive: %s\n", uv_err_name(err));
		return;
	}

	if((err = uv_accept((uv_stream_t *)&netaddr->uni.socket, (uv_stream_t *)stream)))
	{
		fprintf(stderr, "uv_accept: %s\n", uv_err_name(err));
		return;
	}

	if(!netaddr->slip)
	{
		netaddr->recv.nchunk = sizeof(int32_t); // packet size as TCP preamble
		if((err = uv_read_start((uv_stream_t *)stream, _tcp_prefix_alloc, _tcp_prefix_recv_cb)))
		{
			fprintf(stderr, "uv_read_start: %s\n", uv_err_name(err));
			return;
		}
	}
	else //netaddr->slip
	{
		netaddr->recv.nchunk = 0;
		if((err = uv_read_start((uv_stream_t *)stream, _tcp_slip_alloc, _tcp_slip_recv_cb)))
		{
			fprintf(stderr, "uv_read_start: %s\n", uv_err_name(err));
			return;
		}
	}
}

static int
_netaddr_tcp_responder_init(NetAddr_TCP_Endpoint *netaddr, uv_loop_t *loop, const char *addr, NetAddr_Recv_Cb cb, void *dat)
{
	// Server: "osc.tcp://:3333"

	netaddr->type = NETADDR_TCP_RESPONDER;
	
	netaddr->uni.socket.data = netaddr;

	netaddr->recv.cb = cb;
	netaddr->recv.dat = dat;

	if(!strncmp(addr, "osc.tcp://", 10))
	{
		netaddr->version = NETADDR_IP_VERSION_4;
		netaddr->slip = 0;
		addr += 10;
	}
	else if(!strncmp(addr, "osc.tcp4://", 11))
	{
		netaddr->version = NETADDR_IP_VERSION_4;
		netaddr->slip = 0;
		addr += 11;
	}
	else if(!strncmp(addr, "osc.tcp6://", 11))
	{
		netaddr->version = NETADDR_IP_VERSION_6;
		netaddr->slip = 0;
		addr += 11;
	}
	else if(!strncmp(addr, "osc.slip.tcp://", 15))
	{
		netaddr->version = NETADDR_IP_VERSION_4;
		netaddr->slip = 1;
		addr += 15;
	}
	else if(!strncmp(addr, "osc.slip.tcp4://", 16))
	{
		netaddr->version = NETADDR_IP_VERSION_4;
		netaddr->slip = 1;
		addr += 16;
	}
	else if(!strncmp(addr, "osc.slip.tcp6://", 16))
	{
		netaddr->version = NETADDR_IP_VERSION_6;
		netaddr->slip = 1;
		addr += 16;
	}
	else
	{
		fprintf(stderr, "unsupported protocol in address %s\n", addr);
		return -1;
	}

	char *colon = strchr(addr, ':');
	if(!colon)
	{
		fprintf(stderr, "address must have a port\n");
		return -1;
	}

	uint16_t port = atoi(colon+1);

	int err;
	struct sockaddr_in recv_addr4;
	struct sockaddr_in6 recv_addr6;
	const struct sockaddr *recv_addr = NULL;
	unsigned int flags = 0;

	if((err = uv_tcp_init(loop, &netaddr->uni.socket)))
	{
		fprintf(stderr, "uv_tcp_init: %s\n", uv_err_name(err));
		return -1;
	}
	if(netaddr->version == NETADDR_IP_VERSION_4)
	{
		const char *host = "0.0.0.0";
		if((err = uv_ip4_addr(host, port, &recv_addr4)))
		{
			fprintf(stderr, "uv_ip4_addr: %s\n", uv_err_name(err));
			return -1;
		}
		recv_addr = (const struct sockaddr *)&recv_addr4;
	}
	else // NETADDR_IP_VERSION_6
	{
		const char *host = "::1";
		if((err = uv_ip6_addr(host, port, &recv_addr6)))
		{
			fprintf(stderr, "uv_ip6_addr: %s\n", uv_err_name(err));
			return -1;
		}
		recv_addr = (const struct sockaddr *)&recv_addr6;
		flags |= UV_TCP_IPV6ONLY; //TODO make this configurable
	}
	if((err = uv_tcp_bind(&netaddr->uni.socket, recv_addr, flags)))
	{
		fprintf(stderr, "uv_tcp_bind: %s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_listen((uv_stream_t *)&netaddr->uni.socket, 128, _responder_connect)))
	{
		fprintf(stderr, "uv_listen: %s\n", uv_err_name(err));
		return -1;
	}

	return 0;
}

static void
_sender_connect(uv_connect_t *conn, int status)
{
	NetAddr_TCP_Endpoint *netaddr = conn->data;

	uv_tcp_t *stream = eina_list_data_get(netaddr->streams);

	if(status)
	{
		fprintf(stderr, "_sender_connect %s\n", uv_err_name(status));
		return; //TODO
	}
	
	fprintf(stderr, "connect to responder\n");
	netaddr->recv.cb((osc_data_t *)connect_msg, 16, netaddr->recv.dat);

	int err;
	if(!netaddr->slip)
	{
		netaddr->recv.nchunk = sizeof(int32_t); // packet size as TCP preamble
		if((err = uv_read_start((uv_stream_t *)stream, _tcp_prefix_alloc, _tcp_prefix_recv_cb)))
		{
			fprintf(stderr, "uv_read_start: %s\n", uv_err_name(err));
			return;
		}
	}
	else //netaddr->slip
	{
		netaddr->recv.nchunk = 0;
		if((err = uv_read_start((uv_stream_t *)stream, _tcp_slip_alloc, _tcp_slip_recv_cb)))
		{
			fprintf(stderr, "uv_read_start: %s\n", uv_err_name(err));
			return;
		}
	}
}

static int
_netaddr_tcp_sender_init(NetAddr_TCP_Endpoint *netaddr, uv_loop_t *loop, const char *addr, NetAddr_Recv_Cb cb, void *dat)
{
	// Client: "osc.tcp://name.local:4444"

	netaddr->type = NETADDR_TCP_SENDER;

	netaddr->uni.conn.data = netaddr;

	netaddr->recv.cb = cb;
	netaddr->recv.dat = dat;
	
	uv_tcp_t *stream = calloc(1, sizeof(uv_tcp_t));
	stream->data = netaddr;
	netaddr->streams = eina_list_append(netaddr->streams, stream);

	uv_write_t *req = calloc(1, sizeof(uv_write_t));
	netaddr->reqs = eina_list_append(netaddr->reqs, req);

	if(!strncmp(addr, "osc.tcp://", 10))
	{
		netaddr->version = NETADDR_IP_VERSION_4;
		netaddr->slip = 0;
		addr += 10;
	}
	else if(!strncmp(addr, "osc.tcp4://", 11))
	{
		netaddr->version = NETADDR_IP_VERSION_4;
		netaddr->slip = 0;
		addr += 11;
	}
	else if(!strncmp(addr, "osc.tcp6://", 11))
	{
		netaddr->version = NETADDR_IP_VERSION_6;
		netaddr->slip = 0;
		addr += 11;
	}
	else if(!strncmp(addr, "osc.slip.tcp://", 15))
	{
		netaddr->version = NETADDR_IP_VERSION_4;
		netaddr->slip = 1;
		addr += 15;
	}
	else if(!strncmp(addr, "osc.slip.tcp4://", 16))
	{
		netaddr->version = NETADDR_IP_VERSION_4;
		netaddr->slip = 1;
		addr += 16;
	}
	else if(!strncmp(addr, "osc.slip.tcp6://", 16))
	{
		netaddr->version = NETADDR_IP_VERSION_6;
		netaddr->slip = 1;
		addr += 16;
	}
	else
	{
		fprintf(stderr, "unsupported protocol in address %s\n", addr);
		return -1;
	}

	char *colon = strchr(addr, ':');
	if(!colon)
	{
		fprintf(stderr, "address must have a port\n");
		return -1;
	}

	const char *host = NULL;
	host = addr;
	*colon = '\0';

	// DNS resolve
	struct addrinfo hints;
	struct addrinfo *ai;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = netaddr->version == NETADDR_IP_VERSION_4 ? PF_INET : PF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	if(getaddrinfo(host, colon+1, &hints, &ai))
	{
		fprintf(stderr, "address could not be resolved\n");
		return -1;
	}
	char remote [128] = {'\0'};
	struct sockaddr_in *ptr4 = (struct sockaddr_in *)ai->ai_addr;
	struct sockaddr_in6 *ptr6 = (struct sockaddr_in6 *)ai->ai_addr;
	const struct sockaddr *send_addr = NULL;

	int err;
	if(netaddr->version == NETADDR_IP_VERSION_4)
	{
		if((err = uv_ip4_name(ptr4, remote, 127)))
		{
			fprintf(stderr, "up_ip4_name: %s\n", uv_err_name(err));
			return -1;
		}
	}
	else // NETADDR_IP_VERSION_6
	{
		if((err = uv_ip6_name(ptr6, remote, 127)))
		{
			fprintf(stderr, "up_ip6_name: %s\n", uv_err_name(err));
			return -1;
		}
	}
	if((err = uv_tcp_init(loop, stream)))
	{
		fprintf(stderr, "uv_tcp_init: %s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_tcp_nodelay(stream, 1))) // disable Nagle's algo
	{
		fprintf(stderr, "uv_tcp_nodelay: %s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_tcp_keepalive(stream, 1, 5))) // keepalive after 5 seconds
	{
		fprintf(stderr, "uv_tcp_keepalive: %s\n", uv_err_name(err));
		return -1;
	}
	if(netaddr->version == NETADDR_IP_VERSION_4)
	{
		struct sockaddr_in send_addr4;
		if((err = uv_ip4_addr(remote, ntohs(ptr4->sin_port), &send_addr4)))
		{
			fprintf(stderr, "uv_ip4_addr: %s\n", uv_err_name(err));
			return -1;
		}
		send_addr = (const struct sockaddr *)&send_addr4;
	}
	else // NETADDR_IP_VERSION_6
	{
		struct sockaddr_in6 send_addr6;
		if((err = uv_ip6_addr(remote, ntohs(ptr6->sin6_port), &send_addr6)))
		{
			fprintf(stderr, "uv_ip6_addr: %s\n", uv_err_name(err));
			return -1;
		}
		send_addr = (const struct sockaddr *)&send_addr6;
	}
	if((err = uv_tcp_connect(&netaddr->uni.conn, stream, send_addr, _sender_connect)))
	{
		fprintf(stderr, "uv_tcp_connect: %s\n", uv_err_name(err));
		return -1;
	}

	return 0;
}

int
netaddr_tcp_endpoint_init(NetAddr_TCP_Endpoint *netaddr, NetAddr_TCP_Type type, uv_loop_t *loop, const char *addr, NetAddr_Recv_Cb cb, void *dat)
{
	switch(type)
	{
		case NETADDR_TCP_RESPONDER:
			return _netaddr_tcp_responder_init(netaddr, loop, addr, cb, dat);
			break;
		case NETADDR_TCP_SENDER:
			return _netaddr_tcp_sender_init(netaddr, loop, addr, cb, dat);
			break;
	}
}

static void
_netaddr_tcp_responder_deinit(NetAddr_TCP_Endpoint *netaddr)
{
	int err;
	uv_tcp_t *stream;
	uv_write_t *req;

	EINA_LIST_FREE(netaddr->streams,  stream)
	{
		if((err = uv_read_stop((uv_stream_t *)stream)))
			fprintf(stderr, "uv_read_stop: %s\n", uv_err_name(err));
		uv_close((uv_handle_t *)stream, NULL);
		free(stream);
	}
	uv_close((uv_handle_t *)&netaddr->uni.socket, NULL);

	EINA_LIST_FREE(netaddr->reqs, req)
	{
		uv_cancel((uv_req_t *)req);
		free(req);
	}
}

static void
_netaddr_tcp_sender_deinit(NetAddr_TCP_Endpoint *netaddr)
{
	int err;
	uv_tcp_t *stream;
	uv_write_t *req;

	EINA_LIST_FREE(netaddr->streams, stream)
	{
		if((err = uv_read_stop((uv_stream_t *)stream)))
			fprintf(stderr, "uv_read_stop: %s\n", uv_err_name(err));
		uv_close((uv_handle_t *)stream, NULL);
		free(stream);
	}
	//TODO close conn?

	EINA_LIST_FREE(netaddr->reqs, req)
	{
		uv_cancel((uv_req_t *)req);
		free(req);
	}
}

void
netaddr_tcp_endpoint_deinit(NetAddr_TCP_Endpoint *netaddr)
{
	switch(netaddr->type)
	{
		case NETADDR_TCP_RESPONDER:
			_netaddr_tcp_responder_deinit(netaddr);
			break;
		case NETADDR_TCP_SENDER:
			_netaddr_tcp_sender_deinit(netaddr);
			break;
	}
}

void
netaddr_tcp_endpoint_send(NetAddr_TCP_Endpoint *netaddr, uv_buf_t *bufs, int nbufs, size_t len, NetAddr_Send_Cb cb, void *dat)
{
	netaddr->send.cb = cb;
	netaddr->send.dat = dat;
	netaddr->send.len = len;

	if(netaddr->slip)
	{
		static uint8_t bb [TJOST_BUF_SIZE];
		static uv_buf_t bufa;
		bufa.base = (char *)bb;
		bufa.len = slip_encode(bb, &bufs[1], nbufs-1); // discard prefix size (int32_t)

		bufs = &bufa;
		nbufs = 1;
	}

	int err;
	netaddr->count = eina_list_count(netaddr->streams);
	int i;
	for(i=0; i<netaddr->count; i++)
	{
		uv_tcp_t *stream = eina_list_nth(netaddr->streams, i);
		uv_write_t *req = eina_list_nth(netaddr->reqs, i);

		if((err =	uv_write(req, (uv_stream_t *)stream, bufs, nbufs, _tcp_send_cb)))
			fprintf(stderr, "uv_write: %s", uv_err_name(err));
	}
}
