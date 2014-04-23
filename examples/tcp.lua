#!/usr/local/bin/tjost -i

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

resp_out = tjost.plugin('osc_out', 'responder.tx')

tcp_in = tjost.plugin('net_in', 'osc.tcp://:3333', '60', function(...)
	resp_out(...)
end)

resp_in = tjost.plugin('osc_in', 'responder.rx', function(...)
	tcp_in(...)
end)

send_out = tjost.plugin('osc_out', 'sender.tx')

tcp_out = tjost.plugin('net_out', 'osc.tcp://localhost:3333', '0.1', function(...)
	send_out(...)
end)

send_in = tjost.plugin('osc_in', 'sender.rx', function(...)
	tcp_out(...)
end)
