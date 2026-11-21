#!/usr/bin/env python3
"""Diagnostic copy: route the two audited calls directly past a 5-byte jump."""
import runpy
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
TWIN = runpy.run_path(str(HERE.parent / 'g9-rank-hint-ablation-2026-11-20/make-twin.py'))
section, digest = TWIN['section'], TWIN['digest']
CALLS = (0x9b631, 0x9b786)
THUNK, TARGET = 0x11ea0, 0x11d80


def rel32(op, pc, target):
    return bytes([op]) + struct.pack('<i', target - pc - 5)


def main(src, twin):
    if src.resolve() == twin.resolve() or twin.exists():
        raise SystemExit('choose a new file for the diagnostic twin')
    expected = next(s.split()[0] for s in (HERE / 'text.sha256').read_text().splitlines() if s.endswith(' candidate.text'))
    size, vma, off = section(src)
    orig = src.read_bytes()
    if (orig[:6] != b'\x7fELF\x02\x01' or orig[18:20] != b'\x3e\x00' or
            digest(orig[off:off + size]) != expected or not all(vma <= pc <= vma + size - 5 for pc in (*CALLS, THUNK, TARGET))):
        raise SystemExit('unexpected candidate ELF')
    stub = off + THUNK - vma
    if orig[stub:stub+5] != rel32(0xe9, THUNK, TARGET):
        raise SystemExit('audited helper is not an unconditional jump to pairQualifies')
    new = bytearray(orig)
    expected_offsets = set()
    for pc in CALLS:
        i = off + pc - vma
        if orig[i:i+5] != rel32(0xe8, pc, THUNK):
            raise SystemExit(f'audited call differs at {pc:#x}')
        new[i:i+5] = rel32(0xe8, pc, TARGET)
        expected_offsets.update(range(i + 1, i + 5))
    changed = [i for i, (a, b) in enumerate(zip(orig, new)) if a != b]
    assert len(orig) == len(new) and changed and set(changed) <= expected_offsets
    with twin.open('xb') as f:
        f.write(new)
    twin.chmod(src.stat().st_mode & 0o777)
    assert section(twin) == (size, vma, off)
    for pc in CALLS:
        print(f'{pc:#x}: CALL {THUNK:#x} -> CALL {TARGET:#x}')
    print(f'{THUNK:#x}: JMP {TARGET:#x} unchanged')
    print('Whole-ELF differing offsets: ' + ', '.join(map(hex, changed)))
    print(f'{digest(orig)}  outlined.elf')
    print(f'{digest(new)}  direct.elf')
    print(f'{digest(new[off:off+size])}  direct.text')


if __name__ == '__main__':
    main(*map(Path, sys.argv[1:]))
