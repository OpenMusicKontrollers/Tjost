#!/bin/env Rscript

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

path <- 'stat_vel.osc'
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
	fmt <- readChar(f, 8) # ',iffff00'
	sid <- readBin(f, 'integer', 1, 4, endian='big')
	x <- readBin(f, 'numeric', 1, 4, endian='big')
	y <- readBin(f, 'numeric', 1, 4, endian='big')
	vx <- readBin(f, 'numeric', 1, 4, endian='big')
	vy <- readBin(f, 'numeric', 1, 4, endian='big')
	a <- rbind(a, c(x, y, vx, vy))

	i <- i + 1
}

close(f)

write.table(a, file='stat_vel.dat', quote=FALSE, row.names=FALSE, col.names=FALSE)

n <- dim(a)[1]
print(n)

pdf('stat_vel.pdf')
#svg('33_%02d.svg', width=8, height=8, pointsize=16)
	
plot(a[,1], type='l', ylim=c(0,1), col=2, xlab='Sample', ylab='x-dimension')
plot(a[,3], type='l', ylim=c(-1,1), col=2, xlab='Sample', ylab='x-velocity')
for(stiff in c(1, 4, 16, 64))
{
	print(stiff)

	s <- 1/stiff

	b <- c(0)
	for(i in 2:(n-1))
	{
		b <- c(b, s*0.5*(a[i-1, 3] + a[i, 3]) + (1-s)*b[i-1])
	}

	plot(a[,3], type='l', ylim=c(-1,1), col=2, xlab='Sample', ylab='x-velocity')
	lines(b, col=1)
}

plot(a[,2], type='l', ylim=c(0,1), col=2, xlab='Sample', ylab='z-dimension')
plot(a[,4], type='l', ylim=c(-20,20), col=2, xlab='Sample', ylab='z-velocity')
for(stiff in c(1, 4, 16, 64))
{
	print(stiff)

	s <- 1/stiff

	b <- c(0)
	for(i in 2:(n-1))
	{
		b <- c(b, s*0.5*(a[i-1, 4] + a[i, 4]) + (1-s)*b[i-1])
	}

	plot(a[,4], type='l', ylim=c(-20,20), col=2, xlab='Sample', ylab='z-velocity')
	lines(b, col=1)
}

dev.off()
