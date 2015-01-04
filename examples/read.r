#!/bin/env Rscript
 
# Copyright (c) 2015 Hanspeter Portner (dev@open-music-kontrollers.ch)
#
# This is free software: you can redistribute it and/or modify
# it under the terms of the Artistic License 2.0 as published by
# The Perl Foundation.
#
# This source is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# Artistic License 2.0 for more details.
#
# You should have received a copy of the Artistic License 2.0
# along the source as a COPYING file. If not, obtain it from
# http://www.perlfoundation.org/artistic_license_2_0.

f <- xzfile('bin.osc.xz', 'rb')

head <- readChar(f, 8)
srate <- readBin(f, 'integer', 1, 4, endian='big')

print(head)
print(srate)

a <- NULL
while(TRUE)
{
	time <- readBin(f, 'integer', 1, 4, endian='big')
	if(!length(time))
		break;
	size <- readBin(f, 'integer', 1, 4, endian='big')
	path <- readChar(f, 8) # '/blob000'
	fmt <- readChar(f, 4) # ',b00'
	len <- readBin(f, 'integer', 1, 4, endian='big')
	blob <- readBin(f, 'integer', len/2, 2, endian='big')
	a <- rbind(a, c(blob))
}

close(f)

pdf('read.pdf')
{
	mavg <- apply(a, c(1), mean)
	msd <- apply(a, c(1), sd)

	plot(mavg, type='l', ylim=c(0,1024))
	plot(msd, type='l', ylim=c(0,1024))

	#apply(a, c(1), function(o) {
	#	plot(o, type='l')
	#})
}
dev.off()
