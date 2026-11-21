#!/usr/bin/env python3
"""Compare every text symbol whose demangled name mentions the ordered int32 adapter or pairQualifies.

Names are normalized by renaming QueryContext<QueryResult> to Context; instructions by
removing addresses, RIP displacements and call/jump targets (replaced by normalized names or offsets).
"""
import re
import subprocess
import sys
from pathlib import Path

work = Path(sys.argv[1])
ctx = ('QueryContext<QuadTree::QueryResult>', 'Context')
def norm(name):
    return name.replace(*ctx)
def symbols(exe):
    out = {}
    for line in subprocess.check_output(['nm', '-C', '-S', '--defined-only', exe], text=True).splitlines():
        f = line.split(maxsplit=3)
        if len(f) == 4 and f[2] in 'tT' and ('LibraryIndex<int, true>' in f[3] or 'pairQualifies' in f[3]):
            out.setdefault(norm(f[3]), (int(f[0], 16), int(f[1], 16)))
    return out
def body(exe, start, size):
    asm = subprocess.check_output(['objdump', '-d', '-w', '-C', '--no-show-raw-insn', f'--start-address={start}',
                                   f'--stop-address={start+size}', exe], text=True)
    rows = []
    for row in asm.splitlines():
        if not (m := re.match(r'^\s*[0-9a-f]+:\s+(.+)$', row)):
            continue
        ins = re.sub(r'0x[a-f0-9]+\(%rip\)', 'RIP', m[1].split('#')[0])
        if c := re.match(r'((?:j\w+|call)\s+)([0-9a-f]+) <(.+)>', ins):
            t = int(c[2], 16)
            ins = c[1] + (f'+{t-start:#x}' if start <= t < start + size else norm(re.sub(r'\+0x[0-9a-f]+$', '', c[3])))
        rows.append(' '.join(ins.split()))
    return rows
a, b = (symbols(work/f'{k}-build/bench/quadtree-bench') for k in ('control', 'candidate'))
print(f'{len(a)} control / {len(b)} candidate symbols; {len(a.keys() & b.keys())} shared names')
for name in sorted(a.keys() ^ b.keys()):
    print('only in', 'control' if name in a else 'candidate', name[:160])
for name in sorted(a.keys() & b.keys()):
    x, y = body(work/'control-build/bench/quadtree-bench', *a[name]), body(work/'candidate-build/bench/quadtree-bench', *b[name])
    print('same' if x == y else 'DIFFERENT', len(x), len(y), name[:160])
