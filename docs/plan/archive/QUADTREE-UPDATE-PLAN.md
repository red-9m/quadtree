# QuadTree update plan

Functional requirements for the next major version of the QuadTree header
library. This document states **what** the library must do and how its API
changes. It does not choose data layouts, algorithms, or tuning constants;
those stay with the implementation as long as they meet the requirements below.

Requirement IDs: `QT-FR-xx` (functional), `QT-NFR-xx` (non-functional),
`QT-API-xx` (interface changes), `QT-FIX-xx` (existing defects). IDs are stable
and never reused.

## 1. Purpose and scope

The library is a 2D spatial index for axis-aligned rectangles (AABBs). Its main
target is games and simulations: many moving objects, a broad phase for
collision detection, region queries for gameplay (area effects, picking,
visibility), all under a per-frame time budget.

The next version must be usable as a **collision-detection broad phase** of a
game engine:

- correct throughout a precisely defined numeric domain, with no undefined behavior;
- complete: no overlapping pair is lost, none is reported twice;
- deterministic, with an optional client-defined reporting order;
- efficient on measured game workloads, with documented pathological cases;
- linear in live storage, with retained capacity and explicit allocation-free paths.

Out of scope: narrow phase, physics and collision response, non-rectangular
shapes, rotation, automatic swept/continuous detection, ray casts,
nearest-neighbour search, serialization, contact begin/end tracking, resumable
traversal, and multithreaded mutation. Section 13 lists possible future extensions.
Clients may index conservative bounds of rotated or moving shapes, but an AABB
hit is not a narrow-phase or time-of-impact result.

## 2. Current state and gaps

| Area | Current behavior | Gap for game use |
|---|---|---|
| Removal | Locates the item by its *current* rectangle | Removing after a move searches the wrong node; the item stays indexed, and stale data corrupts later queries |
| Ownership | Copy/move are deleted; nodes are raw-owned | No relocation in standard containers or transfer of an existing tree; direct prvalue factory returns already work through guaranteed copy elision |
| Geometry source | Reads geometry through a stateless function pointer on every use | Clients must keep geometry unchanged while indexed; captured lambdas and contexts are not allowed |
| Coordinate range | `x + w` and the centre are computed in the coordinate type; geometry is only `{x, y, w, h}` | Integer overflow (UB) near the type limits; floating-point child bounds can round inward and prune real hits; with `w <= INT64_MAX` a root covers at most about half the `int64_t` range; min/max AABBs (Box2D style) must be converted to `{x, w}`, and in floating point `x + (max - x)` can differ from `max` |
| World extent | Fixed root rectangle; `add` rejects items that are not fully inside | Objects outside the root silently drop out of collision detection unless every caller checks |
| Subdivision | Depth is preallocated up front (4^depth nodes); `nodeItems` only reserves | Memory and setup time are paid whether or not a region is used; no density adaptation |
| Straddling items | An item crossing a centre line stays in that node | Items on split lines pile up near the root and are tested against everything below |
| Pair enumeration | `queryAll` appends every pair to one vector | Memory grows quadratically when everything overlaps; no order, no filter, no early stop; no way to enumerate only pairs of moved entries |
| Filtering | None | Games test incompatible groups (bullet vs bullet) and then throw the results away |
| Per-frame use | No bulk build, no clear that keeps capacity | Rebuilding each frame allocates and costs more than it needs to |
| Configuration | Public member declarations depend on `_DEBUG` | Class definitions differ across translation units (ODR violation), although no data member currently changes layout |
| Type constraints / header hygiene | `IntType` accepts `bool`, `char`, and unsigned types; the header uses `std::equal_to`, `std::pair`, `std::next`, and `NULL` without `<functional>`, `<utility>`, `<iterator>`, `<cstddef>` | `bool` passes the concept without a useful coordinate contract; character/unsigned support needs an explicit domain, not a claim that all such coordinates are invalid. Standalone compilation depends on transitive includes (it currently passes with libstdc++ and libc++) |
| Entry access | No size, iteration, or per-entry lookup | Debug drawing, editor tooling, and save/rebuild code must keep a parallel container |
| constexpr | Every member is `constexpr` | The new `Index` and `Workspace` API is runtime-only; migration notes must document the loss of constant evaluation |
| Packaging | Demo-only CMake with machine-specific flags | No library target to consume; current flags are private to the executable and must stay off future interface targets |
| Tests | A demo prints counts and compares only region-query counts | No assertion-based oracle, pair-set comparison, sanitizer gate, or reproducible benchmark suite |

### Investigation findings

Checked against `quadtree.h`, the demo, and build files. Focused GCC and Clang
ASan/UBSan probes confirmed:

- `Node`/`computeBox` floating-point rounding can lose a valid query hit. With
  `u = nextafter(1.0f, 2.0f) - 1.0f`, root `{1, 0, 5*u, 8}`, depth 1,
  item `{1+4*u, 1, u, 1}`, and point query `{1+5*u, 1.5f, 0, 0}`, insertion
  succeeds but the query returns zero entries instead of one. The NE child's
  far edge `(1+2.5u)+2.5u` rounds to `1+4u`, inside the root's `1+5u`
  (QT-FIX-10); this is separate from integer overflow. In a second tree,
  adding a vertical segment `{1+5*u, 0, 0, 8}` makes `queryAll` report zero
  pairs instead of the item/segment pair. Run the two reproducers in separate
  trees: with both entries in one tree, the point query returns one entry (the
  segment, kept at the root), which hides the lost item if only counts are
  checked.
- Signed overflow (UBSan-confirmed with GCC and Clang) happens whenever a far
  edge `x + w` or `y + h` is not representable: for items in the `add` containment check and
  `getQuadrant` (root `{0, 0, INT64_MAX, INT64_MAX}`, item
  `{INT64_MAX-1, 1, 5, 2}`), and for such roots in `Node` centre and
  `computeBox` arithmetic. Centre arithmetic is safe when the root's far edges
  are representable. Negative-width insertion is currently accepted; NaN items
  are rejected only as a side effect of failing comparisons.
- `doRemoveItem` checks `it != end()` before erasing. Missing-item removal did
  not corrupt the tree in the probe; the original invalid-iterator allegation
  is not supported by the current source (QT-FIX-02).
- Copying is already deleted, so the current implementation does not shallow-copy
  and double-delete (QT-FIX-03). A virtual destructor is a design cost, not a
  demonstrated correctness defect (QT-FIX-07).

The existing demo runs clean under ASan/UBSan in debug and `-O2` builds, but
cannot detect the floating-point false negative. `enumerateNode` has a
`-Wsign-compare` warning (`int` vs `std::size_t`). The project never shows it:
CMake adds `-Wall` only to non-Debug builds, where `_DEBUG` code is compiled
out. Tests must compare identities, not counts.

## 3. Usage models

One `Index` type supports two first-class usage models over the same query API,
sharing stored state and retained capacity. Both must give identical result sets for the same entries, geometry, filters,
and policies. Ordered sequences must also match when the client supplies the
same unique ranks. Unordered traversal order need not match between models.

1. **Persistent index.** Items are inserted once, then moved, resized, and
   removed incrementally. Suits mostly static content: level geometry,
   triggers, slowly changing scenes. Per-step pair work can follow the
   moved set (QT-FR-34) instead of the whole index.
2. **Snapshot index.** The client submits the whole current set of rectangles
   (typically once per simulation step), runs queries, and discards or rebuilds
   the index. Suits scenes where most objects move every frame. Rebuilding must
   reuse retained capacity.

The client chooses the model; the library must not require one model for
correctness.

## 4. Functional requirements

### 4.1 Geometry and coordinate domain

- **QT-FR-01 Coordinate types.** Support signed 32- and 64-bit integers and
  binary IEC 60559 `float` and `double` only in the initial release. Other
  arithmetic types, including `bool`, character types, unsigned integers,
  narrower signed integers, and `long double`, are rejected. Constraints give
  clear diagnostics. The coordinate type is a template parameter, not runtime config.
- **QT-FR-02 Numeric domain and arithmetic.**
  - Canonical geometry is the closed edge form `[minX, maxX] × [minY, maxY]`
    with both edges representable in the coordinate type (QT-FR-33). This
    makes the full integer range indexable and keeps hot comparisons in
    `RectT`.
  - Integers: `{x, y, w, h}` input is accepted when `x + w` and `y + h` are
    representable; check before adding, otherwise reject. Edge comparisons are
    exact. No internal operation may overflow: use overflow-safe split points
    (for example, C++20 `std::midpoint`) and spans. An unsigned difference must
    convert the operands before subtraction (`U(hi) - U(lo)`, where U is the
    corresponding unsigned type), not cast an already-overflowed signed result.
    Portable C++20 is required: do not lose signed 64-bit precision by widening
    to `double`, or require compiler-only `__int128`.
  - Floating point `Rect` input: define geometry by the endpoints obtained from `x + w`
    and `y + h`, rounded to the coordinate type under round-to-nearest,
    ties-to-even. Require finite endpoints and require each positive extent to
    produce an endpoint strictly greater than its origin. Reject overflowing
    or collapsed extents; do not silently turn a positive extent into a point.
    Accept subnormals under gradual underflow and normalize signed zero for
    internal comparisons. Fast-math, flush-to-zero, and other rounding modes
    are outside the supported floating-point environment.
  - Cell bounds must conservatively contain their entries under these rules;
    floating-point rounding must never prune a valid hit. Stop subdivision
    when a split cannot make representable progress, including zero-size
    regions. Valid finite `Box` endpoints may have a span that overflows even
    in floating point (`[-max, +max]`). Split, growth, and heuristic arithmetic
    must handle these boxes without requiring a representable span, area, or
    `{x, y, w, h}` root; overflow/NaN in a heuristic must not affect correctness.
- **QT-FR-03 Invalid geometry and configuration.** Reject negative extents,
  reversed `Box` edges (`minX > maxX` or `minY > maxY`), nonfinite values,
  unsupported floating-point endpoints, and invalid
  configuration with a reported error. Apply validation to roots, insertion,
  update, every bulk-build entry, and query rectangles. Invalid input causes no
  mutation and no result-visitor calls. Fixed roots require strictly increasing
  edges on both axes; snapshot-derived extents may be degenerate.
  Empty snapshot builds succeed without a fabricated root.
