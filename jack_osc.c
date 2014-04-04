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

// characters not allowed in OSC path string
static const char invalid_path_chars [] = {
	' ', '#',
	'\0'
};

// allowed characters in OSC format string
static const char valid_format_chars [] = {
	'i', 'f', 's', 'b',
	'T', 'F', 'N', 'I',
	'h', 'd', 't',
	'S', 'c', 'm',
	'\0'
};

// check for valid path string
static int
is_valid_path(const char *path)
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
static int
is_valid_format(const char *format, int offset)
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
jack_osc_method_dispatch(jack_nframes_t time, uint8_t *buf, size_t size, Jack_OSC_Method *methods, void *dat)
{
	uint8_t *ptr = buf;
	uint8_t *end = buf + size;

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
jack_osc_message_check(uint8_t *buf, size_t size)
{
	uint8_t *ptr = buf;
	uint8_t *end = buf + size;

	const char *path;
	const char *fmt;

	ptr = jack_osc_get_path(ptr, &path);
	if( (ptr > end) || !is_valid_path(path) )
		return 0;

	ptr = jack_osc_get_fmt(ptr, &fmt);
	if( (ptr > end) || !is_valid_format(fmt, 1) )
		return 0;

	const char *type;
	for(type=fmt+1; (*type!='\0') && (ptr <= end); type++)
	{
		switch(*type)
		{
			case 'i':
			case 'f':
			case 'm':
			case 'c':
				ptr += 4;
				break;

			case 's':
			case 'S':
				ptr += jack_osc_strlen((const char *)ptr);
				break;

			case 'b':
				ptr += jack_osc_bloblen(ptr);
				break;

			case 'h':
			case 'd':
			case 't':
				ptr += 8;
				break;

			case 'T':
			case 'F':
			case 'I':
			case 'N':
				break;
		}
	}

	return ptr == end;
}

inline size_t
jack_osc_strlen(const char *buf)
{
	return round_to_four_bytes(strlen(buf) + 1);
}

inline size_t
jack_osc_fmtlen(const char *buf)
{
	return round_to_four_bytes(strlen(buf) + 2) - 1;
}

inline size_t
jack_osc_bloblen(uint8_t *buf)
{
	swap32 s = {.i = *(int32_t *)buf};
	s.u = ntohl(s.u);
	return 4 + round_to_four_bytes(s.i);
}

inline size_t
jack_osc_blobsize(uint8_t *buf)
{
	swap32 s = {.i = *(int32_t *)buf};
	s.u = ntohl(s.u);
	return s.i;
}

inline uint8_t *
jack_osc_skip(char type, uint8_t *buf)
{
	switch(type)
	{
		case 'i':
		case 'f':
		case 'm':
		case 'c':
			return buf + 4;

		case 's':
		case 'S':
			return buf + jack_osc_strlen((const char *)buf);

		case 'b':
			return buf + jack_osc_bloblen(buf);

		case 'h':
		case 'd':
		case 't':
			return buf + 8;

		case 'T':
		case 'F':
		case 'N':
		case 'I':
			return buf;

		default:
			return buf;
	}
}

inline uint8_t *
jack_osc_get_path(uint8_t *buf, const char **path)
{
	*path = (const char *)buf;
	return buf + jack_osc_strlen(*path);
}

inline uint8_t *
jack_osc_get_fmt(uint8_t *buf, const char **fmt)
{
	*fmt = (const char *)buf;
	return buf + jack_osc_strlen(*fmt);
}

inline uint8_t *
jack_osc_get_int32(uint8_t *buf, int32_t *i)
{
	swap32 s = {.i = *(int32_t *)buf};
	s.u = ntohl(s.u);
	*i = s.i;
	return buf + 4;
}

inline uint8_t *
jack_osc_get_float(uint8_t *buf, float *f)
{
	swap32 s = {.f = *(float *)buf};
	s.u = ntohl(s.u);
	*f = s.f;
	return buf + 4;
}

inline uint8_t *
jack_osc_get_string(uint8_t *buf, const char **s)
{
	*s = (const char *)buf;
	return buf + jack_osc_strlen(*s);
}

inline uint8_t *
jack_osc_get_blob(uint8_t *buf, Jack_OSC_Blob *b)
{
	b->size = jack_osc_blobsize(buf);
	b->payload = buf + 4;
	return buf + round_to_four_bytes(b->size) + 4;
}

inline uint8_t *
jack_osc_get_int64(uint8_t *buf, int64_t *h)
{
	swap64 s = {.h = *(int64_t *)buf};
	s.u = ntohll(s.u);
	*h = s.h;
	return buf + 8;
}

inline uint8_t *
jack_osc_get_double(uint8_t *buf, double *d)
{
	swap64 s = {.d = *(double *)buf};
	s.u = ntohll(s.u);
	*d = s.d;
	return buf + 8;
}

inline uint8_t *
jack_osc_get_timetag(uint8_t *buf, uint64_t *t)
{
	swap64 s = {.t = *(uint64_t *)buf};
	s.u = ntohll(s.u);
	*t = s.t;
	return buf + 8;
}

inline uint8_t *
jack_osc_get_symbol(uint8_t *buf, const char **S)
{
	*S = (const char *)buf;
	return buf + jack_osc_strlen(*S);
}

inline uint8_t *
jack_osc_get_char(uint8_t *buf, char *c)
{
	*c = buf[3];
	return buf + 4;
}

inline uint8_t *
jack_osc_get_midi(uint8_t *buf, uint8_t **m)
{
	*m = buf;
	return buf + 4;
}

inline uint8_t *
jack_osc_get(char type, uint8_t *buf, Jack_OSC_Argument *arg)
{
	switch(type)
	{
		case 'i':
			return jack_osc_get_int32(buf, &arg->i);
		case 'f':
			return jack_osc_get_float(buf, &arg->f);
		case 's':
			return jack_osc_get_string(buf, &arg->s);
		case 'b':
			return jack_osc_get_blob(buf, &arg->b);

		case 'h':
			return jack_osc_get_int64(buf, &arg->h);
		case 'd':
			return jack_osc_get_double(buf, &arg->d);
		case 't':
			return jack_osc_get_timetag(buf, &arg->t);

		case 'T':
		case 'F':
		case 'N':
		case 'I':
			return buf;

		case 'S':
			return jack_osc_get_symbol(buf, &arg->S);
		case 'c':
			return jack_osc_get_char(buf, &arg->c);
		case 'm':
			return jack_osc_get_midi(buf, &arg->m);

		default:
			//TODO report error
			return buf;
	}
}

inline uint8_t *
jack_osc_set_path(uint8_t *buf, const char *path)
{
	if(!is_valid_path(path))
		return NULL; //TODO
	size_t len = jack_osc_strlen(path);
	strncpy((char *)buf, path, len);
	return buf + len;
}

inline uint8_t *
jack_osc_set_fmt(uint8_t *buf, const char *fmt)
{
	if(!is_valid_format(fmt, 0))
		return NULL; //TODO
	size_t len = jack_osc_fmtlen(fmt);
	*buf++ = ',';
	strncpy((char *)buf, fmt, len);
	return buf + len;
}

inline uint8_t *
jack_osc_set_int32(uint8_t *buf, int32_t i)
{
	swap32 s = {.i = i};
	s.u = htonl(s.u);
	*(int32_t *)buf = s.i;
	return buf + 4;
}

inline uint8_t *
jack_osc_set_float(uint8_t *buf, float f)
{
	swap32 s = {.f = f};
	s.u = htonl(s.u);
	*(float *)buf = s.f;
	return buf + 4;
}

inline uint8_t *
jack_osc_set_string(uint8_t *buf, const char *s)
{
	size_t len = jack_osc_strlen(s);
	strncpy((char *)buf, s, len);
	return buf + len;
}

inline uint8_t *
jack_osc_set_blob(uint8_t *buf, int32_t size, uint8_t *payload)
{
	size_t len = round_to_four_bytes(size);
	swap32 s = {.i = size};
	s.u = htonl(s.u);
	*(int32_t *)buf = s.i;
	buf += 4;
	memcpy(buf, payload, size);
	memset(buf+size, '\0', len-size); // zero padding
	return buf + len;
}

inline uint8_t *
jack_osc_set_int64(uint8_t *buf, int64_t h)
{
	swap64 s = {.h = h};
	s.u = htonll(s.u);
	*(int64_t *)buf = s.h;
	return buf + 8;
}

inline uint8_t *
jack_osc_set_double(uint8_t *buf, double d)
{
	swap64 s = {.d = d};
	s.u = htonll(s.u);
	*(double *)buf = s.d;
	return buf + 8;
}

inline uint8_t *
jack_osc_set_timetag(uint8_t *buf, uint64_t t)
{
	swap64 s = {.t = t};
	s.u = htonll(s.u);
	*(uint64_t *)buf = s.t;
	return buf + 8;
}

inline uint8_t *
jack_osc_set_symbol(uint8_t *buf, const char *S)
{
	size_t len = jack_osc_strlen(S);
	strncpy((char *)buf, S, len);
	return buf + len;
}

inline uint8_t *
jack_osc_set_char(uint8_t *buf, char c)
{
	buf[0] = '\0';
	buf[1] = '\0';
	buf[2] = '\0';
	buf[3] = c;
	return buf + 4;
}

inline uint8_t *
jack_osc_set_midi(uint8_t *buf, uint8_t *m)
{
	buf[0] = m[0];
	buf[1] = m[1];
	buf[2] = m[2];
	buf[3] = m[3];
	return buf + 4;
}

inline uint8_t *
jack_osc_set(char type, uint8_t *buf, Jack_OSC_Argument *arg)
{
	switch(type)
	{
		case 'i':
			return jack_osc_set_int32(buf, arg->i);
		case 'f':
			return jack_osc_set_float(buf, arg->f);
		case 's':
			return jack_osc_set_string(buf, arg->s);
		case 'b':
			return jack_osc_set_blob(buf, arg->b.size, arg->b.payload);

		case 'h':
			return jack_osc_set_int64(buf, arg->h);
		case 'd':
			return jack_osc_set_double(buf, arg->d);
		case 't':
			return jack_osc_set_timetag(buf, arg->t);

		case 'T':
		case 'F':
		case 'N':
		case 'I':
			return buf;

		case 'S':
			return jack_osc_set_symbol(buf, arg->S);
		case 'c':
			return jack_osc_set_char(buf, arg->c);
		case 'm':
			return jack_osc_set_midi(buf, arg->m);

		default:
			//TODO report error
			return buf;
	}
}

size_t
jack_osc_vararg_set(uint8_t *buf, const char *path, const char *fmt, ...)
{
	uint8_t *ptr = buf;

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
			case 'i':
				ptr = jack_osc_set_int32(ptr, va_arg(args, int32_t));
				break;
			case 'f':
				ptr = jack_osc_set_float(ptr, (float)va_arg(args, double));
				break;
			case 's':
				ptr = jack_osc_set_string(ptr, va_arg(args, char *));
				break;
			case 'b':
				ptr = jack_osc_set_blob(ptr, va_arg(args, int32_t), va_arg(args, uint8_t *));
				break;

			case 'h':
				ptr = jack_osc_set_int64(ptr, va_arg(args, int64_t));
				break;
			case 'd':
				ptr = jack_osc_set_double(ptr, va_arg(args, double));
				break;
			case 't':
				ptr = jack_osc_set_timetag(ptr, va_arg(args, uint64_t));
				break;

			case 'T':
			case 'F':
			case 'N':
			case 'I':
				break;

			case 'S':
				ptr = jack_osc_set_symbol(ptr, va_arg(args, char *));
				break;
			case 'c':
				ptr = jack_osc_set_char(ptr, (char)va_arg(args, int));
				break;
			case 'm':
				ptr = jack_osc_set_midi(ptr, va_arg(args, uint8_t *));
				break;

			default:
				//TODO report error
				break;
		}

  va_end(args);

	return ptr - buf;
}
