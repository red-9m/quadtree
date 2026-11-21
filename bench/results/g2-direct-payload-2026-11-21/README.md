# 03 · Direct payload addressing in bulk dense commit

**Reject direct payload addressing.** It removes the descriptor-pointer dependency, but unordered build medians increase on both 100,000-entry blockers in both blocks. No `G2`/`G9` rerun or runtime change.

The [commit breakdown](../g2-commit-breakdown-2026-11-21/README.md) found hot PCs beside the dependent payload load and its lookahead. The [candidate patch](candidate.patch) uses the spatial permutation's original input index to address `BuildEntry::payload` directly. It starts from the [two-stream + source-radix control](../g9-two-scatter-source-radix-2026-11-21/README.md), with neither outline nor classifier experiment.

Eligibility: `int32`, exact `BuildEntry<Payload,T>` in a contiguous nonvolatile range, no consume or projection, a trivially copyable payload no larger than a pointer, and a successful top-down bulk plan. Capture the first input address during the existing validation pass, so there is no new range operation. Keep descriptor capture, normalization, filters, rank insertion, placement order, rollback, handles, topology, and fallback paths. The existing payload lookahead keeps distance 16; its extra descriptor-pointer hint at distance 32 is omitted only when the pointer is no longer read. Box/descriptor hints stay. The [Release excerpt](direct-load.asm) shows the direct address formula (56-byte input stride, payload at +16) for the hint and current load; GCC passes the value to the constructor clone in `r9d`.

## Pilot

Two reversed nine-round blocks, both library engines, `uniform`, `split-line`, `split-line-translated`, and `mixed-size` at 10,000 and 100,000. CPU 6 with quiet starts and at most 1 °C launch difference per matched pair. A phase gain on the blockers with a resolved representative full-step gain warrants the joint frozen screen; a pilot is not a gate verdict.

| Unordered fixture | Build median in ms, block 1 / 2 | Full-step ratio, block 1 / 2 |
|---|---:|---:|
| `uniform` 100,000 | 5.430 → 5.728 / 5.518 → 5.778 | 1.020 [0.991–1.051] / 1.020 [0.978–1.064] |
| `split-line` 10,000 | 0.440 → 0.439 / 0.433 → 0.442 | 0.986 [0.946–1.028] / 1.001 [0.970–1.032] |
| `split-line-translated` 100,000 | 5.962 → 6.437 / 5.954 → 6.350 | 1.046 [1.001–1.093] / 1.031 [0.996–1.068] |

- `split-line` and translated at 100,000 regress in block 1 only. `mixed-size` ordered at 100,000 regresses in block 2 only (1.034 [1.004–1.064]); that comparison has a higher pair median despite unchanged query source. No full-step gain resolves. The new addressing mode and changed code generation are not isolated by this screen.
- [Phases and intervals](pilot/step-change.csv) · [Verification](pilot/verification.log) · [Order](pilot/run-order.csv) · [Raw samples](pilot/). All 64 rows match checksums, work, allocations and memory; zero warm allocations.

## Correctness and reproduction

[Payload probe](checks/payload-paths.log): 10 cases spanning offset contiguous input, noncontiguous input, consume, projection, unranked/mixed/full ranks, exact handles and output, duplicate/out-of-bounds rollback and warmed allocation requests. GCC and ASan/UBSan streams match. [CTest 21/21](checks/tests.log), [full differential fuzzing](checks/fuzz.log), [topology/dense](checks/dense-order.log), [source order](checks/source-sanitized.log), and [pair prefixes](checks/unordered-sanitized.log) pass.

Reconstruct the source-radix stages as `WORK/control` and `WORK/control-build`. Copy the source and apply `patch -p1 --fuzz=0 < candidate.patch` in `WORK/candidate`. Build with GCC 16.2.1 Release, tests/benchmarks on, `QUADTREE_BENCH_NATIVE=OFF`, and pinned `WORK/box2d` (`178495ba35695aefe9c321e569d2277f06db3b90`). Verify [input hashes](candidate-inputs.sha256), [control ELF](control-executable.sha256), and [`.text` hashes](text.sha256).

Run `bash verify-payload.sh WORK` and the [functional checks](../g2-radix-outline-2026-11-21/verify.sh). Use `bash run.sh WORK NEW_PILOT_DIR --pilot` and `python3 summarize-pilot.py NEW_PILOT_DIR` for the screen.

## Handoff

**Compare build-phase cycle attribution on the exact control and rejected candidate.** On the two 100,000-entry unordered blockers, determine whether the dense commit itself grows or whether the source change shifted the cost outside that phase. Include an ordered translated comparison. Audit the Release callsites and keep this diagnostic separate from gate evidence; don't tune lookahead from the pilot alone.