- **QT-FR-04 Overlap and containment semantics.** Policies belong to indexed
  state, transfer with it on moves, and are shared by all overlap operations;
  there is no per-query boundary override:
  - default: positive-area rectangles overlap only with positive-area
    intersection; if **either** rectangle is degenerate, both axes use closed
    comparisons, preserving the current point/segment behavior;
  - optional touch-inclusive mode: closed comparisons on both axes for all
    rectangles, useful for resting contacts in physics broad phases;
  - valid degenerate entries and query boxes are always accepted; there is no
    configuration option to reject them.

  A rectangle is degenerate when either pair of axis endpoints is equal.
  On each axis, closed overlap is `a.min <= b.max && b.min <= a.max`;
  strict overlap replaces both `<=` with `<`. Require overlap on both axes,
  choosing closed comparisons for both axes if touch-inclusive mode is enabled
  or either rectangle is degenerate. Thus point/point equality and touching
  segment endpoints match; two positive-area boxes touching only at an edge
  or corner do not match in default mode. Test all point/segment/box cases.
  Root containment is a separate inclusive endpoint test, not overlap: an entry
  may match the root's far edge. Internal traversal must be conservative under
  every boundary mode, including sibling-cell contacts. Removal uses identity.
- **QT-FR-33 Edge-form input.** Insert, update, build, and query accept a
  min/max box (`Box {minX, minY, maxX, maxY}`) as well as `Rect {x, y, w, h}`.
  Edge input is stored without endpoint arithmetic, so clients holding min/max
  AABBs never round-trip through a width. `Rect` converts by the QT-FR-02 rules.
  Entry views and `rect(handle)` expose the stored edges. The initial release
  provides no `Box`-to-`Rect` conversion helper; callers handle any needed
  reverse conversion. Document that an integer span may not fit, a floating
  span may overflow or round, and even a finite rounded width may not recover
  the edges.
- **QT-FR-05 Exact AABB results.** Every reported overlap satisfies QT-FR-04
  for the stored, validated endpoints. There is no implicit epsilon. Internal
  loose/fat bounds may produce candidates, never false results. Clients that
  need a contact skin or swept broad-phase bounds supply explicitly expanded,
  validated AABBs; narrow-phase tests remain their responsibility.

### 4.2 Item identity and stored geometry

- **QT-FR-06 Stored geometry.** The tree stores the rectangle it indexed for
  each item. Later changes to the client's own copy of the geometry cannot
  corrupt the index or any query; they reach the index only through an explicit
  update (QT-FR-11) or a rebuild (QT-FR-13).
- **QT-FR-07 Item handles.** Insertion returns an opaque entry handle with
  index ownership and reuse protection. Handle-taking operations require a live
  handle belonging to the participating index. Automatic stale/foreign-handle
  checks are debug diagnostics only; unchecked release calls have no defined
  result on invalid handles. An explicit template policy selects checked or
  unchecked index types; no runtime diagnostics branch is required in unchecked
  operations. Checks follow the selected policy, not `_DEBUG` or `NDEBUG`, so
  the checked type can also be used in optimized builds (QT-NFR-09).
  On an invalid handle, checked operations report the programming error and
  terminate without throwing, before mutation or result callbacks. They do not
  return an invalid-handle status. Explicit `valid(handle)` remains a
  checked operation in all builds (QT-FR-35). Removing an entry
  invalidates only its handle; update, reserve, and shrink preserve all live
  handles. Successful clear/build invalidate every prior handle. Failed
  strong-guarantee operations invalidate none; a failed default build may
  invalidate all as specified in QT-FR-13. Handles follow indexed state through
  moves; move-assignment also invalidates the destination's former handles.
  Generation or identity-counter exhaustion must report resource exhaustion
  rather than make an old handle valid again. Handles are not persistent IDs
  or serialization tokens. Handles are trivially copyable, of documented fixed
  size, equality-comparable, and hashable (`std::hash`), so they can serve as
  keys of client broad-phase caches. A value-initialized handle is always invalid.
  Detection by `valid(handle)` and enabled diagnostics must be exact, not a
  truncated probabilistic tag, including
  after destruction/reconstruction at the same address. Different index template
  specializations use distinct handle types; cross-specialization handle use is
  rejected at compile time. Handles from different instances of the same
  specialization still require exact ownership detection through `valid(handle)`
  and enabled diagnostics. The initial release supports one
  module identity domain: a single executable or one loaded shared library,
  including its translation units. Cross-module handle/index exchange is
  unsupported. Handles/indexes must not survive unloading their owning module;
  identity across reloads is not promised. Any identity counter must be shared
  across the supported domain, not duplicated per translation unit. No shared
  cross-module identity provider is required.
  Handle size is part of the memory budget; publish it.
- **QT-FR-08 Payload.** Each entry owns a payload value (ID, pointer, or small
  object), with no equality requirement. All supported payloads are nothrow
  destructible. Move-only payloads are supported when nothrow move-constructible;
  payload construction, copy, and relocation used by each overload must be
  nonthrowing, with compile-time constraints where expressible. Storing
  a raw pointer does not own or extend the pointee's lifetime. Queries expose
  const references, not copies.
- **QT-FR-09 Geometry access.** Accessor-based convenience insertion/build
  accepts stateful callables and snapshots the returned rectangle once per
  submitted entry. Build validation and construction use the same captured
  geometry, not a second accessor evaluation. The core explicit-rectangle API
  needs no accessor. State whether each convenience overload borrows or stores
  its callable; no query rereads client geometry.
- **QT-FR-10 Duplicates.** Inserting the same payload twice creates two
  independent entries. The library never deduplicates by payload equality.

- **QT-FR-35 Entry access and iteration.** `size()` and `empty()` are O(1).
  `valid(handle)` is O(1), `noexcept`, and returns false for invalid handles in
  all builds. Geometry/payload/filter/rank lookups are O(1) and require valid
  handles, with automatic checks only in debug diagnostics. `forEachEntry(visitor)` visits every live entry once, in a
  deterministic order (QT-NFR-06), with early stop. It supports debug drawing,
  editor tooling, and client-side save/rebuild without a parallel container.

### 4.3 Mutation (persistent model)

- **QT-FR-11 Update.** `update(handle, newRect)` replaces stored geometry while
  preserving the handle, payload, filter, and rank. Validation or allocation
  failure leaves the old geometry indexed. No-op updates avoid relocation;
  small-move efficiency is benchmarked against remove-plus-insert rather than
  promised for every call.
- **QT-FR-12 Remove.** `remove(handle)` requires a valid handle, removes exactly
  that entry, and returns success. Removing twice violates the valid-handle
  precondition; debug diagnostics detect misuse, but release builds do not
  promise safe rejection. Removal never allocates.
- **QT-FR-13 Clear and rebuild.** `clear()` destroys live payloads, removes all
  entries, and retains capacity. `build(range)` replaces the full snapshot.
  Separate nonmutating preparation from replacement:
  - Before replacement, validate all geometry, fixed-root containment, explicit
    rank uniqueness, input/count limits, and the handle-output span size.
    Preparation failure (including allocation failure or insufficient
    workspace) leaves the old snapshot and handles unchanged.
  - Input must be stable across passes. Require `std::ranges::forward_range`
    or explicitly stage single-pass input. Accessor/projection metadata used
    during validation is captured once and reused during construction. Caller
    workspace covers duplicate-rank detection and any normalized descriptors;
    this can require O(n) scratch even in default mode. Never move payloads
    until all input validation is finished. Copy-versus-consume input behavior
    is explicit; do not silently consume lvalue ranges. Supply a C++20-compatible
    consuming form for move-only payloads, not a copy-only bulk-build API.
  - Default mode, after destructive replacement begins: internal allocation
    failure leaves a valid empty index, invalidates all prior handles, and
    returns an error. Client operations must be nonthrowing (QT-NFR-05).
    Within reserved capacity no internal allocation occurs. This mode avoids a second full payload/index copy, not all O(n)
    scratch; publish descriptor and topology-preparation peak memory separately.
  - Opt-in transactional mode: every failure leaves the old snapshot and
    handles unchanged. It may need O(n) additional payload/index staging,
    included in reservation and peak-memory accounting. Moved-from client
    arguments are not rolled back.
  - Hard-capacity mode preserves the strong guarantee, including for build.
    Finish all fallible work before commit, using reserved staging as needed.
    Capacity preflight alone suffices only if remaining work cannot fail.
    Insufficient capacity
    must not clear the old snapshot; reject before replacement if the guarantee
    cannot be met with the supplied storage.

  A successful build invalidates old handles even for unchanged payloads. An
  optional caller-provided `span<Handle>` receives new handles in input order,
  written only on success. Input ranges, projected values/references, and handle
  output must not alias tree storage invalidated by replacement; copying rather
  than moving an aliased payload does not make a destructive build safe. Use
  explicit staging for self-rebuilds. Bulk build is optimized and benchmarked;
  it need not beat individual insertion on every tiny or adversarial input.
- **QT-FR-14 Space reclamation.** Empty subtrees are removed from active
  traversal; repeated churn must not accumulate dead-node query work or memory
  proportional to operation history. Reuse storage before growing. Explicit
  `shrink()` releases unused capacity without invalidating live handles; it may
  allocate or fail with the strong guarantee.

### 4.4 Extent and subdivision

- **QT-FR-15 Extent policy.** The client chooses one of:
  - **fixed bounds**: reject out-of-bounds insertion/update explicitly, leaving
    the tree unchanged. A query outside the root is valid and returns any
    actual overlaps; it is not an out-of-bounds mutation;
  - **unbounded/adaptive**: accept every valid rectangle subject to resource
    limits, without requiring an enclosing public `RectT` rectangle. Distant
    outliers remain queryable and do not create an unbounded chain of roots;
  - **snapshot-derived**: bulk build derives its extent from the inputs.
    The caller must explicitly configure whether later insert/update uses
    adaptive growth or rejects out-of-extent geometry; neither behavior is
    implicit. Missing this choice is invalid configuration.

  Recenter/growth must preserve handles and filters. Empty builds, repeated
  expansion/contraction, and extreme-coordinate outliers have regression tests.
- **QT-FR-16 Density-adaptive subdivision.** Subdivision follows occupancy,
  with a real split threshold and configurable depth/storage limits. Empty
  regions do not preallocate a full 4^depth hierarchy. Stop unproductive splits
  (coincident geometry, no representable progress, or no useful redistribution).
  Depth/configuration validation must prevent stack exhaustion and unchecked
  node-count arithmetic. Document merge policy and avoid split/merge thrashing.
- **QT-FR-17 Straddling items.** Boundary crossing must not cause systematic
  near-root pileups for small, spatially separated objects. Evaluate loose
  cells, bounded references, or another approach in an early prototype. Verify
  split-line workloads and translated copies of the same scene using candidate
  and node-visit counts, not a universal claim that boundaries have zero cost.
  Any multi-cell representation must bound references per entry independently
  of covered area and retain exact, unique streaming results without an
  O(number of pairs) deduplication set.
