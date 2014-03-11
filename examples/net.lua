#!/usr/local/bin/tjost -i

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

remote = 'osc.udp://localhost:3333'
host = 'osc.udp://:4444'

-- outputs
osc_out = plugin('osc_out', host)
net_out = plugin('net_out', remote)

-- inputs
osc_in = plugin('osc_in', remote, function(...)
	net_out(...)
end)

net_in = plugin('net_in', host, function(...)
	osc_out(...)
end)
