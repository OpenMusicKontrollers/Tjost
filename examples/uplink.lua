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

methods = {
	-- responders
	['/jack/client/registration'] = function(time, client, flag)
		--TODO
	end,

	['/jack/port/registration'] = function(time, port, flag)
		--TODO
	end,

	['/jack/port/connect'] = function(time, port_a, port_b, flag)
		--TODO
	end,

	['/jack/port/rename'] = function(time, port_old, port_new)
		--TODO
	end,

	['/jack/graph/order'] = function(time)
		--TODO
	end,

	-- callbacks
	['/jack/ports'] = function(time, ...)
		--TODO
	end,

	['/jack/connections'] = function(time, port, ...)
		--TODO
	end
}

uplink = tjost.plugin({name='uplink'}, function(time, path, fmt, ...)
	message(time, path, fmt, ...)

	local cb = methods[path]
	if cb then
		cb(time, ...)
	end
end)

send = tjost.plugin({name='send'}, uplink)
