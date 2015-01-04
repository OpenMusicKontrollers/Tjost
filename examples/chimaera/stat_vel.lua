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

id = require('id')

message = tjost.plugin({name='dump'})

methods = {
	['/set'] = tjost.plugin({name='write', path='stat_vel.osc'})
}

stream = tjost.plugin({name='net_in', uri='osc.udp://:3333', rtprio=60, unroll='full'}, function(time, path, ...)
	local meth = methods[path]
	if meth then
		meth(time, path, ...)
	end
end)
			
chim = tjost.plugin({name='net_out', uri='osc.udp://chimaera.local:4444'}, function(time, path, fmt, ...)
	if path == '/stream/resolve' then
		chim(0, '/engines/dummy/enabled', 'ii', id(), 1)
		chim(0, '/engines/dummy/redundancy', 'ii', id(), 0)
		chim(0, '/engines/dummy/derivatives', 'ii', id(), 1)

		local hostname = tjost.hostname()
		chim(0, '/engines/address', 'is', id(), hostname..'.local:3333')
	end
end)
tjost.chain(chim, message)
