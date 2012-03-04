#!/usr/bin/env python2

from acquire_ir_pulses import IRPulseReader
from analyze_ir_pulses import IRCommand


def collect_button(irpr):
	print "Press a button on the remote (or Ctrl+C if finished)..."
	command = None
	while True:
		try:
			timings = irpr.get_timings()
			command = IRCommand.from_timings(timings)
			break
		except Exception as e:
			if type(e) == KeyboardInterrupt:
				raise e
			print e
			print "Please retry that button"
			continue
	assert command
	print "Received %s" % (command)
	button_name = raw_input("Please name this command: ").strip()
	return (button_name, command)


def main(args):
	rc_name = raw_input("Enter a name for your remote control: ").strip()
	outf = open(rc_name + ".commands", "a")
	print "Ready to collect commands from '%s'" % (rc_name)

	irpr = IRPulseReader()
	buttons = []

	try:
		while True:
			name, command = collect_button(irpr)
			print >>outf, "%s: %s" % (name, command)
			buttons.append((name, command))

	except KeyboardInterrupt:
		pass

	outf.close()
	print "done. Here are the buttons you registered:"
	for name, command in buttons:
		print "%20s: %s" % (name, command)


if __name__ == '__main__':
	import sys
	sys.exit(main(sys.argv[1:]))
