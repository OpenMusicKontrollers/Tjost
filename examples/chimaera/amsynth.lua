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
local ALL_NOTES_OFF = 0x7b

local mpath = '/midi'
local lv2path = '/lv2/control'

local bases = {}

-- preallocate table
local m = {
	{0, 0, 0, 0},
	{0, 0, 0, 0}
}

--TODO make this configurable
local n = 144
local bot = 2*12 - 0.5 - (n/3 % 12 / 2);
local range = n/3 + 1

local midi = {
	[0] = plugin('midi_out', 'midi.base'),
	[1] = plugin('midi_out', 'midi.lead')
}

local lv2 = {
	[0] = plugin('osc_out', 'osc.jack://lv2.base'),
	[1] = plugin('osc_out', 'osc.jack://lv2.lead')
}

return {
	on = function(time, sid, gid, pid, x, y)
		local key, base, bend, eff

		key = bot + x*range
		base = math.floor(key)
		bend = (key-base)/range*0x2000 + 0x1fff

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
		lv2[gid](time, lv2path, 'if', 17, math.sqrt(y)*0.5+0.5)

		bases[sid] = base
	end,

	off = function(time, sid, gid, pid)
		local base

		base = bases[sid]

		m[1][1] = gid
		m[1][2] = 0x80
		m[1][3] = base
		m[1][4] = 0x00

		bases[sid] = nil

		midi[gid](time, mpath, 'm', m[1])
	end,

	set = function(time, sid, gid, pid, x, y)
		local key, base, bend, eff

		key = bot + x*range
		base = bases[sid]
		bend = (key-base)/range*0x2000 + 0x1fff

		m[1][1] = gid
		m[1][2] = PITCHBEND
		m[1][3] = bit32.band(bend, 0x7f)
		m[1][4] = bit32.rshift(bend, 7)

		midi[gid](time, mpath, 'm', m[1])
		lv2[gid](time, lv2path, 'if', 14, y*2-0.5)
		lv2[gid](time, lv2path, 'if', 17, math.sqrt(y)*0.5+0.5)
	end,

	idle = function(time)
		-- do nothing
	end
}
