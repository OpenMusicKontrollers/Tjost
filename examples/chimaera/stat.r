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

pdf('stat.pdf')
#svg('33_%02d.svg', width=8, height=8, pointsize=16)

{
	f <- file('stat.dump.osc', 'rb')

	head <- readChar(f, 8)
	srate <- readBin(f, 'integer', 1, 4, endian='big')

	print(head)
	print(srate)

	a <- NULL
	i <- 0
	while(i < 3000)
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
}

{
	mavg <- apply(a, c(2), mean)
	msd <- apply(a, c(2), sd)
	print(summary(msd))

	plot(mavg, type='l', ylim=c(-5,5), col=2, xlab='Sensor', ylab='Mean')
	plot(msd, type='l', ylim=c(0,5), col=2, xlab='Sensor', ylab='Standard deviation')
}

for(path in c('stat.none.osc', 'stat.quadratic.osc', 'stat.catmullrom.osc', 'stat.lagrange.osc'))
{
	print(path)
	f <- file(path, 'rb')

	head <- readChar(f, 8)
	srate <- readBin(f, 'integer', 1, 4, endian='big')

	print(head)
	print(srate)

	a <- NULL
	i <- 0
	while(i< 48000)
	{
		time <- readBin(f, 'integer', 1, 4, endian='big')
		if(!length(time))
			break;
		size <- readBin(f, 'integer', 1, 4, endian='big')
		path <- readChar(f, 8) # '/set0000'
		fmt <- readChar(f, 8) # ',iiiff00'
		sid <- readBin(f, 'integer', 1, 4, endian='big')
		gid <- readBin(f, 'integer', 1, 4, endian='big')
		pid <- readBin(f, 'integer', 1, 4, endian='big')
		x <- readBin(f, 'numeric', 1, 4, endian='big')
		y <- readBin(f, 'numeric', 1, 4, endian='big')
		a <- rbind(a, c(x, y))

		i <- i + 1
	}

	close(f)

	len <- length(a[,1])
	s <- function(o)
	{
		return(c(mean(o), sd(o)))
	}

	dx <- abs(a[2:len,1] - a[1:(len-1),1])
	dx <- dx[dx > 0]
	bx <- log(1/dx) / log(2)
	print(s(bx))
	hist(bx, border=2, breaks=1000, xlab='x-dimension bitdepth', main='', xlim=c(5,20))

#	dz <- abs(a[2:len,2] - a[1:(len-1),2])
#	dz <- dz[dz > 0]
#	bz <- log(1/dz) / log(2)
#	print(s(bz))
#	hist(bz, border=2, breaks=1000, xlab='z-dimension bitdepth', main='', xlim=c(5,20))

	plot(a[,1], type='l', ylim=c(0,1), col=2, xlab='Sample', ylab='x-dimension')
	plot(a[,2], type='l', ylim=c(0,1), col=2, xlab='Sample', ylab='z-dimension')
	plot(a[,2] ~ a[,1], type='l', xlim=c(0,1), ylim=c(0,1), col=2, xlab='x-dimension', ylab='z-dimension')
}

dev.off()