- **QT-FR-18 Size disparity.** A few large objects must not prevent useful
  subdivision for small objects. Test large-vs-small and long-thin rectangles
  separately. Cost depends on entries visited and actual overlaps, not world
  area alone; coordinate scale must not dictate memory use. Large objects may
  use a separate/coarser path, but its false-candidate cost must be measured.

### 4.5 Region queries

- **QT-FR-19 Region query.** Report each matching entry exactly once through
  a visitor, or append to a caller-provided output iterator/container. Never
  clear the caller's existing output. The no-filter overload includes all
  entries, including filter-inert ones. Filtering is explicit (QT-FR-27).
  Visitor operations can be allocation-free with reserved workspace; they do
  not materialize a result list unless ordered output needs per-entry scratch.
- **QT-FR-20 Entry-relative query.** Query an entry by handle without returning
  itself, using its stored geometry and pair-filter rules. A valid handle is
  required; automatic invalid-handle checks are debug diagnostics only. Optional greater-/less-than
  rank bounds require explicit unique ranks (QT-FR-24). Query results are not
  sorted merely because rank bounds are used.

### 4.6 Pair enumeration (broad phase)

- **QT-FR-21 Completeness and uniqueness.** All-pairs enumeration reports every
  unordered pair of distinct overlapping entries that passes the enabled
  filters/predicate exactly once. Equal payload values do not imply identity.
- **QT-FR-22 Streaming.** Deliver pairs through a visitor with O(n) total
  workspace, independent of pair count, including ordered mode and all-overlap
  scenes. Two-tree workspace is O(n + m). Do not collect all pairs internally
  to sort or deduplicate. A caller-owned collection convenience form explicitly
  opts into output-sized memory.
- **QT-FR-23 Early stop.** Visitors return an explicit enum with `Continue`
  and `Stop` values, not a boolean. This applies to region queries, entry
  iteration, and pair enumeration. Returning `Stop` yields a distinct
  stopped-by-visitor status. The tree and workspace remain
  reusable after stop or budget exhaustion; already
  delivered callbacks/output are not rolled back.
- **QT-FR-24 Ordered enumeration.** Rank is an optional client-supplied
  `uint64_t`, immutable for a live entry and unique within an index. Both zero
  and `UINT64_MAX` are valid ranks; absence is represented separately. Duplicate
  explicit ranks are rejected transactionally in every build, independently of
  the diagnostics policy. Insertion/build returns a duplicate-rank status without
  changing existing state or handles. Ordered and rank-bound calls
  require ranks on every entry in the participating sets, even if filters
  would suppress it; missing ranks return an error before pair-predicate or
  result callbacks. Selection membership may first need to run to determine
  the participating sets (QT-FR-26). Within-tree pairs are reported as
  `(lower rank, higher rank)` in ascending lexicographic rank order, independent
  of insertion order, tree
  structure, and addresses. Ordering is per call, not a tree mode, and keeps
  QT-FR-22's bound. A simple algorithm already meets the workspace bound and
  serves as the reference. Sort entries by rank (O(n log n) time, O(n)
  space). For each entry in rank order, run a partner query restricted to higher
  ranks, collect at most n - 1 partners, finish that query, sort them, and emit.
  If a budget expires during discovery, discard that unfinished partner row;
  sorting/emitting its partial discoveries would not guarantee a prefix (a
  lower-ranked partner may remain undiscovered). Earlier completed rows remain
  a valid prefix. Workspace stays O(n). The prototype must therefore show speed,
  not feasibility. Prototype faster variants before freezing the API; document
  rank preparation, repeated traversal, and per-entry sorting costs separately
  from unordered traversal. The same asymptotic runtime is not required for both
  modes. Client predicates must not depend on traversal order to select results.
- **QT-FR-25 Pair orientation.** Unordered mode uses a documented deterministic
  entry-based orientation, never payload addresses or payload comparison.
  Ordered self-enumeration uses rank. Custom predicates receive this same
  orientation as the visitor.
- **QT-FR-26 Two-set enumeration.** Support two distinct trees and disjoint
  selections within one tree. Report only left-set/right-set overlaps, once,
  oriented `(left, right)`; ordered cross output sorts `(left.rank, right.rank)`,
  not `(min rank, max rank)`. Explicit ranks need be unique only within each
  participating tree. Equal payloads across trees are distinct entries. Reject
  a same-tree argument or overlapping one-tree selections before pair-predicate
  or result callbacks; selection-preparation callbacks may already have run.
  Ordinary self-enumeration is a different operation. The initial cross-tree
  API requires the same coordinate type and boundary policy;
  mismatches return an error. Payload and allocator/resource types may differ
  (for example, dynamic entity IDs versus static tile IDs); extent policies
  need not match. Honor both entries' pair filters and test dynamic-vs-static
  use directly. One-tree selections use separate unary membership predicates,
  evaluated once per entry into workspace before
  result callbacks; document this O(n) setup cost. They are distinct from
  post-geometry predicates (QT-FR-30) and category/mask compatibility, since
  category membership can span many bits.

- **QT-FR-34 Moved-set enumeration.** Given a client-supplied set S of live
  handles, report every overlapping pair passing entry filters and any enabled
  predicate with at least one endpoint in S exactly once, including S-vs-S.
  All submitted handles must be live and belong to the index; automatic
  invalid-handle checks are debug diagnostics only. Duplicate handles return
  an error before result callbacks.
  Disjoint selections (QT-FR-26) cannot express this operation.
  - S is a changed/dirty set, not just a list of changed AABBs: include inserted,
    updated, refiltered, and payload-changed entries when payload state affects
    eligibility. Changes to external predicate state require dirtying endpoints
    of every affected pair or falling back to full enumeration. Entries outside
    S have unchanged pair eligibility only under that client precondition.
  - Results contain current overlaps, not overlap-end events. Clients remove
    cached pairs incident to S before replacing them with the complete new
    results, and remove pairs incident to deleted entries separately. Removed
    handles cannot be submitted in S. Section 6.1 defines safe cache replacement.
  - Workspace is O(n) or O(|S|), independent of pair count. Prepared unordered
    calls target O(|S| log n + p_S) on representative scenes, including set
    validation/membership setup. Reusing an O(n) membership buffer must not
    entail an O(n) clear/pass on every call; S empty must not scan all entries.
    Membership construction may take O(|S| log |S|). Publish any exceptional
    epoch-reset work and a way to schedule it outside the step.
  - Early stop, budgets (QT-FR-23/32), orientation (QT-FR-25), and optional
    ordering (QT-FR-24) apply. Ordered mode may pay O(n) rank preparation and
    additional sorting/traversal; it does not inherit the sparse-update target
    unless measured. Any maintained dirty list has explicit memory/clearing rules.

### 4.7 Filtering

- **QT-FR-27 Category/mask filter.** Use `uint64_t` category and mask fields,
  defaulting to all bits set. A pair qualifies only when
  `(a.category & b.mask) != 0 && (b.category & a.mask) != 0`. A filtered region
  query treats its category/mask as another entry. Unfiltered region queries
  bypass this rule; pair/partner queries always apply entry filters.
- **QT-FR-28 Filter pruning.** Conservatively skip whole incompatible subtrees
  before entry-level geometry tests when summaries prove rejection. Updates,
  growth, removal, and shrink must keep summaries correct. Mixed categories may
  defeat summaries, so no universal speedup is promised. Benchmark homogeneous
  and interleaved ignoring groups and record node visits as well as tests.
- **QT-FR-29 Filter updates.** `setFilter(handle, filter)` preserves geometry,
  rank, and handle without allocating. A zero category or zero mask suppresses
  pairs and filtered queries, but not explicit unfiltered region queries.
  A valid handle is required; automatic invalid-handle checks are debug
  diagnostics only.
- **QT-FR-30 Custom predicate.** Optional predicates reject candidates only
  after exact geometry and enabled category/mask tests; they do not drive
  pruning. Region predicates receive one const entry view; pair predicates
  receive two. Self-enumeration predicates must be symmetric under exchanging
  endpoints; cross-set predicates may distinguish `(left, right)`. Ordering
  changes sequence/orientation, not the qualifying pair set. Predicates must
  not mutate either participating index, depend on candidate visitation order,
  or assume thread safety of client-owned state. Eligibility must remain stable
  throughout a call. Since predicates may read
  payload/pointee/external state, clients must also follow QT-FR-34's dirty-set
  rules across calls; changing such state is not automatically tracked.

### 4.8 Work accounting

- **QT-FR-31 Statistics.** Expose live entry/node counts, maximum active depth,
  live and retained bytes, and per-call node visits, entry-level geometry tests,
  filter rejections, predicate calls, and reports. Define a candidate test as
  one entry-vs-entry or entry-vs-query overlap evaluation, counted before its
  outcome is known. Count one rectangle overlap evaluation as one test, not
  one per axis; charge repeated evaluations too. Define node/node-pair visits
  for cross traversal and count revisits. Return per-call counters in operation
  results or caller storage, never mutable tree-global "last query" state.
  Document `stats()` costs if active depth/storage totals require traversal.
  Counters saturate rather than wrap. An uninstrumented specialization/path
  omits optional counter maintenance; do not promise zero cost for runtime toggles.
- **QT-FR-32 Work budget.** Pair enumeration accepts a maximum number of
  candidate tests and optionally node visits. Limits are checked before the
  corresponding unit of work; zero is valid. Return complete, stopped-by-visitor,
  or budget-exhausted status distinctly. Reaching the numeric limit alone is
  not exhaustion if no further charged work is needed. A limited run reports
  no duplicate pair and ordered output is a prefix of the unlimited result.
  The reference buffered algorithm discards unfinished rows (QT-FR-24); another
  traversal may stream a row only if it can prove the same prefix guarantee.
  Budget checks include
  every entry-level geometry evaluation, including repeated discovery passes,
  not only pair emission. Rank and selection preparation, sorting, callbacks,
  predicates, and any unbudgeted structural work have separately documented
  costs: a candidate budget alone is not a wall-clock or hard-real-time limit.
  This release has no continuation cursor. Retrying restarts enumeration;
  clients must discard or reconcile a partial output before retry/fallback,
  not blindly append it or assume work can resume next frame.

## 5. Non-functional requirements

- **QT-NFR-01 Complexity.** Document worst-case costs and their assumptions,
  separating traversal, geometry tests, ordering, and output. Targets for
  representative bounded-size/aspect-ratio, spatially distributed scenes are
  amortized O(log n) mutations, O(log n + k) region queries, and
  O(n log n + p) unordered pairs. Bounded overlap count alone does not guarantee
  these bounds for a quadtree. Sparse adversarial geometry or filtering can
  still cause O(n²) candidate work with few results; characterize this and
  expose budgets rather than promise a universal output-sensitive bound.
  The unfiltered all-overlap unordered workload should take O(n + p) work.
  Ordered mode may add sorting/retraversal costs, documented separately.
  Prepared unordered moved-set enumeration (QT-FR-34) targets
  O(|S| log n + p_S), including membership setup, on the same representative
  scenes. A 1%-moving scene must not pay a hidden full-tree pass every step.
  Ordered moved-set preparation is accounted separately.
