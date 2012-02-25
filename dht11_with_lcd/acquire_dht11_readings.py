#!/usr/bin/env python2

import serial
from datetime import datetime

log_file = "./dht11.gnuplot"

tty = "/dev/ttyACM0"
baudrate = 9600

outf = open(log_file, "a")

inf = serial.Serial(tty, baudrate)

header = "# Timestamp\tHumidity [%]\tTemperature [C]"
print header
print >>outf, header

def read_sample():
	now = datetime.now().replace(microsecond = 0)
	sample = inf.readline().strip()

	# Lines are of the following form:
	#   Current runtime/humidity/temperature: 130838 ms, 22.0 %, 26.0 C
	if not sample.startswith("Current runtime/humidity/temperature:"):
		return None

	runtime, humd, temp = sample.split(":")[1].split(",")
	assert runtime.endswith(" ms")
	runtime = int(runtime.lstrip()[:-3])
	assert humd.endswith(" %")
	humd = float(humd.lstrip()[:-2])
	assert temp.endswith(" C")
	temp = float(temp.lstrip()[:-2])

	return (now, humd, temp)

try:
	while True:
		sample = read_sample()
		if sample is None:
			continue
		ts, humd, temp = sample
		line = "%s\t%f\t%f" % (ts.isoformat(), humd, temp)
		print line
		print >>outf, line
except KeyboardInterrupt:
	pass

inf.close()
outf.close()

# Generate graph from gnuplot data
import analyze_dht11_readings
analyze_dht11_readings.main(log_file)
