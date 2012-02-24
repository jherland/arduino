#!/usr/bin/env python2

import sys
import time
import Gnuplot

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


# Generate data file for Gnuplot
datafile = "./gnuplot.data"
f = open(datafile, "w")
for ts, h, t in zip(time_samples, h_samples, t_samples):
	print >>f, time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime(ts)), \
		h, t
f.close()

# Draw gnuplot graph
g = Gnuplot.Gnuplot()
g('set term svg size 1920,1080 dynamic')
g('set out "%s.svg"' % (logfile))
g('set title "Relative humidity/temperature readings from %s to %s \\n'
	'(%u samples over %.1f %s)"' % (
		time.strftime(timeformat, time.localtime(start_ts)),
		time.strftime(timeformat, time.localtime(end_ts)),
		len(time_samples), elapsed, unit))
g('set xlabel "Time"')
g('set ylabel "Humidity [%]"')
g('set y2label "Temperature [C]"')
g('set ytics nomirror')
g('set yrange [0 : 100]')
g('set ytics 10 10')
g('set mytics 2')
g('set y2range [0 : 50]')
g('set y2tics 0, 5')
g('set my2tics 5')
g('set grid x y')
g('set xdata time')
g('set timefmt "%Y-%m-%dT%H:%M:%S"')
g('plot  "%s" using 1:2 axis x1y1 title "Humidity [%%]" with lines,'
	'30 axis x1y1 title "Min. comfortable humidity (30%%)" with lines,'
	'40 axis x1y1 title "Nom. comfortable humidity (40%%)" with lines,'
	'50 axis x1y1 title "Max. comfortable humidity (50%%)" with lines,'
	'"%s" using 1:3 axis x1y2 title "Temperature [C]" with lines'
	% (datafile, datafile))
