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

#include <netdb.h>

#include <netaddr.h>

#define SLIP_END					0300	// indicates end of packet
#define SLIP_ESC					0333	// indicates byte stuffing
#define SLIP_END_REPLACE	0334	// ESC ESC_END means END data byte
#define SLIP_ESC_REPLACE	0335	// ESC ESC_ESC means ESC data byte

// inline SLIP encoding
size_t
slip_encode(uint8_t *buf, size_t len)
{
	uint8_t *src;
	uint8_t *end = buf + len;
	uint8_t *dst;

	size_t count = 0;
	for(src=buf; src<end; src++)
		if( (*src == SLIP_END) || (*src == SLIP_ESC) )
			count++;

	src = end - 1;
	dst = end + count;
	*dst-- = SLIP_END;

	while( (src >= 0) && (src != dst) )
	{
		if(*src == SLIP_END)
		{
			*dst-- = SLIP_END_REPLACE;
			*dst-- = SLIP_ESC;
			src--;
		}
		else if(*src == SLIP_ESC)
		{
			*dst-- = SLIP_ESC_REPLACE;
			*dst-- = SLIP_ESC;
			src--;
		}
		else
			*dst-- = *src--;
	}

	return len + count + 1;
}

// inline SLIP decoding
size_t
slip_decode(uint8_t *buf, size_t len, size_t *size)
{
	uint8_t *src = buf;
	uint8_t *end = buf + len;
	uint8_t *dst = buf;

	while(src < end)
	{
		if(*src == SLIP_ESC)
		{
			src++;
			if(*src == SLIP_END_REPLACE)
				*dst++ = SLIP_END;
			else if(*src == SLIP_ESC_REPLACE)
				*dst++ = SLIP_ESC;
			else
				; //TODO error
			src++;
		}
		else if(*src == SLIP_END)
		{
			src++;
			break;
		}
		else
		{
			if(src != dst)
				*dst = *src;
			src++;
			dst++;
			//TODO *dst++ = *src++;
		}
	}

	*size = dst - buf;
	return src - buf;
}

static void
_tcp_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	NetAddr_TCP_Responder *netaddr = handle->data;

	buf->base = (char *)netaddr->buf;
	buf->len = TJOST_BUF_SIZE;
}

static void
_tcp_recv_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	//printf("_tcp_recv_cb %zu\n", nread);

	NetAddr_TCP_Responder *netaddr = stream->data;

	if(nread > 0)
	{
		uint8_t *ptr0 = (uint8_t *)buf->base;
		uint8_t *end = ptr0 + nread;
		while(ptr0 < end)
		{
			size_t len;
			size_t size = slip_decode(ptr0, end-ptr0, &len);
			netaddr->cb(ptr0, len, netaddr->dat);

			ptr0 += size;
		}
	}
	else if (nread < 0)
	{
		uv_read_stop((uv_stream_t *)&netaddr->recv_client);
		uv_close((uv_handle_t *)&netaddr->recv_client, NULL);
		fprintf(stderr, "%s\n", uv_err_name(nread));
	}
}

static void
_server_connect(uv_stream_t *server, int status)
{
	fprintf(stderr, "_server_connect %i\n", status);

	if(status)
		return; //TODO

	NetAddr_TCP_Responder *netaddr = server->data;
	
	if (uv_accept(server, (uv_stream_t *)&netaddr->recv_client) == 0)
		uv_read_start((uv_stream_t *)&netaddr->recv_client, _tcp_alloc, _tcp_recv_cb);
	else
		uv_close((uv_handle_t *)&netaddr->recv_client, NULL);
}

