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

#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include <jack_osc.h>

#include <tjost_config.h>

#ifdef HAS_METADATA_API
#	include <jack/metadata.h>
#	include <jackey.h>
#endif // HAS_METADATA_API

typedef union _swap32_t swap32_t;
typedef union _swap64_t swap64_t;

union _swap32_t {
	uint32_t u;

	int32_t i;
	float f;
};

union _swap64_t {
	uint64_t u;

	int64_t h;
	uint64_t t;
	double d;
};

// characters not allowed in OSC path string
static const char invalid_path_chars [] = {
	' ', '#',
	'\0'
};

// allowed characters in OSC format string
static const char valid_format_chars [] = {
	JACK_OSC_INT32, JACK_OSC_FLOAT, JACK_OSC_STRING, JACK_OSC_BLOB,
	JACK_OSC_TRUE, JACK_OSC_FALSE, JACK_OSC_NIL, JACK_OSC_BANG,
	JACK_OSC_INT64, JACK_OSC_DOUBLE, JACK_OSC_TIMETAG,
	JACK_OSC_SYMBOL, JACK_OSC_CHAR, JACK_OSC_MIDI,
	'\0'
};

int
jack_osc_mark_port(jack_client_t *client, jack_port_t *port)
{
#ifdef HAS_METADATA_API
	jack_uuid_t uuid = jack_port_uuid(port);
	return jack_set_property(client, uuid, JACKEY_EVENT_TYPES, JACK_EVENT_TYPE__OSC, "text/plain");
#else
	return 0;
#endif // HAS_METADATA_API
}

int
jack_osc_unmark_port(jack_client_t *client, jack_port_t *port)
{
#ifdef HAS_METADATA_API
	jack_uuid_t uuid = jack_port_uuid(port);
	return jack_remove_property(client, uuid, JACKEY_EVENT_TYPES);
#else
	return 0;
#endif // HAS_METADATA_API
}

int
jack_osc_is_marked_port(jack_port_t *port)
{
#ifdef HAS_METADATA_API
	jack_uuid_t uuid = jack_port_uuid(port);
	char *value = NULL;
	char *type = NULL;

	int marked = 0;

	if( (jack_get_property(uuid, JACKEY_EVENT_TYPES, &value, &type) == 0) &&
			(strstr(value, JACK_EVENT_TYPE__OSC) != NULL) )
		marked = 1;
	if(value)
		jack_free(value);
	if(type)
		jack_free(type);
	return marked;
#else
	return 0;
#endif // HAS_METADATA_API
}

// check for valid path string
int
jack_osc_check_path(const char *path)
{
	const char *ptr;

	if(path[0] != '/')
		return 0;

	for(ptr=path+1; *ptr!='\0'; ptr++)
		if( (isprint(*ptr) == 0) || (strchr(invalid_path_chars, *ptr) != NULL) )
			return 0;

	return 1;
}

// check for valid format string 
int
jack_osc_check_fmt(const char *format, int offset)
{
	const char *ptr;

	if(offset)
		if(format[0] != ',')
			return 0;

	for(ptr=format+offset; *ptr!='\0'; ptr++)
		if(strchr(valid_format_chars, *ptr) == NULL)
			return 0;

	return 1;
}

int
jack_osc_method_match(Jack_OSC_Method *methods, const char *path, const char *fmt)
{
	Jack_OSC_Method *meth;
	for(meth=methods; meth->cb; meth++)
		if( (!meth->path || !strcmp(meth->path, path)) && (!meth->fmt || !strcmp(meth->fmt, fmt+1)) )
			return 1;
	return 0;
}

static void
_jack_osc_method_dispatch_message(jack_nframes_t time, jack_osc_data_t *buf, size_t size, Jack_OSC_Method *methods, void *dat)
{
	jack_osc_data_t *ptr = buf;
	jack_osc_data_t *end = buf + size;

	const char *path;
	const char *fmt;

	ptr = jack_osc_get_path(ptr, &path);
	ptr = jack_osc_get_fmt(ptr, &fmt);

	Jack_OSC_Method *meth;
	for(meth=methods; meth->cb; meth++)
		if( (!meth->path || !strcmp(meth->path, path)) && (!meth->fmt || !strcmp(meth->fmt, fmt+1)) )
			if(meth->cb(time, path, fmt+1, ptr, dat))
				break;
}

