#!/usr/bin/env python2
# -*- coding: utf8 -*-

from datetime import datetime
import Gnuplot

Debug = True

# What are the common characteristics of IR remote control signals?
#  - A series of digital pulses of varying lengths.
#  - Shortest pulses are between ~500 µsec and ~1000 µsec.
#  - Pulse lengths are always (near) an integer multiple of the shortest pulse.
#  - There are usually between ~30 and ~150 pulses in one "command".
#  - Some commands are repeated (up to 4 times).
#  - There is a relatively long delay (~15 - ~50 msec) between repetitions.
#
# As input, we get a list of timings, i.e. the (approximate) lengths of
# subsequent high and low pulses (in µsecs) seen by the IR receiver.
#
# From this list of timings, we try to determine the shortest pulse length
# (called the fundamental period - FP), and remodel all the other pulses as
# integer multiples of the FP.
#
# Pulses that are longer than MaxPulseLength, are interpreted as inter-command
# delays, and we expect the pulses following it to be a repetition of the
# preceding pulses.
#
# If successful, the end result is a representation of the remote control
# command, as a 4-tuple comprising:
#  - The fundamental period of the command (in µsec)
#  - The sequence of pulses in the command (as a string of '1's and '0's)
#  - The delay between repetitions of this command (in µsec)
#  - The number of repetitions of the command.

# Expect pulse lengths to be grouped around integer multiples of the FP.
# We need some threshold to be used in the grouping. Since we expect the FP to
# be > ~500 µsec, 200 µsec should be a fairly wide threshold while still
# preventing us from putting neighboring multiples into the wrong group.
# Note that we could have used a relative threshold here as well, but
# experiments have shown that the variance in pulse lengths is fairly constant.
# with the respect to the pulse length, so a fixed µsec threshold is likely to
# yield better results
PulseGroupThreshold = 200 # µsec

# If a single pulse is longer than this, we assume that it is not a pulse, but
# rather a pause/delay between two IR commands.
MaxPulseLength = 15000 # µsec


def debug(s):
	if Debug:
		print "(DEBUG):", s


def find_fundamental_period(timings):
	debug("find_fundamental_period(%s):" % (timings))

	# Group pulse periods that are within PulseGroupThreshold of eachother
	groups = []
	f = 0
	for p in sorted(timings):
		if p > MaxPulseLength:
			# stop when pulses are too long to be part of command
			break
		elif p <= f + PulseGroupThreshold:
			# fits into current period group
			groups[-1].append(p)
		else:
			# start new period group based on p
			f = p
			groups.append([f])

	debug("\t%u period groups/%u pulses" % (len(groups), len(timings)))

	assert len(groups) <= len(timings) / 5, "Don't like pulses/group ratio"

	debug("\tPeriod group stats:")
	gstats = [] # (min, avg, max) period for each period group
	for group in groups:
		avg = sum(group)/float(len(group))
		assert group[0] <= avg <= group[-1]
		gstats.append((group[0], avg, group[-1]))
		debug("\t\t%5u, %5.2f, %5u" % gstats[-1])

	# Assume the fundamental period is the avg from the first group
	fp = int(round(gstats[0][1]))
	debug("\tAttempting fundamental period == %u usec" % (fp))
	assert float(fp) / PulseGroupThreshold, "FP (%u) must be at least twice the grouping threshold (%u)" % (fp, PulseGroupThreshold)

	# Round averages to nearest integer multiple of fp, and verify
	# that all readings are within the PulseGroupThreshold
	for minp, avgp, maxp in gstats:
		ravg = round(avgp / float(fp))
		debug("\t\t%2u (%4u <- %4u -> %4u)" % (ravg, minp, ravg * fp, maxp))
		assert minp >= ravg * fp - PulseGroupThreshold, "%u pulse is too low for %u in %s" % (minp, ravg, timings)
		assert maxp <= ravg * fp + PulseGroupThreshold, "%u pulse is too high for %u in %s" % (maxp, ravg, timings)

	# If we haven't aborted yet, we have found an FP where all pulses are
	# an integer multiple of FP (within PulseGroupThreshold).
	return fp


