#!/usr/bin/env python3
"""Wait for a cold, quiet start; stdout is the accepted Tctl in millidegrees."""
import csv
import sys
import time
from collections import deque
from pathlib import Path

sensor, log = map(Path, sys.argv[1:3])
target = int(sys.argv[3]) if len(sys.argv) > 3 else None
cpus = ('cpu', 'cpu6', 'cpu7')


def ticks():
    data = {}
    with open('/proc/stat') as f:
        for line in f:
            cpu, *fields = line.split()
            if not cpu.startswith('cpu'):
                break
            if cpu in cpus:
                values = list(map(int, fields[:8]))  # Exclude guest double-counting.
                total = sum(values)
                data[cpu] = (total - values[3] - values[4], total)
    return data


with log.open('w') as f:
    writer = csv.writer(f, lineterminator='\n')
    writer.writerow(('unix_seconds', 'tctl_millidegrees', 'host_busy_pct', 'cpu6_busy_pct', 'cpu7_busy_pct', 'accepted'))
    begin = time.monotonic()
    window = deque()
    while time.monotonic() - begin <= 300:
        now, current = time.monotonic(), ticks()
        temp = int(sensor.read_text())
        window.append((now, current))
        while len(window) > 1 and now - window[1][0] >= 10:
            window.popleft()
        busy = []
        if now - window[0][0] >= 10:
            for cpu in cpus:
                a, b = window[0][1][cpu], current[cpu]
                busy.append(100 * (b[0] - a[0]) / (b[1] - a[1]))
        ready = bool(busy and max(busy) <= 5 and temp <= 40000 and (target is None or abs(temp - target) <= 1000))
        percentages = [f'{x:.2f}' for x in busy] if busy else ['', '', '']
        writer.writerow((f'{time.time():.6f}', temp, *percentages, int(ready)))
        f.flush()
        if ready:
            print(temp)
            break
        time.sleep(2)
    else:
        sys.exit(f'Cold/quiet start condition not met in 300 s; see {log}')
