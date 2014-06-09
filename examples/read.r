#!/bin/env Rscript

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
