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

osc_out = tjost.plugin({name='osc_out', port='osc.out'})

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

osc_in = tjost.plugin({name='osc_in', port='osc.in'}, function(time, path, ...)
	local cb = methods[path]
	if cb then cb(time, ...) end
end)