- **QT-NFR-02 Memory.** Active index storage is O(n), with explicit bounds on
  depth and references per entry; document any dependence on configured depth.
  Include handle/rank directories, retired-slot bookkeeping, and filter summaries,
  not just geometry nodes. Actual memory includes retained high-water capacity,
  build descriptor scratch, opt-in transactional payload/index staging, and each
  caller's workspace: O(capacity + staging + workspace), not O(current live count)
  after clear. Publish measured bytes per entry, node, reference, and handle,
  plus peak rebuild bytes. No hidden pair-sized cache or unbounded history table.
- **QT-NFR-03 Steady state without allocation.** Reservation covers entries,
  nodes/references, identity/rank bookkeeping, build descriptors, transactional
  staging, mutation rollback scratch, and workspace separately. Specify peak
  temporary needs, not just final topology. Equal entry counts do not imply
  equal topology requirements. Index storage grows automatically by default when
  needed, subject to resource/configuration limits; internal allocation failure
  returns an allocation-failure status. After these capacities are reserved,
  operations within them allocate nothing, even if geometry changes distribution.
  An opt-in hard-capacity mode reports capacity exhaustion instead of growing and preserves
  the strong guarantee, including for builds (QT-FR-13). Prepared query paths
  check all workspace requirements before result callbacks and never grow it;
  convenience ownership/growth is explicit, including for ordered enumeration.
  Shrink and capacity growth are outside this guarantee, as are allocations
  made by client payloads, accessors, predicates, visitors, or output containers.
- **QT-NFR-04 Allocator support.** Internal index, staging, and workspace
  storage use a custom runtime resource with a nonthrowing allocation interface
  that explicitly reports allocation failure. Resources derive from an abstract
  `Resource` base class with virtual nonthrowing `allocate` and `deallocate`
  operations and a virtual nonthrowing destructor. Indexes/workspaces borrow a
  `Resource*`; they do not own or delete the resource. Resource selection is
  runtime, not an allocator template parameter. Standard throwing allocators and PMR
  allocation alone do not satisfy this contract under `-fno-exceptions`.
  Allocation uses `void* allocate(std::size_t bytes, std::size_t alignment)
  noexcept`; the library requests only `bytes > 0`, with no allocation for
  empty storage. Failure returns `nullptr`, which the library translates into an
  allocation-failure status. Deallocation uses `void deallocate(void* ptr)
  noexcept`; no size or alignment is supplied, and `deallocate(nullptr)` must
  be a no-op. The resource must recover any metadata it needs to release the
  allocation. Over-aligned payloads, such as
  `alignas(64)` objects, are supported. Allocation requests carry the required
  alignment; a resource must return storage satisfying it or `nullptr`, never
  silently reduce alignment. The default heap resource supports over-alignment.
  Resource arguments are optional: omission selects a library-provided nonthrowing heap resource with
  process lifetime. Explicit custom resources remain caller-owned; indexes and
  workspaces borrow them. A custom resource must outlive every object retaining
  its storage, including moved-to objects and cleared objects with retained
  capacity. All internal allocations use the selected resource; no allocation
  bypasses it and no failure falls back to another resource. The resource follows
  indexed state through moves, even when source and destination initially use
  different resources. Destination storage is released through its original
  resource before adoption of the source state/resource. Rank sorting, duplicate
  detection, and ownership
  bookkeeping follow the same resource and failure-reporting rules.
- **QT-NFR-05 Exception-free operation and failure channels.** Insert, update, reserve,
  shrink, filter changes, transactional build, and hard-capacity operations give
  the strong guarantee for indexed state and live handles. Default build follows
  QT-FR-13: preparation failure leaves the old state unchanged; failure after
  destructive replacement begins leaves an empty index, never a partial snapshot.
  The new implementation supports exception-disabled builds, including
  `-fno-exceptions`, without library `throw`/`try`/`catch` paths. Library failures use
  explicit status results, including internal allocation failure from index,
  staging, or workspace storage and convenience paths. Capacity, size-arithmetic,
  and identity exhaustion are distinct reported resource errors, not unchecked
  overflow. Initial index creation uses a status-returning factory for invalid
  configuration/root and internal allocation failure (QT-API-01). Index moves
  transfer the resource with the state without allocation or failure (QT-NFR-07).
  All client operations invoked by the library must be nonthrowing:
  payload construction/copy/move/destruction, resource operations, input-range
  traversal, accessors/projections, selection/pair predicates, visitors, and
  output operations. Enforce this at compile time where expressible and
  document remaining preconditions. Throwing client operations are unsupported,
  even when the consuming program enables exceptions; propagation/recovery is
  not promised. Queries leave both participating indexes unchanged. Already
  emitted output and moved-from client arguments are not rolled back. Remove/
  clear do not allocate or throw under QT-FR-08. Perform all fallible preparation
  before commit where a strong guarantee is promised. Internal allocation
  exhaustion must return a status, not terminate through a throwing allocator.
- **QT-NFR-06 Determinism.** Identical operation sequences and numeric semantics
  give identical order on repeated runs. Results never depend on payload pointer
  values, allocation addresses, or randomized hashes. Structural decisions and
  ties use specified entry keys. Ordered results with explicit ranks also match
  across insertion permutations and usage models. Floating-point reproducibility
  requires QT-FR-02's environment; it is not a lockstep physics guarantee across
  different arithmetic environments or client predicates.
- **QT-NFR-07 Value semantics.** The initial API is non-copyable and movable.
  Ordinary move construction transfers state, live handles, and the borrowed
  resource in O(1), without allocation, and is `noexcept`. Move assignment first
  destroys the destination's old payloads and releases its storage through its
  old resource, then adopts the source state and resource. It is `noexcept` and
  allocation-free even when the original resources differ; its cost includes
  destination cleanup, not just the O(1) state transfer. The destination's former
  handles are invalidated; source handles follow the transferred state. Self-move
  assignment is a no-op. A moved-from tree is empty, queryable, and immediately
  reusable without resource rebinding. It retains a borrowed reference to its
  original resource; both the source and destination continue to depend on that
  resource's lifetime. Its next mutation may allocate. No virtual functions are
  required.
  `Workspace` is also non-copyable and movable. Its moves are allocation-free
  and `noexcept`, transferring storage and the borrowed resource. Move assignment
  releases destination storage through its old resource before adoption;
  self-move is a no-op. The source remains empty and reusable with its original
  borrowed resource. Neither workspace may be active in an operation during a
  move; resource lifetime requirements apply to both moved-to and moved-from
  objects.
- **QT-NFR-08 Reentrancy.** Const queries may run concurrently on unmodified
  participating trees using distinct caller-owned workspaces and independently
  safe callbacks. Resources must support any concurrent allocation/deallocation
  they receive, including convenience workspace paths; prepared no-allocation
  queries impose no concurrent allocation demand. Nested read-only calls are
  allowed with separate workspace;
  reusing the active workspace is forbidden. No mutable caches or statistics
  are stored by const operations. Mutation of either participating tree during
  any query callback (including selection/predicate preparation) is forbidden;
  optional runtime diagnostics detect same-thread misuse. This includes mutable
  payload lookup and mutation through aliases, not just geometry changes.
  The caller synchronizes all writes, destruction, and payload pointee access.
  No internal locking or promise to detect cross-thread data races.
- **QT-NFR-09 Configuration independence.** Library-owned class definitions,
  inline definitions, and declarations are ODR-compatible across `_DEBUG` and
  `NDEBUG` when compiler/standard-library ABI settings are identical. Mixing
  incompatible STL debug-iterator modes, packing, or runtime ABIs is not
  supported. No macro-controlled member API or inline-body diagnostic
  differences for the same specialization. Diagnostics use an explicit template
  policy: checked and unchecked indexes are distinct types. The template default
  is unchecked; callers explicitly select checked types for diagnostics. Policy
  definitions and template defaults must not vary with `_DEBUG` or `NDEBUG`; unchecked
  operations compile out automatic handle validation. Introspection is always
  declared or supplied through a separate header.
- **QT-NFR-10 Header-only and portable.** Header-only C++20, warning-free on
  GCC/Clang with `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
  -Wshadow` and on MSVC with `/W4 /permissive-`, for every supported coordinate
  type, with `_DEBUG` both set and unset. No compiler extensions or
  target-specific flags.
  `Index` and `Workspace` are runtime-only; plain geometry helpers may remain
  `constexpr` (section 8). Headers include their own dependencies and compile
  standalone. Examples use value conversions for foreign rectangle structs,
  not `reinterpret_cast` type-punning based on layout similarity.
- **QT-NFR-11 Small scenes.** Benchmark 0–64 entries against brute force,
  including construction/rebuild and visitor dispatch rather than just traversal.
  A direct small-set path is allowed. Acceptance ratios are set in section 10.
- **QT-NFR-12 Diagnostics.** Invalid configuration/root has an explicit factory
  status result. Invalid geometry, fixed-bound rejection,
  missing/duplicate ranks, incompatible cross-tree policies, insufficient
  workspace/capacity, internal allocation failure,
  and identity/size exhaustion have explicit status results. Publish which
  operations can return each status and their state/output guarantees (QT-FR-13,
  QT-NFR-05). Mark failure-bearing results `[[nodiscard]]`. Valid handles are
  a precondition for handle-taking operations except explicit `valid(handle)`;
  automatic stale/foreign-handle checks are debug diagnostics only. A checked
  operation reports an invalid-handle programming error and terminates without
  throwing; explicit `valid(handle)` instead returns false. Callback mutation
  and invalid storage aliasing are also preconditions with optional
  diagnostics; do not confuse misuse with routine validated API errors.

## 6. API changes

Names below are provisional and illustrate the shape of the interface, not the
final spelling.

