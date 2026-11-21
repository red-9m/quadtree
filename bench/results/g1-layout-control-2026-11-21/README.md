# 02 · Layout control for the two-entry direct result

**The unordered two-entry saving survives layout control; the ordered two-entry shift did not.** With `-falign-functions=64` on both binaries, the [direct tiny-path result](../g1-two-direct-result-2026-11-21/README.md) saves 3.3–4.7 ns/scene at two entries and 4.0–5.3 ns/scene at four, resolved in all eight comparisons. The ordered two-entry change (4.0–4.7 ns faster in the unaligned pair) drops to −1.2 to +0.8 ns, unresolved, so it was a placement artifact. All two-entry rows still fail G1 (2.414–2.475×), 5.3–5.8 ns/scene above 2× brute at the point estimate. Nothing staged; staged G1 stays 8/16 and 08 stays blocked.

## Build and preflight

- **Same sources:** control is the exact packed cross-item tree; candidate is that tree plus [direct.patch](../g1-two-direct-result-2026-11-21/direct.patch). Both verified against their input hashes. The bench adapter, runner, fixtures, brute baseline and gate limits are unchanged.
- **Recipe reproduces the frozen builds:** [build.sh](build.sh) without extra flags yields byte-identical `.text` to the frozen control and to the earlier candidate (`97d8835e…`, `f652e83a…`). The only layout change is `-falign-functions=64`, applied identically to both. New hashes: [text.sha256](text.sha256), [control ELF](control-executable.sha256), [candidate ELF](candidate-executable.sha256). The frozen control build is untouched.
- **Codegen ([log](codegen.log), [callees](codegen-callees.log)):**
  - Ordered `LibraryIndex::pairs` (506 instructions), ordered `Runner::step` (665) and unordered `Runner::step` (652) are identical after normalization.
  - All 28 `Index<unsigned, int, Unchecked, true>` member symbols are identical.
  - Only the unordered adapter differs (350 → 346).
  - On the candidate's tiny path, the exit jumps past the result copy. Only the `selfPairs` branch still copies; it has 4 vector moves against control's 6. [Control](control-dispatch.asm.gz) · [Candidate](candidate-dispatch.asm.gz).
