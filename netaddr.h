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

#ifndef _NETADDR_H_
#define _NETADDR_H_

#include <string.h>
#include <stdint.h>

#include <tjost.h>

typedef struct _NetAddr_UDP_Sender NetAddr_UDP_Sender;
typedef struct _NetAddr_UDP_Responder NetAddr_UDP_Responder;
typedef struct _NetAddr_TCP_Endpoint NetAddr_TCP_Endpoint;
typedef void (*NetAddr_Recv_Cb) (jack_osc_data_t *buf, size_t len, void *dat);
typedef void (*NetAddr_Send_Cb) (size_t len, void *dat);

typedef enum _NetAddr_TCP_Type {
	NETADDR_TCP_RESPONDER, NETADDR_TCP_SENDER
} NetAddr_TCP_Type;

typedef enum _NetAddr_IP_Version {
	NETADDR_IP_VERSION_4, NETADDR_IP_VERSION_6
} NetAddr_IP_Version;

struct _NetAddr_UDP_Sender {
	uv_udp_t send_socket;
	NetAddr_IP_Version version;

	union {
		struct sockaddr ip;
		struct sockaddr_in ipv4;
		struct sockaddr_in6 ipv6;
	} send_addr;

	uv_udp_send_t req;
	NetAddr_Send_Cb cb;
	size_t len;
	void *dat;
};

struct _NetAddr_UDP_Responder {
	uv_udp_t recv_socket;
	NetAddr_Recv_Cb cb;
	NetAddr_IP_Version version;

	void *dat;
	jack_osc_data_t buf [TJOST_BUF_SIZE];
};

struct _NetAddr_TCP_Endpoint {
	NetAddr_TCP_Type type;
	NetAddr_IP_Version version;
	int slip;

	union {
		uv_tcp_t socket; // only used for responder
		uv_connect_t conn; // only used for sender
	} uni;

	// used for both responder and sender
	Eina_List *streams;
	Eina_List *reqs;
	int count;

	struct {
		NetAddr_Recv_Cb cb;
		void *dat;
		jack_osc_data_t buf [TJOST_BUF_SIZE];
		size_t nchunk;
	} recv;

	struct {
		NetAddr_Send_Cb cb;
		void *dat;
		size_t len;
	} send;
};

int netaddr_udp_responder_init(NetAddr_UDP_Responder *netaddr, uv_loop_t *loop, const char *addr, NetAddr_Recv_Cb cb, void *dat);
void netaddr_udp_responder_deinit(NetAddr_UDP_Responder *netaddr);

int netaddr_udp_sender_init(NetAddr_UDP_Sender *netaddr, uv_loop_t *loop, const char *addr);
void netaddr_udp_sender_deinit(NetAddr_UDP_Sender *netaddr);
void netaddr_udp_sender_send(NetAddr_UDP_Sender *netaddr, uv_buf_t *bufs, int nbufs, size_t len, NetAddr_Send_Cb cb, void *dat);

int netaddr_tcp_endpoint_init(NetAddr_TCP_Endpoint *netaddr, NetAddr_TCP_Type type, uv_loop_t *loop, const char *addr, NetAddr_Recv_Cb cb, void *dat);
void netaddr_tcp_endpoint_deinit(NetAddr_TCP_Endpoint *netaddr);
void netaddr_tcp_endpoint_send(NetAddr_TCP_Endpoint *netaddr, uv_buf_t *bufs, int nbufs, size_t len, NetAddr_Send_Cb cb, void *dat);

size_t slip_encode(uint8_t *buf, uv_buf_t *bufs, int nbufs);
size_t slip_decode(uint8_t *buf, size_t len, size_t *size);

#endif
