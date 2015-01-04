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

#include <iostream>
#include <vector>
#include <string>

#include <RtMidi.h>

extern "C" {
#include <stdlib.h>

#include <rtmidi_c.h>

struct _RtMidiC_In {
	RtMidiIn* ptr;
};

struct _RtMidiC_Out {
	RtMidiOut* ptr;
};

/*
 * RtMidiC_In
 */
RtMidiC_In *
rtmidic_in_new(RtMidiC_API api, const char *client_name)
{
	RtMidiC_In *dev = (RtMidiC_In *)calloc(1, sizeof(RtMidiC_In));

	RtMidi::Api _api;

	switch(api)
	{
		case RTMIDIC_API_UNSPECIFIED:
			_api = RtMidi::UNSPECIFIED;
			break;
		case RTMIDIC_API_MACOSX_CORE:
			_api = RtMidi::MACOSX_CORE;
			break;
		case RTMIDIC_API_LINUX_ALSA:
			_api = RtMidi::LINUX_ALSA;
			break;
		case RTMIDIC_API_UNIX_JACK:
			_api = RtMidi::UNIX_JACK;
			break;
		case RTMIDIC_API_WINDOWS_MM:
			_api = RtMidi::WINDOWS_MM;
			break;
		case RTMIDIC_API_RTMIDI_DUMMY:
			_api = RtMidi::RTMIDI_DUMMY;
			break;
	}


	try
	{
		std::string name(client_name);
		dev->ptr = new RtMidiIn(_api, name);
		return dev;
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		free(dev);
		return NULL;
	}
}

int
rtmidic_in_free(RtMidiC_In *dev)
{
	try
	{
		delete dev->ptr;
		free(dev);
		return 0;
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		return -1;
	}
}

RtMidiC_API
rtmidic_in_api_get(RtMidiC_In* dev)
{
	try
	{
		//return (int32_t)dev->ptr->getCurrentApi();
		RtMidi::Api _api = dev->ptr->getCurrentApi();
		switch(_api)
		{
			case RtMidi::UNSPECIFIED:
				return RTMIDIC_API_UNSPECIFIED;
			case RtMidi::MACOSX_CORE:
				return RTMIDIC_API_MACOSX_CORE;
			case RtMidi::LINUX_ALSA:
				return RTMIDIC_API_LINUX_ALSA;
			case RtMidi::UNIX_JACK:
				return RTMIDIC_API_UNIX_JACK;
			case RtMidi::WINDOWS_MM:
				return RTMIDIC_API_WINDOWS_MM;
			case RtMidi::RTMIDI_DUMMY:
				return RTMIDIC_API_RTMIDI_DUMMY;
		}
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		return (RtMidiC_API)100; //FIXME
	}

}

unsigned int
rtmidic_in_port_count(RtMidiC_In* dev)
{
	try
	{
		return dev->ptr->getPortCount();
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		return 0;
	}
}

const char *
rtmidic_in_port_name(RtMidiC_In* dev, unsigned int port_number)
{
	try
	{
		return dev->ptr->getPortName(port_number).c_str();
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		return NULL;
	}
}

int
rtmidic_in_port_open(RtMidiC_In* dev, unsigned int port_number, const char* port_name)
{
	try
	{
		std::string name(port_name); 
		dev->ptr->openPort(port_number, name);
		return 0;
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		return -1;
	}
}

int
rtmidic_in_virtual_port_open(RtMidiC_In* dev, const char* port_name)
{
	try
	{
		std::string name(port_name); 
		dev->ptr->openVirtualPort(name);
		return 0;
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		return -1;
	}
}

int
rtmidic_in_port_close(RtMidiC_In* dev)
{
	try
	{
		dev->ptr->closePort();
		return 0;
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		return -1;
	}
}

static void
callbackWrapper(double timestamp, std::vector<uint8_t> *message, void *data)
{
	uint8_t msg[4];
	for (int i = 0; i < message->size(); i++)
		msg[i] = message->at(i);
	RtMidiC_Callback *ptr = (RtMidiC_Callback *)data;
	RtMidiC_Callback cb = *ptr;
	cb(timestamp, message->size(), msg, (void **)data);
};

int
rtmidic_in_callback_set(RtMidiC_In *dev, RtMidiC_Callback *callback)
{
	try
	{
		dev->ptr->setCallback(&callbackWrapper, (void*)callback);
		return 0;
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		return -1;
	}
}

int
rtmidic_in_callback_unset(RtMidiC_In *dev )
{
	try
	{
		dev->ptr->cancelCallback();
		return 0;
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		return -1;
	}
}

/*
 * RtMidiC_Out
 */
RtMidiC_Out *
rtmidic_out_new(RtMidiC_API api, const char *client_name)
{
	RtMidiC_Out *dev = (RtMidiC_Out *)calloc(1, sizeof(RtMidiC_Out));

	RtMidi::Api _api;

	switch(api)
	{
		case RTMIDIC_API_UNSPECIFIED:
			_api = RtMidi::UNSPECIFIED;
			break;
		case RTMIDIC_API_MACOSX_CORE:
			_api = RtMidi::MACOSX_CORE;
			break;
		case RTMIDIC_API_LINUX_ALSA:
			_api = RtMidi::LINUX_ALSA;
			break;
		case RTMIDIC_API_UNIX_JACK:
			_api = RtMidi::UNIX_JACK;
			break;
		case RTMIDIC_API_WINDOWS_MM:
			_api = RtMidi::WINDOWS_MM;
			break;
		case RTMIDIC_API_RTMIDI_DUMMY:
			_api = RtMidi::RTMIDI_DUMMY;
			break;
	}
	
	try
	{
		std::string name(client_name);
		dev->ptr = new RtMidiOut(_api, name);
		return dev;
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		free(dev);
		return NULL;
	}
}

int
rtmidic_out_free(RtMidiC_Out *dev)
{
	try
	{
		delete dev->ptr;
		free(dev);
		return 0;
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		return -1;
	}
}

RtMidiC_API
rtmidic_out_api_get(RtMidiC_Out* dev)
{
	try
	{
		//return (int32_t)dev->ptr->getCurrentApi();
		RtMidi::Api _api = dev->ptr->getCurrentApi();
		switch(_api)
		{
			case RtMidi::UNSPECIFIED:
				return RTMIDIC_API_UNSPECIFIED;
			case RtMidi::MACOSX_CORE:
				return RTMIDIC_API_MACOSX_CORE;
			case RtMidi::LINUX_ALSA:
				return RTMIDIC_API_LINUX_ALSA;
			case RtMidi::UNIX_JACK:
				return RTMIDIC_API_UNIX_JACK;
			case RtMidi::WINDOWS_MM:
				return RTMIDIC_API_WINDOWS_MM;
			case RtMidi::RTMIDI_DUMMY:
				return RTMIDIC_API_RTMIDI_DUMMY;
		}
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		return (RtMidiC_API)100; //FIXME
	}
}

unsigned int
rtmidic_out_port_count(RtMidiC_Out* dev)
{
	try
	{
		return dev->ptr->getPortCount();
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		return 0;
	}
}

const char *
rtmidic_out_port_name(RtMidiC_Out* dev, unsigned int port_number)
{
	try
	{
		return dev->ptr->getPortName(port_number).c_str();
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		return NULL;
	}
}

int
rtmidic_out_port_open(RtMidiC_Out* dev, unsigned int port_number, const char* port_name)
{
	try
	{
		std::string name(port_name); 
		dev->ptr->openPort(port_number, name);
		return 0;
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		return -1;
	}
}

int
rtmidic_out_virtual_port_open(RtMidiC_Out* dev, const char* port_name)
{
	try
	{
		std::string name(port_name); 
		dev->ptr->openVirtualPort(name);
		return 0;
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		return -1;
	}
}

int
rtmidic_out_port_close(RtMidiC_Out* dev)
{
	try
	{
		dev->ptr->closePort();
		return 0;
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		return -1;
	}
}

int
rtmidic_out_send_message(RtMidiC_Out* dev, size_t len, uint8_t* message)
{
	try
	{
		std::vector<uint8_t> vec(4);
		for(size_t i = 0; i < len; i++)
			vec.push_back(message[i]);
		dev->ptr->sendMessage(&vec);
		return 0;
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		return -1;
	}
}

} // extern "C"
