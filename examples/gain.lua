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
buf_t = ffi.typeof('float *')

audio_out = {
	tjost.plugin('audio_out', 'audio.out.1'),
	tjost.plugin('audio_out', 'audio.out.2'),
}

gain = {
	1.0,
	1.0
}

audio_in = {
	tjost.plugin('audio_in', 'audio.in.1', function(time, path, fmt, last, typ, b)
		local buf = buf_t(b.raw)
		for i = 0, #b/4-1 do
			buf[i] = buf[i] * gain[1]
		end
		audio_out[1](time, path, fmt, last, typ, b)
	end),

	tjost.plugin('audio_in', 'audio.in.2', function(time, path, fmt, last, typ, b)
		local buf = buf_t(b.raw)
		for i = 0, #b/4-1 do
			buf[i] = buf[i] * gain[2]
		end
		audio_out[2](time, path, fmt, last, typ, b)
	end)
}

function set_gain(idx, val)
	gain[idx] = val
end

osc_in = tjost.plugin('send', function(time, path, fmt, ...)
	if path == '/gain' and fmt == 'if' then
		set_gain(...)
	end
end)
