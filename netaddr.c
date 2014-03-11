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

static void
_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	NetAddr_UDP_Responder *netaddr = handle->data;

	buf->base = (char *)netaddr->buf;
	buf->len = TJOST_BUF_SIZE;
}

static void
_recv_cb(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags)
{
	NetAddr_UDP_Responder *netaddr = handle->data;

	if(nread > 0)
		netaddr->cb(netaddr, (uint8_t *)buf->base, nread, netaddr->dat);
	else if (nread < 0)
		fprintf(stderr, "%s\n", uv_err_name(nread));
}

int
netaddr_udp_responder_init(NetAddr_UDP_Responder *netaddr, uv_loop_t *loop, const char *addr, NetAddr_Cb cb, void *dat)
{
	// Server: "osc.udp://:3333"

	netaddr->cb = cb;
	netaddr->dat = dat;
	netaddr->recv_socket.data = netaddr;

	if(strncmp(addr, "osc.udp://", 10))
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
	if((err = uv_udp_init(loop, &netaddr->recv_socket)))
	{
		fprintf(stderr, "%s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_ip4_addr(host, port, &recv_addr)))
	{
		fprintf(stderr, "%s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_udp_bind(&netaddr->recv_socket, (const struct sockaddr *)&recv_addr, 0)))
	{
		fprintf(stderr, "%s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_udp_recv_start(&netaddr->recv_socket, _alloc, _recv_cb)))
	{
		fprintf(stderr, "%s\n", uv_err_name(err));
		return -1;
	}

	return 0;
}

void
netaddr_udp_responder_deinit(NetAddr_UDP_Responder *netaddr)
{
	uv_udp_recv_stop(&netaddr->recv_socket);
}

static void
_send_cb(uv_udp_send_t *req, int status)
{
	uv_udp_t *handle = req->handle;
	NetAddr_UDP_Sender *netaddr = handle->data;

	if(status)
		fprintf(stderr, "%s\n", uv_err_name(status));

	eina_mempool_free(netaddr->pool, req);
}

int
netaddr_udp_sender_init(NetAddr_UDP_Sender *netaddr, uv_loop_t *loop, const char *addr)
{
	// Client: "osc.udp://name.local:4444"

	netaddr->send_socket.data = netaddr;
	netaddr->pool = eina_mempool_add("chained_mempool", "requests", NULL, sizeof(uv_udp_send_t), 64); //TODO how big?

	if(strncmp(addr, "osc.udp://", 10))
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
	if((err = uv_udp_init(loop, &netaddr->send_socket)))
	{
		fprintf(stderr, "%s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_ip4_addr(remote, port, &netaddr->send_addr)))
	{
		fprintf(stderr, "%s\n", uv_err_name(err));
		return -1;
	}

	return 0;
}

void
netaddr_udp_sender_deinit(NetAddr_UDP_Sender *netaddr)
{
	eina_mempool_del(netaddr->pool);
}

void
netaddr_udp_sender_send(NetAddr_UDP_Sender *netaddr, const uint8_t *buf, size_t len)
{
	uv_udp_send_t *send_req = eina_mempool_malloc(netaddr->pool, sizeof(uv_udp_send_t));
	uv_buf_t msg;

	msg.base = (char *)buf;
	msg.len = len;

	int err;
	if((err = uv_udp_send(send_req, &netaddr->send_socket, &msg, 1, (const struct sockaddr *)&netaddr->send_addr, _send_cb)))
		fprintf(stderr, "%s", uv_err_name(err)); //FIXME use tjost_message_push
}
