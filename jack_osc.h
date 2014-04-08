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

#include <jack/jack.h>
#include <jack/types.h>
#include <jack/ringbuffer.h>
#include <jack/weakmacros.h>
#include <jack/midiport.h>
#include <jack/metadata.h>
#include <jack/uuid.h>

#define JACK_METADATA_EVENT_KEY "http://jackaudio.org/event/type"
#define JACK_METADATA_EVENT_TYPE_OSC "Open Sound Control"

#define JACK_DEFAULT_OSC_TYPE JACK_DEFAULT_MIDI_TYPE
#define JACK_DEFAULT_OSC_BUFFER_SIZE 0

#define jack_osc_data_t									jack_midi_data_t
#define jack_osc_event_t								jack_midi_event_t

#define jack_osc_get_event_count				jack_midi_get_event_count
#define jack_osc_event_get							jack_midi_event_get
#define jack_osc_clear_buffer						jack_midi_clear_buffer
#define jack_osc_max_event_size					jack_midi_max_event_size
#define jack_osc_event_reserve					jack_midi_event_reserve
#define jack_osc_event_write						jack_midi_event_write
#define jack_osc_get_lost_event_count		jack_midi_lost_event_count

typedef int (*Jack_OSC_Callback) (jack_nframes_t time, const char *path, const char *fmt, uint8_t *arg, void *dat);
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
	uint8_t *payload;
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

int jack_osc_method_match(Jack_OSC_Method *methods, const char *path, const char *fmt);
void jack_osc_method_dispatch(jack_nframes_t time, uint8_t *buf, size_t size, Jack_OSC_Method *methods, void *dat);
int jack_osc_message_check(uint8_t *buf, size_t size);
#if __BYTE_ORDER == __BIG_ENDIAN
#	define jack_osc_message_ntoh jack_osc_message_check
#	define jack_osc_message_hton jack_osc_message_check
#else
# if __BYTE_ORDER == __LITTLE_ENDIAN
int jack_osc_message_ntoh(uint8_t *buf, size_t size);
int jack_osc_message_hton(uint8_t *buf, size_t size);
# endif
#endif

// OSC object lengths
size_t jack_osc_strlen(const char *buf);
size_t jack_osc_fmtlen(const char *buf);
size_t jack_osc_bloblen(uint8_t *buf);
size_t jack_osc_blobsize(uint8_t *buf);

// get OSC arguments from raw buffer
uint8_t *jack_osc_get_path(uint8_t *buf, const char **path);
uint8_t *jack_osc_get_fmt(uint8_t *buf, const char **fmt);

uint8_t *jack_osc_get_int32(uint8_t *buf, int32_t *i);
uint8_t *jack_osc_get_float(uint8_t *buf, float *f);
uint8_t *jack_osc_get_string(uint8_t *buf, const char **s);
uint8_t *jack_osc_get_blob(uint8_t *buf, Jack_OSC_Blob *b);

uint8_t *jack_osc_get_int64(uint8_t *buf, int64_t *h);
uint8_t *jack_osc_get_double(uint8_t *buf, double *d);
uint8_t *jack_osc_get_timetag(uint8_t *buf, uint64_t *t);

uint8_t *jack_osc_get_symbol(uint8_t *buf, const char **S);
uint8_t *jack_osc_get_char(uint8_t *buf, char *c);
uint8_t *jack_osc_get_midi(uint8_t *buf, uint8_t **m);

uint8_t *jack_osc_skip(Jack_OSC_Type type, uint8_t *buf);
uint8_t *jack_osc_get(Jack_OSC_Type type, uint8_t *buf, Jack_OSC_Argument *arg);

// write OSC argument to raw buffer
uint8_t *jack_osc_set_path(uint8_t *buf, const char *path);
uint8_t *jack_osc_set_fmt(uint8_t *buf, const char *fmt);

uint8_t *jack_osc_set_int32(uint8_t *buf, int32_t i);
uint8_t *jack_osc_set_float(uint8_t *buf, float f);
uint8_t *jack_osc_set_string(uint8_t *buf, const char *s);
uint8_t *jack_osc_set_blob(uint8_t *buf, int32_t size, uint8_t *payload);
uint8_t *jack_osc_set_blob_inline(uint8_t *buf, int32_t size, uint8_t **payload);

uint8_t *jack_osc_set_int64(uint8_t *buf, int64_t h);
uint8_t *jack_osc_set_double(uint8_t *buf, double d);
uint8_t *jack_osc_set_timetag(uint8_t *buf, uint64_t t);

uint8_t *jack_osc_set_symbol(uint8_t *buf, const char *S);
uint8_t *jack_osc_set_char(uint8_t *buf, char c);
uint8_t *jack_osc_set_midi(uint8_t *buf, uint8_t *m);

uint8_t *jack_osc_set(Jack_OSC_Type type, uint8_t *buf, Jack_OSC_Argument *arg);
size_t jack_osc_vararg_set(uint8_t *buf, const char *path, const char *fmt, ...);

#define round_to_four_bytes(size) \
({ \
	size_t len = (size_t)(size); \
	size_t rem; \
	if( (rem = len % 4) ) \
		len += 4 - rem; \
	((size_t)len); \
})

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
