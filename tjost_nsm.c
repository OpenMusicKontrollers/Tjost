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

#include <unistd.h>

#include <tjost.h>


#include <osc_stream.h>

typedef struct _Tjost_NSM Tjost_NSM;

struct _Tjost_NSM {
	char *NSM_URL;
	char *id;
	char *call;
	osc_stream_t responder;
	osc_data_t buf [TJOST_BUF_SIZE];
};

static Tjost_NSM _nsm;
static Tjost_NSM *nsm = &_nsm;

static void
_nsm_send_cb(osc_stream_t *responder, size_t len, void *arg)
{
	Tjost_NSM *nsm = arg;

	// there is nothing to do here
}

static int
_nsm_reply(osc_time_t time, const char *path, const char *fmt, osc_data_t *buf, size_t size, void *dat)
{
	Tjost_NSM *nsm = dat;
	const char *msg;
	osc_data_t *buf_ptr = buf;
	

	buf_ptr = osc_get_string(buf_ptr, &msg);

	if(!strcmp(msg, "/nsm/server/announce"))
	{
		//TODO
	}

	return 1;
}

static int
_nsm_error(osc_time_t time, const char *path, const char *fmt, osc_data_t *buf, size_t size, void *dat)
{
	Tjost_NSM *nsm = dat;
	const char *msg;
	int32_t code;
	const char *err;
	osc_data_t *buf_ptr = buf;

	buf_ptr = osc_get_string(buf_ptr, &msg);
	buf_ptr = osc_get_int32(buf_ptr, &code);
	buf_ptr = osc_get_string(buf_ptr, &err);

	fprintf(stderr, "tjost_nsm: #%i in %s: %s\n", code, msg, err);

	return 1;
}

static int
_nsm_client_open(osc_time_t time, const char *path, const char *fmt, osc_data_t *buf, size_t size, void *dat)
{
	Tjost_NSM *nsm = dat;

	uv_loop_t *loop = uv_default_loop();

	const char *dir;
	const char *name;
	const char *id;

	osc_data_t *buf_ptr = buf;
	buf_ptr = osc_get_string(buf_ptr, &dir);
	buf_ptr = osc_get_string(buf_ptr, &name);
	buf_ptr = osc_get_string(buf_ptr, &id);

	nsm->id = strdup(id);

	osc_data_t *ptr = osc_set_vararg(nsm->buf, nsm->buf+TJOST_BUF_SIZE, "/reply", "ss", "/nsm/client/open", "opened");
	size_t len = ptr - nsm->buf;
	osc_stream_send(&nsm->responder, nsm->buf, len);

	return 1;
}

static int
_nsm_client_save(osc_time_t time, const char *path, const char *fmt, osc_data_t *buf, size_t size, void *dat)
{
	Tjost_NSM *nsm = dat;

	// there is nothing to do here

	osc_data_t *ptr = osc_set_vararg(nsm->buf, nsm->buf+TJOST_BUF_SIZE, "/reply", "ss", "/nsm/client/save", "saved");
	size_t len = ptr - nsm->buf;
	osc_stream_send(&nsm->responder, nsm->buf, len);

	return 1;
}

static int
_nsm_resolve(osc_time_t time, const char *path, const char *fmt, osc_data_t *buf, size_t size, void *dat)
{
	Tjost_NSM *nsm = dat;

	// send announce message
	pid_t pid = getpid();
	osc_data_t *ptr = osc_set_vararg(nsm->buf, nsm->buf + TJOST_BUF_SIZE, "/nsm/server/announce", "sssiii", nsm->call, ":message:", nsm->call, 1, 2, pid);
	size_t len = ptr - nsm->buf;
	
	osc_stream_send(&nsm->responder, nsm->buf, len);

	return 1;
}

static osc_method_t nsm_methods [] = {
	{"/net/resolve", "", _nsm_resolve},

	{"/reply", NULL, _nsm_reply},
	{"/error", "sis", _nsm_error},
	
	{"/nsm/client/open", "sss", _nsm_client_open},
	{"/nsm/client/save", "", _nsm_client_save},

	{NULL, NULL, NULL}
};

static void
_nsm_recv_cb(osc_stream_t *responder, osc_data_t *buf, size_t len, void *data)
{
	if(osc_check_packet(buf, len))
		osc_dispatch_method(0, buf, len, nsm_methods, NULL, NULL, data);
}

const char *
tjost_nsm_init(int argc, const char **argv)
{
	const char *call = strrchr(argv[1], '/');
	if(call)
		nsm->call = strdup(call+1); // skip '/'
	else
		nsm->call = strdup(argv[1]);

	nsm->NSM_URL = getenv("NSM_URL");
	if(!nsm->NSM_URL)
		return nsm->call;

	uv_loop_t *loop = uv_default_loop();

	size_t url_len = strlen(nsm->NSM_URL);
	if(nsm->NSM_URL[url_len-1] == '/')
		nsm->NSM_URL[url_len-1] = '\0';
	
	if(osc_stream_init(loop, &nsm->responder, nsm->NSM_URL, _nsm_recv_cb, _nsm_send_cb, nsm))
		fprintf(stderr, "tjost_nsm: could not create UDP Responder");

	// wait for load signal FIXME add timeout
	while(!nsm->id)
		uv_run(loop, UV_RUN_ONCE);

	return nsm->id;
}

void
tjost_nsm_deinit()
{
	if(!nsm->NSM_URL)
		return;

	if(nsm->id)
		free(nsm->id);

	if(nsm->call)
		free(nsm->call);

	osc_stream_deinit(&nsm->responder);
}
