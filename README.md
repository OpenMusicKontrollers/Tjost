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

	git clone https://github.com/OpenMusicKontrollers/Tjost.git
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

### License

Copyright (c) 2015 Hanspeter Portner (dev@open-music-kontrollers.ch)

This is free software: you can redistribute it and/or modify
it under the terms of the Artistic License 2.0 as published by
The Perl Foundation.

This source is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
Artistic License 2.0 for more details.

You should have received a copy of the Artistic License 2.0
along the source as a COPYING file. If not, obtain it from
<http://www.perlfoundation.org/artistic_license_2_0>.
