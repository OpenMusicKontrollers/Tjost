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
#endif // HAS_METADATA_API

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
	return jack_set_property(client, uuid, JACK_METADATA_EVENT_KEY, JACK_METADATA_EVENT_TYPE_OSC, NULL);
#else
	return 0;
#endif // HAS_METADATA_API
}

int
jack_osc_unmark_port(jack_client_t *client, jack_port_t *port)
{
#ifdef HAS_METADATA_API
	jack_uuid_t uuid = jack_port_uuid(port);
	return jack_remove_property(client, uuid, JACK_METADATA_EVENT_KEY);
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

	if( (jack_get_property(uuid, JACK_METADATA_EVENT_KEY, &value, &type) == 0) &&
			(strcmp(value, JACK_METADATA_EVENT_TYPE_OSC) == 0) )
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

void
jack_osc_method_dispatch(jack_nframes_t time, jack_osc_data_t *buf, size_t size, Jack_OSC_Method *methods, void *dat)
{
	jack_osc_data_t *ptr = buf;
	jack_osc_data_t *end = buf + size/sizeof(jack_osc_data_t);

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

int
jack_osc_message_check(jack_osc_data_t *buf, size_t size)
{
	jack_osc_data_t *ptr = buf;
	jack_osc_data_t *end = buf + size/sizeof(jack_osc_data_t);

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
				ptr += 1;
				break;

			case JACK_OSC_STRING:
			case JACK_OSC_SYMBOL:
				ptr += jack_osc_strquads((const char *)ptr);
				break;

			case JACK_OSC_BLOB:
				ptr += jack_osc_blobquads(ptr);
				break;

			case JACK_OSC_INT64:
			case JACK_OSC_DOUBLE:
			case JACK_OSC_TIMETAG:
				ptr += 2;
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

#if __BYTE_ORDER == __LITTLE_ENDIAN
int
jack_osc_message_ntoh(jack_osc_data_t *buf, size_t size)
{
	jack_osc_data_t *ptr = buf;
	jack_osc_data_t *end = buf + size/sizeof(jack_osc_data_t);

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
			case JACK_OSC_MIDI: //FIXME correct?
			case JACK_OSC_CHAR:
			{
				uint32_t *u = (uint32_t *)ptr;
				*u = ntohl(*u);
				ptr += 1;
				break;
			}

			case JACK_OSC_STRING:
			case JACK_OSC_SYMBOL:
				ptr += jack_osc_strquads((const char *)ptr);
				break;

			case JACK_OSC_BLOB:
			{
				uint32_t *u = (uint32_t *)ptr;
				*u = ntohl(*u);
				ptr += jack_osc_blobquads(ptr);
				break;
			}

			case JACK_OSC_INT64:
			case JACK_OSC_DOUBLE:
			case JACK_OSC_TIMETAG:
			{
				uint64_t *u = (uint64_t *)ptr;
				*u = ntohll(*u);
				ptr += 2;
				break;
			}

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
jack_osc_message_hton(jack_osc_data_t *buf, size_t size)
{
	jack_osc_data_t *ptr = buf;
	jack_osc_data_t *end = buf + size/sizeof(jack_osc_data_t);

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
			case JACK_OSC_MIDI: //FIXME correct?
			case JACK_OSC_CHAR:
			{
				uint32_t *u = (uint32_t *)ptr;
				*u = htonl(*u);
				ptr += 1;
				break;
			}

			case JACK_OSC_STRING:
			case JACK_OSC_SYMBOL:
				ptr += jack_osc_strquads((const char *)ptr);
				break;

			case JACK_OSC_BLOB:
			{
				uint32_t *u = (uint32_t *)ptr;
				ptr += jack_osc_blobquads(ptr);
				*u = htonl(*u);
				break;
			}

			case JACK_OSC_INT64:
			case JACK_OSC_DOUBLE:
			case JACK_OSC_TIMETAG:
			{
				uint64_t *u = (uint64_t *)ptr;
				*u = htonll(*u);
				ptr += 2;
				break;
			}

			case JACK_OSC_TRUE:
			case JACK_OSC_FALSE:
			case JACK_OSC_NIL:
			case JACK_OSC_BANG:
				break;
		}
	}

	return ptr == end;
}
#endif

size_t
jack_osc_strlen(const char *buf)
{
	return round_to_four_bytes(strlen(buf) + 1);
}

size_t
jack_osc_strquads(const char *buf)
{
	return quads(strlen(buf) + 1);
}

size_t
jack_osc_fmtlen(const char *buf)
{
	return round_to_four_bytes(strlen(buf) + 2) - 1;
}

size_t
jack_osc_bloblen(jack_osc_data_t *buf)
{
	int32_t i = *(int32_t *)buf;
	return 4 + round_to_four_bytes(i);
}

size_t
jack_osc_blobquads(jack_osc_data_t *buf)
{
	int32_t i = *(int32_t *)buf;
	return 1 + quads(i);
}

size_t
jack_osc_blobsize(jack_osc_data_t *buf)
{
	int32_t i = *(int32_t *)buf;
	return i;
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
			return buf + 1;

		case JACK_OSC_STRING:
		case JACK_OSC_SYMBOL:
			return buf + jack_osc_strquads((const char *)buf);

		case JACK_OSC_BLOB:
			return buf + jack_osc_blobquads(buf);

		case JACK_OSC_INT64:
		case JACK_OSC_DOUBLE:
		case JACK_OSC_TIMETAG:
			return buf + 2;

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
	return buf + jack_osc_strquads(*path);
}

jack_osc_data_t *
jack_osc_get_fmt(jack_osc_data_t *buf, const char **fmt)
{
	*fmt = (const char *)buf;
	return buf + jack_osc_strquads(*fmt);
}

jack_osc_data_t *
jack_osc_get_int32(jack_osc_data_t *buf, int32_t *i)
{
	*i = *(int32_t *)buf;
	return buf + 1;
}

jack_osc_data_t *
jack_osc_get_float(jack_osc_data_t *buf, float *f)
{
	*f = *(float *)buf;
	return buf + 1;
}

jack_osc_data_t *
jack_osc_get_string(jack_osc_data_t *buf, const char **s)
{
	*s = (const char *)buf;
	return buf + jack_osc_strquads(*s);
}

jack_osc_data_t *
jack_osc_get_blob(jack_osc_data_t *buf, Jack_OSC_Blob *b)
{
	b->size = jack_osc_blobsize(buf);
	b->payload = buf + 1;
	return buf + 1 + quads(b->size);
}

jack_osc_data_t *
jack_osc_get_int64(jack_osc_data_t *buf, int64_t *h)
{
	*h = *(int64_t *)buf;
	return buf + 2;
}

jack_osc_data_t *
jack_osc_get_double(jack_osc_data_t *buf, double *d)
{
	*d = *(double *)buf;
	return buf + 2;
}

jack_osc_data_t *
jack_osc_get_timetag(jack_osc_data_t *buf, uint64_t *t)
{
	*t = *(uint64_t *)buf;
	return buf + 2;
}

jack_osc_data_t *
jack_osc_get_symbol(jack_osc_data_t *buf, const char **S)
{
	*S = (const char *)buf;
	return buf + jack_osc_strquads(*S);
}

jack_osc_data_t *
jack_osc_get_char(jack_osc_data_t *buf, char *c)
{
	*c = *(int32_t *)buf;
	return buf + 1;
}

jack_osc_data_t *
jack_osc_get_midi(jack_osc_data_t *buf, uint8_t **m)
{
	*m = (uint8_t *)buf;
	return buf + 1;
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
	size_t len = jack_osc_strquads(path);
	strncpy((char *)buf, path, len*4);
	return buf + len;
}

jack_osc_data_t *
jack_osc_set_fmt(jack_osc_data_t *buf, const char *fmt)
{
	size_t len = jack_osc_fmtlen(fmt);
	char *fmt_ptr = (char *)buf;
	*fmt_ptr++ = ',';
	strncpy(fmt_ptr, fmt, len);
	return buf + (len+1)/sizeof(jack_osc_data_t);
}

jack_osc_data_t *
jack_osc_set_int32(jack_osc_data_t *buf, int32_t i)
{
	*(int32_t *)buf = i;
	return buf + 1;
}

jack_osc_data_t *
jack_osc_set_float(jack_osc_data_t *buf, float f)
{
	*(float *)buf = f;
	return buf + 1;
}

jack_osc_data_t *
jack_osc_set_string(jack_osc_data_t *buf, const char *s)
{
	size_t len = jack_osc_strquads(s);
	strncpy((char *)buf, s, len*4);
	return buf + len;
}

jack_osc_data_t *
jack_osc_set_blob(jack_osc_data_t *buf, int32_t size, void *payload)
{
	size_t len = quads(size); //FIXME
	*(int32_t *)buf = size;
	buf += 1;
	memcpy(buf, payload, size);
	memset(buf+size, '\0', len*4-size); // zero padding
	return buf + len;
}

jack_osc_data_t *
jack_osc_set_blob_inline(jack_osc_data_t *buf, int32_t size, void **payload)
{
	size_t len = quads(size); //FIXME
	*(int32_t *)buf = size;
	buf += 1;
	*payload = buf;
	buf += size;
	memset(buf+size, '\0', len*4-size); // zero padding
	return buf + len;
}

jack_osc_data_t *
jack_osc_set_int64(jack_osc_data_t *buf, int64_t h)
{
	*(int64_t *)buf = h;
	return buf + 2;
}

jack_osc_data_t *
jack_osc_set_double(jack_osc_data_t *buf, double d)
{
	*(double *)buf = d;
	return buf + 2;
}

jack_osc_data_t *
jack_osc_set_timetag(jack_osc_data_t *buf, uint64_t t)
{
	*(uint64_t *)buf = t;
	return buf + 2;
}

jack_osc_data_t *
jack_osc_set_symbol(jack_osc_data_t *buf, const char *S)
{
	size_t len = jack_osc_strquads(S);
	strncpy((char *)buf, S, len*4);
	return buf + len;
}

jack_osc_data_t *
jack_osc_set_char(jack_osc_data_t *buf, char c)
{
	*(int32_t *)buf = c;
	return buf + 1;
}

jack_osc_data_t *
jack_osc_set_midi(jack_osc_data_t *buf, uint8_t *m)
{
	uint8_t *buf_ptr = (uint8_t *)buf;
	buf_ptr[0] = m[0];
	buf_ptr[1] = m[1];
	buf_ptr[2] = m[2];
	buf_ptr[3] = m[3];
	return buf + 1;
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

	size_t len = (ptr-buf)*sizeof(jack_osc_data_t);
	return len;
}
