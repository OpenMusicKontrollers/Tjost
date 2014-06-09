#!/bin/env Rscript

# Copyright (c) 2014 Hanspeter Portner (dev@open-music-kontrollers.ch)
# 
# This software is provided 'as-is', without any express or implied
# warranty. In no event will the authors be held liable for any damages
# arising from the use of this software.
# 
# Permission is granted to anyone to use this software for any purpose,
# including commercial applications, and to alter it and redistribute it
# freely, subject to the following restrictions:
# 
#     1. The origin of this software must not be misrepresented; you must not
#     claim that you wrote the original software. If you use this software
#     in a product, an acknowledgment in the product documentation would be
#     appreciated but is not required.
# 
#     2. Altered source versions must be plainly marked as such, and must not be
#     misrepresented as being the original software.
# 
#     3. This notice may not be removed or altered from any source
#     distribution.

f <- xzfile('chim.osc.xz', 'rb')

head <- readChar(f, 8)
srate <- readBin(f, 'integer', 1, 4, endian='big')

print(head)
print(srate)

a <- NULL
i <- 0
while(i < 16000)
{
	time <- readBin(f, 'integer', 1, 4, endian='big')
	if(!length(time))
		break;
	size <- readBin(f, 'integer', 1, 4, endian='big')
	path <- readChar(f, 8) # '/dump000'
	fmt <- readChar(f, 4) # ',ib0'
	fid <- readBin(f, 'integer', 1, 4, endian='big')
	len <- readBin(f, 'integer', 1, 4, endian='big')
	blob <- readBin(f, 'integer', len/2, 2, endian='big')
	a <- rbind(a, c(blob))

	i <- i + 1
}

close(f)

pdf('stat.pdf')
{
	mavg <- apply(a, c(2), mean)
	msd <- apply(a, c(2), sd)

	plot(mavg, type='l')
	plot(msd, type='l')

	#apply(a, c(1), function(o) {
	#	plot(o, type='l')
	#})
}
dev.off()
