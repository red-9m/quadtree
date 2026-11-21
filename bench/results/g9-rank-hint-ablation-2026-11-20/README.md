# 03 · Same-layout rank prefetch ablation

**The hint saves ~0.45 ms of ordered build time, but no ordered full-step gain resolves.** Most of the pair-phase movement persists without the hint. Keep direct child initialization as the staged control; no runtime change or gate rerun.

## Matched binaries and runs

- `control`: unmodified direct-child-init Release binary. `hintbin`: unmodified [rank-lookahead Release binary](../g9-home-lookahead-2026-11-20/README.md). `nohintx`: a diagnostic copy of `hintbin` with the single `prefetcht0 [rdx]` at `0x11c45` replaced by a three-byte NOP. Both copies keep the same queue, hash work, addresses and instruction layout. The [whole-ELF comparison](results/twin-verification.log) finds only two changed byte positions; [disassembly](twin-instructions.asm) verifies that the next instruction still starts at `0x11c48`.
- Six blocks, one process per binary in each block, with all six orders represented. Each process measures both library engines on the same two `int32` fixtures at 100,000 entries, 15 rounds, seed 1. CPU 6 pinned; power settings restored. The benchmark visits scenes in its fixed order. No baseline engines or frozen gates were run with the diagnostic twin.
- [Verification](results/verification.log): all 72 rows match the frozen direct-init reference for fixture hashes, pair checksums, work, memory and cold allocations. All warm paths allocate zero. [Raw samples, environment and order](results/).

## Ordered results

Median of six block differences, ms/snapshot. Negative is faster. Build and pair medians are descriptive; full steps use the benchmark's estimate. [All individual blocks and full-step intervals](results/block-comparisons.csv) · [Six-block summary and ranges](results/summary.csv).

| Scene | Comparison | Build | Pairs | Full step |
|---|---|---:|---:|---:|
| `uniform` | Hint − NOP | −0.444 | −0.437 | −0.775 |
| `uniform` | Hint − direct init | −0.705 | +0.588 | −0.213 |
| `uniform` | NOP − direct init | −0.229 | +0.978 | +0.659 |
| `split-line-translated` | Hint − NOP | −0.473 | +0.348 | −0.073 |
| `split-line-translated` | Hint − direct init | −0.678 | +0.675 | +0.181 |
| `split-line-translated` | NOP − direct init | −0.276 | +0.330 | +0.172 |

- **Isolated hint effect:** ordered build medians fall in 5/6 `uniform` blocks and 6/6 translated blocks. For translated the saving ranges from 0.14 to 0.61 ms. Ordered pair medians do not move in a consistent direction across both scenes; translated values range from −0.82 to +1.07 ms. Every ordered full-step interval includes no change for all three comparisons.
- **Pair penalty persists without prefetch:** compared with direct init, ordered `uniform` pairs are slower in all six blocks with either lookahead binary; translated pairs are slower in five of six with either binary. Removing the hint does not restore the original pair timing. The queue and generated code both differ from direct init, so this does not separate queue/cache effects from compiler effects.
- **Unordered negative control:** the hint and NOP cannot execute in unordered builds. Their unordered pair medians differ by about 0.02 ms (`uniform`) and 0.002 ms (translated) across blocks. Both are faster than direct init in all six blocks: 4.9–5.0% (`uniform`) and 3.4–3.6% (translated) at the median. Unordered full-step intervals still include no change. The code generation difference is real: unordered `selfPairs<int32>` goes from **1,956 to 1,310 bytes** and gains two calls to `pairQualifies` rather than inlining those tests. Other query symbols also move. This identifies a factor to isolate, not the cause of the timings. [Symbol sizes and addresses](pair-symbols.csv) · [Call targets](self-pair-calls.csv).

## Reproduction and handoff

Recreate `WORK/control-build`, `WORK/candidate-build`, and `WORK/box2d` per the [rank-lookahead report](../g9-home-lookahead-2026-11-20/README.md#correctness-and-reproduction). Run `bash run.sh WORK NEW_RESULT_DIR`, then `python3 summarize.py NEW_RESULT_DIR`. [`make-twin.py`](make-twin.py) checks the known `.text` hash, instruction bytes, and section mapping before creating a new file; it never writes to either original executable. The twin is not a new source candidate and is not acceptance evidence.

**Next: isolate outlining of `pairQualifies` in unordered `selfPairs` on direct child initialization alone.** Do not stack the rank queue or retune prefetch distance. Confirm the call boundary in the compiled `int32` benchmark, preserve exact candidate/predicate/visitor order and charges, and test both engines with matched build and pair phases. The observed ~0.24–0.37 ms unordered pair savings are not enough to close the full G2 gap on both blockers; treat them as an isolated lead, not a forecast. Rerun G2/G9 together only if the new source change resolves a full-step gain. G1/G2 still block 08.
