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

message = tjost.plugin({name='dump'})
chim = tjost.plugin({name='net_out', uri='osc.udp://chimaera.local:4444'})

calls = {}

id = require('id')

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
					message(time, '/range', 'fff', w[1], w[2], w[3])
				end
				if k == 'values' then
					--TODO
				end
			end
		end
	end
end

function config(time, ...)
	--TODO
end

conf = tjost.plugin({name='net_in', uri='osc.udp://:4444', rtprio=50, unroll=full}, function(time, path, fmt, uuid, dest, ...)
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
