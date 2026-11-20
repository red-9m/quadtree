# 02 · Small snapshots — inline flat-path identity acquisition

**Retained.** Inlining identity acquisition in `buildFlatInteger` removed the out-of-line call and surrounding spill/reload sequence on GCC 16. The 45-round repeat saved 1.3–1.9 ns/scene at 1, 2 and 4 entries across both scenes. G1 remains open: all six 1/2/4 rows still fail.

## Change

`buildFlatInteger` already excludes policies that override identity acquisition. It now calls the inline implementation directly. The normal `acquireModuleIdentity()` wrapper still lets the compiler choose whether to inline at other call sites. The GNU inline hint is guarded by `__has_cpp_attribute`; unsupported compilers use normal `inline`. Both paths share the same module counter, relaxed saturating CAS loop and exhaustion check. Acquisition still follows validation and precedes all commit mutations.

The [disassembly](identity-boundary.asm) shows the old call and reloads and the replacement inline CAS. The staged candidate and retained Release build have byte-identical [`.text`](text.sha256).

## Full G1 step

| Rounds | Control: pass / fail / inconclusive | Inline: pass / fail / inconclusive |
|---:|---:|---:|
| 27 | [8 / 6 / 2](27-control/gates.txt) | [8 / 6 / 2](27-inline/gates.txt) |
| 45 | [8 / 6 / 2](45-control/gates.txt) | [8 / 6 / 2](45-inline/gates.txt) |
| 63 | [8 / 6 / 2](63-control/gates.txt) | [10 / 6 / 0](63-retained/gates.txt) |

At 45 rounds, `uniform` improved from 20.4/36.6/62.0 to 18.9/34.8/60.1 ns/scene at 1/2/4; `clustered` improved from 20.4/36.3/59.1 to 19.1/34.4/57.6. At 63 rounds the gains persisted except at `clustered` singleton, whose retained estimate had a wider interval and matched control. Empty absolute estimates did not regress on repeat; both empty rows passed at 63 rounds but remained inconclusive at 27 and 45. All 8–64 rows passed throughout. The [comparison CSV](g1-comparison.csv) gives all 16 rows per run with absolute times and frozen ratio intervals.

Both library engines matched pair counts, checksums, fixture hashes and counters across the six runs, with zero warm allocations. The [exact-pair probe](exact-pair-probe.log) matched brute identities across all 16 retained rows. [All 16 CTest checks passed](tests.log), including exception-disabled tests and the quick differential fuzzer. The new exhaustion test checks validation before acquiring the last owner, old-handle invalidation on success, and unchanged state and output on exhaustion. Clang 22.1.8 also accepted the test TU with strict warnings and exceptions disabled.

**Next:** stay on 02. Test fusing only the tiny flat entry-store loops at 2–4 entries, without changing pair dispatch. The [replacement profile](../g1-replacement-profile-2026-11-19/README.md) isolates those loops; the [previous fused-store experiment](../g1-tiny-dispatch-2026-11-19/README.md#diagnosis-and-changes) changed overlap dispatch too.

## Reproduction

Reconstruct the [view-construction source](../g1-view-construction-2026-11-19/README.md#reproduction) and apply the [two-entry test patch](../g1-two-entry-dispatch-2026-11-19/retained-tests.patch) for the control. Apply [retained.patch](retained.patch) for the inline build and verify the [input hashes](retained-inputs.sha256). The staged variant used the same patch without the inline attribute guard and new test; its `.text` matched the retained build. Build each with the [baseline Release recipe](../current-snapshot-2026-11-18/README.md#environment-and-reproduction), then run `taskset -c 6 bash bench/results/g1-flat-input-2026-11-19/reproduce.sh /path/to/quadtree-bench /fresh/outdir ROUNDS`. Run order: 27 control, 27 inline, 45 inline, 45 control, 63 control, 63 retained.

Environment: Ryzen 9 5900HS, pinned CPU 6; GCC 16.2.1; Linux 7.3.0-1-custom; `powersave` governor, `power` energy preference, HPET clock source. Invariant TSC benchmark timer. Release flags: `-O3 -DNDEBUG`, no native/LTO. Box2D `v3.2.0` at `178495ba35695aefe9c321e569d2277f06db3b90`.
