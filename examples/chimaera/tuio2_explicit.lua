#!/usr/local/bin/tjost -i

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

message = plugin('dump')
status = plugin('osc_out', 'osc.jack://status')
--data = plugin('osc_out', 'osc.jack://data')
chim = plugin('net_out', 'osc.udp://chimaera.local:4444')
trig = plugin('osc_out', 'osc.jack://trig')

midi = require('midi_explicit')
--midi = require('amsynth')

rate = 3000

control = plugin('osc_in', 'osc.jack://control', function(time, path, fmt, ...)
	chim(time, path, fmt, ...)
end)

success = function(time, uuid, path, ...)
	local methods = {
		['/sensors/number'] = function(time, n)
			local bot = 2*12 - 0.5 - (n % 18 / 6);
			local range = n/3
			midi.bot = bot
			midi.range = range
			message(time, '/number', 'iff', n, bot, range)
		end,

		['/comm/address'] = function(time)
			chim(0, '/sensors/number', 'i', id())
			chim(0, '/sensors/rate', 'ii', id(), rate)
			chim(0, '/sensors/group/reset', 'i', id())
			chim(0, '/sensors/group/attributes', 'iiiffi', id(), 0, 256, 0.0, 1.0, 0)
			chim(0, '/sensors/group/attributes', 'iiiffi', id(), 1, 128, 0.0, 1.0, 0)

			chim(0, '/engines/offset', 'if', id(), 0.002)
			chim(0, '/engines/reset', 'i', id())
			chim(0, '/engines/tuio2/enabled', 'ii', id(), 1)
		end
	}

	local cb = methods[path]
	if cb then
		cb(time, ...)
	end
end

conf = plugin('net_in', 'osc.udp://:4444', function(time, path, fmt, ...)
	status(time, path, fmt, ...)
	message(time, path, fmt, ...)
	if path == '/success' then
		success(time, ...)
	end
end)

debug = plugin('net_in', 'osc.udp://:6666', function(...)
	status(...)
end)

tstamp = 0
blobs = {}
old_blobs = nil
new_blobs = nil

ignore = false
last_fid = 0

methods = {
	['/tuio2/frm'] = function(time, fmt, fid, stamp)
		if fid > last_fid then
			ignore = false
			last_fid = fid
		else
			ignore = true
			return
		end

		old_blobs = new_blobs or {}
		
		--status(time, '/time', 'dt', time, stamp)

		return true
	end,

	['/tuio2/tok'] = function(time, fmt, sid, pid, gid, x, y, a)
		if ignore then return end

		local elmnt
		elmnt = blobs[sid] or {0, 0, 0, 0, 0, 0}
		elmnt[1] = sid
		elmnt[2] = gid
		elmnt[3] = pid
		elmnt[4] = x
		elmnt[5] = y
		elmnt[6] = a
		blobs[sid] = elmnt

		return true
	end,

	['/tuio2/alv'] = function(time, fmt, ...)
		if ignore then return end

		local v, w
		new_blobs = {...}

		if #new_blobs == 0 and #old_blobs == 0 then
			midi:idle(time)
			return
		end

		for _, v in ipairs(old_blobs) do
			local found = false
			for _, w in ipairs(new_blobs) do
				if v == w then
					found = true
					break
				end
			end
			if not found then
				local b = blobs[v]
				midi:off(time, b[1], b[2], b[3])
				blobs[v] = nil
			end
		end

		for _, w in ipairs(new_blobs) do
			local found = false
			for _, v in ipairs(old_blobs) do
				if w == v then
					found = true
					break
				end
			end
			local b = blobs[w]
			if found then
				midi:set(time, b[1], b[2], b[3], b[4], b[5], b[6])
				if b[5] > 0.8 then
					trig(time, '/thresh', 'i', b[2])
				end
			else
				midi:on(time, b[1], b[2], b[3], b[4], b[5], b[6])
				trig(time, '/trig', 'i', b[2])
			end
		end

		return true
	end
}

stream = plugin('net_in', 'osc.udp://:3333', '60', function(time, path, ...)
	--data(time, path, ...)

	local cb = methods[path]
	if cb then
		cb(time, ...)
	end
end)

id = coroutine.wrap(function()
	local i = math.random(1024)
	while true do
		i = i + 1
		coroutine.yield(i)
	end
end)

f = io.popen('hostname')
hostname = f:read('*l')
f:close()

chim(0, '/comm/address', 'is', id(), hostname..'.local')
