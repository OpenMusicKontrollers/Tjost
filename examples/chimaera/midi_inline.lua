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

message = plugin('dump')
status = plugin('osc_out', 'osc.jack://status')
--data = plugin('osc_out', 'osc.jack://data')
chim = plugin('net_out', 'osc.udp://chimaera.local:4444')
--trig = plugin('osc_out', 'osc.jack://trig')

control = plugin('osc_in', 'osc.jack://control', function(time, path, fmt, ...)
	if path:find('/chimaera') then
		chim(time, path, fmt, ...)
	end
end)

conf = plugin('net_in', 'osc.udp://:4444', function(...)
	status(...)
	message(...)
end)

debug = plugin('net_in', 'osc.udp://:6666', function(...)
	status(...)
end)

midi_out = plugin('midi_out', 'midi')
stream = plugin('net_in', 'osc.udp://:3333', '60', midi_out)

id = coroutine.wrap(function()
	local i = math.random(1024)
	while true do
		i = i + 1
		coroutine.yield(i)
	end
end)

f = io.popen('hostname')
hostname = f:read('*l')
f:close()

n = 144
bot = 2*12 - 0.5 - (n/3 % 12 / 2);
range = n/3 + 1

rate = 3000
chim(0, '/comm/address', 'is', id(), hostname..'.local')

chim(0, '/sensors/rate', 'ii', id(), rate)
chim(0, '/sensors/group/reset', 'i', id())
chim(0, '/sensors/group/attributes', 'iiiffi', id(), 0, 256, 0.0, 1.0, 0)
chim(0, '/sensors/group/attributes', 'iiiffi', id(), 1, 128, 0.0, 1.0, 0)

chim(0, '/engines/offset', 'if', id(), 2/rate + 1e-3)
chim(0, '/engines/reset', 'i', id())
chim(0, '/engines/oscmidi/enabled', 'ii', id(), 1)
chim(0, '/engines/oscmidi/offset', 'if', id(), bot)
chim(0, '/engines/oscmidi/range', 'if', id(), range)
chim(0, '/engines/oscmidi/effect', 'ii', id(), 0x4a)
