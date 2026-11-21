#!/usr/bin/env python3
"""Create a diagnostic-only same-layout copy of the audited GCC benchmark ELF."""
import hashlib
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REF = HERE.parent / 'g9-home-lookahead-2026-11-20' / 'text.sha256'
ADDRESS = 0x11c45
HINT = bytes.fromhex('0f180a')    # prefetcht0 [rdx]
NOP = bytes.fromhex('0f1f00')     # three-byte architecturally inactive NOP


def section(path):
    listing = subprocess.check_output(['objdump', '-h', str(path)], text=True)
    line, = (s.split() for s in listing.splitlines() if re.match(r'^\s*\d+\s+\.text\s', s))
    return tuple(int(line[i], 16) for i in (2, 3, 5))  # size, VMA, file offset


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main(source, twin):
    if source.resolve() == twin.resolve() or twin.exists():
        raise SystemExit('choose a new file for the diagnostic twin')
    expected = next(s.split()[0] for s in REF.read_text().splitlines() if s.endswith(' candidate.text'))
    orig = source.read_bytes()
    size, vma, offset = section(source)
    pc = offset + ADDRESS - vma
    if not (orig[:6] == b'\x7fELF\x02\x01' and orig[18:20] == b'\x3e\x00' and
            offset <= pc <= offset + size - len(HINT) and len(HINT) == len(NOP) and
            digest(orig[offset:offset + size]) == expected and orig[pc:pc + len(HINT)] == HINT):
        raise SystemExit('source ELF .text or audited rank prefetch does not match')
    new = orig[:pc] + NOP + orig[pc + len(HINT):]
    with twin.open('xb') as f:
        f.write(new)
    twin.chmod(source.stat().st_mode & 0o777)
    assert section(twin) == (size, vma, offset)
    changes = [i for i, (a, b) in enumerate(zip(orig, new)) if a != b]
    assert len(orig) == len(new) and changes == [pc + 1, pc + 2]
    print(f'virtual address {ADDRESS:#x}, file offset {pc:#x}, {HINT.hex()} -> {NOP.hex()}')
    print('Whole-ELF byte differences: ' + ', '.join(hex(i) for i in changes))
    print(f'.text size {size:#x}, virtual address {vma:#x}, file offset {offset:#x} (unchanged)')
    for name, blob in (('hint', orig), ('nohint', new)):
        print(f'{digest(blob)}  {name}.elf')
        print(f'{digest(blob[offset:offset + size])}  {name}.text')


if __name__ == '__main__':
    main(*map(Path, sys.argv[1:]))
