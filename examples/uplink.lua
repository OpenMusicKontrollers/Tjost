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

message = tjost.plugin('dump')

methods = {
	-- responders
	['/jack/client/registration'] = function(time, client, flag)
		--TODO
	end,

	['/jack/port/registration'] = function(time, port, flag)
		--TODO
	end,

	['/jack/port/connect'] = function(time, port_a, port_b, flag)
		--TODO
	end,

	['/jack/port/rename'] = function(time, port_old, port_new)
		--TODO
	end,

	['/jack/graph/order'] = function(time)
		--TODO
	end,

	-- callbacks
	['/jack/ports'] = function(time, ...)
		--TODO
	end,

	['/jack/connections'] = function(time, port, ...)
		--TODO
	end
}

uplink = tjost.plugin('uplink', function(time, path, fmt, ...)
	message(time, path, fmt, ...)

	local cb = methods[path]
	if cb then
		cb(time, ...)
	end
end)

osc_in = tjost.plugin('osc_in', 'osc.uplink', function(...)
	uplink(...)
end)
