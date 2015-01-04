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

mt = {
	__index = function(self, path)
		local cb = self

		for l in path:gmatch('/(%w+)') do
			local v = rawget(cb, l)

			if v then
				cb = v 
			else
				return nil
			end
		end

		return cb
	end
}

function responder(o)
	local o = o or {}

	setmetatable(o, mt)

	return o
end

tree = responder({
	jack = {
		client = {
			registration = function(time, client, flag)

			end
		},

		port = {
			registration = function(time, port, flag)

			end,
			connect = function(time, port_a, port_b, flag)

			end,
			rename = function(time, port_old, port_new)

			end
		},

		graph = {
			order = function(time)

			end
		}
	}
})

print(tree['/jack/port/connect'])
