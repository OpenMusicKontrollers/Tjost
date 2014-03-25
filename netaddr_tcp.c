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

#if 0
#define SLIP_END					0300	// indicates end of packet
#define SLIP_ESC					0333	// indicates byte stuffing
#define SLIP_END_REPLACE	0334	// ESC ESC_END means END data byte
#define SLIP_ESC_REPLACE	0335	// ESC ESC_ESC means ESC data byte

// inline SLIP encoding
size_t
slip_encode(uint8_t *buf, uv_buf_t *bufs, int nbufs)
{
	uint8_t *dst = buf;

	int i;
	for(i=0; i<nbufs; i++)
	{
		uv_buf_t *ptr = &bufs[i];
		uint8_t *base = (uint8_t *)ptr->base;
		uint8_t *end = base + ptr->len;

		uint8_t *src;
		for(src=base; src<end; src++)
			switch(*src)
			{
				case SLIP_END:
					*dst++ = SLIP_ESC;
					*dst++ = SLIP_END_REPLACE;
					break;
				case SLIP_ESC:
					*dst++ = SLIP_ESC;
					*dst++ = SLIP_ESC_REPLACE;
					break;
				default:
					*dst++ = *src;
					break;
			}
	}
	*dst++ = SLIP_END;

	return dst - buf;
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
#endif

static void
_tcp_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	NetAddr_TCP_Responder *netaddr = handle->data;

	buf->base = (char *)netaddr->buf;
	buf->len = netaddr->nchunk < TJOST_BUF_SIZE ? netaddr->nchunk : TJOST_BUF_SIZE;
}

static void
_tcp_recv_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	NetAddr_TCP_Responder *netaddr = stream->data;

	if(nread > 0)
	{
		if(nread == sizeof(int32_t))
			netaddr->nchunk = ntohl(*(int32_t *)buf->base);
		else if(nread == netaddr->nchunk)
		{
			netaddr->cb((uint8_t *)buf->base, nread, netaddr->dat);
			netaddr->nchunk = sizeof(int32_t);
		}
		else // nread != sizeof(int32_t) && nread != nchunk
		{
			//FIXME should we do here?
			netaddr->nchunk = sizeof(int32_t);
			fprintf(stderr, "_tcp_recv_cb: TCP packet size not matching\n");
		}
	}
	else if (nread < 0)
	{
		int err;
		if((err = uv_read_stop((uv_stream_t *)&netaddr->recv_client)))
			fprintf(stderr, "uv_read_stop: %s\n", uv_err_name(err));
		uv_close((uv_handle_t *)&netaddr->recv_client, NULL);
		fprintf(stderr, "_tcp_recv_cb: %s\n", uv_err_name(nread));
	}
	else // nread == 0
		;
}

static void
_server_connect(uv_stream_t *server, int status)
{
	if(status)
	{
		fprintf(stderr, "_server_connect: %s\n", uv_err_name(status));
		return;
	}

	fprintf(stderr, "connect to client\n");

	NetAddr_TCP_Responder *netaddr = server->data;

	int err;
	if((err = uv_tcp_init(server->loop, &netaddr->recv_client)))
	{
		fprintf(stderr, "uv_tcp_init: %s\n", uv_err_name(err));
		return;
	}
	
	if((err = uv_accept(server, (uv_stream_t *)&netaddr->recv_client)))
	{
		fprintf(stderr, "uv_accept: %s\n", uv_err_name(err));
		return;
	}

	netaddr->nchunk = sizeof(int32_t); // packet size as TCP preamble
	if((err = uv_read_start((uv_stream_t *)&netaddr->recv_client, _tcp_alloc, _tcp_recv_cb)))
	{
		fprintf(stderr, "uv_read_start: %s\n", uv_err_name(err));
		return;
	}
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
		fprintf(stderr, "uv_tcp_init: %s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_tcp_nodelay(&netaddr->recv_socket, 1))) // disable Nagle's algo
	{
		fprintf(stderr, "uv_tcp_nodelay: %s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_tcp_keepalive(&netaddr->recv_socket, 1, 5))) // keepalive after 5 seconds
	{
		fprintf(stderr, "uv_tcp_keepalive: %s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_ip4_addr(host, port, &recv_addr)))
	{
		fprintf(stderr, "uv_ip4_addr: %s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_tcp_bind(&netaddr->recv_socket, (const struct sockaddr *)&recv_addr, 0)))
	{
		fprintf(stderr, "uv_tcp_bind: %s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_listen((uv_stream_t *)&netaddr->recv_socket, 128, _server_connect)))
	{
		fprintf(stderr, "uv_listen: %s\n", uv_err_name(err));
		return -1;
	}

	return 0;
}

void
netaddr_tcp_responder_deinit(NetAddr_TCP_Responder *netaddr)
{
	int err;
	if((err = uv_read_stop((uv_stream_t *)&netaddr->recv_client)))
		fprintf(stderr, "uv_read_stop: %s\n", uv_err_name(err));
	uv_close((uv_handle_t *)&netaddr->recv_client, NULL);
	uv_close((uv_handle_t *)&netaddr->recv_socket, NULL);
}

static void
_client_connect(uv_connect_t *server, int status)
{
	NetAddr_TCP_Sender *netaddr = server->data;

	if(status)
	{
		fprintf(stderr, "_client_connect %s\n", uv_err_name(status));
		return; //TODO
	}

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
		fprintf(stderr, "up_ip4_name: %s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_tcp_init(loop, &netaddr->send_socket)))
	{
		fprintf(stderr, "uv_tcp_init: %s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_ip4_addr(remote, port, &netaddr->send_addr)))
	{
		fprintf(stderr, "uv_ip4_addr: %s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_tcp_connect(&netaddr->send_remote, &netaddr->send_socket, (const struct sockaddr *)&netaddr->send_addr, _client_connect)))
	{
		fprintf(stderr, "uv_tcp_connect: %s\n", uv_err_name(err));
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
	uv_stream_t *stream = req->handle;
	NetAddr_TCP_Sender *netaddr = stream->data;

	if(!status)
		netaddr->cb(netaddr->len, netaddr->dat);
	else
		fprintf(stderr, "_tcp_send_cb: %s\n", uv_err_name(status));

}

void
netaddr_tcp_sender_send(NetAddr_TCP_Sender *netaddr, uv_buf_t *bufs, int nbufs, size_t len, NetAddr_Send_Cb cb, void *dat)
{
	netaddr->cb = cb;
	netaddr->dat = dat;
	netaddr->len = len;

	int err;
	if((err =	uv_write(&netaddr->req, (uv_stream_t *)&netaddr->send_socket, bufs, nbufs, _tcp_send_cb)))
		fprintf(stderr, "uv_write: %s", uv_err_name(err));
}
