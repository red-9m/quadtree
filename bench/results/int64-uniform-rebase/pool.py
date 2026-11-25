#!/usr/bin/env python3
"""Pool a directory's two gate files using the preregistered screen rule."""
import math
import re
import sys
from pathlib import Path

for arg in sys.argv[1:]:
    rows = [re.search(r'^G2 .*? (\d+\.\d+) \[(\d+\.\d+)-(\d+\.\d+)\]',
                      (Path(arg) / f'{i}.gates').read_text(), re.M).groups() for i in (1, 2)]
    (a, lo1, hi1), (b, lo2, hi2) = [[math.log(float(x)) for x in row] for row in rows]
    m = (a + b) / 2
    h = math.hypot(hi1 - lo1, hi2 - lo2) / 4
    lo, mid, hi = map(math.exp, (m - h, m, m + h))
    print(f'{arg}: {mid:.3f} [{lo:.3f}-{hi:.3f}]',
          'pass' if hi <= 2 else 'FAIL' if lo > 2 else 'inconclusive')
