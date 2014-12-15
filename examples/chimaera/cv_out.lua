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
