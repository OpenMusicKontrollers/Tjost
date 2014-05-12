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

static void
_udp_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	NetAddr_UDP_Responder *netaddr = handle->data;

	buf->base = (char *)netaddr->buf;
	buf->len = TJOST_BUF_SIZE;
}

static void
_udp_recv_cb(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags)
{
	NetAddr_UDP_Responder *netaddr = handle->data;

	if(nread > 0)
		netaddr->cb((jack_osc_data_t *)buf->base, nread, netaddr->dat);
	else if (nread < 0)
	{
		uv_close((uv_handle_t *)handle, NULL);
		fprintf(stderr, "_udp_recv_cb: %s\n", uv_err_name(nread));
	}
}

int
netaddr_udp_responder_init(NetAddr_UDP_Responder *netaddr, uv_loop_t *loop, const char *addr, NetAddr_Recv_Cb cb, void *dat)
{
	// Server: "osc.udp://:3333"

	netaddr->cb = cb;
	netaddr->dat = dat;
	netaddr->recv_socket.data = netaddr;

	if(!strncmp(addr, "osc.udp://", 10))
	{
		netaddr->version = NETADDR_IP_VERSION_4;
		addr += 10;
	}
	else if(!strncmp(addr, "osc.udp4://", 11))
	{
		netaddr->version = NETADDR_IP_VERSION_4;
		addr += 11;
	}
	else if(!strncmp(addr, "osc.udp6://", 11))
	{
		netaddr->version = NETADDR_IP_VERSION_6;
		addr += 11;
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
	const struct sockaddr *recv_addr = NULL;
	struct sockaddr_in recv_addr4;
	struct sockaddr_in6 recv_addr6;
	unsigned int flags = 0;

	if((err = uv_udp_init(loop, &netaddr->recv_socket)))
	{
		fprintf(stderr, "uv_udp_init: %s\n", uv_err_name(err));
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
		flags |= UV_UDP_IPV6ONLY; //TODO make this configurable
	}
	if((err = uv_udp_bind(&netaddr->recv_socket, recv_addr, flags)))
	{
		fprintf(stderr, "uv_udp_bind: %s\n", uv_err_name(err));
		return -1;
	}
	if((err = uv_udp_recv_start(&netaddr->recv_socket, _udp_alloc, _udp_recv_cb)))
	{
		fprintf(stderr, "uv_udp_recv_start: %s\n", uv_err_name(err));
		return -1;
	}

	return 0;
}

void
netaddr_udp_responder_deinit(NetAddr_UDP_Responder *netaddr)
{
	int err;
	if((err =	uv_udp_recv_stop(&netaddr->recv_socket)))
		fprintf(stderr, "uv_udp_recv_stop: %s\n", uv_err_name(err));
}

int
netaddr_udp_sender_init(NetAddr_UDP_Sender *netaddr, uv_loop_t *loop, const char *addr)
{
	// Client: "osc.udp://name.local:4444"

	netaddr->send_socket.data = netaddr;

	if(!strncmp(addr, "osc.udp://", 10))
	{
		netaddr->version = NETADDR_IP_VERSION_4;
		addr += 10;
	}
	else if(!strncmp(addr, "osc.udp4://", 11))
	{
		netaddr->version = NETADDR_IP_VERSION_4;
		addr += 11;
	}
	else if(!strncmp(addr, "osc.udp6://", 11))
	{
		netaddr->version = NETADDR_IP_VERSION_6;
		addr += 11;
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
	hints.ai_family = netaddr->version == NETADDR_IP_VERSION_4 ? PF_INET : PF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	if(getaddrinfo(host, colon+1, &hints, &ai))
	{
		fprintf(stderr, "getaddrinfo: address could not be resolved\n");
		return -1;
	}
	char remote [128] = {'\0'};

	int err;
	if(netaddr->version == NETADDR_IP_VERSION_4)
	{
		struct sockaddr_in *ptr4 = (struct sockaddr_in *)ai->ai_addr;
		if((err = uv_ip4_name(ptr4, remote, 127)))
		{
			fprintf(stderr, "up_ip4_name: %s\n", uv_err_name(err));
			return -1;
		}
		if((err = uv_udp_init(loop, &netaddr->send_socket)))
		{
			fprintf(stderr, "uv_udp_init: %s\n", uv_err_name(err));
			return -1;
		}
		if((err = uv_ip4_addr(remote, ntohs(ptr4->sin_port), &netaddr->send_addr.ipv4)))
		{
			fprintf(stderr, "uv_ip4_addr: %s\n", uv_err_name(err));
			return -1;
		}
	}
	else // NETADDR_IP_VERSION_6
	{
		struct sockaddr_in6 *ptr6 = (struct sockaddr_in6 *)ai->ai_addr;
		if((err = uv_ip6_name(ptr6, remote, 127)))
		{
			fprintf(stderr, "up_ip6_name: %s\n", uv_err_name(err));
			return -1;
		}
		if((err = uv_udp_init(loop, &netaddr->send_socket)))
		{
			fprintf(stderr, "uv_udp_init: %s\n", uv_err_name(err));
			return -1;
		}
		if((err = uv_ip6_addr(remote, ntohs(ptr6->sin6_port), &netaddr->send_addr.ipv6)))
		{
			fprintf(stderr, "uv_ip6_addr: %s\n", uv_err_name(err));
			return -1;
		}
	}

	return 0;
}

void
netaddr_udp_sender_deinit(NetAddr_UDP_Sender *netaddr)
{
	//TODO
}

static void
_udp_send_cb(uv_udp_send_t *req, int status)
{
	uv_udp_t *handle = req->handle;
	NetAddr_UDP_Sender *netaddr = handle->data;

	if(!status)
		netaddr->cb(netaddr->len, netaddr->dat);
	else
		fprintf(stderr, "_udp_send_cb: %s\n", uv_err_name(status));
}

void
netaddr_udp_sender_send(NetAddr_UDP_Sender *netaddr, uv_buf_t *bufs, int nbufs, size_t len, NetAddr_Send_Cb cb, void *dat)
{
	netaddr->cb = cb;
	netaddr->dat = dat;
	netaddr->len = len; // message size without TCP preamble and OSC bundle header

	int err;
	if((err = uv_udp_send(&netaddr->req, &netaddr->send_socket, bufs, nbufs, &netaddr->send_addr.ip, _udp_send_cb)))
		fprintf(stderr, "uv_udp_send: %s", uv_err_name(err));
}