static void
_jack_osc_method_dispatch_bundle(jack_nframes_t time, jack_osc_data_t *buf, size_t size, Jack_OSC_Method *methods, void *dat)
{
	jack_osc_data_t *ptr = buf;
	jack_osc_data_t *end = buf + size;

	ptr += 16; // skip bundle header

	while(ptr < end)
	{
		int32_t len = ntohl(*((int32_t *)ptr));
		ptr += sizeof(int32_t);
		switch(*ptr)
		{
			case '#':
				_jack_osc_method_dispatch_bundle(time, ptr, len, methods, dat);
				break;
			case '/':
				_jack_osc_method_dispatch_message(time, ptr, len, methods, dat);
				break;
		}
		ptr += len;
	}
}

void
jack_osc_method_dispatch(jack_nframes_t time, jack_osc_data_t *buf, size_t size, Jack_OSC_Method *methods, void *dat)
{
	switch(*buf)
	{
		case '#':
			_jack_osc_method_dispatch_bundle(time, buf, size, methods, dat);
			break;
		case '/':
			_jack_osc_method_dispatch_message(time, buf, size, methods, dat);
			break;
	}
}

int
jack_osc_message_check(jack_osc_data_t *buf, size_t size)
{
	jack_osc_data_t *ptr = buf;
	jack_osc_data_t *end = buf + size;

	const char *path;
	const char *fmt;

	ptr = jack_osc_get_path(ptr, &path);
	if( (ptr > end) || !jack_osc_check_path(path) )
		return 0;

	ptr = jack_osc_get_fmt(ptr, &fmt);
	if( (ptr > end) || !jack_osc_check_fmt(fmt, 1) )
		return 0;

	const char *type;
	for(type=fmt+1; (*type!='\0') && (ptr <= end); type++)
	{
		switch(*type)
		{
			case JACK_OSC_INT32:
			case JACK_OSC_FLOAT:
			case JACK_OSC_MIDI:
			case JACK_OSC_CHAR:
				ptr += 4;
				break;

			case JACK_OSC_STRING:
			case JACK_OSC_SYMBOL:
				ptr += jack_osc_strlen((const char *)ptr);
				break;

			case JACK_OSC_BLOB:
				ptr += jack_osc_bloblen(ptr);
				break;

			case JACK_OSC_INT64:
			case JACK_OSC_DOUBLE:
			case JACK_OSC_TIMETAG:
				ptr += 8;
				break;

			case JACK_OSC_TRUE:
			case JACK_OSC_FALSE:
			case JACK_OSC_NIL:
			case JACK_OSC_BANG:
				break;
		}
	}

	return ptr == end;
}

int
jack_osc_bundle_check(jack_osc_data_t *buf, size_t size)
{
	jack_osc_data_t *ptr = buf;
	jack_osc_data_t *end = buf + size;
	
	if(strncmp((char *)ptr, "#bundle", 8)) // bundle header valid?
		return 0;
	ptr += 16; // skip bundle header

	while(ptr < end)
	{
		int32_t *len = (int32_t *)ptr;
		int32_t hlen = htonl(*len);
		ptr += sizeof(int32_t);

		switch(*ptr)
		{
			case '#':
				if(!jack_osc_bundle_check(ptr, hlen))
					return 0;
				break;
			case '/':
				if(!jack_osc_message_check(ptr, hlen))
					return 0;
				break;
			default:
				return 0;
		}
		ptr += hlen;
	}

	return ptr == end;
}

int
jack_osc_packet_check(jack_osc_data_t *buf, size_t size)
{
	jack_osc_data_t *ptr = buf;
	
	switch(*ptr)
	{
		case '#':
			if(!jack_osc_bundle_check(ptr, size))
				return 0;
			break;
		case '/':
			if(!jack_osc_message_check(ptr, size))
				return 0;
			break;
		default:
			return 0;
	}

	return 1;
}

