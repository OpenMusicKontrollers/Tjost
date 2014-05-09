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

#ifndef _JACK_OSC_H_
#define _JACK_OSC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <arpa/inet.h> // hton & co.
#include <stdint.h>

<<<<<<< HEAD
#include <jack_osc/jack_osc.h>
=======
#include <jack/jack.h>
#include <jack/types.h>
#include <jack/ringbuffer.h>
#include <jack/weakmacros.h>
#include <jack/midiport.h>

#define JACK_DEFAULT_OSC_TYPE JACK_DEFAULT_MIDI_TYPE
#define JACK_DEFAULT_OSC_BUFFER_SIZE 0

typedef jack_midi_data_t								jack_osc_data_t;
typedef jack_midi_event_t								jack_osc_event_t;

#define jack_osc_get_event_count				jack_midi_get_event_count
#define jack_osc_event_get							jack_midi_event_get
#define jack_osc_clear_buffer						jack_midi_clear_buffer
#define jack_osc_max_event_size					jack_midi_max_event_size
#define jack_osc_event_reserve					jack_midi_event_reserve
#define jack_osc_event_write						jack_midi_event_write
#define jack_osc_get_lost_event_count		jack_midi_get_lost_event_count
>>>>>>> 65124c2169b7c3f37b3b00235ab01253a98f49b0

typedef int (*Jack_OSC_Callback) (jack_nframes_t time, const char *path, const char *fmt, jack_osc_data_t *arg, void *dat);
typedef struct _Jack_OSC_Method Jack_OSC_Method;
typedef struct _Jack_OSC_Blob Jack_OSC_Blob;
typedef union _Jack_OSC_Argument Jack_OSC_Argument;

typedef enum _Jack_OSC_Type {
	JACK_OSC_INT32		=	'i',
	JACK_OSC_FLOAT		=	'f',
	JACK_OSC_STRING		=	's',
	JACK_OSC_BLOB			=	'b',
	
	JACK_OSC_TRUE			=	'T',
	JACK_OSC_FALSE		=	'F',
	JACK_OSC_NIL			=	'N',
	JACK_OSC_BANG			=	'I',
	
	JACK_OSC_INT64		=	'h',
	JACK_OSC_DOUBLE		=	'd',
	JACK_OSC_TIMETAG	=	't',
	
	JACK_OSC_SYMBOL		=	'S',
	JACK_OSC_CHAR			=	'c',
	JACK_OSC_MIDI			=	'm'

} Jack_OSC_Type;


struct _Jack_OSC_Method {
	const char *path;
	const char *fmt;
	Jack_OSC_Callback cb;
};

struct _Jack_OSC_Blob {
	int32_t size;
	void *payload;
};

union _Jack_OSC_Argument {
	int32_t i;
	float f;
	const char *s;
	Jack_OSC_Blob b;

	int64_t h;
	double d;
	uint64_t t;

	const char *S;
	char c;
	uint8_t *m;
};

int jack_osc_mark_port(jack_client_t *client, jack_port_t *port);
int jack_osc_unmark_port(jack_client_t *client, jack_port_t *port);
int jack_osc_is_marked_port(jack_port_t *port);

int jack_osc_check_path(const char *path);
int jack_osc_check_fmt(const char *format, int offset);

int jack_osc_method_match(Jack_OSC_Method *methods, const char *path, const char *fmt);
void jack_osc_method_dispatch(jack_nframes_t time, jack_osc_data_t *buf, size_t size, Jack_OSC_Method *methods, void *dat);
int jack_osc_message_check(jack_osc_data_t *buf, size_t size);

// OSC object lengths
size_t jack_osc_strlen(const char *buf);
size_t jack_osc_fmtlen(const char *buf);
size_t jack_osc_bloblen(jack_osc_data_t *buf);
size_t jack_osc_blobsize(jack_osc_data_t *buf);

// get OSC arguments from raw buffer
jack_osc_data_t *jack_osc_get_path(jack_osc_data_t *buf, const char **path);
jack_osc_data_t *jack_osc_get_fmt(jack_osc_data_t *buf, const char **fmt);

jack_osc_data_t *jack_osc_get_int32(jack_osc_data_t *buf, int32_t *i);
jack_osc_data_t *jack_osc_get_float(jack_osc_data_t *buf, float *f);
jack_osc_data_t *jack_osc_get_string(jack_osc_data_t *buf, const char **s);
jack_osc_data_t *jack_osc_get_blob(jack_osc_data_t *buf, Jack_OSC_Blob *b);

jack_osc_data_t *jack_osc_get_int64(jack_osc_data_t *buf, int64_t *h);
jack_osc_data_t *jack_osc_get_double(jack_osc_data_t *buf, double *d);
jack_osc_data_t *jack_osc_get_timetag(jack_osc_data_t *buf, uint64_t *t);

jack_osc_data_t *jack_osc_get_symbol(jack_osc_data_t *buf, const char **S);
jack_osc_data_t *jack_osc_get_char(jack_osc_data_t *buf, char *c);
jack_osc_data_t *jack_osc_get_midi(jack_osc_data_t *buf, uint8_t **m);

jack_osc_data_t *jack_osc_skip(Jack_OSC_Type type, jack_osc_data_t *buf);
jack_osc_data_t *jack_osc_get(Jack_OSC_Type type, jack_osc_data_t *buf, Jack_OSC_Argument *arg);

// write OSC argument to raw buffer
jack_osc_data_t *jack_osc_set_path(jack_osc_data_t *buf, const char *path);
jack_osc_data_t *jack_osc_set_fmt(jack_osc_data_t *buf, const char *fmt);

jack_osc_data_t *jack_osc_set_int32(jack_osc_data_t *buf, int32_t i);
jack_osc_data_t *jack_osc_set_float(jack_osc_data_t *buf, float f);
jack_osc_data_t *jack_osc_set_string(jack_osc_data_t *buf, const char *s);
jack_osc_data_t *jack_osc_set_blob(jack_osc_data_t *buf, int32_t size, void *payload);
jack_osc_data_t *jack_osc_set_blob_inline(jack_osc_data_t *buf, int32_t size, void **payload);

jack_osc_data_t *jack_osc_set_int64(jack_osc_data_t *buf, int64_t h);
jack_osc_data_t *jack_osc_set_double(jack_osc_data_t *buf, double d);
jack_osc_data_t *jack_osc_set_timetag(jack_osc_data_t *buf, uint64_t t);

jack_osc_data_t *jack_osc_set_symbol(jack_osc_data_t *buf, const char *S);
jack_osc_data_t *jack_osc_set_char(jack_osc_data_t *buf, char c);
jack_osc_data_t *jack_osc_set_midi(jack_osc_data_t *buf, uint8_t *m);

jack_osc_data_t *jack_osc_set(Jack_OSC_Type type, jack_osc_data_t *buf, Jack_OSC_Argument *arg);
size_t jack_osc_vararg_set(jack_osc_data_t *buf, const char *path, const char *fmt, ...);

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

#ifdef __cplusplus
}
#endif

#endif
