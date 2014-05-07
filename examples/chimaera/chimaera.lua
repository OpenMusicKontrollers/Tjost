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

JSON = require('JSON')

message = tjost.plugin('dump')
chim = tjost.plugin('net_out', 'osc.udp://chimaera.local:4444')

calls = {}

id = coroutine.wrap(function()
	local i = math.random(1024)
	while true do
		i = i + 1
		coroutine.yield(i)
	end
end)

function query(time, dest, json)
	local sub = dest:sub(0, dest:find('!')-1)
	local val = JSON:decode(json)
	
	if val.type == 'node' then
		message(time, '/json', 'ss', val.type, val.description)
		
		for i, v in ipairs(val.items) do
			--message(time, dest, 'is', i, v)

			local query = sub .. v .. '!'
			calls[query] = true
			chim(0, query, 'i', id())
		end
	elseif val.type == 'method' then
		message(time, '/json', 'ss', val.type, val.description)
		
		for i, v in ipairs(val.arguments) do
			for k, w in pairs(v) do
				message(time, '/json', 'iss', i, v.description, k)
				if k == 'range' then
					message(time, '/range', 'ff', w[1], w[2])
				end
			end
		end
	end
end

function config(time, ...)
	--TODO
end

conf = tjost.plugin('net_in', 'osc.udp://:4444', function(time, path, fmt, uuid, dest, ...)
	if path == '/success' then
		if calls[dest] then
			if dest:find('!') then
				query(time, dest, ...)
			else
				config(time, dest, ...)
			end
			calls[dest] = nil
		end
	elseif path == '/fail' then
		--TODO
	end
end)

calls['/!'] = true
chim(0, '/!', 'i', id())
