#!/usr/bin/env python2

import serial
import time

log_file = "./dht11.log"

tty = "/dev/ttyACM0"
baudrate = 9600

outf = open(log_file, "a")

inf = serial.Serial(tty, baudrate)

try:
	while True:
		sample = inf.readline().strip()
		if not sample.startswith("Current "):
			continue
		ts = time.strftime("%Y-%m-%d %H:%M:%S")
		line = "%s: %s" % (ts, sample)
		print line
		print >>outf, line
except KeyboardInterrupt:
	pass

inf.close()
outf.close()
