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

message = tjost.plugin('dump')

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

uplink = tjost.plugin('uplink', function(time, path, fmt, ...)
	message(time, path, fmt, ...)

	local cb = methods[path]
	if cb then
		cb(time, fmt, ...)
	end
end)

clone_graph()
