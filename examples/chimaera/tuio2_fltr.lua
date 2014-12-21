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

local tuio2 = class:new({
	ignore = false,
	last_fid = 0,
	last_time = 0,

	init = function(self, cb)
		self.blobs = {}
		self.cb = cb
	end,

	['/tuio2/frm'] = function(self, time, fid, stamp, dim, src)
		if ( (fid > self.last_fid) and (time >= self.last_time) or (fid == 1) ) then
			self.ignore = false
			self.last_fid = fid
			self.last_time = time
		else
			print('tuio2_fltr: ignoring out-of-order frame #'..fid)
			self.ignore = true
			return
		end

		self.old_blobs = self.new_blobs or {}
	end,

	['/tuio2/tok'] = function(self, time, sid, pid, gid, x, y, a)
		if self.ignore then return end

		-- add blob to hash
		local elmnt
		elmnt = self.blobs[sid] or {0, 0, 0, 0, 0}
		elmnt[1] = sid
		elmnt[2] = gid
		elmnt[3] = pid
		elmnt[4] = x
		elmnt[5] = y
		-- a is ignored
		self.blobs[sid] = elmnt
	end,

	['/tuio2/alv'] = function(self, time, ...)
		if self.ignore then return end

		local v, w
		self.new_blobs = {...}

		-- are there any blobs active?
		if #self.new_blobs == 0 and #self.old_blobs == 0 then
			self.cb(time, '/idle', '')
			return
		end

		-- have any blobs disappeared?
		for _, v in ipairs(self.old_blobs) do
			local found = false
			for _, w in ipairs(self.new_blobs) do
				if v == w then
					found = true
					break
				end
			end
			if not found then
				local b = self.blobs[v]
				self.cb(time, '/off', 'i', b[1])
				self.blobs[v] = nil
			end
		end

		-- have any blobs appeared or need updating?
		for _, w in ipairs(self.new_blobs) do
			local found = false
			for _, v in ipairs(self.old_blobs) do
				if w == v then
					found = true
					break
				end
			end
			local b = self.blobs[w]
			if found then
				self.cb(time, '/set', 'iff', b[1], b[4], b[5])
			else
				self.cb(time, '/on', 'iiiff', unpack(b))
			end
		end
	end
})

return tuio2
