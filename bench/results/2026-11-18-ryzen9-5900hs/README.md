# Benchmark results: 2026-11-18, AMD Ryzen 9 5900HS

Published per `bench/MANIFEST.md` (step 4). Candidate engines are the step 3
prototypes; the library engines replace them once implemented.

## Environment

- AMD Ryzen 9 5900HS (8 cores, 16 threads), amd-pstate-epp, energy preference
  and platform profile `performance`, AC power.
- Linux 7.3.0-1-custom; kernel clock source HPET; the harness times with
  `rdtsc` (invariant TSC, 3.2938 GHz, 7.8 ns per read).
- GCC 16.2.1, `CMAKE_BUILD_TYPE=Release` (`-O3 -DNDEBUG`), no `-march=native`.
- Box2D: pinned v3.2.0 source (SHA-256 `ffec747b…180d`), built static as Release.
- Every result file starts with its full metadata (`#` lines).

## Commands

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target quadtree-bench
quadtree-bench --model=snapshot --types=int32,float --csv=snapshot.csv --raw=snapshot-raw.csv
quadtree-bench --model=persistent --types=float --csv=persistent-float.csv --raw=persistent-float-raw.csv
quadtree-bench --model=persistent --types=int32 --csv=persistent-int32.csv --raw=persistent-int32-raw.csv
quadtree-bench --gate-files=snapshot.csv,persistent-float.csv,persistent-int32.csv > gate.txt
```

Calibration (three identical processes, see `calibration-persistent.txt`):

```sh
quadtree-bench --model=persistent --types=float --persistent-sizes=1000,10000 \
    --engines=b2tree,b2tree-fat,grid,grid-rebuild,qt-loose --csv=calibration-persistent-N.csv
quadtree-bench --diff=calibration-persistent-1.csv,calibration-persistent-2.csv
```

## Files

| File | Content |
|---|---|
| `snapshot.csv`, `snapshot.txt` | snapshot model, int32 and float, all scenes and engines (8.5 min) |
| `persistent-float.csv`, `persistent-int32.csv`, `*.txt` | persistent model, all scenes, workloads, modes and engines (28 min each) |
| `*-raw.csv.gz` | every timed repetition (snapshot) or step (persistent) with its round |
| `gate.txt` | gate verdicts on the three result files |
| `calibration-persistent-*.csv`, `calibration-persistent.txt` | persistent process-term calibration |
| `timing.txt` | run log, including the reruns below |

Reruns: the first snapshot run had energy preference `power` (the machine had
switched profiles); it was repeated under `performance`. The persistent runs
were repeated after the persistent process term was calibrated at 3%.

## Gate summary (prototype candidates)

`qt-loose-sort` (snapshot), `qt-loose` (persistent), `qt-refs-ordered` against
`qt-refs` (ordered). Exit code 3: hard gates fail, as expected for prototypes.

| Gate | Pass | Fail | Inconclusive | Notes |
|---|---|---|---|---|
| G0 correctness | all | 0 | 0 | no mismatch in any row |
| G1 small scenes | 8 | 24 | 0 | 32–64 entries pass; 0–16 fail (radix-sort setup cost): needs the small-set path |
| G2 representative | 39 | 0 | 1 | `split-line-translated` 10² at 3.95 against ≤ 4 |
| G3 work growth | 18 | 0 | 0 | |
| G4 translation | 16 | 0 | 0 | |
| G6 allocations | 210 | 344 | 0 | all failures persistent `qt-loose` (per-node vectors): needs pooled storage |
| G7 hard (dynamic tree) | 240 | 0 | 0 | |
| G7 soft (grid) | 238 | 0 | 2 | `uniform` 10³ at 3.94–4.00 against ≤ 4 |
| G8a, G8b, G8c hard | 80 | 0 | 0 | |
| G8c soft (grid) | 47 | 0 | 1 | |
| G9 ordered | 28 | 2 | 0 | `mixed-size` 10⁵ at 6.5–6.8 against ≤ 4 |
