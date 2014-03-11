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

osc_out = plugin('osc_out', 'osc.jack://tx')

methods = {
	['/trigger'] = function(time, ...)
		osc_out(time, '/code', 's', 'return 0')
	end,

	['/code'] = function(time, fmt, str)
		local f = load(str)
		local val = f()

		osc_out(time, '/code', 's', 'return ' .. val .. ' + 1')
	end
}

osc_in = plugin('osc_in', 'osc.jack://rx', function(time, path, ...)
	local cb = methods[path]
	if cb then cb(time, ...) end
end)
