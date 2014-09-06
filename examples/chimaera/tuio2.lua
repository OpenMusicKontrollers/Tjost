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

message = tjost.plugin({name='dump'})
status = tjost.plugin({name='osc_out', port='osc.jack://status'})
--data = tjost.plugin({name='osc_out', port='osc.jack://data'})
chim = tjost.plugin({name='net_out', uri='osc.udp://chimaera.local:4444'})

id = require('id')
tuio2 = require('tuio2_fltr')
scsynth = require('scsynth_out')
midi = require('midi_out')
drum = require('drum_out')

rate = 3000

control = tjost.plugin({name='osc_in', port='osc.jack://control'}, function(time, path, fmt, ...)
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

		['/comm/address'] = function(time)
			chim(0, '/sensors/number', 'i', id())
			chim(0, '/sensors/rate', 'ii', id(), rate)
			chim(0, '/sensors/group/reset', 'i', id())
			chim(0, '/sensors/group/attributes/0', 'iffiii', id(), 0.0, 1.0, 0, 1, 0)
			chim(0, '/sensors/group/attributes/1', 'iffiii', id(), 0.0, 1.0, 1, 0, 0)

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

conf = tjost.plugin({name='net_in', uri='osc.udp://:4444', rtprio=50, unroll='full'}, function(time, path, fmt, ...)
	if path == '/success' then
		success(time, ...)
	end
end)
tjost.chain(conf, message)

debug = tjost.plugin({name='net_in', uri='osc.udp://:6666', rtprio=50, unroll='full'}, status)

sc1 = scsynth:new({
	port = 'scsynth.1',
	inst = {'base', 'lead'}
})

md1 = midi:new({
	port = 'midi.1',
	effect = SOUND_EFFECT_5
})

dr1 = drum:new({
	port = 'drum.1'
})

tu1 = tuio2:new({}, function(...)
	sc1(...)
	md1(...)
	dr1(...)
end)

stream = tjost.plugin({name='net_in', uri='osc.udp://:3333', rtprio=60, unroll='full'}, tu1)
--tjost.chain(stream, data)

hostname = tjost.hostname()
chim(0, '/comm/address', 'is', id(), hostname..'.local')
