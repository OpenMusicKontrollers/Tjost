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

local map = {
	new = function(self, o, ...)
		o = o or {}
		self.__index = self
		self.__call = function(self, val)
			return self:cb(val)
		end
		setmetatable(o, self)

		self.init(o, ...)

		return o
	end,

	init = function(self)
		--
	end
}

map_linear = map:new({
	oct = 2,
	n = 128,

	cb = function(self, val)
		val = self.bot + val*self.range
		return val
	end,

	init = function(self)
		self.bot = self.oct*12 - 0.5 - (self.n % 18 / 6);
		self.range = self.n/3
	end
})

map_step = map:new({
	oct = 2,
	n = 128,

	cb = function(self, val)
		val = self.bot + val*self.range
		val = math.floor(val + 0.5) -- floor(x+0.5) == round(x)
		return val
	end,

	init = function(self)
		self.bot = self.oct*12 - 0.5 - (self.n % 18 / 6);
		self.range = self.n/3
	end
})

map_poly_step = map:new({
	oct = 2,
	n = 128,
	order = 2,

	cb = function(self, val)
		val = self.bot + val*self.range
		local ro = math.floor(val + 0.5) -- floor(x+0.5) == round(x)
		local rel = val - ro
		local pow = rel * self.ex
		for i = 2, self.order do
			pow = pow * pow
		end
		if rel < 0 then
			rel = pow * self.sign
		end
		val = ro + rel
		return val

		--[[
			val = bot*val + range
			ro = round(val)
			rel = val - ro
			ex = 2^((\frac{order-1}{order})
			val = ro + (rel * ex)^order
		]]--
	end,

	init = function(self)
		self.bot = self.oct*12 - 0.5 - (self.n % 18 / 6);
		self.range = self.n/3
		self.ex = math.pow(2, (self.order-1)/self.order)
		if math.fmod(self.order, 2) == 0 then
			self.sign = -1.0
		else
			self.sign = 1.0
		end
	end
})

map_2nd_step = map_poly_step:new({
	order = 2
})

map_3rd_step = map_poly_step:new({
	order = 3
})

map_4th_step = map_poly_step:new({
	order = 4
})

map_5th_step = map_poly_step:new({
	order = 5
})

	--[[
local test1 = map_step:new({oct=2, n=48})
local test2 = map_2nd_step:new({oct=2, n=48})
local test3 = map_3rd_step:new({oct=2, n=48})
local test4 = map_5th_step:new({oct=2, n=48})
local test5 = map_linear:new({oct=2, n=48})

for i = 0, 1, 0.001 do
	print(i, test1(i), test2(i), test3(i), test4(i), test5(i))
end
--]]

return map
