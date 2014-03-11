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
#include <tjost.h>

static uint8_t buf_i [TJOST_BUF_SIZE];

static void
_watcher(struct ev_loop *loop, ev_io *w, int revents)
{
	NetAddr *netaddr = w->data;
	if(netaddr)
	{
		socklen_t slen = sizeof(struct sockaddr_in);
		int len;
		while((len = recvfrom(netaddr->fd, buf_i, sizeof(buf_i), MSG_DONTWAIT | MSG_WAITALL, (struct sockaddr*) &(netaddr->addr), &slen)) != -1)
			netaddr->cb(netaddr, buf_i, len, netaddr->dat);
	}
}

int
netaddr_udp_init(NetAddr *netaddr, const char *addr)
{
	uint32_t ip;
	uint16_t port;
	int server;

	// Server: "osc.udp://:3333"
	// Client: "osc.udp://name.local:4444"

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
	{
		host = "localhost";
		server = 1;
	}
	else
	{
		*colon = '\0';
		host = addr;
		server = 0;
	}

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
	struct sockaddr_in *ptr = (struct sockaddr_in *)ai->ai_addr;
	ip = server ? htonl(INADDR_ANY) : ptr->sin_addr.s_addr;
	port = ptr->sin_port;

	bzero(&(netaddr->addr), sizeof(netaddr->addr));

	netaddr->addr.sin_family = AF_INET;
	netaddr->addr.sin_port = port;
	netaddr->addr.sin_addr.s_addr = ip;

	netaddr->cb = NULL;
	netaddr->dat = NULL;
	netaddr->fd = -1;

	if(!server)
		return 0;
	
	netaddr->fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	//int optval = 1;
	//setsockopt(netaddr->fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	if(bind(netaddr->fd, (struct sockaddr*) &(netaddr->addr), sizeof(netaddr->addr)) != 0)
	{
		fprintf(stderr, "socket could not be bound\n");
		return -1;
	}

	return 0;
}

void
netaddr_udp_deinit(NetAddr *netaddr)
{
	if(netaddr->fd != -1)
		close(netaddr->fd);
	netaddr->fd = -1;
}

void
netaddr_udp_listen(NetAddr *netaddr, struct ev_loop *loop, NetAddrCb cb, void *dat)
{
	if(!netaddr->cb && cb)
	{
		netaddr->cb = cb;
		netaddr->dat = dat;

		netaddr->watcher.data = netaddr;
		ev_io_init(&(netaddr->watcher), _watcher, netaddr->fd, EV_READ);
		ev_io_start(loop, &(netaddr->watcher));
	}
}

void
netaddr_udp_unlisten(NetAddr *netaddr, struct ev_loop *loop)
{
	if(netaddr->cb)
	{
		netaddr->watcher.data = NULL;
		ev_io_stop(loop, &(netaddr->watcher));
		netaddr->cb = NULL;
	}
}

inline void
netaddr_udp_sendto(NetAddr *netaddr, NetAddr *dest, const uint8_t *buf, size_t len)
{
	if(sendto(netaddr->fd, buf, len, 0, (const struct sockaddr *) &(dest->addr), sizeof(dest->addr)) != len)
		fprintf(stderr, "sendto failed\n");
}
