#!/usr/bin/env python2

from analyze_ir_pulses import IRCommand


def main(args):
	remote, button = args
	if not remote.endswith(".commands"):
		remote += ".commands"
	f = open(remote)
	buttons = {}
	for line in f:
		name, command = line.split(":", 1)
		buttons[name] = IRCommand.from_string(command)

	try:
		cmd = buttons[button]
	except KeyError:
		print "Button '%s' does not exist in %s" % (button, remote)
		print "Available buttons:",
		print ", ".join(sorted(buttons.keys()))
		return 1

	print "const long timings[] = {",
	print ", ".join(map(str, cmd.timings())),
	print " }; // pulse lengths [usecs]"

	return 0


if __name__ == '__main__':
	import sys
	sys.exit(main(sys.argv[1:]))
