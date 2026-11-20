#!/usr/bin/env python3
"""Create layout-matched diagnostic copies of the original G1 control ELF.

Only run the copies with isolate.sh's single-entry snapshot command.
"""
import hashlib
from pathlib import Path
import sys

# Verified against the original GCC 16.2.1 control. This ELF's .text VMA
# equals its file offset. Each tuple spans complete instructions.
CONTROL = '72a0b043ccb7adda010de914b6aedcae42d0ed2c08cf40488540fbb71d8c1ac3'
WORKSPACE = {
    0xe29f2: '48399190000000 0f8231010000',  # descriptor capacity
    0xe2a05: '483991a8000000 0f821e010000',  # key capacity
    0xe2a12: '483991c0000000 0f8211010000',  # temp capacity
    0xe2a1f: '80b9cc00000000 0f8562110000',  # active check
    0xe2a4f: 'c681cc00000001',              # enter
    0xe2b16: '41c681cc00000000',            # leave
}
DISPATCH = {
    0xe2a61: '4c39d2 720f',                 # small-set threshold
    0xe2b60: '8b87d0000000 4c39d0 0f8206ffffff',  # slot capacity
    0xe2b72: '803f00 0f85ad040000',         # snapshot extent, always branches
}
NOP = ('', '90', '6690', '0f1f00', '0f1f4000', '0f1f440000',
       '660f1f440000', '0f1f8000000000', '0f1f840000000000',
       '660f1f840000000000')


def nops(count):
    return bytes.fromhex(NOP[9] * (count // 9) + NOP[count % 9])


def main():
    source, out = map(Path, sys.argv[1:])
    original = source.read_bytes()
    if hashlib.sha256(original).hexdigest() != CONTROL:
        raise SystemExit('Expected the original, unpadded Release control ELF')
    for name, patches in (('control', {}), ('workspace', WORKSPACE),
                          ('dispatch', DISPATCH), ('combined', WORKSPACE | DISPATCH)):
        image = bytearray(original)
        for start, pattern in patches.items():
            old = bytes.fromhex(pattern)
            end = start + len(old)
            if image[start:end] != old:
                raise SystemExit(f'{name}: unexpected bytes at {start:#x}')
            if start == 0xe2b72:
                # Keep the taken branch to the unchanged flat path and its target.
                delta = 0xe3028 - (start + 5)
                image[start:end] = b'\xe9' + delta.to_bytes(4, 'little', signed=True) + nops(len(old) - 5)
            else:
                image[start:end] = nops(len(old))
        path = out / name
        path.write_bytes(image)
        path.chmod(0o700)
        print(f'{hashlib.sha256(image).hexdigest()}  {name}')


if __name__ == '__main__':
    main()
