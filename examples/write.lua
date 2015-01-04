#!/usr/bin/env tjost

--[[
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
--]]

local ffi = require('ffi')
ffi.cdef('int16_t htons(int16_t)')
buf_t = ffi.typeof('int16_t *')

n = 16
b = tjost.blob(2*n)
buf = buf_t(b.raw)

dump = tjost.plugin({name='dump', verbose=1})
write = tjost.plugin({name='write', path='bin.osc'})

function newblob()
	for i=1, n do
		buf[i-1] = ffi.C.htons(math.random(1024))
	end
	loopback(0, '/blob', 'b', b)
end

loopback = tjost.plugin({name='loopback'}, function(time, ...)
	dump(time, ...)
	write(time, ...)
	newblob()
end)

newblob()
