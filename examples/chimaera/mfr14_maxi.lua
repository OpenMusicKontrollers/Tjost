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

midi = require('midi_out')
map = require('map')
mfr14_dummy = require('mfr14_dummy')

mfr14_dummy({
	name = 'maxi',

	scsynth = {
		out_offset = 8,
		gid_offset = 106,
		sid_offset = 800,
		inst = {'base4', 'lead4'}
	},

	midi = {
		--effect = SOUND_EFFECT_5,
		effect = VOLUME,
		gid_offset = 6,
		map = function(n) return map_linear:new({n=n, oct=2}) end
	},

	drum = false,
	data = false
})
