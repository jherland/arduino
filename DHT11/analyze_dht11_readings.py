#!/usr/bin/env python2

import sys
import time

timeformat = "%Y-%m-%d %H:%M:%S"

try:
	logfile = sys.argv[1]
except:
	logfile = "./dht11.log"
print "Reading samples from %s" % (logfile)

time_samples = []
h_samples = []
t_samples = []

f = open(logfile)
for line in f:
	# Lines are of the following form:
	#   2012-02-10 01:01:21: Current humdity = 20.0%  temperature = 24.0C
	line = line.strip()
	if not line or line.startswith('#'):
		continue

	ts = time.mktime(time.strptime(line[:19], timeformat))
	humid = float(line.split('%')[0].split('=')[1])
	temp = float(line.split('temperature =')[1].split('C')[0])
#	print "%s: %s%%, %sC" % (
#		time.strftime(timeformat, time.localtime(ts)), humid, temp)

	time_samples.append(ts)
	h_samples.append(humid)
	t_samples.append(temp)

f.close()

# Time
start_ts = time_samples[0]
end_ts = time_samples[-1]
elapsed = end_ts - start_ts
unit = "seconds"
if elapsed > 60:
	elapsed /= 60
	unit = "minutes"
	if elapsed > 60:
		elapsed /= 60
		unit = "hours"
		if elapsed > 24:
			elapsed /= 24
			unit = "days"
			if elapsed > 365:
				elapsed /= 365.25
				unit = "years"
			elif elapsed > 14:
				elapsed /= 7
				unit = "weeks"

# Humidity
h_max = h_min = None
h_sum = 0
for h in h_samples:
	h_sum += h
	if h < h_min or h_min is None:
		h_min = h
	if h > h_max or h_max is None:
		h_max = h
h_avg = h_sum / len(h_samples)

# Temperature
t_max = t_min = None
t_sum = 0
for t in t_samples:
	t_sum += t
	if t < t_min or t_min is None:
		t_min = t
	if t > t_max or t_max is None:
		t_max = t
t_avg = t_sum / len(t_samples)

# Output analysis
print "Read %u samples from between %s and %s (%.1f %s)" % (
	len(time_samples),
	time.strftime(timeformat, time.localtime(start_ts)),
	time.strftime(timeformat, time.localtime(end_ts)),
	elapsed, unit)
print "Min/Avg/Max humidity:    %2.1f/%2.1f/%2.1f %%" % (h_min, h_avg, h_max)
print "Min/Avg/Max temperature: %2.1f/%2.1f/%2.1f C" % (t_min, t_avg, t_max)
