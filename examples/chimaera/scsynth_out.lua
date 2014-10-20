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

local scsynth = class:new({
	port = 'scsynth.out',
	out_offset = 0,
	gid_offset = 100,
	sid_offset = 200,
	wrap = 100,
	inst = {'inst1', 'inst2', 'inst3', 'inst4', 'inst5', 'inst6', 'inst7', 'inst8'},

	init = function(self)
		self.serv = tjost.plugin({name='osc_out', port=self.port})
	end,

	['/on'] = function(self, time, sid, gid, pid, x, y)
		sid = sid%self.wrap + self.sid_offset
		self.serv(0, '/s_new', 'siiisisi',
			self.inst[gid+1], sid, 0, gid+self.gid_offset, 'out', gid+self.out_offset, 'gate', 0)
		self.serv(time, '/n_set', 'iififiisi',
			sid, 0, x, 1, y, 2, pid, 'gate', 1)
	end,

	['/off'] = function(self, time, sid)
		sid = sid%self.wrap + self.sid_offset
		self.serv(time, '/n_set', 'isi',
			sid, 'gate', 0)
	end,

	['/set'] = function(self, time, sid, x, y)
		sid = sid%self.wrap + self.sid_offset
		self.serv(time, '/n_set', 'iifif',
			sid, 0, x, 1, y)
	end
})

return scsynth
