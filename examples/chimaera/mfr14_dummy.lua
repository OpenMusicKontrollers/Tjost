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

return function(config)
	uri = 'osc.tcp://' .. config.name .. '.local'

	message = tjost.plugin({name='dump'})

	id = require('id')
	scsynth = require('scsynth_out')
	midi = require('midi_out')
	drum = require('drum_out')

	rate = 2000
	hostname = tjost.hostname()

	success = function(time, uuid, path, ...)
		local methods = {
			['/sensors/number'] = function(time, n)
				if config.midi then
					md1.map = config.midi.map(n)
				end
			end
		}

		local cb = methods[path]
		if cb then
			cb(time, ...)
		end
	end

	chim = tjost.plugin({name='net_out', uri=uri..':4444', rtprio=50, unroll='full'}, function(time, path, fmt, ...)
		if path == '/success' then
			success(time, ...)
		elseif path == '/connect' then
			chim(0, '/sensors/number', 'i', id())
			chim(0, '/sensors/rate', 'ii', id(), rate)
			chim(0, '/sensors/group/reset', 'i', id())
			chim(0, '/sensors/group/attributes/0', 'iffiii', id(), 0.0, 1.0, 0, 1, 0)
			chim(0, '/sensors/group/attributes/1', 'iffiii', id(), 0.0, 1.0, 1, 0, 0)

			chim(0, '/engines/offset', 'if', id(), 0.0025)
			chim(0, '/engines/reset', 'i', id())
			chim(0, '/engines/dummy/enabled', 'ii', id(), 1)
			chim(0, '/engines/dummy/redundancy', 'ii', id(), 0)
		end
	end)
	tjost.chain(chim, message)
	control = tjost.plugin({name='osc_in', port='control'}, chim)

	if config.scsynth then
		sc1 = scsynth:new({
			port = 'scsynth.1',
			inst = config.scsynth.inst,
			out_offset = config.scsynth.out_offset,
			gid_offset = config.scsynth.gid_offset,
			sid_offset = config.scsynth.sid_offset
		})
	end

	if config.midi then
		md1 = midi:new({
			port = 'midi.1',
			effect = config.midi.effect,
			gid_offset = config.midi.gid_offset,
			ltable = {
				--[0] = {0, 1, 2, 3},
				--[1] = {4, 5, 6, 7}
				[0] = {0},
				[1] = {1}
			}
		})
	end

	if config.drum then
		dr1 = drum:new({
			port = 'drum.1'
		})
	end

	stream = tjost.plugin({name='net_out', uri=uri..':3333', rtprio=60, unroll='full'}, function(...)
		if sc1 then sc1(...) end
		if md1 then md1(...) end
		if dr1 then dr1(...) end
	end)

	if config.data then
		data = tjost.plugin({name='osc_out', port='data'})
		tjost.chain(stream, data)
	end
end
