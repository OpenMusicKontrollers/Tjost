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

osc_in = 0
osc_out = 0

mt = {
	__index = function(self, path)
		rawset(self, path, function(time, fmt, ...)
			osc_out(time, path, fmt, ...)
		end)
		return rawget(self, path)
	end
}

osc_methods = {}
setmetatable(osc_methods, mt)

osc_in = plugin('osc_in', 'osc.jack://rx', function(time, path, fmt, ...)
	local method = osc_methods[path]
	if method then
		method(time, fmt, ...)
	end
end)

osc_out = plugin('osc_out', 'osc.jack://tx')

osc_stereo = 0
osc_mono = {}

osc_stereo = plugin('osc_in', 'osc.jack://stereo', function(time, path, ...)
	local method = osc_mono[path]
	if method then
		method(time, '/mono', ...)
	end
end)

osc_mono['/left'] = plugin('osc_out', 'osc.jack://left')
osc_mono['/right'] = plugin('osc_out', 'osc.jack://right')

midi_out = {}

midi_in = plugin('osc_in', 'osc.jack://midi', function(...)
	midi_out.base(...)
	midi_out.lead(...)
end)

net_out = plugin('net_out', 'osc.udp://localhost:3333')
net_in = plugin('net_in', 'osc.udp://:4444', function(...)
	print(...)
	net_out(...)
end)

midi_clk = plugin('midi_in', 'clk', function(...)
	midi_out.base(...)
	midi_out.lead(...)
	net_out(...)
end)

midi_out.base = plugin('midi_out', 'base')
midi_out.lead = plugin('midi_out', 'lead')