| ID | Change |
|---|---|
| QT-API-01 | Runtime configuration: extent/root policy, subdivision and storage limits, boundary policy. Coordinate/payload types and the explicit diagnostics policy are template parameters; a custom runtime resource may be supplied explicitly, otherwise a library-provided nonthrowing heap resource is used. A status-returning factory validates config/root and returns either a valid index or an explicit error. Invalid configuration/root and internal allocation failure do not escape as exceptions or produce a usable index. |
| QT-API-02 | `insert(rect, payload[, filter][, rank]) -> InsertResult {status, handle}`. Replaces `add(item) -> bool`; failure never yields a usable handle. |
| QT-API-03 | `update(handle, rect)`, `remove(handle)`, `setFilter(handle, filter)` return status. `rect(handle)`, `payload(handle)`, `filter(handle)`, and `rank(handle)` require valid handles and return access; automatic handle checks are debug diagnostics only. `rect(handle)` returns canonical `Box` edges. `payload(handle)` also has a mutable overload, but eligibility-changing edits follow QT-FR-34's dirty-set rules. Geometry/filter change only through mutators; rank is immutable. |
| QT-API-04 | `clear()`, `build(range[, handleOutput][, options])` with an opt-in transactional mode and explicit preparation workspace, capacity-aware `reserve(...)`, `shrink()`. Specify the build element type (`BuildEntry {box-or-rect, payload, filter, optional rank}` or documented projections), stable-range/move-only consumption rules, and aliasing preconditions. Reservation names entry/node/reference/descriptor/staging needs, not a topology-independent `reserve(n)` promise. Hard-capacity builds retain the strong guarantee. |
| QT-API-05 | `query(rect[, filter], visitor[, options])` and a distinctly named output-iterator/container adapter to avoid callable/container overload ambiguity. Returns status and optional per-call counters. |
| QT-API-06 | `queryPartners(handle, visitor[, options])`, including optional rank bounds. |
| QT-API-07 | `forEachPair(visitor[, options])`. Options: ordering, predicate, budgets, instrumentation, workspace. Returns completion status/counters. Replaces `queryAll(vector<pair>&)`; collection is a separate convenience adapter. |
| QT-API-08 | `forEachPair(otherTree, visitor[, options])` and a disjoint-selection overload for one tree (QT-FR-26). |
| QT-API-09 | `stats()` reports index/storage statistics, not shared last-query counters. Const node introspection is always declared or supplied by a separate debug header. |
| QT-API-10 | Explicit geometry is primary; accessor-based insert/build adapters accept stateful callables and snapshot their result. |
| QT-API-11 | `Filter {uint64_t category, uint64_t mask}`, default all bits set. |
| QT-API-12 | Allocator-aware caller-owned `Workspace` with explicit reservation; separate workspace per concurrent or nested call. Prepared calls check capacity before result callbacks and report insufficient workspace without growing. Include build metadata/rank-validation scratch in documented reservation recipes. Convenience ownership/allocation is explicit. |
| QT-API-13 | `[[nodiscard]]` status/result types distinguish success, invalid configuration, invalid input, out-of-bounds, rank/policy errors, insufficient workspace/capacity, allocation failure, identity/size exhaustion, visitor stop, and budget exhaustion. Publish the per-operation failure/status matrix (QT-NFR-05/12). C++20 must not require C++23 `std::expected`. |
| QT-API-14 | `size()`, `empty()`, `valid(handle)`, `forEachEntry(visitor[, options])` (QT-FR-35). `Box` edge-form overloads for insert/update/build/query (QT-FR-33). |
| QT-API-15 | `forEachPairInvolving(span<const Handle> moved, visitor[, options])` (QT-FR-34). |

Visitors receive a const entry view containing handle, stored rectangle, payload
reference, filter, and optional rank. The view itself is valid only for the
callback's duration. Borrowed payload references and lookup references remain
valid only until the next non-const operation, move, or destruction; handles have
the stronger lifetime in QT-FR-07. Visitors return an explicit enum with
`Continue` and `Stop` values (illustrated as `QuadTree::Continue` and
`QuadTree::Stop`); document predicate signatures before implementation.
The entry/pair whose callback requests stop
has already been delivered and is included in report counts. Preparation
accessors/membership predicates may run before a validation error; their client
side effects are not rolled back even though no result callback runs. Client
callbacks must not retain entry views as persistent identifiers.

Illustrative per-step broad phase in a game (AABB candidates, not confirmed
narrow-phase contacts):

```cpp
auto created = QuadTree::Index<EntityId, int>::create(config);
if (!created.ok())
    return handleCreateFailure(created);
auto index = std::move(created).value();      // provisional factory result API
QuadTree::Workspace<int> workspace;           // caller-owned, reused every step
// Reserve index and workspace for the measured workload before the loop.
// Every simulation step: colliders carry (rect, id, filter, unique rank).
auto built = index.build(colliders, {.workspace = &workspace});
if (!built.ok())
    return handleBuildFailure(built);          // old snapshot must not be used as current

auto result = index.forEachPair([&](const auto &a, const auto &b) noexcept
{
    handleCandidate(a.payload, b.payload); // nonthrowing client code; a.rank < b.rank
    return QuadTree::Continue;
}, {.ordered = true, .workspace = &workspace});
if (!result.complete())
    return handlePartialResult(result);        // discard/reconcile partial candidates
```

Illustrative persistent use for static level geometry:

```cpp
auto inserted = index.insert(rect, wallId, {Wall, Player | Projectile | Explosion});
if (!inserted.ok())
    return handleInsertFailure(inserted);
auto wall = inserted.handle;
check(index.update(wall, movedRect));             // retains handle; failure keeps old rect
check(index.query(explosionArea, {Explosion, Player | Wall}, visitor)); // both masks must agree
check(index.remove(wall));
```

### 6.1 Persistent broad-phase cache protocol

Moved-set output alone cannot maintain a correct pair cache: pairs that cease
intersecting or become filter-incompatible are absent, not reported as removals.
For a complete old cache C and current live dirty set S:

1. Record removed handles and changed live handles separately; include
   eligibility-changing payload/predicate dependencies (QT-FR-34).
2. Enumerate current pairs P_S involving S into a replaceable client delta.
3. Only on completion, discard old pairs incident to S or removed handles,
   then insert P_S. Preserve only old pairs whose live endpoints are both
   outside S. Partial/stopped/budgeted output is not an authoritative update;
   discard/reconcile it and retry or run a complete fallback.
4. Use client adjacency/bookkeeping to avoid scanning the full cache for every
   dirty proxy. Client delta/cache memory is output-sized and outside the
   library's O(n) workspace bound.

Test separation, deletion, filter disable/re-enable, and eligibility changes
without AABB changes. Retained AABB pairs are not confirmed contacts: the client
still runs any required narrow phase. Key persistent caches by entry handles;
multiple proxies with the same payload remain distinct. Snapshot builds invalidate
handles, so cross-frame caches instead need stable client collider IDs, not just
body/entity IDs when one body has several colliders. Changes to a static tree
also require refreshing affected dynamic-vs-static cached pairs.

## 7. Existing defects to fix

Each correctness defect needs a regression test. Stable IDs also retain the
original design limitations below; corrected allegations are not presented as
confirmed bugs. Fix the currently supported API in the safety-only patch,
without promising the new handle API or adding compatibility wrappers.

| ID | Defect | Required behavior |
|---|---|---|
| QT-FIX-01 | Removal routes by current geometry; changing it first can leave an old entry indexed. The README currently forbids such changes: this is an API hazard, not a failure under that precondition. | The new stored-geometry/handle API removes that precondition (QT-FR-06/07/12). |
| QT-FIX-02 | Missing-item removal is already guarded in `doRemoveItem`; no invalid erase was found. Regression coverage is missing. | Preserve safe missing removal in the old API; the new handle API requires valid handles, with debug diagnostics and explicit `valid(handle)` checks. |
| QT-FIX-03 | Copy and move are explicitly deleted; shallow-copy double deletion is not a current defect. Lack of transfer is an API limitation. | Movable, initially non-copyable state (QT-NFR-07), with allocator-aware guarantees. |
| QT-FIX-04 | Far-edge arithmetic (`add`, `getQuadrant`) overflows when an item's `x + w`/`y + h` is not representable; centre/`computeBox` arithmetic overflows for such roots. The `{x, w}` root cannot cover the full integer range. | QT-FR-02/33. |
| QT-FIX-05 | `add` returns false for out-of-root items. Callers that ignore the result lose collisions. | QT-FR-15. Fixed-bounds rejection must be explicit and hard to ignore (for example, `[[nodiscard]]`). |
| QT-FIX-06 | `_DEBUG`-dependent member declarations violate ODR across mixed translation units; current instance data layout is unchanged. | QT-NFR-09. |
| QT-FIX-07 | A virtual destructor is an unnecessary extension/size cost, not a proven correctness failure. | Remove it in the major-version redesign unless a base-class contract is intentional. |
| QT-FIX-08 | `nodeItems` only reserves; full depth is preallocated, and even depth 0 allocates four unused children. | QT-FR-16; verify depth 0/1 and constructor-allocation failures. |
| QT-FIX-09 | `queryAll` collects every pair into one vector. | QT-FR-22 (collection uses a separate new-API convenience adapter, not the legacy form). |
| QT-FIX-10 | Floating-point child bounds can round inward; subtree pruning then loses valid query hits and pairs. | Conservative bounds and nonprogress termination (QT-FR-02/04); preserve both section 2 reproducers. |
| QT-FIX-11 | Invalid roots, negative extents, and nonfinite geometry are not consistently validated. | QT-FR-03 on every geometry entry point, transactionally. |
| QT-FIX-12 | Demo reference queries reinterpret unrelated rectangle types (`(RectLocal<RectT>*)&query_rect`), and the README advertises `SDL_Rect`-style substitutes that only work through aliasing. CMake's declared 3.5 minimum predates `target_link_options` (3.13). Debug detection uses `CMAKE_BUILD_TYPE`, which multi-config generators ignore. Warnings are enabled only where `_DEBUG` code is compiled out, which hides the `enumerateNode` sign-compare warning. | Safe value conversion, an honest tested CMake minimum, per-configuration generator expressions, standalone-header/include checks, and warning-clean optional targets in every configuration. |
| QT-FIX-13 | `IntType`/`FloatType` accept `bool`, character, and unsigned types. The header relies on transitive includes for `std::equal_to`, `std::pair`, `std::next`, and `NULL`; `quadtree-debug.h` does the same for `std::string`. | QT-FR-01 constraints; each header includes what it uses (QT-NFR-10), verified by a one-header translation unit per header. |

## 8. Compatibility and versioning

- This is a new major version with no deprecated legacy compatibility layer.
  Old `add(item)` / `remove(item)` calls migrate to explicit geometry and entry
  handles. Equality-based removal is not retained: duplicate payloads identify
  distinct entries, and the new core API imposes no payload equality requirement.
  Provide a migration guide instead of compatibility wrappers.
- The README maps each old call to its replacement and calls out supported
  coordinate types (including any loss of implicit unsigned/character support),
  the narrower floating-point domain, stored geometry, handle invalidation,
  fixed-bound errors, and visitor/output lifetime.