size_t
jack_osc_strlen(const char *buf)
{
	return round_to_four_bytes(strlen(buf) + 1);
}

size_t
jack_osc_fmtlen(const char *buf)
{
	return round_to_four_bytes(strlen(buf) + 2) - 1;
}

size_t
jack_osc_bloblen(jack_osc_data_t *buf)
{
	swap32_t s = {.u = *(uint32_t *)buf}; 
	s.u = ntohl(s.u);
	return 4 + round_to_four_bytes(s.i);
}

size_t
jack_osc_blobsize(jack_osc_data_t *buf)
{
	swap32_t s = {.u = *(uint32_t *)buf}; 
	s.u = ntohl(s.u);
	return s.i;
}

jack_osc_data_t *
jack_osc_skip(Jack_OSC_Type type, jack_osc_data_t *buf)
{
	switch(type)
	{
		case JACK_OSC_INT32:
		case JACK_OSC_FLOAT:
		case JACK_OSC_MIDI:
		case JACK_OSC_CHAR:
			return buf + 4;

		case JACK_OSC_STRING:
		case JACK_OSC_SYMBOL:
			return buf + jack_osc_strlen((const char *)buf);

		case JACK_OSC_BLOB:
			return buf + jack_osc_bloblen(buf);

		case JACK_OSC_INT64:
		case JACK_OSC_DOUBLE:
		case JACK_OSC_TIMETAG:
			return buf + 8;

		case JACK_OSC_TRUE:
		case JACK_OSC_FALSE:
		case JACK_OSC_NIL:
		case JACK_OSC_BANG:
			return buf;

		default:
			return buf;
	}
}

jack_osc_data_t *
jack_osc_get_path(jack_osc_data_t *buf, const char **path)
{
	*path = (const char *)buf;
	return buf + jack_osc_strlen(*path);
}

jack_osc_data_t *
jack_osc_get_fmt(jack_osc_data_t *buf, const char **fmt)
{
	*fmt = (const char *)buf;
	return buf + jack_osc_strlen(*fmt);
}

jack_osc_data_t *
jack_osc_get_int32(jack_osc_data_t *buf, int32_t *i)
{
	swap32_t s = {.u = *(uint32_t *)buf};
	s.u = ntohl(s.u);
	*i = s.i;
	return buf + 4;
}

jack_osc_data_t *
jack_osc_get_float(jack_osc_data_t *buf, float *f)
{
	swap32_t s = {.u = *(uint32_t *)buf};
	s.u = ntohl(s.u);
	*f = s.f;
	return buf + 4;
}

jack_osc_data_t *
jack_osc_get_string(jack_osc_data_t *buf, const char **s)
{
	*s = (const char *)buf;
	return buf + jack_osc_strlen(*s);
}

jack_osc_data_t *
jack_osc_get_blob(jack_osc_data_t *buf, Jack_OSC_Blob *b)
{
	b->size = jack_osc_blobsize(buf);
	b->payload = buf + 4;
	return buf + 4 + round_to_four_bytes(b->size);
}

jack_osc_data_t *
jack_osc_get_int64(jack_osc_data_t *buf, int64_t *h)
{
	swap64_t s = {.u = *(uint64_t *)buf};
	s.u = ntohll(s.u);
	*h = s.h;
	return buf + 8;
}

jack_osc_data_t *
jack_osc_get_double(jack_osc_data_t *buf, double *d)
{
	swap64_t s = {.u = *(uint64_t *)buf};
	s.u = ntohll(s.u);
	*d = s.d;
	return buf + 8;
}

jack_osc_data_t *
jack_osc_get_timetag(jack_osc_data_t *buf, uint64_t *t)
{
	swap64_t s = {.u = *(uint64_t *)buf};
	s.u = ntohll(s.u);
	*t = s.t;
	return buf + 8;
}

jack_osc_data_t *
jack_osc_get_symbol(jack_osc_data_t *buf, const char **S)
{
	*S = (const char *)buf;
	return buf + jack_osc_strlen(*S);
}

