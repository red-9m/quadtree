#!/usr/bin/env python3
"""Verify the benchmark's int32 unordered selfPairs call boundary."""
import csv
import re
import subprocess
import sys
from pathlib import Path

INDEX = 'QuadTree::Index<unsigned int, int, QuadTree::Unchecked, true>::'


def inspect(elf):
    nm = subprocess.check_output(['nm', '-nS', '-C', elf], text=True)
    pattern = re.compile(r'^([\da-f]+) ([\da-f]+) \w (?:(?:bool|void) )?'+re.escape(INDEX)+r'(\w+)<?(.*)$')
    symbols, calls = [], []
    for line in nm.splitlines():
        hit = pattern.fullmatch(line)
        if not hit:
            continue
        address, size, name, label = hit.groups()
        pc, length = int(address, 16), int(size, 16)
        if name == 'selfPairQualifies' or (name == 'pairQualifies' and label.startswith('QuadTree::detail::AcceptAll>')):
            symbols.append((name, hex(pc), length, pc % 64))
        if (name in ('selfPairs', 'crossPairs') and 'LibraryIndex<int, false>' in label or
                name == 'walkOrdered' and 'LibraryIndex<int, true>' in label):
            symbols.append((name, hex(pc), length, pc % 64))
        if name != 'selfPairs' or 'LibraryIndex<int, false>' not in label:
            continue
        asm = subprocess.check_output(['objdump', '-d', '-Mintel', '--no-show-raw-insn',
                                       f'--start-address={pc}', f'--stop-address={pc + length}', elf], text=True)
        dump = subprocess.check_output(['c++filt'], input=asm, text=True)
        for insn in dump.splitlines():
            call = re.match(r'^\s*([\da-f]+):\s+call\s+([\da-f]+) <(.*)>$', insn)
            if call:
                target = re.search(re.escape(INDEX)+r'(\w+)', call[3])
                calls.append((hex(int(call[1], 16)), hex(int(call[2], 16)), target[1] if target else call[3]))
    assert sum(s[0] == 'selfPairs' for s in symbols) == 1
    return symbols, calls


def main(work, out):
    symbols, calls = [], []
    for variant in ('control', 'candidate'):
        items, targets = inspect(work / f'{variant}-build/bench/quadtree-bench')
        outlined = sum(t[2] == 'selfPairQualifies' for t in targets)
        assert outlined == (0 if variant == 'control' else 2), (variant, targets)
        assert not any(t[2] == 'pairQualifies' for t in targets), (variant, targets)
        symbols.extend((variant, *row) for row in items)
        calls.extend((variant, *row) for row in targets)
    for filename, columns, rows in [('symbols.csv', 'variant symbol vma bytes vma_mod64', symbols),
                                    ('self-pair-calls.csv', 'variant call_vma target_vma target', calls)]:
        with (out / filename).open('w') as f:
            writer = csv.writer(f, lineterminator='\n')
            writer.writerow(columns.split())
            writer.writerows(rows)


if __name__ == '__main__':
    main(*map(Path, sys.argv[1:]))