- The current API is `constexpr` throughout. The new `Index` and `Workspace`
  API is runtime-only, with no constant-evaluation guarantee. Document this
  change in the migration notes. Plain geometry helpers may still be
  `constexpr`; this does not imply compile-time index support.
- `QuadTree::Rect` remains a standard-layout aggregate `{x, y, w, h}`, joined by
  the edge-form `QuadTree::Box` (QT-FR-33). Foreign
  rectangles convert by value or a documented adapter. Similar layout does not
  authorize aliasing through `reinterpret_cast` or promise ABI compatibility
  with arbitrary third-party structs.

## 9. Build, packaging, and documentation

- A CMake `INTERFACE` target and alias (for example, `QuadTree::QuadTree`),
  exporting only include paths and `cxx_std_20`. Support `add_subdirectory`,
  `FetchContent`, and installed package config/version files with exported
  targets and relocatable build/install include paths. Install public headers
  and the license, not demo/test helpers. Use a tested minimum such as CMake
  3.21, the first version with `PROJECT_IS_TOP_LEVEL` for subproject-off
  defaults. The current 3.5 declaration cannot support `target_link_options`
  (3.13) and sits at the CMake 4.x removal boundary for old policies.
- Demo, tests, and benchmarks are separately optional, off as a subproject.
  Register tests with CTest; keep benchmark/reference dependencies test-only.
  Do not set parent-wide standard/flags or force a build type. Optimization,
  LTO, `-march=native`, alternate standard libraries, and linker choices are
  opt-in and private to optional targets, never interface usage requirements.
- CI builds GCC/libstdc++, Clang/libstdc++, Clang/libc++, and MSVC, in debug
  and release configurations, including a downstream installed-package smoke
  test and mixed-`_DEBUG`/`NDEBUG` multi-translation-unit consumer with identical
  standard-library ABI settings.
- The README covers:
  - both usage models;
  - overlap semantics, including degenerate rectangles;
  - extent policies;
  - ordering guarantees;
  - filtering;
  - budgets, partial-result handling, and the absence of resumable traversal;
  - coordinate/FP environment, `Rect` vs `Box` input and rounding,
    touch-inclusive contacts, and explicit skin bounds;
  - full vs moved-set pair enumeration, dirty eligibility, deletion/separation,
    and the cache-replacement protocol (section 6.1), including partial failure;
  - reserve/workspace recipes, hard-capacity builds, descriptor scratch, and
    default versus transactional peak rebuild memory;
  - payload/view/handle lifetimes, rank uniqueness, and move/allocator behavior;
  - measured complexity and workload limitations;
  - exception-disabled support, nonthrowing client requirements, and threading guarantees;
  - a dynamic-vs-static engine example and client-owned contact-cache keys;
    snapshot handles must not be used as stable cross-frame entity IDs.

## 10. Verification and acceptance

- **Independent reference oracle.** Brute-force comparison uses entry identity,
  not payload equality or counts. It shares no overlap helpers or traversal
  arithmetic with the index. The oracle compares stored `Box` edges directly. It
  checks `Rect`-to-edge conversion and overflow rejection independently, with
  test-only wide arithmetic or exact fixtures, never the same `RectT` addition
  as the index. Floating fixtures normalize endpoints per QT-FR-02 and test
  adjacent representable values explicitly. Compare query identity multisets
  (to detect duplicate hits), pair multisets, and exact ordered sequences;
  independently test filter and policy truth tables. Full O(n²) oracle runs
  are for bounded test sizes, not every 10^5-entry benchmark.
- **Scenario matrix.** Cover both usage models and extent/boundary policies,
  ordered/unordered pairs, filtering, and supported coordinate types:
  - sparse uniform, clustered, rows, columns, and long-thin rectangles;
  - all-overlap piles, coincident boxes, repeated points, and zero-result scenes;
  - split lines, sibling edge/corner contacts, and translated copies of scenes;
  - mixed sizes, distant outliers, empty trees, and empty rebuilds;
  - points/segments against boxes and each other, on every box/root boundary;
  - `INT32`/`INT64` limits, full-range `Box` entries `[MIN, MAX]`, rejected
    `Rect` input whose far edge is outside the coordinate range, adaptive spans
    that cannot fit in a public width;
  - finite float/double `Box` endpoints `[-max, +max]` whose span overflows,
    zero-span snapshot-derived extents, and reversed min/max inputs;
  - `Box` vs `Rect` input of the same geometry, including floating-point
    min/max values whose `{x, w}` round trip changes the far edge;
  - `nextafter` neighbors, subnormals, huge origins with tiny extents,
    collapsed/overflowing endpoints, NaN/infinity, and signed zero;
  - invalid roots/configuration, fixed-bound update failure, valid external
    queries, repeated adaptive growth/shrink, and the section 2 float reproducer;
  - duplicate payloads with distinct handles, missing/duplicate/extreme ranks,
    insertion permutations, and 0/1/2-entry trees;
  - homogeneous and interleaved filter groups, zero/all-bit filters, filter
    changes without geometry changes, symmetric self-predicates across ordering/
    usage models, asymmetric cross-set predicates, and all-reject predicates;
  - dynamic/static cross-tree pairs, unequal set sizes, different payload and
    allocator types, equal payloads across trees, incompatible policies, and
    overlapping one-tree selections;
  - moved-set enumeration with empty, single, all-entry, duplicate, and stale
    handle sets (stale handles through `valid(handle)` and debug diagnostics,
    not unchecked release calls), and pairs inside the set; apply section 6.1's replacement rule
    and compare with full enumeration after separation, removal, refiltering,
    payload/predicate eligibility changes, and aborted/failed updates. Measure
    setup independently at |S| = 0, 1, and 1% to detect hidden O(n) passes.
- **Randomized and property tests.** Check operation sequences including
  insert/update/remove/clear/build/reserve/shrink/setFilter/move against the
  oracle after every step. Preserve failing seeds and minimized fuzz inputs.
  Test slot reuse and stale/foreign detection through `valid(handle)` and debug
  diagnostics between instances of the same specialization. Use compile-time
  rejection tests for cross-specialization handle use, without invoking unchecked
  release operations with invalid handles. Use subprocess/death tests for checked
  invalid-handle calls: verify diagnostic reporting and termination without
  exceptions, including with `NDEBUG` defined. Cover
  destruction/address reuse within one module and multi-translation-unit identity
  uniqueness in that domain, forced
  identity exhaustion through a test policy, moves/self-move, and failed builds.
  Test preparation failures (unchanged), destructive default-build failures
  (empty), and transactional/hard-capacity failures (unchanged). Mutate client
  rectangles after insertion and verify the snapshot is unchanged. Count accessor
  and projection calls; verify prepared geometry is reused, move-only input is
  not consumed before validation, and explicitly staged self-rebuilds are safe.
- **Stop/budget tests.** Exercise zero/one/exact/insufficient candidate and
  node budgets and stop after every output prefix, including ordered, moved-set,
  and cross-tree output. Force budget exhaustion mid-row where traversal discovers
  a higher-ranked partner before a lower-ranked one; no unfinished row may leak
  non-prefix output. Test budgets exhausted before the first report despite many
  discovered candidates, and exact limits that still complete. Retry tests show
  safe discard/reconciliation rather than duplicate output or corrupt caches.
- **Sanitizers and concurrency.** Run ASan/UBSan and standard-library debug
  modes. Run concurrent/nested read-only query tests with distinct workspaces;
  use TSan where available. Sanitizers do not prove aliasing or arithmetic
  correctness, so retain independent oracle and standalone-header tests.
- **Allocation/memory tests.** Counting/failing resources verify warmed and
  explicitly reserved operations with changing geometry distribution, ordered
  and unordered output, cross-tree traversal, and failed hard-capacity calls.
  Test node/reference/descriptor/staging/workspace limits separately, filter-only
  changes, clear/rebuild churn, rank/identity directories, and shrink. Verify
  hard-capacity build failure preserves old handles even with default build
  options. Check all internal paths (including sorting and duplicate detection)
  use the requested resource. Pair-count growth must not grow workspace; measure
  default/transactional rebuild peaks including preparation scratch.
- **Exception-disabled and failure tests.** Compile and run the new API with
  `-fno-exceptions` on GCC/Clang and the documented exception-disabled MSVC
  configuration. Inject reported allocation failure at every internal allocation
  site, including convenience workspace paths; verify errors return without
  termination. Distinguish preparation from commit failures and verify the
  published failure/status matrix, state, handle-output span, resource cleanup,
  and workspace reuse. Cover move-only payloads and moves between indexes with
  different resources; verify allocation-free transfer and cleanup through the
  correct original resource.
  Enforce documented nonthrowing constraints with compile-time rejection tests
  for unsupported payload/callback/resource operations.
- **Reproducible benchmarks.** Publish fixtures or specify both the PRNG and
  integer-to-coordinate mapping, not just a seed: `default_random_engine` and
  standard distributions do not specify identical sequences across libraries.
  Publish compiler flags, hardware, warm-up/repetition counts, and raw results.
  Include 0–64, 10², 10³, 10⁴, and 10⁵ entries where output permits.
  Measure build + pairs
  for snapshots; update + pairs at 0%, 1%, 10%, and 100% movement for persistent
  use, with both full and moved-set enumeration; report no-op, small-motion, teleport, and spawn/despawn churn separately.
  Report median and p95 step time, candidate/node counts, allocations, retained
  and peak bytes, and ordered-mode overhead. Use streaming checksums/counters,
  not pair vectors, and compare equivalent filter/boundary semantics.
- **Baselines and limits.** Compare brute force, sort-and-sweep, and a simple
  uniform grid/spatial hash on appropriate workloads. A maintained dynamic-AABB
  tree baseline is required for persistent-motion claims; pin baseline versions
  and include any exact-result refinement and update costs. Compare unordered
  and ordered modes separately, including any
  baseline sorting. Do not require universal dominance over sweep: sweep and
  trees have different adversarial cases. Cap dense runs by n, emitted pairs,
  or elapsed time and mark capped/incomplete results explicitly. At 10^5 entries
  an all-overlap case emits 4,999,950,000 pairs; it is not a routine full-matrix
  CI run. Zero-result cases use work counts and time, never cost per pair.
- **Acceptance gates.** Before scalability implementation, publish a benchmark
  manifest naming scenes, sizes, hardware, baseline, and numeric ratio/work
  limits. Initial targets: small-scene total cost at most 2x brute force;
  representative sparse/clustered/split-line/mixed-size step cost at most 2x
  the best applicable baseline. Batch small/empty cases and publish absolute
  overhead; do not divide by zero or timer-noise-scale brute-force timings.
  Calibrate these provisional ratios with the prototype and freeze them before
  the scalability phase, not after seeing final results. On the fixed low-overlap
  split-line/mixed-size fixtures, candidate counts must not grow quadratically
  as n increases; compare both
  original and translated scenes. Record explicit pathological exceptions and
  their budget/fallback guidance. Correctness, allocation, and memory bounds
  are hard gates, not waived by faster timings.

