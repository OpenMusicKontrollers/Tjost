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

osc_in = 0
osc_out = 0

mt = {
	__index = function(self, path)
		rawset(self, path, function(time, fmt, ...)
			osc_out(time, path, fmt, ...)
		end)
		return rawget(self, path)
	end
}

osc_methods = {}
setmetatable(osc_methods, mt)

osc_in = tjost.plugin({name='osc_in', port='osc.in'}, function(time, path, fmt, ...)
	local method = osc_methods[path]
	if method then
		method(time, fmt, ...)
	end
end)

osc_out = tjost.plugin({name='osc_out', port='osc.out'})

osc_stereo = 0
osc_mono = {}

osc_stereo = tjost.plugin({name='osc_in', port='osc.stereo'}, function(time, path, ...)
	local method = osc_mono[path]
	if method then
		method(time, '/mono', ...)
	end
end)

osc_mono['/left'] = tjost.plugin({name='osc_out', port='osc.left'})
osc_mono['/right'] = tjost.plugin({name='osc_out', port='osc.right'})

midi_out = {
	base = tjost.plugin({name='midi_out', port='base'}),
	lead = tjost.plugin({name='midi_out', port='lead'})
}

midi_in = tjost.plugin({name='osc_in', port='osc.midi'})
tjost.chain(midi_in, midi_out.base)
tjost.chain(midi_in, midi_out.lead)

net_out = tjost.plugin({name='net_out', uri='osc.udp://localhost:3333'})
net_in = tjost.plugin({name='net_in', uri='osc.udp://:4444'}, net_out)

midi_clk = tjost.plugin({name='midi_in', port='clk'})
tjost.chain(midi_clk, midi_out.base)
tjost.chain(midi_clk, midi_out.lead)
tjost.chain(midi_clk, net_out)