jack_osc_data_t *
jack_osc_get_char(jack_osc_data_t *buf, char *c)
{
	swap32_t s = {.u = *(uint32_t *)buf};
	s.u = ntohl(s.u);
	*c = s.i & 0xff;
	return buf + 4;
}

jack_osc_data_t *
jack_osc_get_midi(jack_osc_data_t *buf, uint8_t **m)
{
	*m = (uint8_t *)buf;
	return buf + 4;
}

jack_osc_data_t *
jack_osc_get(Jack_OSC_Type type, jack_osc_data_t *buf, Jack_OSC_Argument *arg)
{
	switch(type)
	{
		case JACK_OSC_INT32:
			return jack_osc_get_int32(buf, &arg->i);
		case JACK_OSC_FLOAT:
			return jack_osc_get_float(buf, &arg->f);
		case JACK_OSC_STRING:
			return jack_osc_get_string(buf, &arg->s);
		case JACK_OSC_BLOB:
			return jack_osc_get_blob(buf, &arg->b);

		case JACK_OSC_INT64:
			return jack_osc_get_int64(buf, &arg->h);
		case JACK_OSC_DOUBLE:
			return jack_osc_get_double(buf, &arg->d);
		case JACK_OSC_TIMETAG:
			return jack_osc_get_timetag(buf, &arg->t);

		case JACK_OSC_TRUE:
		case JACK_OSC_FALSE:
		case JACK_OSC_NIL:
		case JACK_OSC_BANG:
			return buf;

		case JACK_OSC_SYMBOL:
			return jack_osc_get_symbol(buf, &arg->S);
		case JACK_OSC_CHAR:
			return jack_osc_get_char(buf, &arg->c);
		case JACK_OSC_MIDI:
			return jack_osc_get_midi(buf, &arg->m);

		default:
			//TODO report error
			return buf;
	}
}

jack_osc_data_t *
jack_osc_set_path(jack_osc_data_t *buf, const char *path)
{
	size_t len = jack_osc_strlen(path);
	strncpy((char *)buf, path, len);
	return buf + len;
}

jack_osc_data_t *
jack_osc_set_fmt(jack_osc_data_t *buf, const char *fmt)
{
	size_t len = jack_osc_fmtlen(fmt);
	*buf++ = ',';
	strncpy((char *)buf, fmt, len);
	return buf + len;
}

jack_osc_data_t *
jack_osc_set_int32(jack_osc_data_t *buf, int32_t i)
{
	swap32_t *s = (swap32_t *)buf;
	s->i = i;
	s->u = htonl(s->u);
	return buf + 4;
}

jack_osc_data_t *
jack_osc_set_float(jack_osc_data_t *buf, float f)
{
	swap32_t *s = (swap32_t *)buf;
	s->f = f;
	s->u = htonl(s->u);
	return buf + 4;
}

jack_osc_data_t *
jack_osc_set_string(jack_osc_data_t *buf, const char *s)
{
	size_t len = jack_osc_strlen(s);
	strncpy((char *)buf, s, len);
	return buf + len;
}

jack_osc_data_t *
jack_osc_set_blob(jack_osc_data_t *buf, int32_t size, void *payload)
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

jack_osc_data_t *
jack_osc_set_blob_inline(jack_osc_data_t *buf, int32_t size, void **payload)
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

jack_osc_data_t *
jack_osc_set_int64(jack_osc_data_t *buf, int64_t h)
{
	swap64_t *s = (swap64_t *)buf;
	s->h = h;
	s->u = htonll(s->u);
	return buf + 8;
}

jack_osc_data_t *
jack_osc_set_double(jack_osc_data_t *buf, double d)
{
	swap64_t *s = (swap64_t *)buf;
	s->d = d;
	s->u = htonll(s->u);
	return buf + 8;
}

jack_osc_data_t *
jack_osc_set_timetag(jack_osc_data_t *buf, uint64_t t)
{
	swap64_t *s = (swap64_t *)buf;
	s->t = t;
	s->u = htonll(s->u);
	return buf + 8;
}

jack_osc_data_t *
jack_osc_set_symbol(jack_osc_data_t *buf, const char *S)
{
	size_t len = jack_osc_strlen(S);
	strncpy((char *)buf, S, len);
	return buf + len;
}

