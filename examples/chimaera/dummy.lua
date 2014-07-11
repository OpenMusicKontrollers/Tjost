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
status = tjost.plugin('osc_out', 'status')
--data = tjost.plugin('osc_out', 'data')
chim = tjost.plugin('net_out', 'osc.udp://chimaera.local:4444')

midi = require('midi_explicit')
scsynth = require('scsynth_explicit')

rate = 3000

control = tjost.plugin('osc_in', 'control', function(time, path, fmt, ...)
	chim(time, path, fmt, ...)
end)

success = function(time, uuid, path, ...)
	local methods = {
		['/sensors/number'] = function(time, n)
			local bot = 2*12 - 0.5 - (n % 18 / 6);
			local range = n/3

			midi.bot = bot
			midi.range = range
			message(time, '/number', 'iff', n, bot, range)
		end,

		['/engines/mode'] = function(time)
			chim(0, '/engines/enabled', 'ii', id(), 1)
		end,
	
		['/comm/address'] = function(time)
			chim(0, '/sensors/number', 'i', id())
			chim(0, '/sensors/rate', 'ii', id(), rate)
			chim(0, '/sensors/group/reset', 'i', id())
			chim(0, '/sensors/group/attributes/0', 'iffiii', id(), 0.0, 1.0, 0, 1, 0)
			chim(0, '/sensors/group/attributes/1', 'iffiii', id(), 0.0, 1.0, 1, 0, 0)

			chim(0, '/engines/enabled', 'ii', id(), 0)
			chim(0, '/engines/mode', 'is', id(), 'osc.tcp')
			chim(0, '/engines/offset', 'if', id(), 0.002)
			chim(0, '/engines/reset', 'i', id())
			chim(0, '/engines/dummy/enabled', 'ii', id(), 1)
		end
	}

	local cb = methods[path]
	if cb then
		cb(time, ...)
	end
end

conf = tjost.plugin('net_in', 'osc.udp://:4444', '50', 'full', function(time, path, fmt, ...)
	status(time, path, fmt, ...)
	message(time, path, fmt, ...)
	if path == '/success' then
		success(time, ...)
	end
end)

debug = tjost.plugin('net_in', 'osc.udp://:6666', '50', 'full', function(...)
	status(...)
end)

methods = {
	['/on'] = function(time, fmt, ...)
		midi:on(time, ...)
		scsynth:on(time, ...)
	end,

	['/off'] = function(time, fmt, ...)
		midi:off(time, ...)
		scsynth:off(time, ...)
	end,

	['/set'] = function(time, fmt, ...)
		midi:set(time, ...)
		scsynth:set(time, ...)
	end,

	['/idle'] = function(time, fmt)
		midi:idle(time)
		scsynth:idle(time)
	end
}

stream = tjost.plugin('net_in', 'osc.tcp://:3333', '60', 'full', function(time, path, ...)
	--data(time, path, ...)

	local cb = methods[path]
	if cb then
		cb(time, ...)
	end
end)

id = coroutine.wrap(function()
	local i = math.random(1024)
	while true do
		i = i + 1
		coroutine.yield(i)
	end
end)

hostname = tjost.hostname()
chim(0, '/comm/address', 'is', id(), hostname..'.local')
