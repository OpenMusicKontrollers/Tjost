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

#include <osc.h>

#include <tjost_config.h>

#ifdef HAS_METADATA_API
#	include <jack/metadata.h>
#	include <jackey.h>
#	include <jack_osc/jack_osc.h>
#endif // HAS_METADATA_API

// characters not allowed in OSC path string
static const char invalid_path_chars [] = {
	' ', '#',
	'\0'
};

// allowed characters in OSC format string
static const char valid_format_chars [] = {
	OSC_INT32, OSC_FLOAT, OSC_STRING, OSC_BLOB,
	OSC_TRUE, OSC_FALSE, OSC_NIL, OSC_BANG,
	OSC_INT64, OSC_DOUBLE, OSC_TIMETAG,
	OSC_SYMBOL, OSC_CHAR, OSC_MIDI,
	'\0'
};

int
osc_mark_port(jack_client_t *client, jack_port_t *port)
{
#ifdef HAS_METADATA_API
	jack_uuid_t uuid = jack_port_uuid(port);
	return jack_set_property(client, uuid, JACKEY_EVENT_TYPES, JACK_EVENT_TYPE__OSC, "text/plain");
#else
	return 0;
#endif // HAS_METADATA_API
}

int
osc_unmark_port(jack_client_t *client, jack_port_t *port)
{
#ifdef HAS_METADATA_API
	jack_uuid_t uuid = jack_port_uuid(port);
	return jack_remove_property(client, uuid, JACKEY_EVENT_TYPES);
#else
	return 0;
#endif // HAS_METADATA_API
}

int
osc_is_marked_port(jack_port_t *port)
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
osc_check_path(const char *path)
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
osc_check_fmt(const char *format, int offset)
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
osc_method_match(OSC_Method *methods, const char *path, const char *fmt)
{
	OSC_Method *meth;
	for(meth=methods; meth->cb; meth++)
		if( (!meth->path || !strcmp(meth->path, path)) && (!meth->fmt || !strcmp(meth->fmt, fmt+1)) )
			return 1;
	return 0;
}

static void
_osc_method_dispatch_message(jack_nframes_t time, osc_data_t *buf, size_t size, OSC_Method *methods, void *dat)
{
	osc_data_t *ptr = buf;
	osc_data_t *end = buf + size;

	const char *path;
	const char *fmt;

	ptr = osc_get_path(ptr, &path);
	ptr = osc_get_fmt(ptr, &fmt);

	OSC_Method *meth;
	for(meth=methods; meth->cb; meth++)
		if( (!meth->path || !strcmp(meth->path, path)) && (!meth->fmt || !strcmp(meth->fmt, fmt+1)) )
			if(meth->cb(time, path, fmt+1, ptr, dat))
				break;
}

static void
_osc_method_dispatch_bundle(jack_nframes_t time, osc_data_t *buf, size_t size, OSC_Method *methods, OSC_Bundle_In bundle_in, OSC_Bundle_Out bundle_out, void *dat)
{
	osc_data_t *ptr = buf;
	osc_data_t *end = buf + size;

	if(bundle_in)
		bundle_in(time, dat);

	ptr += 16; // skip bundle header

	while(ptr < end)
	{
		int32_t len = ntohl(*((int32_t *)ptr));
		ptr += sizeof(int32_t);
		switch(*ptr)
		{
			case '#':
				_osc_method_dispatch_bundle(time, ptr, len, methods, bundle_in, bundle_out, dat);
				break;
			case '/':
				_osc_method_dispatch_message(time, ptr, len, methods, dat);
				break;
		}
		ptr += len;
	}

	if(bundle_out)
		bundle_out(time, dat);
}

void
osc_method_dispatch(jack_nframes_t time, osc_data_t *buf, size_t size, OSC_Method *methods, OSC_Bundle_In bundle_in, OSC_Bundle_Out bundle_out, void *dat)
{
	switch(*buf)
	{
		case '#':
			_osc_method_dispatch_bundle(time, buf, size, methods, bundle_in, bundle_out, dat);
			break;
		case '/':
			_osc_method_dispatch_message(time, buf, size, methods, dat);
			break;
	}
}

