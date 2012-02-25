#!/usr/bin/env python2

import serial
from datetime import date, time, datetime, timedelta

log_file = "./eeprom.gnuplot"

tty = "/dev/ttyACM0"
baudrate = 9600

outf = open(log_file, "w")

inf = serial.Serial(tty, baudrate)
t = datetime.combine(date.today(), time(0))
dt = timedelta(minutes = 1)
state = "unknown"

header = "# Timestamp\tHumidity [%]\tTemperature [C]"
print header
print >>outf, header

while True:
	sample = inf.readline().strip()
	if state == "unknown":
		if sample == "Ready":
			state = "ready"
		continue
	elif state == "ready":
		if sample == "EEPROM dump start":
			state = "reading"
		continue
	elif state == "reading" and sample == "EEPROM dump end":
		break

	try:
		assert state == "reading", "%s" % (state)
		address, data = sample.split(":")
		address = int(address, 16)
		readings = data.split()
		assert len(readings) <= 8
		for r in readings:
			humd, temp = r.split("/")
			humd = float(humd)
			temp = float(temp)
			line = "%s\t%f\t%f" % (t.isoformat(), humd, temp)
			print line
			print >>outf, line
			t += dt
	except ValueError, AssertionError:
		pass

inf.close()
outf.close()

# Generate graph from gnuplot data
import analyze_dht11_readings
analyze_dht11_readings.main(log_file)
