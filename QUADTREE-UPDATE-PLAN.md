# QuadTree update: remaining work

**Next: [09 — other coordinate types](docs/plan/09-other-coordinate-types.md).** The `int32` release is [accepted](docs/plan/08-final-acceptance-result.md) on `a72bc80`, with owner-accepted G1/G2 exceptions. `int64`, `float` and `double` are functionally supported, but their performance acceptance belongs to 09. MSVC is out of scope until explicitly requested. For a new AI session, this index and one task brief are the starting context.

## How to use this plan

1. Read this index first. For a status question, use the table and its linked evidence; do not start implementation or benchmarks.
2. For implementation, open the brief named by the user. If asked to continue without a task number, use **Next**. Numbers identify tasks, not a mandatory sequence: 04, 05 and 07 can start independently.
3. Read the selected brief in full: goal, non-goals, named files/symbols, and exit criteria. Follow only the links needed for that task. For a prerequisite, read its **result**, not its whole brief. If the evidence or required environment is missing, report the blocker rather than assuming a pass.
4. Work on one deliverable. Use the shared references below for a specific question; do not preload sibling briefs, the full manifest, or the archive. Stop at the brief's exit or blocker and leave the handoff described below.

## Common context

The [runtime API](INDEX.md), adaptive tree, bulk loader, and ordered/cross/moved-set queries are implemented. The [current G1/G2 snapshot baseline](bench/results/current-snapshot-2026-11-18/README.md) includes the [mutation and ordered moved-set fixes](bench/results/index-fixes/README.md#changes). The [earlier development run](bench/results/index-development/README.md#results) remains historical evidence for persistent rows. Neither run is release acceptance.

- **Runtime vs legacy:** [quadtree-index.h](quadtree-index.h) and [detail/quadtree-index-impl.h](detail/quadtree-index-impl.h) implement the header-only C++20 `QuadTree::Index`. Snapshot `build` and persistent mutations share one index and the same exact AABB semantics. [quadtree.h](quadtree.h) is the separate legacy `Tree`; do not include both APIs in one translation unit or use tests for one as evidence for the other.
- **Public contract:** consult the relevant heading in [INDEX.md](INDEX.md) for geometry, handles, build failures, workspace, ordering or budgets. Preserve exception-disabled operation, state/handle guarantees, and allocation-free prepared paths when optimizing. Compare entry/pair identities, not counts alone.
- **Performance evidence:** use the [frozen gate definitions](bench/MANIFEST.md#frozen-acceptance-limits) for limits and verdicts. Measure the library engines `index` / `index-ordered`, not just prototypes. Keep raw samples, environment and tested source revision (including local changes). An inconclusive interval is not a pass; a passing subset is not release acceptance.

## Which brief to open

| Task | Open when | Evidence or gap |
|---|---|---|
| [01 · Current baseline](docs/plan/01-current-baseline.md) | Done; measured and repeated | [Current verdicts and raw samples](bench/results/current-snapshot-2026-11-18/README.md) |
| [02 · Small snapshots](docs/plan/02-g1-small-snapshots.md) | Closed as accepted exception (owner, 2026-11-21): rows of 0–4 entries fail G1 at 2.0–3.3× and do not block release if ≤ 3.5×; 8–64 pass. No candidate staged. | [Decision record](docs/plan/02-g1-small-snapshots-result.md) · [Combined candidate and stop rule](bench/results/g1-two-combined-2026-11-21/README.md) · [Layout control](bench/results/g1-layout-control-2026-11-21/README.md) · [Staged G1 slice](bench/results/g1-staged-small-2026-11-21/README.md) |
| [03 · Representative snapshots](docs/plan/03-g2-snapshot-cost.md) | Done for the measured `int32` slice; two-stream + source radix + packed cross-item, [promoted](bench/results/promotion-2026-11-21/README.md) in `e73f6c5`. G2/G9 pass 15/15 at nine and 45 rounds; translated 100,000 was inconclusive on both at 15. Broader coverage remains with 08. | [45-round confirmation](bench/results/g2-packed-cross-item-confirm-2026-11-21/README.md) · [9/15 matched screen](bench/results/g2-packed-cross-item-2026-11-21/README.md#frozen-g1g2g9-screen) · [Uncertainty audit](bench/results/g2-translated-uncertainty-2026-11-21/README.md) · [Exact instruction profile](bench/results/g2-exact-eligibility-profile-2026-11-21/README.md) · [Item census](bench/results/g2-item-eligibility-census-2026-11-21/README.md) · [Source-radix screen](bench/results/g9-two-scatter-source-radix-2026-11-21/README.md) · [Two-stream screen](bench/results/g2-two-scatter-2026-11-21/README.md) |
| [04 · Verification gaps](docs/plan/04-verification-gaps.md) | Done; platform and acceptance checks remain with 06/08 | [Coverage record and reproduction](docs/plan/04-verification-coverage.md) · [Nonthrowing API checks](docs/plan/04-verification-coverage.md#nonthrowing-api-checks) |
| [05 · Installable package](docs/plan/05-package.md) | Done; source and relocated install verified | [Consumer and install evidence](docs/plan/05-package-result.md) |
| [06 · Portability CI](docs/plan/06-portability-result.md) | Done; all nine hosted Linux jobs passed on `57d30dc` and on release candidate `a72bc80`. MSVC deferred. | [Hosted runs and local evidence](docs/plan/06-portability-result.md) · [Workflow](.github/workflows/portability.yml) |
| [07 · Release docs and example](docs/plan/07-release-docs.md) | Done; runtime quick start and exact cross-pair example verified | [Example and link verification](docs/plan/07-release-docs-result.md) |
| [08 · Final acceptance](docs/plan/08-final-acceptance.md) | **Done for `int32`** (release scope). All hard gates pass, with the accepted G1 (0–4 entries) and G2 (`int32` 10²/10³ rows) exceptions; the G2 10⁵ row passes by pre-specified pooling. | [Sign-off](docs/plan/08-final-acceptance-result.md) · [`int32` confirmation](bench/results/release-int32-2026-11-21/README.md) · [Four-type run](bench/results/release-acceptance-2026-11-21/README.md) |
| [09 · Other coordinate types](docs/plan/09-other-coordinate-types.md) | Next: G2 for `int64`/`float`/`double`, G1 for `float`/`double` | [Four-type baseline](bench/results/release-acceptance-2026-11-21/README.md#blocking-and-unresolved-rows) |
| [Legacy safety patch](docs/plan/legacy-safety.md) | Only when asked about shipping or fixing the legacy API | [Old header](quadtree.h), not the runtime implementation |

**Archive lookup only:** use the [original plan](docs/plan/archive/QUADTREE-UPDATE-PLAN.md) if a brief cites a requirement or a contract is unclear. Search its stable ID (for example, `rg -n 'QT-FR-13' docs/plan/archive/QUADTREE-UPDATE-PLAN.md`) and read the defining paragraph. §11 is historical sequencing, not current status; §13 is out of scope. Leave the archive unchanged.

## Session handoff

### Fresh session: Task 09 — other coordinate types

Work in `/home/dima/src/quadtree.git`. Read [the 09 brief](docs/plan/09-other-coordinate-types.md) in full and the [four-type release run](bench/results/release-acceptance-2026-11-21/README.md#blocking-and-unresolved-rows).

**Next deliverable:** a phase attribution per type before any code change. For `int64`, `float` and `double`, compare `prepare_median_ns` and `pairs_median_ns` against `int32` on the G2 rows, from the release run's `library-rows.csv`. Confirm or reject the hypothesis that these types fall back to generic, non-`int32` paths: check the `std::same_as<T, std::int32_t>` and `!FloatCoordinate<T>` branches in `detail/quadtree-index-impl.h`. Pick the single largest measured cost for the first candidate.

Keep `int32` code generation unchanged or re-verified. The `int32` release evidence must stay valid.

### Update the handoff after the session

Update the active row with its state and a link to the result: a test/CI run, consumer check, or benchmark report as appropriate. Mark a task done only when its exit criteria hold. If blocked, record the missing check and how to resume. If 01 shows every measured G1 or G2 row passes, mark the corresponding optimization skipped **with that evidence**; 08 still owns unmeasured rows. Point **Next** to the next actionable brief. A read-only session reports findings without changing the plan.