// extract nested bundles with non-matching timestamps
static int
_unroll_partial(osc_data_t *buf, size_t size, OSC_Unroll_Inject *inject, void *dat)
{
	if(strncmp((char *)buf, "#bundle", 8)) // bundle header valid?
		return 0;

	osc_data_t *end = buf + size;
	osc_data_t *ptr = buf;

	uint64_t timetag = ntohll(*(uint64_t *)(buf + 8));
	inject->stamp(timetag, dat);

	int has_messages = 0;
	int has_nested_bundles = 0;

	ptr = buf + 16; // skip bundle header
	while(ptr < end)
	{
		int32_t *size = (int32_t *)ptr;
		int32_t hsize = htonl(*size);
		ptr += sizeof(int32_t);

		char c = *(char *)ptr;
		switch(c)
		{
			case '#':
				has_nested_bundles = 1;
				if(!_unroll_partial(ptr, hsize, inject, dat))
					return 0;
				break;
			case '/':
				has_messages = 1;
				// ignore for now
				break;
			default:
				return 0;
		}

		ptr += hsize;
	}

	if(!has_nested_bundles)
	{
		if(has_messages)
			inject->bundle(buf, size, dat);
		return 1;
	}

	if(!has_messages)
		return 1; // discard empty bundles

	// repack bundle with messages only, ignoring nested bundles
	ptr = buf + 16; // skip bundle header
	osc_data_t *dst = ptr;
	while(ptr < end)
	{
		int32_t *size = (int32_t *)ptr;
		int32_t hsize = htonl(*size);
		ptr += sizeof(int32_t);

		char *c = (char *)ptr;
		if(*c == '/')
		{
			memmove(dst, ptr - sizeof(int32_t), sizeof(int32_t) + hsize);
			dst += sizeof(int32_t) + hsize;
		}

		ptr += hsize;
	}

	size_t nlen = dst - buf; 
	inject->bundle(buf, nlen, dat);

	return 1;
}

// fully unroll bundle into single messages
static int
_unroll_full(osc_data_t *buf, size_t size, OSC_Unroll_Inject *inject, void *dat)
{
	if(strncmp((char *)buf, "#bundle", 8)) // bundle header valid?
		return 0;

	osc_data_t *end = buf + size;
	osc_data_t *ptr = buf;

	uint64_t timetag = ntohll(*(uint64_t *)(buf + 8));
	inject->stamp(timetag, dat);

	int has_nested_bundles = 0;

	ptr = buf + 16; // skip bundle header
	while(ptr < end)
	{
		int32_t *size = (int32_t *)ptr;
		int32_t hsize = htonl(*size);
		ptr += sizeof(int32_t);

		char c = *(char *)ptr;
		switch(c)
		{
			case '#':
				has_nested_bundles = 1;
				// ignore for now, messages are handled first
				break;
			case '/':
				inject->message(ptr, hsize, dat);
				break;
			default:
				return 0;
		}

		ptr += hsize;
	}

	if(!has_nested_bundles)
		return 1;

	ptr = buf + 16; // skip bundle header
	while(ptr < end)
	{
		int32_t *size = (int32_t *)ptr;
		int32_t hsize = htonl(*size);
		ptr += sizeof(int32_t);

		char *c = (char *)ptr;
		if(*c == '#')
			if(!_unroll_full(ptr, hsize, inject, dat))
				return 0;

		ptr += hsize;
	}

	return 1;
}

int
osc_packet_unroll(osc_data_t *buf, size_t size, OSC_Unroll_Mode mode, OSC_Unroll_Inject *inject, void *dat)
{
	char c = *(char *)buf;
	switch(c)
	{
		case '#':
			switch(mode)
			{
				case OSC_UNROLL_MODE_NONE:
					inject->bundle(buf, size, dat);
					break;
				case OSC_UNROLL_MODE_PARTIAL:
					if(!_unroll_partial(buf, size, inject, dat))
						return 0;
					break;
				case OSC_UNROLL_MODE_FULL:
					if(!_unroll_full(buf, size, inject, dat))
						return 0;
					break;
			}
			break;
		case '/':
			inject->message(buf, size, dat);
			break;
		default:
			return 0;
	}

	return 1;
}

