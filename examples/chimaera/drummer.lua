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

local ffi = require('ffi')
midi_t = ffi.typeof('uint8_t *')

beat = tjost.plugin('midi_out', 'drum')

local m = tjost.midi()
local raw = midi_t(m.raw)

octave = 2
base = octave*12

tom = {
	on = tjost.midi(),
	off = tjost.midi()
}

tom_raw = {
	on = midi_t(tom.on.raw),
	off = midi_t(tom.off.raw)
}

tom_raw.on[0] = 0x02
tom_raw.on[1] = 0x90
tom_raw.on[2] = base+11
tom_raw.on[3] = 0x7f

tom_raw.off[0] = 0x02
tom_raw.off[1] = 0x80
tom_raw.off[2] = base+11
tom_raw.off[3] = 0x00

snare = {
	on = tjost.midi(),
	off = tjost.midi()
}

snare_raw = {
	on = midi_t(snare.on.raw),
	off = midi_t(snare.off.raw)
}

snare_raw.on[0] = 0x02
snare_raw.on[1] = 0x90
snare_raw.on[2] = base+20
snare_raw.on[3] = 0x1f

snare_raw.off[0] = 0x02
snare_raw.off[1] = 0x80
snare_raw.off[2] = base+20
snare_raw.off[3] = 0x00

counter = 0
num = 4
dur = 1
state = false
last = nil

methods = {
	['/trig'] = function(time, gid)
		if gid ~= 0 then return end

		counter = counter + 1
		if state == false then
			if counter >= num then
				counter = 0
				state = true
				beat:clear()
				beat(time, '/beat', 'm', tom.on)
				beat(time, '/beat', 'm', snare.off)
				if last then
					local diff = (time-last)/num
					beat(time + diff*0.5, '/beat', 'm', snare.off)
					beat(time + diff*1.5, '/beat', 'm', snare.off)
					beat(time + diff*2.5, '/beat', 'm', snare.off)
					beat(time + diff*3.5, '/beat', 'm', snare.off)
					beat(time + diff*0.5 + 1, '/beat', 'm', snare.on)
					beat(time + diff*1.5 + 1, '/beat', 'm', snare.on)
					beat(time + diff*2.5 + 1, '/beat', 'm', snare.on)
					beat(time + diff*3.5 + 1, '/beat', 'm', snare.on)
				end
				last = time
			end
		else -- state == true
			if counter >= dur then
				counter = dur
				state = false
				beat(time, '/beat', 'm', tom.off)
			end
		end
	end,

	['/thres'] = function(time, gid)
		if gid ~= 0 then return end
		--TODO
	end
}

control = tjost.plugin('osc_in', 'osc.jack://trig', function(time, path, fmt, ...)
	local cb = methods[path]
	if cb then
		cb(time, ...)
	end
end)
