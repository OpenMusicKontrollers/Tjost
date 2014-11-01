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

local __newindex = function(self, k, v)
	print('CLI parameter <' .. k .. '> does not exist')
end

local cli = {
	new = function(self, o)
		o = o or {}
		self.__index = self
		self.__newindex = __newindex
		setmetatable(o, self)
		return o
	end,

	parse = function(self, argv)
		local conf = {}

		if argv then
			for _, v in ipairs(argv) do
				local f = loadstring('conf.' .. v)
				if f then f() end
			end
		end

		for k, v in pairs(conf) do
			self[k] = v
		end

		str = ''
		for k, v in pairs(self) do
			str = str .. k .. '=' .. v .. ' '
		end
		print(str)
	end
}

return cli
