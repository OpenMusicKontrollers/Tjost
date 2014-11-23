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
--data = tjost.plugin({name='osc_out', port='data'})
chim = tjost.plugin({name='net_out', uri='osc.udp://chimaera.local:4444'}, function(time, path, fmt, ...)
	if path == '/stream/resolve' then
		local hostname = tjost.hostname()
		chim(0, '/comm/address', 'is', id(), hostname..'.local')
	end
end)

id = require('id')
scsynth = require('scsynth_out')
midi = require('midi_out')
drum = require('drum_out')
map = require('map')

rate = 3000

control = tjost.plugin({name='send'}, function(...)
	chim(...)
end)

success = function(time, uuid, path, ...)
	local methods = {
		['/sensors/number'] = function(time, n)
			--md1.map = map_poly_step:new({n=n, oct=2, order=3})
			md1.map = map_linear:new({n=n, oct=2})
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
			chim(0, '/engines/server', 'ii', id(), 0)
			chim(0, '/engines/mode', 'is', id(), 'osc.tcp')
			chim(0, '/engines/offset', 'if', id(), 0.0025)
			chim(0, '/engines/reset', 'i', id())

			chim(0, '/engines/dummy/enabled', 'ii', id(), 1)
			chim(0, '/engines/dummy/redundancy', 'ii', id(), 0)
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

sc1 = scsynth:new({
	port = 'scsynth.1',
	inst = {'base', 'lead'}
})

md1 = midi:new({
	port = 'midi.1',
	effect = SOUND_EFFECT_5
	--effect = VOLUME
})

dr1 = drum:new({
	port = 'drum.1'
})

stream = tjost.plugin({name='net_in', uri='osc.tcp://:3333', rtprio=60, unroll='full'}, function(...)
	sc1(...)
	md1(...)
	dr1(...)
end)
--tjost.chain(stream, data)
