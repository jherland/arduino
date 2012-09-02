#!/usr/bin/env python2
# -*- coding: utf8 -*-

import sys
import math
import serial


Timer1_Max = 65536 # 16-bit timer
Timer1_Res = 16 # counts/µsec (16MHz system clock, no prescaler)


def do_pulse_stats(pulses):
	avg = sum(pulses) / float(len(pulses))
	sigsq = sum([ p ** 2 for p in pulses ]) / float(len(pulses)) - avg ** 2
	sigma = math.sqrt(sigsq)
	return (avg, sigma)


def analyze(sampler):
	samples = [] # Raw samples as printed by Arduino
	for i, s in sampler:
		assert i == len(samples)
		samples.append(s)
	print

	print "Acquired %u samples" % (len(samples))
	print

	# Samples in µsecs, first sample at 0
	usecs = []
	base = 0
	s0 = 0
	for s in samples:
		if s == 0: # This is a Timer1 overflow
			base += Timer1_Max # 16-bit counter
		else:
			cycles = base + s
			if not s0:
				s0 = cycles
			s_usec = (cycles - s0) / float(Timer1_Res)
			usecs.append(s_usec)

	# Lengths of high and low pulses in usecs
	pulses = [ usecs[i] - usecs[i - 1] for i in xrange(1, len(usecs)) ]
	assert len(pulses) + 1 == len(usecs)

	print "Registered %u pulses" % (len(pulses))
	print

	h = True
	for u, p in zip(usecs[1:], pulses):
		print " %4s pulse @ %11.3f µsecs (length %11.3f µsecs)" % (
			h and "high" or "low", u, p)
		h = not h
	print

	high_pulses = pulses[::2]
	low_pulses = pulses[1::2]
	full_pulses = [ h + l for h, l in zip(high_pulses, low_pulses) ]
	duty_cycles = [ h / (h + l) for h, l in zip(high_pulses, low_pulses) ]

	print "                            Average/Standard deviation"
	(havg, sigma) = do_pulse_stats(high_pulses)
	print "    High pulse lengths: %11.3f/%11.3f µsecs" % (havg, sigma)
	(lavg, sigma) = do_pulse_stats(low_pulses)
	print "     Low pulse lengths: %11.3f/%11.3f µsecs" % (lavg, sigma)
	(favg, sigma) = do_pulse_stats(full_pulses)
	print "High+Low pulse lengths: %11.3f/%11.3f µsecs" % (favg, sigma)
	(davg, sigma) = do_pulse_stats(duty_cycles)
	print "           Duty cycles: %11.3f/%11.3f %%" % (100 * davg, 100 * sigma)
	print
	print "Average period: %.3f µsecs (frequency: %.3f Hz)" % (
		favg, 1000000 / favg)
	print "Average duty cycle: %.1f %%" % (100 * davg)


class ArduinoSampler(object):
	DefaultTTY = "/dev/ttyACM0"
	DefaultBaudrate = 115200

	def __init__(self, tty = DefaultTTY, baudrate = DefaultBaudrate):
		self.inf = serial.Serial(tty, baudrate)

	def __iter__(self):
		# Yield (i, s) tuples where i is the sample number, and s is
		# the value of that sample.
		# Samples start after the "--BEGIN--" line, and end at the
		# "--END--" line
		state = "preamble"
		while True:
			line = self.inf.readline().strip()
			if state == "preamble" and line == "--BEGIN--":
				state = "samples"
			elif state == "samples" and line == "--END--":
				return
			elif state == "samples":
				# print line
				i, s = [ int(x) for x in line.split(':') ]
				yield (i, s)


def main(args):
	sampler = ArduinoSampler()
	analyze(sampler)
	return 0


if __name__ == '__main__':
	import sys
	sys.exit(main(sys.argv[1:]))
