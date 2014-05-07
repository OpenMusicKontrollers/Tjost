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

local bit32 = bit32 or bit -- compatibility with Lua5.2 and LuaJIT

local PITCHBEND = 0xe0
local CONTROLLER = 0xb0

local MODULATION = 0x01
local BREATH = 0x02
local VOLUME = 0x07
local SOUND_EFFECT_5 = 0x4a
local ALL_NOTES_OFF = 0x7b

local mpath = '/midi'

local bases = {}

-- preallocate table
local m = {
	tjost.midi(),
	tjost.midi(),
	tjost.midi(),
	tjost.midi()
}

local raw = {
	midi_t(m[1].raw),
	midi_t(m[2].raw),
	midi_t(m[3].raw),
	midi_t(m[4].raw)
}

local n = 128

local midi = {
	[0] = tjost.plugin('midi_out', 'midi.base'),
	[1] = tjost.plugin('midi_out', 'midi.lead')
}

return {
	bot = 3*12 - 0.5 - (n % 18 / 6),
	range = n/3,
	--effect = VOLUME,
	--double_precision = true,
	effect = SOUND_EFFECT_5,
	double_precision = false,

	on = function(self, time, sid, gid, pid, x, y)
		local key, base, bend, eff

		key = self.bot + x*self.range
		base = math.floor(key)
		bend = (key-base)/self.range*0x2000 + 0x1fff
		eff = y * 0x3fff

		raw[1][0] = gid
		raw[1][1] = 0x90
		raw[1][2] = base
		raw[1][3] = 0x7f

		raw[2][0] = gid
		raw[2][1] = PITCHBEND
		raw[2][2] = bit32.band(bend, 0x7f)
		raw[2][3] = bit32.rshift(bend, 7)

		if self.double_precision then
			raw[3][0] = gid
			raw[3][1] = CONTROLLER
			raw[3][2] = bit32.bor(self.effect, 0x20)
			raw[3][3] = bit32.band(eff, 0x7f)

			raw[4][0] = gid
			raw[4][1] = CONTROLLER
			raw[4][2] = self.effect
			raw[4][3] = bit32.rshift(eff, 7)

			midi[gid](time, mpath, 'mmmm', m[1], m[2], m[3], m[4])
		else
			raw[3][0] = gid
			raw[3][1] = CONTROLLER
			raw[3][2] = self.effect
			raw[3][3] = bit32.rshift(eff, 7)

			midi[gid](time, mpath, 'mmm', m[1], m[2], m[3])
		end

		bases[sid] = base
	end,

	off = function(self, time, sid, gid, pid)
		local base

		base = bases[sid]

		raw[1][0] = gid
		raw[1][1] = 0x80
		raw[1][2] = base
		raw[1][3] = 0x00

		bases[sid] = nil

		midi[gid](time, mpath, 'm', m[1])
	end,

	set = function(self, time, sid, gid, pid, x, y)
		local key, base, bend, eff

		key = self.bot + x*self.range
		base = bases[sid]
		bend = (key-base)/self.range*0x2000 + 0x1fff
		eff = y * 0x3fff

		raw[1][0] = gid
		raw[1][1] = PITCHBEND
		raw[1][2] = bit32.band(bend, 0x7f)
		raw[1][3] = bit32.rshift(bend, 7)

		if self.double_precision then
			raw[2][0] = gid
			raw[2][1] = CONTROLLER
			raw[2][2] = bit32.bor(self.effect, 0x20)
			raw[2][3] = bit32.band(eff, 0x7f)

			raw[3][0] = gid
			raw[3][1] = CONTROLLER
			raw[3][2] = self.effect
			raw[3][3] = bit32.rshift(eff, 7)

			midi[gid](time, mpath, 'mmm', m[1], m[2], m[3])
		else
			raw[2][0] = gid
			raw[2][1] = CONTROLLER
			raw[2][2] = self.effect
			raw[2][3] = bit32.rshift(eff, 7)

			midi[gid](time, mpath, 'mm', m[1], m[2])
		end
	end,

	idle = function(self, time)
		--[[
		raw[1][0] = 0
		raw[1][1] = CONTROLLER
		raw[1][2] = ALL_NOTES_OFF
		raw[1][3] = 0x0

		raw[2][0] = 1
		raw[2][1] = CONTROLLER
		raw[2][2] = ALL_NOTES_OFF
		raw[2][3] = 0x0
		
		midi[0](time, mpath, 'm', m[1])
		midi[1](time, mpath, 'm', m[2])
		--]]
	end
}