def binarify_signal(fp, timings):
	"""Return a list of bitstreams corresponding to the IR commands
	found in the given timings."""
	debug("binarify_signal(%u, %s):" % (fp, timings))
	ret = [] # List of bitstreams
	bits = [] # Current bitstream (list of 1s and 0s)
	bit = 0 # Current bit
	interval = 0 # Shortest interval seen between bitstreams [usec]
	for t in timings:
		bit = (bit + 1) % 2
		if t > MaxPulseLength:
			# Start next bitstream
			ret.append(bits)
			bits = []
			if interval == 0 or t < interval:
				interval = t
			continue
		p = int(round(t / float(fp)));
		assert t >= p * fp - PulseGroupThreshold, timings
		assert t <= p * fp + PulseGroupThreshold, timings
		for i in range(p):
			bits.append(bit)
	if bits:
		ret.append(bits)
	debug("\tFound the following bitstreams:")
	assert ret
	for r in ret:
		debug("\t\t%s" % "".join([str(b) for b in r]))
		assert r
	return ret, interval


class IRCommand(object):
	@classmethod
	def from_timings(cls, timings):
		# Find the fundamental period that all pulses are an integer
		# multiple of.
		fp = find_fundamental_period(timings)

		# Divide the pulses by the fundamental period to get one or more
		# bitstreams.
		bitstreams, interval = binarify_signal(fp, timings)

		bitstream = bitstreams.pop(0)
		for bits in bitstreams:
			assert bits == bitstream

		return cls(fp, bitstream, interval, len(bitstreams) + 1)

	@classmethod
	def from_string(cls, s):
		s = s.strip()
		assert s.startswith("<IRCommand: ")
		assert s.endswith(">")
		data, repeats = s[12:-1].split("x")
		fp, bits, pause = data.strip().split("/")
		fp = int(fp)
		bits = map(int, list(bits))
		pause = int(pause.strip())
		repeats = int(repeats.strip())
		return cls(fp, bits, pause, repeats)

	def __init__(self, fp, bits, interval, repeats = 1):
		self.fp = fp
		self.bits = bits
		self.interval = interval
		self.repeats = repeats
		assert self.bits[0] == 1 and self.bits[-1] == 1, self.bits

	def __str__(self):
		return "<IRCommand: %u/%s/%u x %u>" % (self.fp,
			self.bitstream(), self.interval, self.repeats)

	def bitstream(self):
		"""Return command bitstream as a string of 1s and 0s."""
		return "".join([str(b) for b in self.bits])

	def timings(self):
		"""Return command as a list of usec timings"""
		ret = []
		prev = 1
		t = 0
		for b in self.bits:
			if b == prev:
				t += self.fp
			else:
				ret.append(t)
				t = self.fp
				prev = b
		assert prev == 1
		ret.append(t)
		ret.append(self.interval)
		return ret


def prepare_gnuplot(logfile, period = None):
	# Produce gnuplot data form logfile
	gnuplotfile = logfile + ".gnuplot"
	inf = open(logfile)
	outf = open(gnuplotfile, "w")
	print >>outf, "# Time [usec]\tPulse [1/0]"
	t = 0
	v = 1
	print >>outf, "0\t0"
	for line in inf:
		sample = int(line.strip())
		print >>outf, "%u\t%u" % (t, v)
		t += sample
		print >>outf, "%u\t%u" % (t, v)
		v = (v + 1) % 2
	print >>outf, "%u\t%u" % (t, v)
	inf.close()
	outf.close()

	# Draw gnuplot graph
	g = Gnuplot.Gnuplot()
	g('set term svg size 1920,1080 dynamic')
	g('set out "%s.svg"' % (gnuplotfile))
	g('set title "IR pulses"')
	g('set xlabel "Time [usec]"')
	g('set ylabel "Pulses [1/0]"')
	g('set yrange [-0.9 : 1.9]')
	g('set ytics 0 1')
	if period:
		g('set xtics 0 %u' % (4 * period))
		g('set mxtics 4')
	g('set grid x')
	g('plot "%s" using 1:2 title "IR pulses" with lines' % (gnuplotfile))


def main(logfile):
	print "Reading samples from %s" % (logfile)

	f = open(logfile)
	timings = map(int, f.read().split())
	f.close()

	command = IRCommand.from_timings(timings)
	print command
	debug(command.timings())

	prepare_gnuplot(logfile, command.fp)
	return 0


if __name__ == '__main__':
	import sys
	Debug = True
	sys.exit(main(sys.argv[1]))
