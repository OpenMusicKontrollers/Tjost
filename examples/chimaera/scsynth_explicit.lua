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

local counter = 0

return {
	gid_offset = 100,
	sid_offset = 200,

	serv = tjost.plugin('osc_out', 'scsynth.out'),

	inst = {
		'base',
		'lead'
	},

	on = function(self, time, sid, gid, pid, x, y)
		sid = sid + self.sid_offset
		self.serv(0, '/s_new', 'siiisisi',
			self.inst[gid+1], sid, gid+self.gid_offset, 0, 'out', gid, 'gate', 0)
		self.serv(time, '/n_set', 'iififsi',
			sid, 0, x, 1, y, 'gate', 1)
	end,

	off = function(self, time, sid, gid, pid)
		sid = sid + self.sid_offset
		self.serv(time, '/n_set', 'isi',
			sid, 'gate', 0)
	end,

	set = function(self, time, sid, gid, pid, x, y)
		sid = sid + self.sid_offset
		self.serv(time, '/n_set', 'iifif',
			sid, 0, x, 1, y)
	end,

	idle = function(self, time)
		--
	end
}
