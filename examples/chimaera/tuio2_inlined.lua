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

message = tjost.plugin('dump')
status = tjost.plugin('osc_out', 'osc.jack://status')
--data = tjost.plugin('osc_out', 'osc.jack://data')
chim = tjost.plugin('net_out', 'osc.udp://chimaera.local:4444')
--trig = tjost.plugin('osc_out', 'osc.jack://trig')

midi = require('midi')
tuio2 = require('tuio2')

rate = 3000

control = tjost.plugin('osc_in', 'osc.jack://control', function(time, path, fmt, ...)
	chim(time, path, fmt, ...)
end)

success = function(time, uuid, path, ...)
	local methods = {
		['/sensors/number'] = function(time, n)
			local bot = 2*12 - 0.5 - (n % 18 / 6);
			local range = n/3

			midi_fltr(time, '/bottom', 'f', bot)
			midi_fltr(time, '/range', 'f', range)
			--midi_fltr(time, '/effect', 'i', 0x4a)
			midi_fltr(time, '/effect', 'i', 0x01)

			message(time, '/number', 'iff', n, bot, range)
		end,

		['/comm/address'] = function(time)
			chim(0, '/sensors/number', 'i', id())
			chim(0, '/sensors/rate', 'ii', id(), rate)
			chim(0, '/sensors/group/reset', 'i', id())
			chim(0, '/sensors/group/attributes', 'iiiffi', id(), 0, 256, 0.0, 1.0, 0)
			chim(0, '/sensors/group/attributes', 'iiiffi', id(), 1, 128, 0.0, 1.0, 0)

			chim(0, '/engines/offset', 'if', id(), 0.002)
			chim(0, '/engines/reset', 'i', id())
			chim(0, '/engines/tuio2/enabled', 'ii', id(), 1)
		end
	}

	local cb = methods[path]
	if cb then
		cb(time, ...)
	end
end

conf = tjost.plugin('net_in', 'osc.udp://:4444', function(time, path, fmt, ...)
	status(time, path, fmt, ...)
	message(time, path, fmt, ...)
	if path == '/success' then
		success(time, ...)
	end
end)

debug = tjost.plugin('net_in', 'osc.udp://:6666', function(...)
	status(...)
end)

midi_out = tjost.plugin('midi_out', 'midi')
midi_fltr = midi(midi_out)
tuio2_fltr = tuio2(midi_fltr)
stream = tjost.plugin('net_in', 'osc.udp://:3333', '60', tuio2_fltr)

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

chim(0, '/comm/address', 'is', id(), hostname..'.local')
