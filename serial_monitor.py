#!/usr/bin/env python2

import os
import fnmatch
import glob
import signal
import errno
import serial


class AnsiColors(object):
	Codes = {
	    'normal':      '0',
	    'black':       '0;30', 'dark_gray':     '1;30',
	    'red':         '0;31', 'bright_red':    '1;31',
	    'green':       '0;32', 'bright_green':  '1;32',
	    'yellow':      '0;33', 'bright_yellow': '1;33',
	    'blue':        '0;34', 'bright_blue':   '1;34',
	    'purple':      '0;35', 'bright_purple': '1;35',
	    'cyan':        '0;36', 'bright_cyan':   '1;36',
	    'bright_gray': '0;37', 'white':         '1;37',
	}

	@classmethod
	def switch(cls, color):
		return "\033[" + cls.Codes[color] + "m"

	@classmethod
	def embed(cls, color, text):
		return cls.switch(color) + text + cls.switch("normal")

for color in AnsiColors.Codes.iterkeys():
	@classmethod
	def colorizer(cls, text, color = color):
		return cls.embed(color, text)
	setattr(AnsiColors, color, colorizer)


class Lock(object):
	"""A file in the filesystem the locks a corresponding resource.

	The lock is taken in the file exists. The PID of the process
	holding the lock is written into the file."""

	def __init__(self, path):
		self.path = path

	def taken(self):
		"""Return True iff lock is already taken."""
		return os.path.exists(self.path)

	def getpid(self):
		"""Return the PID currently holding this lock.

		Returns None if this lock is currently not taken"""
		if not self.taken():
			return None
		return int(file(self.path).read().strip())

	def take(self):
		assert not self.taken()
		f = file(self.path, "w")
		print >>f, os.getpid()
		f.close()
		os.chmod(self.path, 0444)

	def release(self):
		assert self.getpid() == os.getpid() # I own this lock
		os.remove(self.path)

	def kill_holder(self):
		"""Attempt to kill the holder of this lock, and free it."""
		pid = self.getpid()
		if pid is None:
			return True # Already released
		assert pid != os.getpid(), "We already hold this lock!"
		try:
			os.kill(pid, signal.SIGTERM)
		except OSError, e:
			if e.errno != errno.ESRCH: # No such process
				return False
		# Remove the lock file manually
		try:
			os.remove(self.path)
		except OSError, e:
			if e.errno != errno.ENOENT: # No such file
				return False
		return True


class SerialPort(object):
	DevPathPatterns = [
		"/dev/ttyACM*",
		"/dev/ttyAMA*",
		"/dev/ttyUSB*",
		"/dev/ttyS*",
	]
	LockPathPrefix = "/run/lock/LCK.."

	@classmethod
	def enumerate(cls):
		for pattern in cls.DevPathPatterns:
			for path in glob.iglob(pattern):
				yield cls(path)

	def __init__(self, path):
		self.path = path
		self.name = os.path.basename(self.path)
		self.lock = Lock(self.LockPathPrefix + self.name)
		self.f = None

	def is_open(self):
		return self.f is not None

	def __repr__(self):
		state = self.is_open() and "open" or (
			self.lock.taken() and "locked" or "available")
		return "<SerialPort %s at %s (%s)>" % (
			self.name, self.path, state)

	def __cmp__(self, other):
		'''Order serial ports based on their corresponding patterns
		in DevPathPatterns, otherwise alphabetically.'''
		for p in self.DevPathPatterns:
			self_match = fnmatch.fnmatch(self.path, p)
			other_match = fnmatch.fnmatch(other.path, p)
			if self_match and other_match:
				return cmp(self.name, other.name)
			elif self_match:
				return -1
			elif other_match:
				return 1
		return cmp(self.name, other.name)

	def open(self, baudrate):
		assert not self.is_open()
		self.lock.take()
		self.f = serial.Serial(self.path, baudrate)

	def close(self):
		assert self.is_open()
		self.f.close()
		self.lock.release()
		self.f = None


def choose_serialport(default = None):
	default_is_first_available = default is None

	print "Detecting serial ports..."
	ports = sorted(SerialPort.enumerate())

	print
	default_port = None
	for i, p in enumerate(ports):
		print   AnsiColors.white("%3u)" % i), "% 8s" % p.name,
		if p.lock.taken():
			print AnsiColors.red("(LOCKED)"),
		else:
			print AnsiColors.green("(available)"),
		if (default_is_first_available and not p.lock.taken()) \
		   or (p.name == default):
			print AnsiColors.bright_cyan("(default)"),
			default_port = p
			default_is_first_available = False
		print

	print
	print "Which serial port to monitor?",
	if default_port:
		print AnsiColors.bright_cyan("[%s]" % default_port.name),
	choice = raw_input(AnsiColors.white("(0-%u): " % (len(ports) - 1)))

	if not choice and default_port:
		return default_port
	try:
		return ports[int(choice)]
	except:
		pass

	print
	print AnsiColors.bright_red("*** Invalid choice '%s'!" % choice)
	print
	return choose_serialport(default)


def choose_kill_locker(serial_port):
	pid = serial_port.lock.getpid()
	print
	print AnsiColors.bright_red(
		"*** Serial port %s is currently locked by PID %s." % (
		serial_port.name, pid))
	print
	print "Attempt to kill PID %s and claim %s for yourself?" % (
		pid, serial_port.name),
	choice = raw_input(AnsiColors.white("(y/N): "))
	if not choice or choice.lower().startswith("n"):
		return False
	elif choice.lower().startswith("y"):
		if serial_port.lock.kill_holder():
			return True
		print
		print AnsiColors.bright_red("*** Failed to free up lock!")
		print
	else:
		print
		print AnsiColors.bright_red("*** Invalid choice '%s'!" % choice)
		print
	return choose_kill_locker(serial_port)


def choose_baudrate(default = None):
	print "Available baud rates:"
	baudrates = sorted(filter(lambda b: 9600 <= b <= 115200,
		serial.baudrate_constants))

	if default is None:
		default = baudrates[-1]

	print
	for i, rate in enumerate(baudrates):
		print   AnsiColors.white("%3u)" % i), "% 8u" % rate,
		if rate == default:
			print AnsiColors.bright_cyan("(default)"),
		print

	print
	print "Which baud rate to use?",
	if default is not None:
		print AnsiColors.bright_cyan("[%u]" % default),
	choice = raw_input(AnsiColors.white("(0-%u): " % (
		len(baudrates) - 1)))

	if not choice and default is not None:
		return default
	try:
		return baudrates[int(choice)]
	except:
		try:
			if int(choice) in baudrates:
				return int(choice)
		except:
			pass

	print
	print AnsiColors.bright_red("*** Invalid choice '%s'!" % choice)
	print
	return choose_baudrate(default)


def main(args):
	serial_port = None
	baudrate = None

	# Select serial port
	while serial_port is None:
		serial_port = choose_serialport()
		if serial_port.lock.taken():
			if not choose_kill_locker(serial_port):
				serial_port = None

	# Select baud rate
	baudrate = choose_baudrate()

	print
	print AnsiColors.bright_green("--- Using %s @ %ubps" % (
		serial_port.name, baudrate))
	print

	serial_port.open(baudrate)
	try:
		while True:
			line = serial_port.f.readline().rstrip()
			print line
	finally:
		serial_port.close()
	return 0


if __name__ == '__main__':
	import sys
	try:
		sys.exit(main(sys.argv[1:]))
	except KeyboardInterrupt:
		print "Aborted!"
		sys.exit(0)
