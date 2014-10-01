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

midi = require('midi_out')
map = require('map')
mfr14_dummy = require('mfr14_dummy')

mfr14_dummy({
	name = 'medi',

	scsynth = {
		out_offset = 6,
		gid_offset = 104,
		sid_offset = 600,
		inst = {'base3', 'lead3'}
	},

	midi = {
		--effect = SOUND_EFFECT_5,
		effect = VOLUME,
		gid_offset = 4,
		map = function(n) return map_linear:new({n=n, oct=2}) end
	},

	drum = false,
	data = false
})
