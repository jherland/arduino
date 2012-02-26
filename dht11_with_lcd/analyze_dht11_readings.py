#!/usr/bin/env python2

from datetime import datetime
import Gnuplot

def main(logfile):
	print "Reading samples from %s" % (logfile)

	time_samples = []
	h_samples = []
	t_samples = []

	f = open(logfile)
	for line in f:
		# Lines are of the following form:
		#   <timestamp><TAB><humidity><TAB><temperature>
		line = line.strip()
		if not line or line.startswith('#'):
			continue
		ts, humd, temp = line.split("\t")
		ts = datetime.strptime(ts, "%Y-%m-%dT%H:%M:%S")
		humd = float(humd)
		temp = float(temp)

		time_samples.append(ts)
		h_samples.append(humd)
		t_samples.append(temp)

	f.close()

	# Time
	start_ts = time_samples[0]
	end_ts = time_samples[-1]
	diff = end_ts - start_ts
	elapsed = ""
	if diff.days:
		elapsed = "%u days, " % (diff.days)
	seconds = diff.seconds % 60
	minutes = (diff.seconds // 60) % 60
	hours = seconds / (60 * 60)
	elapsed += "%02u:%02u:%02u" % (hours, minutes, seconds)

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
	print "Read %u samples from between %s and %s (%s)" % (
		len(time_samples), start_ts.isoformat(" "),
		end_ts.isoformat(" "), elapsed)
	print "Min/Avg/Max humidity:    %2.2f/%2.2f/%2.2f %%" % (
		h_min, h_avg, h_max)
	print "Min/Avg/Max temperature: %2.2f/%2.2f/%2.2f C" % (
		t_min, t_avg, t_max)

	# Draw gnuplot graph
	g = Gnuplot.Gnuplot()
	g('set term svg size 1920,1080 dynamic')
	g('set out "%s.svg"' % (logfile))
	g('set title "Relative humidity/temperature readings from %s to %s \\n'
		'(%u samples over %s)"' % (
			start_ts.isoformat(" "), end_ts.isoformat(" "),
			len(time_samples), elapsed))
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
		% (logfile, logfile))


if __name__ == '__main__':
	import sys
	sys.exit(main(sys.argv[1]))
