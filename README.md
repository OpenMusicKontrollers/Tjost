# Tjost

## {T}jost is {J}ackified {O}pen{S}oundControl {T}ransmission

### Build status

[![Build Status](https://travis-ci.org/OpenMusicKontrollers/Tjost.svg?branch=master)](https://travis-ci.org/OpenMusicKontrollers/Tjost)

### About

### Dependencies

* [JACK](http://jackaudio.org) (Audio connection kit)
* [Lua](http://lua.org) (Lightweight embeddable language)
* [LuaJIT](http://luajit.org) (Lightning fast lightweight embeddable language)
* [libuv](https://github.com/joyent/libuv) (Lightweight event library)
* [Eina](http://enlightenment.org) (Lightweight data structures)
* [TLSF](http://tlsf.baisoku.org) (Two Level Segregated Fit real-time-safe memory allocator)

### JACK

Tjost makes use of JACK's new metadata API. However, as this is only available in JACK1, if you link to JACK2, it won't use it.

### Lua

Tjost should compile with Lua 5.1, Lua 5.2 and LuaJIT 2.0.3. We suggest to link to LuaJIT as it comes with a lot of optimizations.

LuaJIT on 64-bit platforms has custom memory allocators disabled by default because it requires all its memory to be mapped into the lower 32-bit range. As Tjost needs its own real-time-safe memory allocator to work properly (TLSF-3.0), we provide a patch (LuaJIT-2.0.3-rt.patch) to reenable custom memory allocators in LuaJIT, Tjost then will allocate memory in the lower 32-bit range only to comply with LuaJIT's requirements. 

### Build / install

	git clone git@github.com:OpenMusicKontrollers/Tjost.git Tjost
	cd Tjost
	git submodule init
	git submodule update
	mkdir build
	cd build
	cmake ..
	make
	sudo make install

### Invocation

  tjost script.lua [command line argument list]

### Utility scrits

#### tjost\_dump

Dump JACK OSC messages to stdout in human readable form.

	tjost_dump

#### tjost\_send

Inject OSC messages into JACK from stdin in human readable form.

	tjost_send

	/hello s world
	/midi m 01904a7f
	/mix ifTFINhdtc 13 1.2 45 3.4 00000000.00000001 a
	/blob b 090a7f4a37

#### tjost\_net2osc

Inject OSC from UDP/TCP into JACK.

Listen on UDP port 3333 with a realtime priority of 60 for the network thread.

	tjost_net2osc osc.udp://:3333 60

Listen on TCP port 4444 (and send to client as this is a two-way communication).

	tjost_net2osc osc.tcp://:4444 60

#### tjost\_osc2net

Eject OSC from JACK to UDP/TCP.

Send to localhost on UDP port 3333 with no scheduling delay.

	tjost_osc2net osc.udp://localhost:3333 0.0

Send to localhost on TCP port 4444 with 1ms scheduling delay (and receive from server as this is a two-way communication).

	tjost_osc2net osc.tcp://localhost:4444 0.001

#### tjost\_midi2osc

Embed MIDI from JACK MIDI in OSC MIDI.

	tjost_midi2osc

#### tjost\_osc2midi

Extract embedded MIDI in OSC to JACK MIDI.

	tjost_osc2midi

#### tjost\_raw2osc

Embed MIDI from ALSA Raw MIDI in OSC MIDI.

	tjost_raw2osc

#### tjost\_osc2raw

Extract embedded MIDI in OSC to ALSA Raw MIDI.

	tjost_osc2raw

#### tjost\_seq2osc

Embed MIDI from ALSA Sequencer MIDI in OSC MIDI.

	tjost_seq2osc

#### tjost\_osc2seq

Extract embedded MIDI in OSC to ALSA Sequencer MIDI.

	tjost_osc2seq

#### tjost\_audio2osc

Embed JACK Audio into JACK OSC.

	tjost_audio2osc

#### tjost\_osc2audio

Extract embedded audio from OSC to JACK Audio.

	tjost_osc2audio

### Scripting

(under construction)
