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

resp_out = tjost.plugin({name='osc_out', port='responder.tx'})

tcp_in = tjost.plugin({name='net_in', uri='osc.tcp://:3333', rtprio=60}, resp_out)

resp_in = tjost.plugin({name='osc_in', port='responder.rx'}, tcp_in)

send_out = tjost.plugin({name='osc_out', port='sender.tx'})

tcp_out = tjost.plugin({name='net_out', uri='osc.tcp://localhost:3333', rtprio=60, offset=0.1}, send_out)

send_in = tjost.plugin({name='osc_in', port='sender.rx'}, tcp_out)
