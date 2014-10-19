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

#ifndef _OSC_H_
#define _OSC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <arpa/inet.h> // hton & co.
#include <stdint.h>

#define quads(size) (((((size_t)size-1) & ~0x3) >> 2) + 1)
#define round_to_four_bytes(size) (quads((size_t)size) << 2)

#ifndef htonll
#	if __BYTE_ORDER == __BIG_ENDIAN
#	 define htonll(x) (x)
#	else
#	 if __BYTE_ORDER == __LITTLE_ENDIAN
#	  define htonll(x) (((uint64_t)htonl((uint32_t)(x)))<<32 | htonl((uint32_t)((x)>>32)))
#	 endif
#	endif
#endif

#ifndef ntohll
#	define ntohll htonll
#endif

#define OSC_IMMEDIATE 1ULL

typedef uint8_t osc_data_t;
typedef uint64_t osc_time_t;

typedef int (*osc_method_cb_t) (osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, void *dat);
typedef void (*osc_bundle_in_cb_t) (osc_time_t time, void *dat);
typedef void (*osc_bundle_out_cb_t) (osc_time_t time, void *dat);

typedef struct _osc_method_t osc_method_t;
typedef struct _osc_blob_t osc_blob_t;
typedef union _osc_argument_t osc_argument_t;

typedef union _swap32_t swap32_t;
typedef union _swap64_t swap64_t;

typedef enum _osc_type_t osc_type_t;

union _swap32_t {
	uint32_t u;

	int32_t i;
	float f;
};

union _swap64_t {
	uint64_t u;

	int64_t h;
	osc_time_t t;
	double d;
};

enum _osc_type_t {
	OSC_INT32		=	'i',
	OSC_FLOAT		=	'f',
	OSC_STRING		=	's',
	OSC_BLOB			=	'b',
	
	OSC_TRUE			=	'T',
	OSC_FALSE		=	'F',
	OSC_NIL			=	'N',
	OSC_BANG			=	'I',
	
	OSC_INT64		=	'h',
	OSC_DOUBLE		=	'd',
	OSC_TIMETAG	=	't',
	
	OSC_SYMBOL		=	'S',
	OSC_CHAR			=	'c',
	OSC_MIDI			=	'm'
};


struct _osc_method_t {
	const char *path;
	const char *fmt;
	osc_method_cb_t cb;
};

struct _osc_blob_t {
	int32_t size;
	void *payload;
};

union _osc_argument_t {
	int32_t i;
	float f;
	const char *s;
	osc_blob_t b;

	int64_t h;
	double d;
	osc_time_t t;

	const char *S;
	char c;
	uint8_t *m;
};

int osc_check_path(const char *path);
int osc_check_fmt(const char *format, int offset);

int osc_method_match(osc_method_t *methods, const char *path, const char *fmt);
void osc_method_dispatch(osc_time_t time, osc_data_t *buf, size_t size, osc_method_t *methods, osc_bundle_in_cb_t bundle_in, osc_bundle_out_cb_t bundle_out, void *dat);

typedef enum _osc_unroll_mode_t osc_unroll_mode_t;
typedef void (*osc_unroll_stamp_inject_cb_t) (osc_time_t tstamp, void *dat);
typedef void (*osc_unroll_message_inject_cb_t) (osc_data_t *buf, size_t size, void *dat);
typedef void (*osc_unroll_bundle_inject_cb_t) (osc_data_t *buf, size_t size, void *dat);
typedef struct _osc_unroll_inject_t osc_unroll_inject_t;

enum _osc_unroll_mode_t {
	OSC_UNROLL_MODE_NONE,
	OSC_UNROLL_MODE_PARTIAL,
	OSC_UNROLL_MODE_FULL
};

struct _osc_unroll_inject_t {
	osc_unroll_stamp_inject_cb_t stamp;
	osc_unroll_message_inject_cb_t message;
	osc_unroll_bundle_inject_cb_t bundle;
};

int osc_packet_unroll(osc_data_t *buf, size_t size, osc_unroll_mode_t mode, osc_unroll_inject_t *inject, void*dat);

int osc_message_check(osc_data_t *buf, size_t size);
int osc_bundle_check(osc_data_t *buf, size_t size);
int osc_packet_check(osc_data_t *buf, size_t size);

