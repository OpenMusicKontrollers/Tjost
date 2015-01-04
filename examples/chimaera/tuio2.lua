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

message = tjost.plugin({name='dump'})
--data = tjost.plugin({name='osc_out', port='data'})

chim = {}

success = function(time, uuid, path, ...)
	local methods = {
		['/sensors/number'] = function(time, n)
			--md1.map = map_poly_step:new({n=n, oct=2, order=3})
			md1.map = map_linear:new({n=n, oct=2})
		end,

		['/engines/mode'] = function(time)
			chim(0, '/engines/enabled', 'ii', id(), 1)
		end,

		['/engines/address'] = function(time)
			chim(0, '/sensors/number', 'i', id())
			chim(0, '/sensors/rate', 'ii', id(), 3000)
			chim(0, '/sensors/group/reset', 'i', id())
			chim(0, '/sensors/group/attributes/0', 'iffiii', id(), 0.0, 1.0, 0, 1, 0)
			chim(0, '/sensors/group/attributes/1', 'iffiii', id(), 0.0, 1.0, 1, 0, 0)

			chim(0, '/engines/enabled', 'ii', id(), 0)
			chim(0, '/engines/server', 'ii', id(), 0)
			chim(0, '/engines/mode', 'is', id(), 'osc.udp')
			chim(0, '/engines/offset', 'if', id(), 0.0025)
			chim(0, '/engines/reset', 'i', id())

			chim(0, '/engines/tuio2/enabled', 'ii', id(), 1)
			chim(0, '/engines/tuio2/derivatives', 'ii', id(), 0)
		end
	}

	local cb = methods[path]
	if cb then
		cb(time, ...)
	end
end

chim = tjost.plugin({name='net_out', uri='osc.udp://chimaera.local:4444'}, function(time, path, fmt, ...)
	if path == '/success' then
		success(time, ...)
	elseif path == '/stream/resolve' then
		local hostname = tjost.hostname()
		chim(0, '/engines/address', 'is', id(), hostname..'.local:3333')
	end
end)
tjost.chain(chim, message)

id = require('id')
tuio2 = require('tuio2_fltr')
scsynth = require('scsynth_out')
midi = require('midi_out')
drum = require('drum_out')
map = require('map')

control = tjost.plugin({name='send'}, function(...)
	chim(...)
end)

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

tu1 = tuio2:new({}, function(...)
	sc1(...)
	md1(...)
	dr1(...)
end)

stream = tjost.plugin({name='net_in', uri='osc.udp://:3333', rtprio=60, unroll='full'}, tu1)
--tjost.chain(stream, data)
