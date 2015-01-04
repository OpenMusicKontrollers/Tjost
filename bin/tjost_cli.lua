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
