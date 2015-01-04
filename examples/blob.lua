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

local ffi = require('ffi')
buf_t = ffi.typeof('uint8_t *')

osc_in = tjost.plugin({name='send'}, function(time, path, fmt, ...)
	for i, v in ipairs({...}) do
		local buf = buf_t(v.raw)
		print(i, v, #v, buf, buf[0], buf[1], buf[2], buf[3])
	end
end)
