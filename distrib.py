#!/usr/bin/python3
import sys
import statistics

vals = []
for line in sys.stdin:
    val = float(line)
    vals.append(val)

total = sum(vals)
avg = sum(vals) / len(vals)
stdev = statistics.stdev(vals)
stdevp = 100 * statistics.stdev(vals) / avg

print("[cnt=%d min=%.1f max=%.1f avg=%.1f stdev=%.1f aka %.1f%%]" % (len(vals), min(vals), max(vals), avg, stdev, stdevp))
