/*
 * Copyright (c) 2015 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

#ifndef RTMIDI_C_H
#define RTMIDI_C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum _RtMidiC_API {
	RTMIDIC_API_UNSPECIFIED,    /*!< Search for a working compiled API. */
	RTMIDIC_API_MACOSX_CORE,    /*!< Macintosh OS-X Core Midi API. */
	RTMIDIC_API_LINUX_ALSA,     /*!< The Advanced Linux Sound Architecture API. */
	RTMIDIC_API_UNIX_JACK,      /*!< The JACK Low-Latency MIDI Server API. */
	RTMIDIC_API_WINDOWS_MM,     /*!< The Microsoft Multimedia MIDI API. */
	RTMIDIC_API_RTMIDI_DUMMY    /*!< A compilable but non-functional API. */
} RtMidiC_API;

typedef struct _RtMidiC_In RtMidiC_In;
typedef struct _RtMidiC_Out RtMidiC_Out;

typedef void (*RtMidiC_Callback)(double timestamp, size_t len, uint8_t* message, void **cb);

/*
 * RtMidiC_In
 */
RtMidiC_In *rtmidic_in_new(RtMidiC_API api, const char *client_name);
int rtmidic_in_free(RtMidiC_In *dev);

RtMidiC_API rtmidic_in_api_get(RtMidiC_In* dev);
unsigned int rtmidic_in_port_count(RtMidiC_In* dev);
const char *rtmidic_in_port_name(RtMidiC_In* dev, unsigned int port_number);

int rtmidic_in_port_open(RtMidiC_In* dev, unsigned int port_number, const char* port_name);
int rtmidic_in_virtual_port_open(RtMidiC_In* dev, const char* port_name);
int rtmidic_in_port_close(RtMidiC_In* dev);

int rtmidic_in_callback_set(RtMidiC_In *dev, RtMidiC_Callback *callback);
int rtmidic_in_callback_unset(RtMidiC_In *dev );

/*
 * RtMidiC_Out
 */
RtMidiC_Out *rtmidic_out_new(RtMidiC_API api, const char *client_name);
int rtmidic_out_free(RtMidiC_Out *dev);

RtMidiC_API rtmidic_out_api_get(RtMidiC_Out* dev);
unsigned int rtmidic_out_port_count(RtMidiC_Out* dev);
const char *rtmidic_out_port_name(RtMidiC_Out* dev, unsigned int port_number);

int rtmidic_out_port_open(RtMidiC_Out* dev, unsigned int port_number, const char* port_name);
int rtmidic_out_virtual_port_open(RtMidiC_Out* dev, const char* port_name);
int rtmidic_out_port_close(RtMidiC_Out* dev);

int rtmidic_out_send_message(RtMidiC_Out* dev, size_t len, uint8_t* message);

#ifdef __cplusplus
}
#endif

#endif // RTMIDI_C_H