int
netaddr_tcp_responder_init(NetAddr_TCP_Responder *netaddr, uv_loop_t *loop, const char *addr, NetAddr_Recv_Cb cb, void *dat)
{
	// Server: "osc.tcp://:3333"

	netaddr->cb = cb;
	netaddr->dat = dat;
	netaddr->recv_socket.data = netaddr;
	netaddr->recv_client.data = netaddr;

	if(strncmp(addr, "osc.tcp://", 10))
	{
		fprintf(stderr, "unsupported protocol in address %s\n", addr);
		return -1;
	}
	addr += 10;

	char *colon = strchr(addr, ':');
	if(!colon)
	{
		fprintf(stderr, "address must have a port\n");
		return -1;
	}

	const char *host = "0.0.0.0";
	uint16_t port = atoi(colon+1);

	int err;
	struct sockaddr_in recv_addr;
	if((err = uv_tcp_init(loop, &netaddr->recv_socket)))
	{
		fprintf(stderr, "%s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_tcp_init(loop, &netaddr->recv_client)))
	{
		fprintf(stderr, "%s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_tcp_nodelay(&netaddr->recv_socket, 1))) // disable Nagle's algo
	{
		fprintf(stderr, "%s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_tcp_keepalive(&netaddr->recv_socket, 1, 5))) // keepalive after 5 seconds
	{
		fprintf(stderr, "%s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_ip4_addr(host, port, &recv_addr)))
	{
		fprintf(stderr, "%s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_tcp_bind(&netaddr->recv_socket, (const struct sockaddr *)&recv_addr, 0)))
	{
		fprintf(stderr, "%s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_listen((uv_stream_t *)&netaddr->recv_socket, 128, _server_connect)))
	{
		fprintf(stderr, "%s\n", uv_err_name(err));
		return -1;
	}

	return 0;
}

void
netaddr_tcp_responder_deinit(NetAddr_TCP_Responder *netaddr)
{
	uv_read_stop((uv_stream_t *)&netaddr->recv_client);
	uv_close((uv_handle_t *)&netaddr->recv_client, NULL);
	uv_close((uv_handle_t *)&netaddr->recv_socket, NULL);
}

static void
_client_connect(uv_connect_t *server, int status)
{
	fprintf(stderr, "_client_connect %i\n", status);

	NetAddr_TCP_Sender *netaddr = server->data;

	if(status)
		return; //TODO

	//TODO do something
}

int
netaddr_tcp_sender_init(NetAddr_TCP_Sender *netaddr, uv_loop_t *loop, const char *addr)
{
	// Client: "osc.tcp://name.local:4444"

	netaddr->send_socket.data = netaddr;
	netaddr->send_remote.data = netaddr; // FIXME eina_mempool

	if(strncmp(addr, "osc.tcp://", 10))
	{
		fprintf(stderr, "unsupported protocol in address %s\n", addr);
		return -1;
	}
	addr += 10;

	char *colon = strchr(addr, ':');
	if(!colon)
	{
		fprintf(stderr, "address must have a port\n");
		return -1;
	}

	const char *host = NULL;

	if(colon == addr)
		host = "255.255.255.255"; //FIXME implement broadcast and multicast
	else
	{
		*colon = '\0';
		host = addr;
	}

	// DNS resolve
	struct addrinfo hints;
	struct addrinfo *ai;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	if(getaddrinfo(host, colon+1, &hints, &ai))
	{
		fprintf(stderr, "address could not be resolved\n");
		return -1;
	}
	char remote [17] = {'\0'};
	struct sockaddr_in *ptr = (struct sockaddr_in *)ai->ai_addr;
	uint16_t port = ntohs(ptr->sin_port);

	int err;
	if((err = uv_ip4_name((struct sockaddr_in *)ai->ai_addr, remote, 16)))
	{
		fprintf(stderr, "%s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_tcp_init(loop, &netaddr->send_socket)))
	{
		fprintf(stderr, "%s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_ip4_addr(remote, port, &netaddr->send_addr)))
	{
		fprintf(stderr, "%s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_tcp_connect(&netaddr->send_remote, &netaddr->send_socket, (const struct sockaddr *)&netaddr->send_addr, _client_connect)))
	{
		fprintf(stderr, "%s\n", uv_err_name(err));
		return -1;
	}

	return 0;
}

void
netaddr_tcp_sender_deinit(NetAddr_TCP_Sender *netaddr)
{
	//TODO
}

static void
_tcp_send_cb(uv_write_t *req, int status)
{
	printf("_tcp_send_cb %i\n", status);

	uv_stream_t *stream = req->handle;
	NetAddr_TCP_Sender *netaddr = stream->data;

	if(status)
		fprintf(stderr, "%s\n", uv_err_name(status));

	netaddr->cb(netaddr->len, netaddr->dat);
}

void
netaddr_tcp_sender_send(NetAddr_TCP_Sender *netaddr, uv_buf_t *bufs, int nbufs, NetAddr_Send_Cb cb, void *dat)
{
	netaddr->cb = cb;
	netaddr->dat = dat;

	int err;
	if((err =	uv_write(&netaddr->req, (uv_stream_t *)&netaddr->send_socket, bufs, nbufs, _tcp_send_cb)))
		fprintf(stderr, "%s", uv_err_name(err)); //FIXME use tjost_message_push
}
