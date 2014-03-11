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
