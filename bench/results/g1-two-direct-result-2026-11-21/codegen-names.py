#!/usr/bin/env python3
"""Compare captured adapter symbols after demangling call targets and renaming the owning context.

QueryContext<QueryResult> is the candidate's alias target for the control's Context, so the
mangled call targets differ even when the instruction sequence is the same.
"""
import gzip
import re
import subprocess
import sys
from pathlib import Path

out = Path(sys.argv[1])
seqs = {}
for kind in ('control', 'candidate'):
    text = gzip.open(out/f'{kind}-dispatch.asm.gz', 'rt').read()
    for section in text.split('\n# ')[1:]:
        label = section.split()[1]
        start = None
        rows = []
        for row in section.splitlines():
            if m := re.match(r'^([0-9a-f]+) <', row):
                start = int(m[1], 16)
            if not (m := re.match(r'^\s*([a-f0-9]+):\s+(?:[a-f0-9]{2}\s+)+(.+)$', row)):
                continue
            ins = re.sub(r'0x[a-f0-9]+\(%rip\)', 'RIP_REL(%rip)', m[2].split('#')[0])
            ins = ' '.join(ins.split())
            if c := re.fullmatch(r'((?:j\w+|callq?) )([a-f0-9]+) <(.+)>', ins):
                name = re.sub(r'\+0x[0-9a-f]+$', '', c[3])
                name = subprocess.run(['c++filt', name], capture_output=True, text=True).stdout.strip()
                name = name.replace('QuadTree::Index<unsigned int, int, QuadTree::Unchecked, true>::QueryContext<QuadTree::QueryResult>',
                                    'QuadTree::Index<unsigned int, int, QuadTree::Unchecked, true>::Context')
                target = int(c[2], 16)
                own = start is not None and start <= target and name.startswith('bench::Counters bench::LibraryIndex')
                ins = c[1] + (f'{target-start:#x}' if own else name)
            rows.append(ins)
        seqs[kind, label] = rows
for label in ('ordered_pairs', 'ordered_step', 'unordered_pairs', 'unordered_step'):
    a, b = seqs[('control', label)], seqs[('candidate', label)]
    print(label, f'{len(a)} -> {len(b)} instructions;', 'same' if a == b else 'different',
          'after branch/RIP normalization and Context renaming')
