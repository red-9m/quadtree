# 02 · Codegen-first screen: outline unordered traversal only

**Stopped before timing.** The ordered pair adapter retains its normalized instructions, but the unordered empty path still uses the staged mixed-width counter return. It does not meet the entry criteria for an 81/117-round run. No candidate staged; no new G1 or ordered performance verdict.

## Variant

[Patch](candidate.patch): add the explicit unordered empty return and outline only the subsequent unordered traversal into `unorderedPairTraversal(Context&, ...)`. The caller retains `Context` creation, `QueryResult` return, the ordered code and workspace scope. The helper uses the initial candidate budget (uncharged at that point) for tiny dispatch. No adapter, runner, brute or fixture changes. The [focused API probe](pair-check.log) passed empty/singleton statuses, ranks, two-entry budgets, orientation and Stop.

## Codegen preflight

| Check | Observed in the exact GCC 16.2.1 Release ELFs | Decision |
|---|---|---|
| Ordered pair adapter stays close to staged | 506 → 506 instructions, same after normalizing branch/RIP displacements. Ordered runner 668 → 665 with different padding/internal branch offsets. | Met at codegen level only; not a timing result |
| Unordered empty avoids old counter transfer | Adapter remains out of line at `0xed5d0`; GCC merges zero and one into `cmp $1` / `jbe` (`0xed603` / `0xed607`). The common return emits separate 8-byte candidate/reference stores (`0xed6a8`, `0xed6b0`), followed by the caller's single 16-byte reload at `0xedfd0`. | Not met; do not time |

The unordered adapter shrank from 1,656 to 320 bytes but retained the relevant call and store/load sequence. The [earlier two-mode probe](../g1-small-headroom-2026-11-21/README.md) removed that boundary and passed empty G1, but regressed ordered eight-entry steps. The two findings do not establish a hardware latency or an ordered performance equivalence for this candidate.

[Codegen log](codegen.log) · [Control assembly](control-dispatch.asm.gz) · [Candidate assembly](candidate-dispatch.asm.gz) · [Input hashes](candidate-inputs.sha256) · [`.text` hashes](text.sha256). Rebuild as in the [previous split](../g1-unordered-empty-2026-11-21/README.md#reproduction), using this directory's patch and `bash build.sh WORK`. Generate disassembly with `python3 ../g1-unordered-empty-2026-11-21/codegen.py WORK EXISTING_OUTPUT_DIR`. No benchmark or power-setting change was needed.

## Handoff

**Pause this outlining approach.** Staged G1 remains 8/16 and blocks 08. Resume only with a materially different contract-preserving way to remove the empty counter return and a plan for the remaining 1/2/4-entry gaps; do not schedule repeats for this candidate.
