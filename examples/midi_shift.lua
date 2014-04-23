#!/usr/local/bin/tjost -i

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
buf_t = ffi.typeof('uint8_t *')

midi_out = tjost.plugin('midi_out', 'midi.out')

midi_in = tjost.plugin('midi_in', 'midi.in', function(time, path, fmt, ...)
	for _, v in ipairs({...}) do
		local m = buf_t(v.raw)

		if (m[1] == 0x90) or (m[1] == 0x80) then
			m[2] = m[2] + 12 -- shift frequency up by 1 octave
		end
	end
	midi_out(time, path, fmt, ...)
end)
