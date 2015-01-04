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

dump = tjost.plugin({name='dump', verbose=1})

responder = {
	none = tjost.plugin({name='net_in', uri='osc.udp://:3331', rtprio=60, unroll='none'}, dump),
	partial = tjost.plugin({name='net_in', uri='osc.udp://:3332', rtprio=60, unroll='partial'}, dump),
	full = tjost.plugin({name='net_in', uri='osc.udp://:3333', rtprio=60, unroll='full'}, dump),
}

sender = {
	none = tjost.plugin({name='net_out', uri='osc.udp://localhost:3331'}),
	partial = tjost.plugin({name='net_out', uri='osc.udp://localhost:3332'}),
	full = tjost.plugin({name='net_out', uri='osc.udp://localhost:3333'})
}

loopback = tjost.plugin({name='loopback'}, function(...)
	sender.none(...)
	sender.partial(...)
	sender.full(...)

	--loopback(...)
end)

function newbundle()
	loopback(0)
		loopback('/hello', 's', 'world')
		loopback(0)
			loopback('/LAC', 'i', 2014)
			loopback('/pong', 'T')
		loopback()
		loopback('/ping', '')
	loopback()
end

newbundle()