// OSC object lengths
__always_inline size_t
osc_strlen(const char *buf)
{
	return round_to_four_bytes(strlen(buf) + 1);
}

__always_inline size_t
osc_fmtlen(const char *buf)
{
	return round_to_four_bytes(strlen(buf) + 2) - 1;
}

__always_inline size_t
osc_blobsize(osc_data_t *buf)
{
	swap32_t s = {.u = *(uint32_t *)buf}; 
	s.u = ntohl(s.u);
	return s.i;
}

__always_inline size_t
osc_bloblen(osc_data_t *buf)
{
	return 4 + osc_blobsize(buf);
}

// get OSC arguments from raw buffer
__always_inline osc_data_t *
osc_get_path(osc_data_t *buf, const char **path)
{
	*path = (const char *)buf;
	return buf + osc_strlen(*path);
}

__always_inline osc_data_t *
osc_get_fmt(osc_data_t *buf, const char **fmt)
{
	*fmt = (const char *)buf;
	return buf + osc_strlen(*fmt);
}

__always_inline osc_data_t *
osc_get_int32(osc_data_t *buf, int32_t *i)
{
	swap32_t s = {.u = *(uint32_t *)buf};
	s.u = ntohl(s.u);
	*i = s.i;
	return buf + 4;
}

__always_inline osc_data_t *
osc_get_float(osc_data_t *buf, float *f)
{
	swap32_t s = {.u = *(uint32_t *)buf};
	s.u = ntohl(s.u);
	*f = s.f;
	return buf + 4;
}

__always_inline osc_data_t *
osc_get_string(osc_data_t *buf, const char **s)
{
	*s = (const char *)buf;
	return buf + osc_strlen(*s);
}

__always_inline osc_data_t *
osc_get_blob(osc_data_t *buf, osc_blob_t *b)
{
	b->size = osc_blobsize(buf);
	b->payload = buf + 4;
	return buf + 4 + round_to_four_bytes(b->size);
}

__always_inline osc_data_t *
osc_get_int64(osc_data_t *buf, int64_t *h)
{
	swap64_t s = {.u = *(uint64_t *)buf};
	s.u = ntohll(s.u);
	*h = s.h;
	return buf + 8;
}

__always_inline osc_data_t *
osc_get_double(osc_data_t *buf, double *d)
{
	swap64_t s = {.u = *(uint64_t *)buf};
	s.u = ntohll(s.u);
	*d = s.d;
	return buf + 8;
}

__always_inline osc_data_t *
osc_get_timetag(osc_data_t *buf, osc_time_t *t)
{
	swap64_t s = {.u = *(uint64_t *)buf};
	s.u = ntohll(s.u);
	*t = s.t;
	return buf + 8;
}

__always_inline osc_data_t *
osc_get_symbol(osc_data_t *buf, const char **S)
{
	*S = (const char *)buf;
	return buf + osc_strlen(*S);
}

__always_inline osc_data_t *
osc_get_char(osc_data_t *buf, char *c)
{
	swap32_t s = {.u = *(uint32_t *)buf};
	s.u = ntohl(s.u);
	*c = s.i & 0xff;
	return buf + 4;
}

__always_inline osc_data_t *
osc_get_midi(osc_data_t *buf, uint8_t **m)
{
	*m = (uint8_t *)buf;
	return buf + 4;
}

osc_data_t *osc_skip(osc_type_t type, osc_data_t *buf);
osc_data_t *osc_get(osc_type_t type, osc_data_t *buf, osc_argument_t *arg);
size_t osc_vararg_get(osc_data_t *buf, const char **path, const char **fmt, ...);

// write OSC argument to raw buffer
__always_inline osc_data_t *
osc_set_path(osc_data_t *buf, const char *path)
{
	size_t len = osc_strlen(path);
	strncpy((char *)buf, path, len);
	return buf + len;
}

__always_inline osc_data_t *
osc_set_fmt(osc_data_t *buf, const char *fmt)
{
	size_t len = osc_fmtlen(fmt);
	*buf++ = ',';
	strncpy((char *)buf, fmt, len);
	return buf + len;
}