## 11. Delivery phases

1. **Contract and feasibility gate.** Resolve section 12, approve the numeric,
   handle/rank, allocator, and workspace contracts, and prototype straddling
   and mixed-size handling. Measure the reference ordered algorithm
   (QT-FR-24) against unordered mode. Establish benchmark
   baselines and freeze workload-specific acceptance limits before selecting
   the storage/traversal strategy. Failure of a prototype changes the design
   or scope before the public API is frozen.
2. **Safety baseline.** Add independent oracle/regression tests, especially
   QT-FIX-01/02/04/06/10/11/12/13, plus sanitizers and a CMake interface target.
   Include/ODR/arithmetic fixes may form a maintenance patch for the old API;
   coordinate-type narrowing, handle guarantees, safe moves, and removal of
   virtual dispatch belong to the major-version core.
3. **Core API.** Implement QT-FR-01…14, 19, 21–23, 33, 35 with stored
   edge-form geometry, debug-checked handle preconditions and explicit
   `valid(handle)`, both default and transactional
   build with the preparation/failure contracts, streaming output, allocator-aware
   moves, reservation/hard-capacity paths, and explicit workspace/statistics.
   Carry filtering/rank metadata now to avoid a later ABI/API redesign.
4. **Scalability.** Adaptive extent/subdivision, straddling and mixed-size
   handling (QT-FR-15…18), allocation/memory gates, and the frozen benchmark
   manifest. Verify transactional staging under changed distributions.
5. **Game broad phase.** Filter pruning, ordered mode, partner/cross-tree
   enumeration, moved-set enumeration, and work budgets (QT-FR-20, 24–32, 34). Promote the feasibility
   prototypes to tested implementations; rerun the full oracle matrix.
6. **Release.** Migration guide, examples, installed-package and compiler CI,
   and final acceptance runs. No legacy compatibility layer is included.

## 12. Decisions and remaining gates

Approved decisions:

- Integer geometry uses exact, overflow-safe arithmetic throughout. Reject
  unrepresentable `Rect` endpoints before addition; splits and span arithmetic
  must not overflow. Preserve full signed 64-bit precision in portable C++20,
  without floating-point conversion or mandatory compiler extensions
  (QT-FR-02/33).
- `Index` is non-copyable and movable: copy construction and copy assignment
  are deleted, with no deep-copy API in the initial release. Moves transfer
  indexed state and live handles under the approved resource/move rules; clients
  explicitly rebuild when they need another index (QT-NFR-07).
- Concurrent and nested read-only queries are supported on unmodified indexes,
  with a separate workspace for each active call. Callbacks and resources must
  support any concurrent use they receive; participating indexes must not be
  mutated. Queries use no mutable tree-global caches or last-query statistics
  (QT-NFR-08).
- Borrowed payload and lookup references are valid only until the next
  non-const operation, move, or destruction of the index. Clients reacquire
  references after mutation; handles retain their stronger validity guarantees
  (section 6, QT-FR-07).
- `payload(handle)` provides mutable access outside query callbacks. Clients
  must include entries whose payload edits change pair eligibility in the dirty
  set for moved-set enumeration. Mutation of participating indexes, including
  payload mutation through aliases, is forbidden during query callbacks
  (QT-API-03, QT-FR-34, QT-NFR-08).
- Live handles remain valid across `update`, `reserve`, and `shrink`, including
  geometry changes and storage relocation. Clients need not replace handles or
  cache keys for those operations (QT-FR-07/11/14).
- Different index specializations use distinct handle types, rejecting
  cross-specialization handle use at compile time. Foreign handles from another
  instance of the same specialization are still detected by `valid(handle)` and
  checked operations. Cross-index enumeration may expose different left/right
  handle types (QT-FR-07/26).
- Handle identity is supported within a single module domain in the initial
  release, including all of that module's translation units. Cross-module
  handle/index exchange and survival across owning-module unload/reload are
  unsupported; no shared cross-module identity provider is required. Exact
  foreign/stale detection and address-reuse protection still apply within the
  supported domain (QT-FR-07).
- Enumeration budgets limit candidate tests and optionally node visits. Charge
  every entry-level rectangle overlap evaluation, including repeated tests,
  before performing it; one rectangle evaluation is one test, not one per axis.
  Report counts alone do not define the work budget. Preparation, sorting, and
  callbacks have separately documented costs; these budgets are not wall-clock
  limits (QT-FR-31/32).
- Self-enumeration predicates must be symmetric under exchanging endpoints,
  so pair orientation or ordering mode cannot change eligibility. Cross-set
  predicates may distinguish left from right (QT-FR-30).
- Custom pair predicates are supported after exact geometry and category/mask
  checks. They may reject pairs but do not drive pruning; predicates must be
  nonthrowing and must not mutate either participating index (QT-FR-30,
  QT-NFR-05/08).
- The initial release supports cross-pair enumeration between two disjoint
  selections within one index. Evaluate each membership predicate once per
  entry into workspace, with O(n) preparation. Overlapping selections return
  an error before pair-predicate or result callbacks; membership callbacks may
  already have run (QT-FR-26, QT-API-08).
- The initial release includes pair enumeration between two distinct indexes,
  including different payload types and resources. Coordinate types and boundary
  policies must match; both entries' filters apply. Output contains only
  left/right cross pairs, oriented `(left, right)`; ordered output sorts by
  `(left.rank, right.rank)`, with rank uniqueness required only within each
  index (QT-FR-26, QT-API-08).
- Persistent contact caches and begin/end events remain client-owned. The
  library reports current qualifying AABB overlaps, not contact history or
  confirmed narrow-phase contacts; clients maintain pair history and derive
  events using the completion/cache protocol in section 6.1 (QT-FR-34).
- Snapshot-derived extent policy requires an explicit configuration choice for
  later out-of-extent insertion/update: adaptive growth or rejection. Neither
  behavior is implicit; omitting the choice is invalid configuration (QT-FR-15).
- `clear()` destroys all live entries and invalidates their handles while
  retaining allocated capacity for reuse. Explicit `shrink()` releases unused
  capacity; `clear()` does not allocate (QT-FR-13/14, QT-NFR-05).
- Index storage grows automatically by default when needed, subject to resource
  and configuration limits. Internal allocation failure returns a status.
  An explicit hard-capacity mode forbids automatic growth and returns
  capacity-exhausted status instead, preserving the required state/handle
  guarantees (QT-NFR-03/05).
- Provide convenience query/enumeration calls that may allocate or grow
  workspace, distinctly exposed from prepared no-allocation calls. Internal
  workspace allocation failure returns an allocation-failure status; allocation
  uses the selected runtime resource (QT-NFR-03/04/05, QT-API-12).
- Prepared queries/enumerations check workspace capacity before any result
  callback. Insufficient capacity returns an insufficient-workspace status;
  prepared calls never allocate or grow workspace. Index state is unchanged
  and workspace remains reusable (QT-NFR-03, QT-API-12).
- Visitors use an explicit `Continue`/`Stop` enum, not a boolean, for region
  queries, entry iteration, and pair enumeration. `Stop` returns the distinct
  stopped-by-visitor status; the entry/pair delivered to that callback remains
  included in report counts (QT-FR-23/35).
- Enumeration stops with budget-exhausted status when further charged work
  would exceed the budget; no continuation cursor is provided. Retrying starts
  from scratch, so clients must discard or reconcile partial output rather
  than blindly append it. Reaching the numeric limit alone does not imply
  exhaustion when no further charged work is needed (QT-FR-32).
- The initial release includes moved-set pair enumeration (QT-FR-34): report
  each current qualifying pair involving at least one supplied live dirty handle
  exactly once, including dirty-set internal pairs. Dirty sets cover all
  eligibility changes, not just geometry. Cache maintenance follows section 6.1:
  replace affected pairs only after complete enumeration and handle deletions
  separately. Prepared unordered setup scales with dirty-set size rather than
  a mandatory full-index pass; ordered preparation has separate costs.
- Region queries without an explicit filter bypass entry category/mask filtering
  and include every geometric match, including zero-category or zero-mask
  entries. Pair and partner queries always apply entry filters (QT-FR-19/27).
- Default filters set all bits in both `uint64_t` category and mask fields.
  Default-filtered entries are mutually compatible; explicitly supplied filters
  still apply the two-way acceptance rule (QT-FR-27, QT-API-11).
- Collision filters use fixed `uint64_t` category and mask fields. A pair
  qualifies only when `(a.category & b.mask) != 0` and
  `(b.category & a.mask) != 0`; both directions must accept. Configurable mask
  widths are outside the initial release (QT-FR-27, QT-API-11).
- Duplicate explicit ranks are rejected in all builds, independently of the
  diagnostics policy. Insertion/build returns a duplicate-rank status without
  changing existing indexed state or handles (QT-FR-13/24).
- Ordered enumeration uses optional client-supplied `uint64_t` ranks, unique
  within each index and immutable for each live entry. Ordered calls require
  ranks on every participating entry; ordering is selected per call (QT-FR-24).
- Repeated insertion of the same payload creates independent entries with
  distinct handles. The index never deduplicates by payload and requires no
  payload equality operation (QT-FR-08/10).
- The default diagnostics template policy is unchecked in every build.
  Callers explicitly select the checked type when diagnostics are wanted;
  `_DEBUG` and `NDEBUG` never change this default (QT-NFR-09).
- Checked handle-taking operations report an invalid-handle programming error
  and terminate without throwing, before mutation or result callbacks. They do
  not return an invalid-handle status. Explicit `valid(handle)` remains a
  nonterminating validity check that returns false for invalid handles
  (QT-FR-07/35, QT-NFR-12).
- Debug diagnostics are selected by an explicit template policy. Checked and
  unchecked indexes are distinct types; unchecked operations compile out
  automatic handle validation without a runtime diagnostics branch. Policy
  definitions/defaults do not depend on `_DEBUG` or `NDEBUG`, preserving ODR
  compatibility. Checked types remain usable in optimized builds (QT-NFR-09).
- Automatic stale/foreign-handle validation is debug-only. Handle-taking
  operations require valid handles in unchecked release builds; invalid use has
  no defined result or safe-rejection guarantee. Explicit `valid(handle)` remains
  checked in every build. Diagnostic configuration must preserve QT-NFR-09's
  cross-translation-unit ODR guarantees (QT-FR-07/35, QT-NFR-12).
