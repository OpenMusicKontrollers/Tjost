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

local bit32 = bit32 or bit -- compatibility with Lua5.2 and LuaJIT

local PITCHBEND = 0xe0
local CONTROLLER = 0xb0

local MODULATION = 0x01
local BREATH = 0x02
local VOLUME = 0x07
local SOUND_EFFECT_5 = 0x4a
local ALL_NOTES_OFF = 0x7b

local mpath = '/midi'
local lv2path = '/lv2/control'

local bases = {}

-- preallocate table
local m = {
	{0, 0, 0, 0},
	{0, 0, 0, 0}
}

local n = 128

local midi = {
	[0] = tjost.plugin('midi_out', 'midi.base'),
	[1] = tjost.plugin('midi_out', 'midi.lead')
}

local lv2 = {
	[0] = tjost.plugin('osc_out', 'osc.jack://lv2.base'),
	[1] = tjost.plugin('osc_out', 'osc.jack://lv2.lead')
}

return {
	bot = 2*12 - 0.5 - (n % 18 / 6),
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

		m[1][1] = gid
		m[1][2] = 0x90
		m[1][3] = base
		m[1][4] = 0x7f

		m[2][1] = gid
		m[2][2] = PITCHBEND
		m[2][3] = bit32.band(bend, 0x7f)
		m[2][4] = bit32.rshift(bend, 7)

		midi[gid](time, mpath, 'mm', m[1], m[2])
		lv2[gid](time, lv2path, 'if', 14, y*2-0.5)
		--lv2[gid](time, lv2path, 'if', 17, math.sqrt(y)*0.5+0.5)

		bases[sid] = base
	end,

	off = function(self, time, sid, gid, pid)
		local base

		base = bases[sid]

		m[1][1] = gid
		m[1][2] = 0x80
		m[1][3] = base
		m[1][4] = 0x00

		bases[sid] = nil

		midi[gid](time, mpath, 'm', m[1])
	end,

	set = function(self, time, sid, gid, pid, x, y)
		local key, base, bend, eff

		key = self.bot + x*self.range
		base = bases[sid]
		bend = (key-base)/self.range*0x2000 + 0x1fff

		m[1][1] = gid
		m[1][2] = PITCHBEND
		m[1][3] = bit32.band(bend, 0x7f)
		m[1][4] = bit32.rshift(bend, 7)

		midi[gid](time, mpath, 'm', m[1])
		lv2[gid](time, lv2path, 'if', 14, y*2-0.5)
		--lv2[gid](time, lv2path, 'if', 17, math.sqrt(y)*0.5+0.5)
	end,

	idle = function(self, time)
		--[[
		m[1][1] = 0
		m[1][2] = CONTROLLER
		m[1][3] = ALL_NOTES_OFF
		m[1][4] = 0x0

		m[2][1] = 1
		m[2][2] = CONTROLLER
		m[2][3] = ALL_NOTES_OFF
		m[2][4] = 0x0
		
		midi[0](time, mpath, 'm', m[1])
		midi[1](time, mpath, 'm', m[2])
		--]]
	end
}