__always_inline osc_data_t *
osc_set_int32(osc_data_t *buf, int32_t i)
{
	swap32_t *s = (swap32_t *)buf;
	s->i = i;
	s->u = htonl(s->u);
	return buf + 4;
}

__always_inline osc_data_t *
osc_set_float(osc_data_t *buf, float f)
{
	swap32_t *s = (swap32_t *)buf;
	s->f = f;
	s->u = htonl(s->u);
	return buf + 4;
}

__always_inline osc_data_t *
osc_set_string(osc_data_t *buf, const char *s)
{
	size_t len = osc_strlen(s);
	strncpy((char *)buf, s, len);
	return buf + len;
}

__always_inline osc_data_t *
osc_set_blob(osc_data_t *buf, int32_t size, void *payload)
{
	size_t len = round_to_four_bytes(size);
	swap32_t *s = (swap32_t *)buf;
	s->i = size;
	s->u = htonl(s->u);
	buf += 4;
	memcpy(buf, payload, size);
	memset(buf+size, '\0', len-size); // zero padding
	return buf + len;
}

__always_inline osc_data_t *
osc_set_blob_inline(osc_data_t *buf, int32_t size, void **payload)
{
	size_t len = round_to_four_bytes(size);
	swap32_t *s = (swap32_t *)buf;
	s->i = size;
	s->u = htonl(s->u);
	buf += 4;
	*payload = buf;
	memset(buf+size, '\0', len-size); // zero padding
	return buf + len;
}

__always_inline osc_data_t *
osc_set_int64(osc_data_t *buf, int64_t h)
{
	swap64_t *s = (swap64_t *)buf;
	s->h = h;
	s->u = htonll(s->u);
	return buf + 8;
}

__always_inline osc_data_t *
osc_set_double(osc_data_t *buf, double d)
{
	swap64_t *s = (swap64_t *)buf;
	s->d = d;
	s->u = htonll(s->u);
	return buf + 8;
}

__always_inline osc_data_t *
osc_set_timetag(osc_data_t *buf, osc_time_t t)
{
	swap64_t *s = (swap64_t *)buf;
	s->t = t;
	s->u = htonll(s->u);
	return buf + 8;
}

__always_inline osc_data_t *
osc_set_symbol(osc_data_t *buf, const char *S)
{
	size_t len = osc_strlen(S);
	strncpy((char *)buf, S, len);
	return buf + len;
}

__always_inline osc_data_t *
osc_set_char(osc_data_t *buf, char c)
{
	swap32_t *s = (swap32_t *)buf;
	s->i = c;
	s->u = htonl(s->u);
	return buf + 4;
}

__always_inline osc_data_t *
osc_set_midi(osc_data_t *buf, uint8_t *m)
{
	buf[0] = m[0];
	buf[1] = m[1];
	buf[2] = m[2];
	buf[3] = m[3];
	return buf + 4;
}

__always_inline osc_data_t *
osc_set_midi_inline(osc_data_t *buf, uint8_t **m)
{
	*m = buf;
	return buf + 4;
}

__always_inline osc_data_t *
osc_start_bundle(osc_data_t *buf, osc_time_t t, osc_data_t **bndl)
{
	*bndl = buf;
	strncpy((char *)buf, "#bundle", 8);
	osc_set_timetag(buf + 8, t);
	return buf + 16;
}

__always_inline osc_data_t *
osc_end_bundle(osc_data_t *buf, osc_data_t *bndl)
{
	size_t len =  buf - (bndl + 16);
	if(len > 0)
		return buf;
	else // empty bundle
		return bndl;
}

__always_inline osc_data_t *
osc_start_bundle_item(osc_data_t *buf, osc_data_t **itm)
{
	*itm = buf;
	return buf + 4;
}

__always_inline osc_data_t *
osc_end_bundle_item(osc_data_t *buf, osc_data_t *itm)
{
	size_t len = buf - (itm + 4);
	if(len > 0)
	{
		osc_set_int32(itm, len);
		return buf;
	}
	else // empty item
		return itm;
}

osc_data_t *osc_set(osc_type_t type, osc_data_t *buf, osc_argument_t *arg);
size_t osc_vararg_set(osc_data_t *buf, const char *path, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