int
osc_message_check(osc_data_t *buf, size_t size)
{
	osc_data_t *ptr = buf;
	osc_data_t *end = buf + size;

	const char *path;
	const char *fmt;

	ptr = osc_get_path(ptr, &path);
	if( (ptr > end) || !osc_check_path(path) )
		return 0;

	ptr = osc_get_fmt(ptr, &fmt);
	if( (ptr > end) || !osc_check_fmt(fmt, 1) )
		return 0;

	const char *type;
	for(type=fmt+1; (*type!='\0') && (ptr <= end); type++)
	{
		switch(*type)
		{
			case OSC_INT32:
			case OSC_FLOAT:
			case OSC_MIDI:
			case OSC_CHAR:
				ptr += 4;
				break;

			case OSC_STRING:
			case OSC_SYMBOL:
				ptr += osc_strlen((const char *)ptr);
				break;

			case OSC_BLOB:
				ptr += osc_bloblen(ptr);
				break;

			case OSC_INT64:
			case OSC_DOUBLE:
			case OSC_TIMETAG:
				ptr += 8;
				break;

			case OSC_TRUE:
			case OSC_FALSE:
			case OSC_NIL:
			case OSC_BANG:
				break;
		}
	}

	return ptr == end;
}

int
osc_bundle_check(osc_data_t *buf, size_t size)
{
	osc_data_t *ptr = buf;
	osc_data_t *end = buf + size;
	
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
				if(!osc_bundle_check(ptr, hlen))
					return 0;
				break;
			case '/':
				if(!osc_message_check(ptr, hlen))
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
osc_packet_check(osc_data_t *buf, size_t size)
{
	osc_data_t *ptr = buf;
	
	switch(*ptr)
	{
		case '#':
			if(!osc_bundle_check(ptr, size))
				return 0;
			break;
		case '/':
			if(!osc_message_check(ptr, size))
				return 0;
			break;
		default:
			return 0;
	}

	return 1;
}

extern size_t osc_strlen(const char *buf);
extern size_t osc_fmtlen(const char *buf);
extern size_t osc_blobsize(osc_data_t *buf);
extern size_t osc_bloblen(osc_data_t *buf);

extern osc_data_t *osc_get_path(osc_data_t *buf, const char **path);
extern osc_data_t *osc_get_fmt(osc_data_t *buf, const char **fmt);

extern osc_data_t *osc_get_int32(osc_data_t *buf, int32_t *i);
extern osc_data_t *osc_get_float(osc_data_t *buf, float *f);
extern osc_data_t *osc_get_string(osc_data_t *buf, const char **s);
extern osc_data_t *osc_get_blob(osc_data_t *buf, OSC_Blob *b);

extern osc_data_t *osc_get_int64(osc_data_t *buf, int64_t *h);
extern osc_data_t *osc_get_double(osc_data_t *buf, double *d);
extern osc_data_t *osc_get_timetag(osc_data_t *buf, uint64_t *t);

extern osc_data_t *osc_get_symbol(osc_data_t *buf, const char **S);
extern osc_data_t *osc_get_char(osc_data_t *buf, char *c);
extern osc_data_t *osc_get_midi(osc_data_t *buf, uint8_t **m);

osc_data_t *
osc_skip(OSC_Type type, osc_data_t *buf)
{
	switch(type)
	{
		case OSC_INT32:
		case OSC_FLOAT:
		case OSC_MIDI:
		case OSC_CHAR:
			return buf + 4;

		case OSC_STRING:
		case OSC_SYMBOL:
			return buf + osc_strlen((const char *)buf);

		case OSC_BLOB:
			return buf + osc_bloblen(buf);

		case OSC_INT64:
		case OSC_DOUBLE:
		case OSC_TIMETAG:
			return buf + 8;

		case OSC_TRUE:
		case OSC_FALSE:
		case OSC_NIL:
		case OSC_BANG:
			return buf;

		default:
			return buf;
	}
}

osc_data_t *
osc_get(OSC_Type type, osc_data_t *buf, OSC_Argument *arg)
{
	switch(type)
	{
		case OSC_INT32:
			return osc_get_int32(buf, &arg->i);
		case OSC_FLOAT:
			return osc_get_float(buf, &arg->f);
		case OSC_STRING:
			return osc_get_string(buf, &arg->s);
		case OSC_BLOB:
			return osc_get_blob(buf, &arg->b);

		case OSC_INT64:
			return osc_get_int64(buf, &arg->h);
		case OSC_DOUBLE:
			return osc_get_double(buf, &arg->d);
		case OSC_TIMETAG:
			return osc_get_timetag(buf, &arg->t);

		case OSC_TRUE:
		case OSC_FALSE:
		case OSC_NIL:
		case OSC_BANG:
			return buf;

		case OSC_SYMBOL:
			return osc_get_symbol(buf, &arg->S);
		case OSC_CHAR:
			return osc_get_char(buf, &arg->c);
		case OSC_MIDI:
			return osc_get_midi(buf, &arg->m);

		default:
			//TODO report error
			return buf;
	}
}

size_t
osc_vararg_get(osc_data_t *buf, const char **path, const char **fmt, ...)
{
	osc_data_t *ptr = buf;

	ptr = osc_get_path(ptr, path);
	ptr = osc_get_fmt(ptr, fmt);

  va_list args;
  va_start (args, fmt);

  const char *type;
  for(type=*fmt; *type != '\0'; type++)
		switch(*type)
		{
			case OSC_INT32:
				ptr = osc_get_int32(ptr, va_arg(args, int32_t *));
				break;
			case OSC_FLOAT:
				ptr = osc_get_float(ptr, va_arg(args, float *));
				break;
			case OSC_STRING:
				ptr = osc_get_string(ptr, va_arg(args, const char **));
				break;
			case OSC_BLOB:
				ptr = osc_get_blob(ptr, va_arg(args, OSC_Blob *));
				break;

			case OSC_INT64:
				ptr = osc_get_int64(ptr, va_arg(args, int64_t *));
				break;
			case OSC_DOUBLE:
				ptr = osc_get_double(ptr, va_arg(args, double *));
				break;
			case OSC_TIMETAG:
				ptr = osc_get_timetag(ptr, va_arg(args, uint64_t *));
				break;

			case OSC_TRUE:
			case OSC_FALSE:
			case OSC_NIL:
			case OSC_BANG:
				break;

			case OSC_SYMBOL:
				ptr = osc_get_symbol(ptr, va_arg(args, const char **));
				break;
			case OSC_CHAR:
				ptr = osc_get_char(ptr, va_arg(args, char *));
				break;
			case OSC_MIDI:
				ptr = osc_get_midi(ptr, va_arg(args, uint8_t **));
				break;

			default:
				//TODO report error
				break;
		}

  va_end(args);

	size_t len = ptr-buf;
	return len;
}

extern osc_data_t *osc_set_path(osc_data_t *buf, const char *path);
extern osc_data_t *osc_set_fmt(osc_data_t *buf, const char *fmt);

extern osc_data_t *osc_set_int32(osc_data_t *buf, int32_t i);
extern osc_data_t *osc_set_float(osc_data_t *buf, float f);
extern osc_data_t *osc_set_string(osc_data_t *buf, const char *s);
extern osc_data_t *osc_set_blob(osc_data_t *buf, int32_t size, void *payload);
extern osc_data_t *osc_set_blob_inline(osc_data_t *buf, int32_t size, void **payload);

extern osc_data_t *osc_set_int64(osc_data_t *buf, int64_t h);
extern osc_data_t *osc_set_double(osc_data_t *buf, double d);
extern osc_data_t *osc_set_timetag(osc_data_t *buf, uint64_t t);

extern osc_data_t *osc_set_symbol(osc_data_t *buf, const char *S);
extern osc_data_t *osc_set_char(osc_data_t *buf, char c);
extern osc_data_t *osc_set_midi(osc_data_t *buf, uint8_t *m);
extern osc_data_t *osc_set_midi_inline(osc_data_t *buf, uint8_t **m);

extern osc_data_t *osc_start_bundle(osc_data_t *buf, uint64_t t, osc_data_t **bndl);
extern osc_data_t *osc_end_bundle(osc_data_t *buf, osc_data_t *bndl);
extern osc_data_t *osc_start_bundle_item(osc_data_t *buf, osc_data_t **itm);
extern osc_data_t *osc_end_bundle_item(osc_data_t *buf, osc_data_t *itm);

osc_data_t *
osc_set(OSC_Type type, osc_data_t *buf, OSC_Argument *arg)
{
	switch(type)
	{
		case OSC_INT32:
			return osc_set_int32(buf, arg->i);
		case OSC_FLOAT:
			return osc_set_float(buf, arg->f);
		case OSC_STRING:
			return osc_set_string(buf, arg->s);
		case OSC_BLOB:
			return osc_set_blob(buf, arg->b.size, arg->b.payload);

		case OSC_INT64:
			return osc_set_int64(buf, arg->h);
		case OSC_DOUBLE:
			return osc_set_double(buf, arg->d);
		case OSC_TIMETAG:
			return osc_set_timetag(buf, arg->t);

		case OSC_TRUE:
		case OSC_FALSE:
		case OSC_NIL:
		case OSC_BANG:
			return buf;

		case OSC_SYMBOL:
			return osc_set_symbol(buf, arg->S);
		case OSC_CHAR:
			return osc_set_char(buf, arg->c);
		case OSC_MIDI:
			return osc_set_midi(buf, arg->m);

		default:
			//TODO report error
			return buf;
	}
}

size_t
osc_vararg_set(osc_data_t *buf, const char *path, const char *fmt, ...)
{
	osc_data_t *ptr = buf;

	if(!(ptr = osc_set_path(ptr, path)))
		return 0;
	if(!(ptr = osc_set_fmt(ptr, fmt)))
		return 0;

  va_list args;
  va_start (args, fmt);

  const char *type;
  for(type=fmt; *type != '\0'; type++)
		switch(*type)
		{
			case OSC_INT32:
				ptr = osc_set_int32(ptr, va_arg(args, int32_t));
				break;
			case OSC_FLOAT:
				ptr = osc_set_float(ptr, (float)va_arg(args, double));
				break;
			case OSC_STRING:
				ptr = osc_set_string(ptr, va_arg(args, char *));
				break;
			case OSC_BLOB:
				ptr = osc_set_blob(ptr, va_arg(args, int32_t), va_arg(args, void *));
				break;

			case OSC_INT64:
				ptr = osc_set_int64(ptr, va_arg(args, int64_t));
				break;
			case OSC_DOUBLE:
				ptr = osc_set_double(ptr, va_arg(args, double));
				break;
			case OSC_TIMETAG:
				ptr = osc_set_timetag(ptr, va_arg(args, uint64_t));
				break;

			case OSC_TRUE:
			case OSC_FALSE:
			case OSC_NIL:
			case OSC_BANG:
				break;

			case OSC_SYMBOL:
				ptr = osc_set_symbol(ptr, va_arg(args, char *));
				break;
			case OSC_CHAR:
				ptr = osc_set_char(ptr, (char)va_arg(args, int));
				break;
			case OSC_MIDI:
				ptr = osc_set_midi(ptr, va_arg(args, uint8_t *));
				break;

			default:
				//TODO report error
				break;
		}

  va_end(args);

	size_t len = ptr-buf;
	return len;
}
