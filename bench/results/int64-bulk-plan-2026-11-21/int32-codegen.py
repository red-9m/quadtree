#!/usr/bin/env python3
"""Compare every int32 library/adapter text symbol between WORK/control-build and WORK/candidate-build benches
after removing addresses, RIP displacements and alignment nops (local branch targets become
instruction indices; external targets by name)."""
import re
import subprocess
import sys
from pathlib import Path

work = Path(sys.argv[1])
keep = re.compile(r'Index<unsigned int, int, |LibraryIndex<int, |Runner<bench::LibraryIndex<int, ')
def symbols(exe):
    out = {}
    for line in subprocess.check_output(['nm', '-C', '-S', '--defined-only', exe], text=True).splitlines():
        f = line.split(maxsplit=3)
        if len(f) == 4 and f[2] in 'tT' and keep.search(f[3]):
            out.setdefault(f[3], (int(f[0], 16), int(f[1], 16)))
    return out
def body(exe, start, size):
    """Instructions without alignment nops; local branch targets become instruction indices."""
    asm = subprocess.check_output(['objdump', '-d', '-w', '-C', '--no-show-raw-insn', f'--start-address={start}',
                                   f'--stop-address={start + size}', exe], text=True)
    parsed = []
    for row in asm.splitlines():
        if not (m := re.match(r'^\s*([0-9a-f]+):\s+(.+)$', row)):
            continue
        ins = m[2].split('#')[0].strip()
        if re.match(r'^(?:(?:data16|cs)\s+)*(?:nop\w*|xchg\s+%ax,%ax)\b', ins):
            continue
        parsed.append((int(m[1], 16), ' '.join(re.sub(r'0x[a-f0-9]+\(%rip\)', 'RIP', ins).split())))
    index = {addr: i for i, (addr, _) in enumerate(parsed)}
    rows = []
    for addr, ins in parsed:
        if c := re.match(r'((?:j\w+|call)\s+)([0-9a-f]+) <(.+)>', ins):
            t = int(c[2], 16)
            if start <= t < start + size:
                # A target that is itself padding resolves to the next real instruction.
                nxt = min((a for a in index if a >= t), default=None)
                ins = c[1] + f'L{index[nxt]}' if nxt is not None else c[1] + 'END'
            else:
                ins = c[1] + re.sub(r'\+0x[0-9a-f]+$', '', c[3])
        rows.append(ins)
    return rows
exes = [work/f'{k}-build/bench/quadtree-bench' for k in ('control', 'candidate')]
a, b = map(symbols, exes)
print(f'{len(a)} control / {len(b)} candidate int32 symbols; {len(a.keys() & b.keys())} shared')
for name in sorted(a.keys() ^ b.keys()):
    print('only in', 'control' if name in a else 'candidate', name[:150])
diff = [n for n in sorted(a.keys() & b.keys()) if body(exes[0], *a[n]) != body(exes[1], *b[n])]
print(f'{len(diff)} differ')
for n in diff:
    print('DIFFERENT', n[:150])