- One `Index` type supports incremental `insert/update/remove` and full snapshot
  replacement through `build`, sharing stored state, query semantics, and retained
  capacity. Workspace remains a separate caller-owned type (section 3).
- The major release includes no deprecated legacy compatibility layer. Provide
  a migration guide from old calls to the new API; do not retain equality-based
  `remove(item)` or require payload equality (section 8).
- `Index` and `Workspace` are runtime-only; compile-time evaluation of their
  operations is not supported. Plain geometry helpers may still be `constexpr`.
  Migration notes must document this change (QT-NFR-10, section 8).
- `deallocate(nullptr)` is valid and must be a no-op for every resource
  (QT-NFR-04).
- The library never requests zero-byte allocations: resource allocation calls
  require `bytes > 0`, and empty storage needs no allocation (QT-NFR-04).
- Custom resources derive from an abstract `Resource` base class with virtual
  nonthrowing `allocate`/`deallocate` operations and a virtual nonthrowing
  destructor. Indexes/workspaces borrow a `Resource*`; this does not require
  virtual functions on `Index` or `Workspace` (QT-NFR-04).
- Over-aligned payloads are supported. Allocation requests specify the required
  alignment; resources return suitably aligned storage or `nullptr`. The default
  heap resource supports over-alignment. Pointer-only deallocation must correctly
  release these allocations as well (QT-NFR-04).
- The runtime resource deallocation operation is
  `void deallocate(void* ptr) noexcept`, with no size or alignment arguments.
  The resource must recover any metadata needed to release the allocation
  (QT-NFR-04).
- The runtime resource allocation operation is
  `void* allocate(std::size_t bytes, std::size_t alignment) noexcept`.
  Failure returns `nullptr`; the library translates it into an explicit
  allocation-failure status (QT-NFR-04/05).
- Index and workspace resource arguments are optional. Omission selects a
  library-provided nonthrowing heap resource with process lifetime; allocation
  failure returns a status. Explicit custom resources remain caller-owned and
  borrowed. All internal allocations use the selected resource, with no fallback
  to the default resource on custom-resource failure (QT-NFR-04, QT-API-01/12).
- `Workspace` follows the index's resource/move rules: non-copyable,
  allocation-free `noexcept` moves, destination cleanup through its old resource,
  and self-move as a no-op. The source remains empty and reusable with its
  original borrowed resource. Active workspaces must not be moved (QT-NFR-07/08).
- A moved-from index retains a borrowed reference to its original resource.
  It is empty, queryable, and immediately reusable without resource rebinding;
  its next mutation may allocate. Both source and destination continue to depend
  on that resource's lifetime (QT-NFR-07).
- The borrowed resource follows indexed state through moves. Move assignment
  releases destination payloads/storage through its old resource, then adopts
  the source state/resource, without allocation. Moves are `noexcept`; move
  assignment includes destination cleanup cost. Source handles follow the state,
  and destination's former handles are invalidated (QT-NFR-04/07).
- Explicit custom runtime resources are caller-owned and borrowed by
  indexes/workspaces, with no shared-ownership bookkeeping. A custom resource
  must outlive every object retaining
  its storage, including moved-to objects and cleared objects with retained
  capacity (QT-NFR-04).
- Internal index, staging, and workspace allocation uses a custom runtime
  resource with nonthrowing operations and explicit allocation-failure reporting,
  rather than an allocator template. The interface must support pools, arenas,
  counting, and failure injection, including over-aligned allocation (QT-NFR-04).
- The new API and implementation fully support exception-disabled builds,
  including `-fno-exceptions`. Library failures return statuses; internal
  allocation uses a nonthrowing, failure-reporting interface, including for
  convenience workspace paths. Client operations invoked by the library must
  be nonthrowing; exception propagation is not supported. This supersedes the
  earlier client-exception propagation decision. Index moves transfer the
  resource with the state without allocation (QT-NFR-04/05/07).
- Initial index creation uses a status-returning factory, not a throwing
  configuration/root constructor. Invalid configuration/root and internal
  allocation failure return explicit errors without producing a usable index
  (QT-API-01).
- Insertion/update outside a fixed root returns an explicit out-of-bounds
  status without changing indexed state. Failed updates preserve the old
  stored geometry; submitted bounds are never clipped (QT-FR-15).
- The initial release provides no `Box`-to-`Rect` conversion helper. Geometry
  lookups return `Box`; callers handle any needed reverse conversion. Checked
  `Rect`-to-`Box` input conversion remains supported (QT-FR-33).
- Overlap uses exact comparisons of stored, validated endpoints, with no
  implicit floating-point epsilon. Clients needing contact tolerance supply
  explicitly expanded, validated bounds (QT-FR-05).
- Valid queries may extend outside a fixed root and return actual overlaps
  with indexed entries. A fully disjoint query succeeds with no results;
  query geometry is not subject to fixed-root containment rejection (QT-FR-15).
- Fixed-root containment uses inclusive endpoint comparisons, independently
  of overlap mode. Entries may touch any root boundary, including valid
  points/segments on its far edges (QT-FR-04).
- Fixed roots require strictly increasing edges on both axes; zero-width or
  zero-height fixed roots are rejected. Snapshot-derived extents may be
  degenerate, and valid point/segment entries and queries remain allowed
  (QT-FR-03).
- Floating-point zero endpoints are normalized to `+0.0` in stored geometry;
  `-0.0` and `+0.0` have identical overlap semantics (QT-FR-02).
- Fast-math compiler modes, including `-ffast-math` and `/fp:fast`, are
  unsupported for floating-point use. Standard floating-point semantics are
  required to preserve validation and endpoint guarantees (QT-FR-02).
- Floating-point arithmetic requires gradual underflow and supports subnormal
  values. Flush-to-zero and denormals-are-zero modes are unsupported caller
  settings; the library does not switch these modes (QT-FR-02). This preserves
  the finite-endpoint contract without per-operation mode switching; it is
  not a claim that subnormal arithmetic is equally fast on every processor.
- Floating-point arithmetic requires round-to-nearest, ties-to-even as a
  caller precondition. Other rounding modes are unsupported; the library does
  not change the rounding mode (QT-FR-02).
- A positive floating-point `Rect` extent whose converted endpoint rounds
  back to its origin is rejected as invalid geometry, not silently converted
  to a zero extent. Rejection leaves indexed state unchanged and invokes no
  result visitor (QT-FR-02/03).
- Floating-point geometry requires finite input components and finite
  converted endpoints. NaN and infinity are rejected as invalid geometry,
  leaving indexed state unchanged and invoking no result visitor (QT-FR-02/03).
- `Rect` input with negative width or height is rejected as invalid geometry,
  never normalized into backward-extending bounds. Rejection leaves indexed
  state unchanged and invokes no result visitor (QT-FR-03).
- Reversed `Box` edges (`minX > maxX` or `minY > maxY`) are rejected as
  invalid geometry, never normalized by swapping endpoints. Rejection leaves
  indexed state unchanged and invokes no result visitor (QT-FR-03).
- Valid zero-width or zero-height entries and query boxes (points/segments)
  are always accepted, with no configuration option to reject them. This
  does not change the strictly increasing fixed-root requirement (QT-FR-03).
  When either box is degenerate, overlap uses inclusive comparisons on both
  axes: points on box boundaries, touching segment endpoints, and identical
  points overlap (QT-FR-04).
- For two positive-area boxes, default overlap requires positive-area
  intersection: edge-only and corner-only contact do not count. An optional
  touch-inclusive mode counts those contacts (QT-FR-04).
- Initial-release coordinate types are signed 32- and 64-bit integers and
  binary IEC 60559 `float` and `double` only (QT-FR-01). Other arithmetic
  types are rejected with clear constraint diagnostics.
- Canonical geometry is `Box {minX, minY, maxX, maxY}` in the public API
  and throughout the internal implementation. `Rect {x, y, w, h}` remains
  accepted through checked input conversion; geometry lookups expose `Box`.
  Far edges must be representable, for integers as well as floats.
- Build validates before mutating; preparation failures leave the old snapshot
  and handles unchanged. After destructive replacement starts, default-mode
  failure leaves a valid empty index. Transactional and hard-capacity builds
  preserve old state on every failure. Default mode avoids duplicate payload/
  index staging, but may still use O(n) descriptor/rank-validation scratch
  (QT-FR-13, QT-NFR-05).
- The QT-FR-24 reference algorithm is the baseline for ordered enumeration:
  rank-sorted entries, a complete higher-rank partner query per entry, then
  sorting/emission of at most n - 1 partners. Discard a row if discovery exhausts
  its budget. This gives O(n) workspace and prefix-ordered output; faster
  variants must keep both bounds.

Contract approval is complete. The implementation gates below remain; design
and prototype evidence must resolve them before the public API is frozen.

Remaining implementation gates:

- Choose reservation units and hard-capacity preflight/staging strategy using
  the approved runtime resource contract. Standard throwing allocator/PMR paths
  alone do not suffice under `-fno-exceptions`.
- Ordered enumeration in O(n) workspace is feasible by construction
  (QT-FR-24 reference algorithm). The gate is its cost against unordered mode
  on split-line/mixed-size data. Bounded deduplication for any multi-cell
  representation (QT-FR-17) still needs a prototype.
- Specify the foreign-handle mechanism, address-reuse behavior within the
  approved single-module domain, fixed size, and exhaustion handling (QT-FR-07). Prove that rank/identity bookkeeping
  and sparse dirty-set validation meet memory, allocation, and time targets.
- Publish the per-operation failure/status matrix and build workspace layout,
  including descriptor capture, aliasing/consumption rules, hard-capacity
  preflight or staging, and preservation of handle-output spans on failure.
- Choose adaptive root/outlier handling and implement both explicitly selected
  snapshot incremental-growth behaviors (adaptive growth or rejection).
- Freeze benchmark fixtures and numeric thresholds on designated hardware.

## 13. Possible future extensions (not part of this update)

Ray/segment casts support line of sight, hitscan, and ground probes and are a
proposed high-priority follow-up, not a validated demand ranking. Keep internal
traversal extensible for a separate cast API with bound clipping (for example,
a maximum ray fraction) and near-to-far visits. Do not impose ray-specific
mutable-bound semantics on the current AABB visitors or promise this API before
its numeric and ordering contracts are designed.

- Swept-AABB queries (moving rectangle over a step) against the index.
- Ray and segment casts with ordered hits.
- Nearest-neighbour and k-nearest queries.
- Multiple shapes per entry or per-entry sub-shapes.
- Parallel read-only enumeration that splits work across threads.
- Resumable traversal with explicit cursor invalidation and snapshot lifetime.
- Optional client-keyed overlap begin/stay/end tracking, separate from geometry.
- Origin rebasing helpers for large floating-point worlds.
