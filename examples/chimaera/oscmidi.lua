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
			local bot = 2*12 - 0.5 - (n % 18 / 6);
			local range = n/3

			chim(0, '/engines/oscmidi/reset', 'i', id())
			chim(0, '/engines/oscmidi/attributes/0', 'isffi', id(), 'control_change', bot, range, 0x07)
			chim(0, '/engines/oscmidi/attributes/1', 'isffi', id(), 'control_change', bot, range, 0x07)
			message(time, '/number', 'iff', n, bot, range)
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
			chim(0, '/engines/mode', 'is', id(), 'osc.tcp')
			chim(0, '/engines/offset', 'if', id(), 0.0025)
			chim(0, '/engines/reset', 'i', id())

			chim(0, '/engines/oscmidi/enabled', 'ii', id(), 1)
			chim(0, '/engines/oscmidi/multi', 'ii', id(), 1)
			chim(0, '/engines/oscmidi/format', 'is', id(), 'midi')
			chim(0, '/engines/oscmidi/path', 'is', id(), '/midi')
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
midi_out = tjost.plugin({name='midi_out', port='midi.out'})
stream = tjost.plugin({name='net_in', uri='osc.tcp://:3333', rtprio=60, unroll='full'}, midi_out)
--tjost.chain(stream, data)
