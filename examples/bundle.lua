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

loopback = tjost.plugin({name='loopback'}, function(...)
	dump(...)
	loopback(...)
end)

function newbundle()
	loopback('/bundle/push', '')
		loopback('/hello', 's', 'world')
		loopback('/bundle/push', '')
			loopback('/LAC', 'i', 2014)
			loopback('/pong', 'T')
		loopback('/bundle/pop', '')
		loopback('/ping', '')
	loopback('/bundle/pop', '')
end

newbundle()
