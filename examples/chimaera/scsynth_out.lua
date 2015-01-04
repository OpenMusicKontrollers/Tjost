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

	['/on'] = function(self, time, sid, gid, pid, x, y, vx, vy)
		sid = sid%self.wrap + self.sid_offset
		self.serv(0, '/s_new', 'siiiiisisi',
			self.inst[gid+1], sid, 0, gid+self.gid_offset, 4, pid, 'out', gid+self.out_offset, 'gate', 1)
		if(vx) then
			self.serv(time, '/n_setn', 'iiiffff',
				sid, 0, 4, x, y, vx, vy)
		else
			self.serv(time, '/n_setn', 'iiiff',
				sid, 0, 2, x, y)
		end
	end,

	['/off'] = function(self, time, sid)
		sid = sid%self.wrap + self.sid_offset
		self.serv(time, '/n_set', 'isi',
			sid, 'gate', 0)
	end,

	['/set'] = function(self, time, sid, x, y, vx, vy)
		sid = sid%self.wrap + self.sid_offset
		if(vx) then
			self.serv(time, '/n_setn', 'iiiffff',
				sid, 0, 4, x, y, vx, vy)
		else
			self.serv(time, '/n_setn', 'iiiff',
				sid, 0, 2, x, y)
		end
	end
})

return scsynth
