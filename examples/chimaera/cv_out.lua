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

local cvpath = '/cv'
local cvfmt = 'f'

local cv = class:new({
	port = 'cv.out',
	n = 1,

	init = function(self)
		self.gate = {}
		self.x = {}
		self.y = {}
		self.hash = {}

		local i
		for i=1, self.n do
			self.gate[i] = tjost.plugin({name='cv_out', port=self.port..'.gate.'..i, pretty='Gate '..i})
			self.x[i] = tjost.plugin({name='cv_out', port=self.port..'.x.'..i, pretty='Frequency '..i})
			self.y[i] = tjost.plugin({name='cv_out', port=self.port..'.y.'..i, pretty='Pressure '..i})
		end
	end,

	['/on'] = function(self, time, sid, gid, pid, x, y)
		gid = gid + 1
		self.hash[sid] = gid
		self.gate[gid](cvpath, cvfmt, 1.0)
		self.x[gid](cvpath, cvfmt, x)
		self.y[gid](cvpath, cvfmt, y)
	end,

	['/off'] = function(self, time, sid)
		local gid = self.hash[sid]
		self.hash[sid] = nil
		self.gate[gid](cvpath, cvfmt, 0.0)
	end,

	['/set'] = function(self, time, sid, x, y)
		local gid = self.hash[sid]
		self.x[gid](cvpath, cvfmt, x)
		self.y[gid](cvpath, cvfmt, y)
	end
})

return cv
