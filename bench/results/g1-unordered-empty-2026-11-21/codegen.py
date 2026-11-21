#!/usr/bin/env python3
"""Capture the real int32 adapter/runner symbols from the exact benchmark ELFs."""
import gzip
import re
import subprocess
import sys
from pathlib import Path

work,out = map(Path,sys.argv[1:3])
normalize = ('ordered_step','ordered_pairs')
if sys.argv[3:] == ['--unordered']:
    normalize += ('unordered_pairs',)
elif sys.argv[3:]:
    sys.exit('usage: codegen.py WORK OUT [--unordered]')
patterns = {
    'ordered_pairs': r'^bench::Counters bench::LibraryIndex<int, true>::pairs<\(',
    'unordered_pairs': r'^bench::Counters bench::LibraryIndex<int, false>::pairs<\(',
    'ordered_step': r'^\(anonymous namespace\)::Runner<bench::LibraryIndex<int, true>, int>::step\(',
    'unordered_step': r'^\(anonymous namespace\)::Runner<bench::LibraryIndex<int, false>, int>::step\(',
    'unordered_helper': r'^(?:QuadTree::QueryResult|void) QuadTree::Index<unsigned int, int, .*::(?:forEachNonemptyUnorderedPair|unorderedPairTraversal)<.*LibraryIndex<int, false>::pairs<',
}
ordered = {}
for kind in ('control','candidate'):
    exe = work/f'{kind}-build/bench/quadtree-bench'
    symbols = subprocess.check_output(['nm','-C','-S','--defined-only',exe],text=True)
    captured = set()
    with gzip.GzipFile(out/f'{kind}-dispatch.asm.gz','wb',mtime=0) as f:
        for line in symbols.splitlines():
            fields = line.split(maxsplit=3)
            if len(fields)!=4 or fields[2] not in ('t','T') or '[clone .cold]' in fields[3]:
                continue
            start,size = map(lambda s:int(s,16),fields[:2])
            for label,pattern in patterns.items():
                if not re.search(pattern,fields[3]):
                    continue
                captured.add(label)
                print(kind,label,hex(start),size)
                f.write(f'\n# {kind} {label} {fields[3]}\n'.encode())
                asm = subprocess.check_output(['objdump','-d','-w',f'--start-address={start}',f'--stop-address={start+size}',exe])
                f.write(asm)
                if label in normalize:
                    sequence = []
                    for row in asm.decode().splitlines():
                        if not (m := re.match(r'^\s*[a-f0-9]+:\s+(?:[a-f0-9]{2}\s+)+(.+)$', row)):
                            continue
                        ins = re.sub(r'0x[a-f0-9]+\(%rip\)', 'RIP_REL(%rip)', m[1].split('#')[0])
                        ins = ' '.join(ins.split())
                        if m := re.fullmatch(r'((?:j\w+|callq?) )([a-f0-9]+) <(.+)>', ins):
                            target = int(m[2], 16)
                            ins = m[1]+(f'{target-start:#x}' if start <= target < start+size else m[3])
                        sequence.append(ins)
                    ordered[kind,label] = sequence
        for label in patterns.keys()-captured:
            print(kind,label,'no separate symbol')
    assert {'ordered_step','unordered_step'} <= captured

for label in normalize:
    a,b = (ordered.get((kind,label),[]) for kind in ('control','candidate'))
    match = 'same' if a==b else 'different'
    print(label, f'{len(a)} -> {len(b)} instructions;', match, 'after branch/RIP displacement normalization')
