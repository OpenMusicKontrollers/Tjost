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

dump = tjost.plugin('dump', 'verbose')

responder = {
	none = tjost.plugin('net_in', 'osc.udp://:3331', '60', 'none', dump),
	partial = tjost.plugin('net_in', 'osc.udp://:3332', '60', 'partial', dump),
	full = tjost.plugin('net_in', 'osc.udp://:3333', '60', 'full', dump),
}

sender = {
	none = tjost.plugin('net_out', 'osc.udp://localhost:3331'),
	partial = tjost.plugin('net_out', 'osc.udp://localhost:3332'),
	full = tjost.plugin('net_out', 'osc.udp://localhost:3333')
}

loopback = tjost.plugin('loopback', function(...)
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
