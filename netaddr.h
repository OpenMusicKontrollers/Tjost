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

#ifndef _NETADDR_H
#define _NETADDR_H

#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <resolv.h>
#include <unistd.h>

#include <tjost.h>

#include <uv.h>
#include <Eina.h>

typedef struct _NetAddr_UDP_Sender NetAddr_UDP_Sender;
typedef struct _NetAddr_UDP_Responder NetAddr_UDP_Responder;
typedef struct _NetAddr_TCP_Sender NetAddr_TCP_Sender;
typedef struct _NetAddr_TCP_Responder NetAddr_TCP_Responder;
typedef void (*NetAddr_Recv_Cb) (uint8_t *buf, size_t len, void *dat);
typedef void (*NetAddr_Send_Cb) (size_t len, void *dat);

struct _NetAddr_UDP_Sender {
	uv_udp_t send_socket;

	struct sockaddr_in send_addr;
	Eina_Mempool *pool;

	uv_udp_send_t req;
	NetAddr_Send_Cb cb;
	size_t len;
	void *dat;
};

struct _NetAddr_UDP_Responder {
	uv_udp_t recv_socket;
	NetAddr_Recv_Cb cb;

	void *dat;
	uint8_t buf [TJOST_BUF_SIZE];
};

struct _NetAddr_TCP_Sender {
	uv_tcp_t send_socket;
	uv_connect_t send_remote;

	struct sockaddr_in send_addr;
	Eina_Mempool *pool;

	uv_write_t req;
	NetAddr_Send_Cb cb;
	size_t len;
	void *dat;
};

struct _NetAddr_TCP_Responder {
	uv_tcp_t recv_socket;
	uv_tcp_t recv_client;
	NetAddr_Recv_Cb cb;

	void *dat;
	uint8_t buf [TJOST_BUF_SIZE];
	size_t nchunk;
};

int netaddr_udp_responder_init(NetAddr_UDP_Responder *netaddr, uv_loop_t *loop, const char *addr, NetAddr_Recv_Cb cb, void *dat);
void netaddr_udp_responder_deinit(NetAddr_UDP_Responder *netaddr);

int netaddr_udp_sender_init(NetAddr_UDP_Sender *netaddr, uv_loop_t *loop, const char *addr);
void netaddr_udp_sender_deinit(NetAddr_UDP_Sender *netaddr);
void netaddr_udp_sender_send(NetAddr_UDP_Sender *netaddr, uv_buf_t *bufs, int nbufs, size_t len, NetAddr_Send_Cb cb, void *dat);

int netaddr_tcp_responder_init(NetAddr_TCP_Responder *netaddr, uv_loop_t *loop, const char *addr, NetAddr_Recv_Cb cb, void *dat);
void netaddr_tcp_responder_deinit(NetAddr_TCP_Responder *netaddr);

int netaddr_tcp_sender_init(NetAddr_TCP_Sender *netaddr, uv_loop_t *loop, const char *addr);
void netaddr_tcp_sender_deinit(NetAddr_TCP_Sender *netaddr);
void netaddr_tcp_sender_send(NetAddr_TCP_Sender *netaddr, uv_buf_t *bufs, int nbufs, size_t len, NetAddr_Send_Cb cb, void *dat);

#if 0
size_t slip_encode(uint8_t *buf, uv_buf_t *bufs, int nbufs);
size_t slip_decode(uint8_t *buf, size_t len, size_t *size);
#endif

#endif // _NETADDR_H
