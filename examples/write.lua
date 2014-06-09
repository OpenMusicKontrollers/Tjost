#!/usr/bin/env tjost

--[[
-- Copyright (c) 2014 Hanspeter Portner (dev@open-music-kontrollers.ch)
-- 
-- This software is provided 'as-is', without any express or implied
-- warranty. In no event will the authors be held liable for any damages
-- arising from the use of this software.
-- 
-- Permission is granted to anyone to use this software for any purpose,
-- including commercial applications, and to alter it and redistribute it
-- freely, subject to the following restrictions:
-- 
--     1. The origin of this software must not be misrepresented; you must not
--     claim that you wrote the original software. If you use this software
--     in a product, an acknowledgment in the product documentation would be
--     appreciated but is not required.
-- 
--     2. Altered source versions must be plainly marked as such, and must not be
--     misrepresented as being the original software.
-- 
--     3. This notice may not be removed or altered from any source
--     distribution.
--]]

local ffi = require('ffi')
ffi.cdef('int16_t htons(int16_t)')
buf_t = ffi.typeof('int16_t *')

n = 16
b = tjost.blob(2*n)
buf = buf_t(b.raw)

dump = tjost.plugin('dump', 'verbose')
write = tjost.plugin('write', 'bin.osc')

function newblob()
	for i=1, n do
		buf[i-1] = ffi.C.htons(math.random(1024))
	end
	loopback(0, '/blob', 'b', b)
end

loopback = tjost.plugin('loopback', function(time, ...)
	dump(time, ...)
	write(time, ...)
	newblob()
end)

newblob()
