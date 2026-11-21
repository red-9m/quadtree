# 02 · Two-entry slot-write headroom

**Stop before implementing implicit slots.** Skipping both slot records in the warm unranked two-entry build saves 0.3–0.7 ns/scene on point estimates; none of the four matched intervals resolves. The staged control needs 9.1–9.7 ns/scene just to reach 2× brute. Keep staged G1 unchanged; no runtime patch or new acceptance result.

## Diagnostic

[Patch](probe.patch): keep the existing warm integer flat-build path (validation, fresh owner, metadata, extent, payload/count publication). Only when `count == 2`, omit constructing the two dense-to-slot records. This is an **invalid diagnostic**: it assumes a preceding cold build initialized the same slots with generation zero. The frozen size-two fixture repeats cold-then-warm builds without intermediate mutations. The new branch has a cost, and the study does not remove query-side slot reads; this measures omission of the writes, not a bound on every implicit-slot design.

[Disassembly](slot-write.asm): the staged build runs the slot loop at `0xd224b`; the probe jumps around it for count two at `0xd227f`. [Direct check](slot-probe.cpp) verifies repeated cold-then-warm builds yield exact pair handles and invalidate old owners. It also shows why the patch cannot ship: a remove/insert followed by a successful rebuild returns an invalid fresh handle on the scratch binary; the control passes both paths. [Probe log](slot-probe.log).

## Matched timing

Frozen six-engine G1 batch retained for context, but the table reports only the two-entry `index` target. Times are full-step ns/scene; A/B is diagnostic/control. Point gap uses the matched control's brute result. The evaluator can print ratios for the scratch binary, but they are not G1 verdicts.

| Rounds | Scene | Staged → diagnostic | A/B 95% interval | Staged point gap to 2× |
|---:|---|---:|---:|---:|
| 81 | `clustered` | 33.7 → 33.3 | 0.959–1.018 | 9.1 ns |
| 81 | `uniform` | 34.5 → 34.2 | 0.945–1.040 | 9.3 ns |
| 117 | `clustered` | 34.0 → 33.3 | 0.951–1.009 | 9.4 ns |
| 117 | `uniform` | 33.9 → 33.5 | 0.960–1.018 | 9.7 ns |

Even the most favorable interval endpoint is only about 1.9 ns/scene faster. There is no evidence the omitted writes justify changing slot/mutation representation. [Target CSV](diagnostic/two-entry-headroom.csv) · [Raw samples, environment and gate logs](diagnostic/) · [Verification](diagnostic/verification.log). All 192 matched engine rows agree on fixture, work, pair counts, checksums and memory; all 32 warm library rows per run allocate zero. Those measurements cannot prove the public handle contract.

## Reproduction

Start with the exact [staged packed cross-item tree and Release build](../g2-packed-cross-item-2026-11-21/README.md#checks-and-reproduction) as `WORK/control` and `WORK/control-build`, link `WORK/box2d`, copy to `WORK/candidate`, and apply `probe.patch` only to that scratch copy. `bash build.sh WORK` uses GCC 16.2.1 Release and the pinned control Box2D archive. `bash run.sh WORK OUT` verifies provenance and runs the frozen batch on CPU 6 (81 control → probe, 117 probe → control; quiet starts ≤40 °C and matched within 1 °C). Power restored. Run `python3 summarize.py OUT`. For the direct check, compile `c++ -std=gnu++20 -O3 -DNDEBUG -IWORK/VARIANT slot-probe.cpp -o probe` for each variant and run `probe control` or `probe invalid-slot-probe` respectively.

## Handoff

**Do not implement slot materialization on this evidence.** The exact staged binary still fails both two-entry rows. A new two-entry idea needs a measured path to most of the 9–10 ns gap, not another slot-store reduction or a repeat of the [single-candidate dispatch experiments](../g1-two-entry-dispatch-2026-11-19/README.md). The [earlier result-copy variant](../g1-direct-pair-result-2026-11-19/README.md) saved 3.5–4.2 ns at two entries with no gate closure; treat that as history, not an additive credit. Keep task 02 open and 08 blocked.
