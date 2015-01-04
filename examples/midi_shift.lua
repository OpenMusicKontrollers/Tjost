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

local bit32 = require('bit')
local ffi = require('ffi')
buf_t = ffi.typeof('uint8_t *')

midi_out = tjost.plugin({name='midi_out', port='midi.out'})

midi_in = tjost.plugin({name='midi_in', port='midi.in'}, function(time, path, fmt, ...)
	for _, v in ipairs({...}) do
		local m = buf_t(v.raw)

		if (bit32.band(m[1], 0xf0) == 0x90) or (bit32.band(m[1], 0xf0) == 0x80) then
			m[2] = m[2] + 12 -- shift frequency up by 1 octave
		end
	end
	midi_out(time, path, fmt, ...)
end)
