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

message = tjost.plugin({name='dump'})

graph = {}

rules = {
	{'system:capture_1', 'system:playback_1'},
	{'system:capture_2', 'system:playback_2'}
}

function update_plumbing()
	for _, v in ipairs(rules) do
		local a = v[1]
		local b = v[2]
		if graph[a] and graph[b] and (not graph[a][b]) then
			uplink(0, '/jack/connect', 'ss', a, b)
		end
	end
end

function clone_graph()
	graph = {}
	uplink(0, '/jack/ports', '')
end

methods = {
	['/jack/ports'] = function(time, fmt, ...)
		for _, v in ipairs({...}) do
			graph[v] = {}
			uplink(0, '/jack/connections', 's', v)
		end
	end, 

	['/jack/connections'] = function(time, fmt, port, ...)
		for _, v in ipairs({...}) do
			graph[port][v] = true
		end
		update_plumbing()
	end,

	['/jack/client/registration'] = function(time, fmt, name, state)
		--
	end,

	['/jack/port/registration'] = function(time, fmt, name, state)
		if state then
			graph[name] = {}
			uplink(0, '/jack/connections', 's', name)
		else
			graph[name] = nil
		end
	end,

	['/jack/port/connect'] = function(time, fmt, name_a, name_b, state)
		graph[name_a][name_b] = state

		if not state then
			update_plumbing()
		end
	end,

	['/jack/port/rename'] = function(time, fmt, name_old, name_new)
		graph[name_new] = graph[name_old]
		graph[name_old] = nil

		for k, v in pairs(graph) do
			if v[name_old] then
				v[name_new] = v[name_old]
				v[name_old] = nil
			end
		end
	end,

	['/jack/graph/order'] = function(time)
		--
	end
}

uplink = tjost.plugin({name='uplink'}, function(time, path, fmt, ...)
	message(time, path, fmt, ...)

	local cb = methods[path]
	if cb then
		cb(time, fmt, ...)
	end
end)

clone_graph()
