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

beat = tjost.plugin('midi_out', 'drum')
--beat = tjost.plugin('osc_out', 'osc.jack://drum')

octave = 2
base = octave*0x0c

tom = {
	on = {0x02, 0x90, base+0x0b, 0x7f},
	off = {0x02, 0x80, base+0x0b, 0x00}
}

sstamp = 1
tstamp = 1
spe = nil

groove = {
	tom.on,
	false,
	tom.off,
	false,
	false,
	false,
	false,
	false
}

sequencer = coroutine.create(function()
	while true do
		for i, v in ipairs(groove) do
			if v then beat(sstamp, '/beat', 'm', v) end
			coroutine.yield()
		end
	end
end)

rhythm = coroutine.create(function()
	local last = tstamp -- last sample
	coroutine.yield()

	spe = math.floor((tstamp-last) / #groove) -- samples per eighth
	coroutine.yield()

	while true do
		local dur = tstamp-last
		local eighths = math.floor(dur/spe)
		if eights < 1 then eights = 1 end
		if eights > #groove then eights = #groove end
		spe = math.floor(dur/eighths)
		last = tstamp
		coroutine.yield()
	end
end)

methods = {
	['/trig'] = function(time, fmt, gid)
		if gid ~= 0 then return end

		tstamp = time
		coroutine.resume(rhythm)
	end
}

control = tjost.plugin('osc_in', 'osc.jack://trig', function(time, path, fmt, ...)
	local cb = methods[path]
	if cb then
		cb(time, fmt, ...)
	end
end)

loopback = tjost.plugin('loopback', function(time, path, fmt, ...)
	if spe then
		sstamp = time
		coroutine.resume(sequencer)
		loopback(time + spe, '/next', '')
	else
		loopback(0, '/keepalive', '')
	end
end)

loopback(0, '/keepalive', '')
