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

local class = require('class')

local bit32 = require('bit')
local ffi = require('ffi')
midi_t = ffi.typeof('uint8_t *')

local octave = 2
local base = octave*12

local tom = {
	on = tjost.midi(),
	off = tjost.midi()
}

local tom_raw = {
	on = midi_t(tom.on.raw),
	off = midi_t(tom.off.raw)
}

channel = 0x02

tom_raw.on[0] = 0x00
tom_raw.on[1] = bit32.bor(0x90, channel)
tom_raw.on[2] = base+11
tom_raw.on[3] = 0x7f

tom_raw.off[0] = 0x00
tom_raw.off[1] = bit32.bor(0x80, channel)
tom_raw.off[2] = base+11
tom_raw.off[3] = 0x00

local snare = {
	on = tjost.midi(),
	off = tjost.midi()
}

local snare_raw = {
	on = midi_t(snare.on.raw),
	off = midi_t(snare.off.raw)
}

snare_raw.on[0] = 0x00
snare_raw.on[1] = bit32.bor(0x90, channel)
snare_raw.on[2] = base+20
snare_raw.on[3] = 0x1f

snare_raw.off[0] = 0x00
snare_raw.off[1] = bit32.bor(0x80, channel)
snare_raw.off[2] = base+20
snare_raw.off[3] = 0x00

local drum = class:new({
	port = 'drum',
	counter = 0,
	num = 4,
	dur = 1,
	state = false,
	last = nil,

	init = function(self, ...)
		self.beat = tjost.plugin({name='midi_out', port=self.port})

		self.m = tjost.midi()
		self.raw = midi_t(self.m.raw)
	end,

	['/on'] = function(self, time, sid, gid)
		if gid ~= 0 then return end

		self.counter = self.counter + 1
		if self.state == false then
			if self.counter >= self.num then
				self.counter = 0
				self.state = true
				self.beat:clear()
				self.beat(time, '/beat', 'm', tom.on)
				self.beat(time, '/beat', 'm', snare.off)
				if self.last then
					local diff = (time-self.last)/self.num
					self.beat(time + diff*0.5, '/beat', 'm', snare.off)
					self.beat(time + diff*1.5, '/beat', 'm', snare.off)
					self.beat(time + diff*2.5, '/beat', 'm', snare.off)
					self.beat(time + diff*3.5, '/beat', 'm', snare.off)
					self.beat(time + diff*0.5 + 1, '/beat', 'm', snare.on)
					self.beat(time + diff*1.5 + 1, '/beat', 'm', snare.on)
					self.beat(time + diff*2.5 + 1, '/beat', 'm', snare.on)
					self.beat(time + diff*3.5 + 1, '/beat', 'm', snare.on)
				end
				self.last = time
			end
		else -- self.state == true
			if self.counter >= self.dur then
				self.counter = self.dur
				self.state = false
				self.beat(time, '/beat', 'm', tom.off)
			end
		end
	end
})

return drum