jack_osc_data_t *
jack_osc_set_char(jack_osc_data_t *buf, char c)
{
	swap32_t *s = (swap32_t *)buf;
	s->i = c;
	s->u = htonl(s->u);
	return buf + 4;
}

jack_osc_data_t *
jack_osc_set_midi(jack_osc_data_t *buf, uint8_t *m)
{
	buf[0] = m[0];
	buf[1] = m[1];
	buf[2] = m[2];
	buf[3] = m[3];
	return buf + 4;
}

jack_osc_data_t *
jack_osc_set(Jack_OSC_Type type, jack_osc_data_t *buf, Jack_OSC_Argument *arg)
{
	switch(type)
	{
		case JACK_OSC_INT32:
			return jack_osc_set_int32(buf, arg->i);
		case JACK_OSC_FLOAT:
			return jack_osc_set_float(buf, arg->f);
		case JACK_OSC_STRING:
			return jack_osc_set_string(buf, arg->s);
		case JACK_OSC_BLOB:
			return jack_osc_set_blob(buf, arg->b.size, arg->b.payload);

		case JACK_OSC_INT64:
			return jack_osc_set_int64(buf, arg->h);
		case JACK_OSC_DOUBLE:
			return jack_osc_set_double(buf, arg->d);
		case JACK_OSC_TIMETAG:
			return jack_osc_set_timetag(buf, arg->t);

		case JACK_OSC_TRUE:
		case JACK_OSC_FALSE:
		case JACK_OSC_NIL:
		case JACK_OSC_BANG:
			return buf;

		case JACK_OSC_SYMBOL:
			return jack_osc_set_symbol(buf, arg->S);
		case JACK_OSC_CHAR:
			return jack_osc_set_char(buf, arg->c);
		case JACK_OSC_MIDI:
			return jack_osc_set_midi(buf, arg->m);

		default:
			//TODO report error
			return buf;
	}
}

size_t
jack_osc_vararg_set(jack_osc_data_t *buf, const char *path, const char *fmt, ...)
{
	jack_osc_data_t *ptr = buf;

	if(!(ptr = jack_osc_set_path(ptr, path)))
		return 0;
	if(!(ptr = jack_osc_set_fmt(ptr, fmt)))
		return 0;

  va_list args;
  va_start (args, fmt);

  const char *type;
  for(type=fmt; *type != '\0'; type++)
		switch(*type)
		{
			case JACK_OSC_INT32:
				ptr = jack_osc_set_int32(ptr, va_arg(args, int32_t));
				break;
			case JACK_OSC_FLOAT:
				ptr = jack_osc_set_float(ptr, (float)va_arg(args, double));
				break;
			case JACK_OSC_STRING:
				ptr = jack_osc_set_string(ptr, va_arg(args, char *));
				break;
			case JACK_OSC_BLOB:
				ptr = jack_osc_set_blob(ptr, va_arg(args, int32_t), va_arg(args, void *));
				break;

			case JACK_OSC_INT64:
				ptr = jack_osc_set_int64(ptr, va_arg(args, int64_t));
				break;
			case JACK_OSC_DOUBLE:
				ptr = jack_osc_set_double(ptr, va_arg(args, double));
				break;
			case JACK_OSC_TIMETAG:
				ptr = jack_osc_set_timetag(ptr, va_arg(args, uint64_t));
				break;

			case JACK_OSC_TRUE:
			case JACK_OSC_FALSE:
			case JACK_OSC_NIL:
			case JACK_OSC_BANG:
				break;

			case JACK_OSC_SYMBOL:
				ptr = jack_osc_set_symbol(ptr, va_arg(args, char *));
				break;
			case JACK_OSC_CHAR:
				ptr = jack_osc_set_char(ptr, (char)va_arg(args, int));
				break;
			case JACK_OSC_MIDI:
				ptr = jack_osc_set_midi(ptr, va_arg(args, uint8_t *));
				break;

			default:
				//TODO report error
				break;
		}

  va_end(args);

	size_t len = ptr-buf;
	return len;
}
