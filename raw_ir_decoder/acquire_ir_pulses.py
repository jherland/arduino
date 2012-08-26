#!/usr/bin/env python2

import serial

from analyze_ir_pulses import IRCommand, prepare_gnuplot

log_file = "./pulses.log"


class IRPulseReader(object):
	DefaultTTY = "/dev/ttyACM0"
	DefaultBaudrate = 9600

	def __init__(self, tty = DefaultTTY, baudrate = DefaultBaudrate):
		self.inf = serial.Serial(tty, baudrate)

	def read_pulses(self):
		sample = self.inf.readline().strip()

		# Lines are of the following form:
		#   Received IR command with <num> pulses: BEGIN <ints> END
		# where <num> is the number of pulses between "BEGIN" and
		# "END", and <ints> is a space-separated list of IR pulse
		# periods (in usecs), the first is the duration of a high/1
		# signal, the second is the duration of the following low/0
		# signal, end so on.
		if not sample.startswith("Received IR command"):
			return None

		start = sample.index("BEGIN ") + 6
		end = sample.index(" END")
		assert end > start
		timings = map(int, sample[start:end].split())
		return timings

	def get_timings(self):
		while True:
			timings = self.read_pulses()
			if timings:
				return timings


def main(args):
	irpr = IRPulseReader(args[0])
	timings = None
	period = None
	try:
		while True:
			timings = irpr.get_timings()
			command = IRCommand.from_timings(timings)
			print command
			period = command.fp
	except Exception as e:
		print e
	except KeyboardInterrupt:
		pass

	if not timings:
		return 0

	# Produce log
	outf = open(log_file, "w")
	for t in timings:
		print >>outf, "%u" % (t)
	outf.close()

	# Generate graph from gnuplot data
	prepare_gnuplot(log_file, period)

	return 0


if __name__ == '__main__':
	import sys
	sys.exit(main(sys.argv[1:]))
