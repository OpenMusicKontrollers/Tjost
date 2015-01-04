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