- **Semantics:** unchanged from the [direct-result checks](../g1-two-direct-result-2026-11-21/README.md#candidate-and-preflight). The alignment flag does not change source semantics, and all 96 benchmark rows match on work and results (below).

## Matched G1 results

Frozen G1 workload on CPU 6, HPET, `performance`, quiet starts ≤40 °C and within 1 °C per pair. Order: 81 rounds control → candidate, then 117 rounds candidate → control. In each run, all 96 rows match between binaries: pairs, checksums, fixture hashes, candidates, node visits, memory and allocations. Both library engines allocate zero when warm. [Verification](diagnostic/verification.log) · [16 rows per run](diagnostic/g1-comparison.csv) · [library A/B, n=0–8](diagnostic/library-change.csv) · [raw samples](diagnostic/).

| Run | Control G1 pass / fail / inconclusive | Candidate G1 pass / fail / inconclusive |
|---:|---:|---:|
| 81 | 8 / 8 / 0 | 8 / 7 / 1 |
| 117 | 8 / 8 / 0 | 8 / 8 / 0 |

Rows 8–64 pass, and rows 0–4 fail or are inconclusive, on both binaries in both runs. The one inconclusive (`clustered` empty, 81 rounds) comes from a noisier candidate estimate (±0.9 ns), not a speedup.

Unordered `index`, full step ns/scene (A/B is a log-ratio 95% interval):

| Rounds | Scene | n | Control → candidate | Saved | A/B interval | Candidate G1 [95%] |
|---:|---|---:|---:|---:|---|---|
| 81 | `clustered` | 2 | 34.9 → 30.2 | 4.7 | 0.825–0.907 faster | 2.475 [2.398–2.555] FAIL |
| 81 | `uniform` | 2 | 34.9 → 30.9 | 4.0 | 0.833–0.941 faster | 2.452 [2.330–2.581] FAIL |
| 117 | `clustered` | 2 | 34.2 → 30.9 | 3.3 | 0.865–0.944 faster | 2.414 [2.307–2.526] FAIL |
| 117 | `uniform` | 2 | 34.4 → 30.4 | 4.0 | 0.859–0.909 faster | 2.432 [2.358–2.509] FAIL |
| 81 / 117 | `clustered` | 4 | 56.7 → 51.4 / 56.4 → 51.7 | 5.3 / 4.7 | faster / faster | FAIL / FAIL |
| 81 / 117 | `uniform` | 4 | 59.6 → 55.3 / 59.7 → 55.7 | 4.3 / 4.0 | faster / faster | FAIL / FAIL |

The 0- and 1-entry rows are unresolved in both engines, with changes ≤0.9 ns.

Ordered `index-ordered` (its code is unchanged):

| Rows | Unaligned pair | Aligned pair |
|---|---|---|
| n=2 | 4.0–4.7 ns faster, resolved ×4 | 0.0, −1.2, −0.8, +0.1 ns, unresolved |
| `clustered` n=4 | −0.5, +1.9, unresolved | −2.3 unresolved; −4.4 **slower, resolved** at 117 (lower bound 1.0034) |
| `uniform` n=4 | +2.1, +1.5, unresolved | −2.7, −1.0, unresolved |
| `clustered` n=8 | −14.9, −11.3, slower, resolved | −8.2, −0.4, unresolved |
| `uniform` n=8 | −7.6, −6.2, unresolved | −7.4, −7.9, unresolved |

Negative = candidate slower. The ordered n=8 rows still lean 6–8 ns slower in five of eight comparisons with identical ordered code. That residual comes from placement this flag does not control (page, BTB or i-cache sets), and it is not resolved. All 8–64 G1 rows still pass.

## Interpretation

- **The pair/result saving is real:** about 3.3–4.7 ns/scene at two entries and 4.0–5.3 at four, robust to function alignment. It is the first repeatable two-entry pair-side gain on valid code.
- **It is not enough alone:** two entries still need 5.3–5.8 ns/scene more at the point estimate, and more to clear the upper bound. The [invalid warm-copy probe](../g1-two-copy-headroom-2026-11-21/README.md) bounded build-side headroom at 9–10 ns but omitted validation, extent, a fresh owner and handles. Only a valid build-side saving of roughly 6–7 ns, measured together with this result, could clear 2×. Its additivity is unproven.
- **Ordered n=8 needs watching:** any staged version must show ordered 4/8 timing without a resolved regression under both layouts.
- Unmatched, cross-session observation (not evidence): the aligned control's ordered two-entry rows ran at 64.3–66.9 ns against 71.7–73.0 for the frozen control. Small ordered rows are sensitive to placement by several ns.

## Reproduction

Set `WORK` to the path stored in `/tmp/quadtree-g1-layout.current`. It links `control` (packed cross-item tree), `candidate` (direct-result tree), `box2d` and `frozen-control-build`. Run from this directory:

1. Optional check that the recipe reproduces the frozen `.text`: `bash build.sh WORK plain`.
2. `bash build.sh WORK a64 -falign-functions=64`, then symlink `WORK/{control,candidate}-build` to `WORK/{control,candidate}-a64`.
3. `python3 ../g1-unordered-empty-2026-11-21/codegen.py WORK . --unordered`
4. `python3 ../g1-two-direct-result-2026-11-21/codegen-names.py .`
5. `bash run.sh WORK diagnostic`
6. `python3 summarize.py diagnostic`

[codegen-callees.log](codegen-callees.log): `python3 codegen-callees.py WORK 'QuadTree::Index<unsigned int, int, QuadTree::Unchecked, true>::'`.

## Handoff

**Follow-up done:** the [combined candidate](../g1-two-combined-2026-11-21/README.md) saved only 4.4–5.0 ns at two entries and regressed ordered 2/4/8. The stop rule applies.

**The direct result is a valid pair-side component, not a fix.** Do not stage it alone. Next: one valid combined candidate, direct result plus a contract-preserving warm two-entry build path. That build path must keep input validation, extent publication, a fresh owner, handle invalidation and the allocation limits. Measure it with the same layout control (aligned control vs aligned candidate) and the unchanged runner. Report all 16 G1 verdicts, ordered 4/8 timing, exact pairs, work counts and warm allocations, and rerun build/failure tests because the build path changes. If the combined saving is under about 10 ns/scene at two entries, stop: the 2-entry row cannot clear 2× within the current contract and adapter.
