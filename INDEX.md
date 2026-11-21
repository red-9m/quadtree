# Runtime `Index`

Include `quadtree-index.h`; link the header-only CMake target
`QuadTree::QuadTree`. C++20, no compiler extensions or exceptions required.
Do not include the legacy `quadtree.h` in the same translation unit.

The implementation has a density-adaptive loose quadtree, a sorted bulk loader,
pooled nodes, a direct small-set path, and persistent moved-set enumeration.
**Performance scope.** Performance acceptance covers `int32` coordinates only;
it is in progress for this release. `int64`, `float` and `double` are
functionally supported: they pass the same correctness, allocation and
portability checks. Their snapshot builds are not yet performance-accepted. On
the [release run](bench/results/release-acceptance-2026-11-21/README.md), G2
ratios reach 2.7–5.1× (`int64`) and 2.4–11.5× (`float`/`double`) the best
baseline, against 2–4× limits. Persistent motion passes every applicable hard
gate for every type.

Very small `int32` snapshots carry fixed rebuild overhead. With 0–4 entries, a
build plus pair scan measured 10–62 ns per rebuild, 2.0–3.3× a plain all-pairs
loop on the reference machine. This is an
[accepted G1 exception](docs/plan/02-g1-small-snapshots-result.md). Callers
that rebuild tiny sets very often may test them directly.

## Types and creation

```cpp
using I = QuadTree::Index<std::uint32_t, float>; // Payload, coordinate
// Index<Payload, Coordinate, Diagnostics = Unchecked, Instrumented = true>
QuadTree::Config<float> config;
config.extent = QuadTree::Extent::Adaptive;
auto created = I::create(config);               // Optional borrowed Resource*
if (!created.ok())
    return created.status;
auto index = std::move(created).value();
```

Supported coordinates: signed 32/64-bit integers (excluding character types),
IEC 60559 binary `float` and `double`. `Rect<T>{x,y,w,h}` converts with checked
endpoint arithmetic. `Box<T>{minX,minY,maxX,maxY}` is canonical stored geometry.
Both forms work with insert, update, build and query. Fixed roots use `Box`.

Negative extents, reversed edges, nonfinite values, integer overflow, and positive
floating extents rounded back to their origin are rejected without mutation or
result callbacks. Floating point requires round-to-nearest/ties-to-even, gradual
underflow, and no fast-math/FTZ/DAZ. Stored zeros normalize to positive zero.
Integer endpoints retain all 64 bits; no `double` conversion or `__int128` is used.
Full-range integer and finite floating boxes whose spans overflow are supported.
There is no `Box`-to-`Rect` helper: a span may overflow or round, and recomputing
an endpoint from a rounded width need not recover the original edge.

`Config<T>`:

- `extent = Adaptive`: optional initial `root`, growth accepts every valid box
  subject to resource limits. Growth pads the union of the old extent and new
  geometry, saturating at coordinate limits; it rebuilds topology, not a chain
  of roots. `shrink()` contracts adaptive bounds to current geometry.
- `extent = Fixed`: `root` is required, strictly increasing on both axes.
  Inclusive containment permits entries on every root edge. Mutations outside
  it return `OutOfBounds`; queries may extend outside it.
- `extent = Snapshot`: `snapshotGrowth` must explicitly be `Adaptive` or `Reject`.
  Builds derive their extent. Empty builds have no extent; later insertion into
  an empty rejecting snapshot returns `OutOfBounds` until a nonempty build.
  Degenerate derived extents are valid.
- `boundary = Default`: strict overlap for two positive-area boxes; closed
  comparisons on **both** axes when either box is a point or segment.
  `TouchInclusive` uses closed comparisons for every box. There is no epsilon.
- `splitThreshold = 8`, `mergeThreshold = 4`, `smallSetThreshold = 64`,
  `maxDepth = 64`. Merge must be smaller than split, split must be positive,
  depth must be at most 64. Depth zero is valid.
- `maxEntries` and `maxNodes` bound storage; `hardCapacity = false` permits
  automatic growth. Exhausting the topology limit returns `CapacityExhausted`,
  without silently accepting an unindexed entry.

## Persistent operations and handles

```cpp
auto inserted = index.insert(QuadTree::Box<float>{0, 0, 4, 4}, std::uint32_t(42),
    QuadTree::Filter{1, 3}, std::uint64_t(100));
if (!inserted.ok())
    return inserted.status;
auto handle = inserted.handle;
auto updated = index.update(handle, QuadTree::Rect<float>{1, 1, 4, 4});
if (updated != QuadTree::Status::Ok)
    return updated;                             // Old bounds remain indexed
```

- `insert(geometry, payload, filter = {}, rank = {})` returns `{status, handle}`.
  Copy insertion requires nothrow copy construction; move insertion requires
  nothrow move construction. All payloads must be nothrow destructible. Equal
  payload values create distinct entries; payload equality is never required.
  Pass the payload's exact type. A raw pointer payload does not own or extend
  the pointee's lifetime.
- `insertWith(payload, accessor, filter, rank)` borrows a stateful accessor and
  snapshots its `Box`/`Rect` result once. Queries never reread client geometry.
- `remove(handle)` and `setFilter(handle, filter)` return `Ok`, never allocate.
- `rect`, `payload`, `filter`, `rank` lookups are O(1). Geometry is exposed as
  const `Box` edges; payload has const/mutable overloads; rank is immutable.
- `size`, `empty`, and explicit `valid(handle)` are O(1). `valid` is always
  checked, `noexcept`, and false for stale, foreign, or value-initialized handles.
- `clear()` destroys payloads, invalidates every handle, and retains capacity.
  `reserve()` and successful `shrink()` preserve live handles.
- All other handle-taking operations require live handles from this index.
  `Checked` diagnoses and aborts on misuse, even with `NDEBUG`; `Unchecked`
  compiles out automatic validation. Invalid unchecked use has no defined result.

A handle is a trivially copyable, equality-comparable, `std::hash`-compatible
16-byte value: 64-bit module owner, 32-bit slot, 32-bit generation. Each complete
index specialization has a distinct handle type. Identity counters are shared
across translation units, never reused, and report exhaustion, not wraparound.
Slot reuse/retirement rules are documented beside `Handle` in the header. The
supported identity domain is one executable or one loaded shared library;
cross-module exchange and surviving module unload/reload are unsupported.

Moves are nonthrowing and allocation-free. Move construction is O(1); assignment
also destroys/releases the destination's old state through its original resource.
Handles follow the transferred state. Self-move is a no-op. A moved-from index
is empty, queryable, reusable, and still borrows its original resource.

## Build and input ownership

```cpp
using Entry = QuadTree::BuildEntry<std::uint32_t, float>; // Geometry defaults to Box<float>
// Rect input: BuildEntry<std::uint32_t, float, QuadTree::Rect<float>>
std::span<const Entry> entries = /* stable client-owned entries */;
QuadTree::Workspace<float> workspace;
auto reserved = workspace.reserve(entries.size());
if (reserved != QuadTree::Status::Ok)
    return reserved;
auto built = index.build(entries, {.workspace = &workspace});
if (built != QuadTree::Status::Ok)
    return built;
```

`BuildEntry<Payload,T,Geometry>` is `{rect, payload, filter = {}, rank = {}}`.
A compatible record with those exact field types also works. Inputs must be
stable forward ranges yielding lvalue references. Range traversal and client
operations must not throw. Single-pass and value-producing proxy ranges are not
accepted.

- `build(range[, span<Handle> output][, BuildOptions<T>])` copies, including for
  an rvalue range; it never silently consumes lvalues.
- `buildConsume(...)` explicitly moves payloads and supports C++20 move-only
  ranges, including mutable lvalue ranges. Const ranges cannot be consumed.
- `buildWith(payloadRange, geometryAccessor, options)` borrows the accessor,
  captures geometry once per entry, copies payloads, and uses default filters
  with absent ranks. `buildConsumeWith` is the explicit moving equivalent.
- Optional handle output must have at least input size; it is filled in **input
  order**, only on success. Extra output elements are untouched.
- Input, projected references, and output must not alias storage invalidated by
  replacement. Stage a self-rebuild outside the index first.

Preparation counts input, checks output/workspace capacity, captures normalized
geometry/metadata/payload references, checks containment and duplicate ranks,
prepares sorted topology and all storage, and acquires a fresh identity. Only
then does commit destroy old payloads and construct the new entries. No payload
is moved during validation. New dense entries are spatially sorted; output
handles still map to input order.

**Both default and `transactional = true` builds currently give the strong
state/handle/output guarantee.** This exceeds the default-mode contract: all
fallible work is before commit. Hard-capacity build uses the same proof, not
capacity preflight followed by fallible insertion. There is no second live
payload snapshot or hidden transactional staging buffer. Failed preparation
can modify workspace scratch and invoke accessors; those client side effects
are not rolled back. A successful empty build also invalidates every old handle.

## Queries, pairs, filters and ordering

Visitors return `QuadTree::Continue` or `QuadTree::Stop`, never `bool`. They receive
const entry views `{handle, rect, payload, filter, optional rank}`. A view lasts
only for the callback; borrowed references last until the next non-const
operation, move, or destruction. Retain handles, not views/references.

All operations return `QueryResult {status, counters}`. `complete()`/`ok()` mean
`status == Ok`. `Stopped` includes the entry/pair whose callback returned Stop.

- `query(geometry, visitor, options, predicate)` reports each geometric match
  once. Its predicate takes one view. Without an explicit filter it includes
  zero-category/zero-mask entries.
- `query(geometry, Filter, visitor, options, predicate)` additionally applies
  two-way category/mask compatibility.
- `queryPartners(handle, visitor, options, pairPredicate)` reports partners,
  excludes itself, and always applies entry filters. `greaterRank`/`lessRank`
  are exclusive optional bounds, require ranks on the entire index, and do not
  imply sorted output. The visitor takes one partner view.
- `forEachPair(visitor, options, pairPredicate)` reports every qualifying
  unordered pair once. Unordered orientation is ascending live slot number,
  independent of payload values, pointers, or allocation addresses.
- `forEachPair(otherIndex, visitor, options, pairPredicate)` reports only
  left/right pairs, oriented `(this entry, other entry)`. Different payload and
  diagnostics types/resources are allowed. Coordinate/boundary mismatch returns
  `IncompatiblePolicy`; passing the same index returns `InvalidArgument`.
- `forEachPairSelected(leftMembership, rightMembership, visitor, options,
  pairPredicate)` enumerates disjoint selections in one index. Membership
  predicates each run once per entry into workspace. Overlap is rejected before
  pair predicates/results. Its O(n) preparation is not a sparse moved-set path.
- `forEachEntry(visitor)` visits dense storage order deterministically, with
  early stop. Bulk build may change that order.
- `forEachNode(visitor)` is always declared; it visits reachable pool nodes in
  preorder, including empty siblings, with const cell/bounds/filter/count/depth
  views and early stop. Small sets have no nodes.
- `queryTo(geometry, outputIterator, options)` appends handles;
  `pairsTo(outputIterator, options)` appends handle pairs. Neither clears output.
  Client output-sized storage/allocations are explicitly outside library bounds.

Filters default to all bits. Pair eligibility requires both
`(a.category & b.mask) != 0` and `(b.category & a.mask) != 0`.
Subtree OR summaries prune provably incompatible subtrees. Custom predicates
run only after filters and exact geometry; they do not drive pruning. Self-pair
predicates must be symmetric. Cross predicates may distinguish left/right.
All eligibility must stay stable during a call; predicates cannot depend on
traversal order or mutate participating indexes. Predicates and visitors are
borrowed for the call, never copied or stored.

`QueryOptions<T>{.ordered = true}` requires unique explicit `uint64_t` ranks on
all participating entries, even filter-inert entries. Zero and `UINT64_MAX` are
valid ranks. Insert/build reject duplicate explicit ranks in every diagnostics
mode. Ordered self pairs are `(lower rank, higher rank)`, lexicographically
sorted; ordered cross pairs sort `(left.rank, right.rank)`. Region/partner
results sort by rank. Pair predicates see the visitor's orientation.

Ordered pairs use rank-sorted sources, a complete partner query for each source,
and one sorted row at a time. O(n) scratch, never pair-sized storage. A budget
failure discards the unfinished row, so delivered output is always a prefix.

## Persistent moved sets and cache replacement

```cpp
std::span<const I::HandleType> dirty = /* changed live handles */;
auto result = index.forEachPairInvolving(dirty,
    [&](const auto& a, const auto& b) noexcept {
        // Write to a preprepared, nonthrowing client delta sink.
        recordCandidate(a.handle, b.handle);
        return QuadTree::Continue;
    }, {.workspace = &workspace});
if (!result.complete())
    return result.status;                       // Delta is not authoritative
```

Every qualifying current pair with at least one dirty endpoint is reported
once, including dirty/dirty pairs. Duplicates return `DuplicateHandle` before
pair predicates/results. All submitted handles must be live and owned; checked
misuse terminates before callbacks.

Dirty means **eligibility changed**, not just an AABB move. Include insertion,
geometry, filter, payload/pointee changes, and affected external predicate state.
If that dependency cannot be tracked, enumerate all pairs. Results are current
AABB overlaps, not confirmed contacts or overlap-end events.

For a complete client cache:

1. Keep removed handles separately; never submit removed handles in the dirty set.
2. Collect a replaceable delta of current dirty pairs.
3. Only after completion, remove old pairs incident to dirty or removed handles,
   then insert the delta. Preserve pairs whose live endpoints are both clean.
4. Discard/reconcile partial output before retry/fallback. Use client adjacency
   to avoid a full cache scan on each dirty proxy.

Key caches by handles (or stable collider IDs across snapshot builds), not
payload/body equality: one body may have several proxies. Static-tree changes
also require refreshing affected dynamic/static pairs. Cross-tree enumeration
can directly query a dynamic-ID tree against a static-tile-payload tree.

Prepared unordered setup touches only submitted handles: exact O(|S|) validation,
epoch-stamped membership, and one pruned query per dirty entry. It never scans or
clears n membership slots per ordinary call. Empty S is O(1), needs no workspace,
and does not prepare ranks even when ordered. Sparse ordered calls first query
S for clean lower-rank geometric/filter-compatible partners, then sort only S
plus those discovered source rows. They use epoch-tagged marks, not an n-entry
clear/pass. Custom predicates run during row discovery, not source preparation.
Both geometry passes charge budgets; incomplete source discovery emits nothing.
Clean rows query a tiny S directly to avoid full pile scans. At |S| >= ceil(n/2),
the full rank-ordered traversal remains the cheaper fallback. Worst-case ordered
work can still be large; source/row sorting and discovery are separate costs.

Workspace membership uses 32-bit epochs. Once per 2^32-1 nonempty moved calls,
wrap resets the entire retained membership array. `resetMovedMembership()` does
that O(capacity) work explicitly outside the step; subsequent calls start fresh.
No dirty list or pair history is retained by the index.

## Reservation, workspace and allocation

Indexes/workspaces borrow a runtime `Resource*`. Omission selects a process-life
nonthrowing heap resource supporting over-alignment. `allocate(bytes,alignment)
noexcept` returns aligned storage or null; `deallocate(ptr) noexcept` handles
null and needs no size/alignment. No zero-byte requests, fallback resources,
throwing allocators, `try`, `catch`, or library `throw` paths are used.
The resource must outlive cleared, moved-to, and reusable moved-from objects.

Prepared calls pass a non-null `options.workspace`. They check every needed
capacity before result callbacks and never grow it. Omit the pointer to opt
into a local convenience workspace, allocated through the index's resource.
Unordered region/partner/full-pair calls use bounded traversal stack only and
need no workspace. A supplied but unused workspace is not grown.

`Workspace::reserve(n)` covers ordered/moved/build calls up to n entries.
`reserve(WorkspaceReservation{entries, movedEntries, buildEntries})` separates:

- `entries`: rank order, partner row, membership stamps, selection flags.
- `movedEntries`: normalized submitted handles.
- `buildEntries`: captured boxes, build descriptors and two key-sort buffers.

`Workspace` is non-copyable, allocation-free/nothrow movable with the same
resource rules as Index. Never move or reuse an active workspace. Concurrent or
nested read-only calls use distinct workspaces and independently safe callbacks.
The indexes stay unchanged; client writes/destruction/pointee access require
synchronization. There are no mutable query caches or last-query counters.

Example measured reservation recipe, **not** a topology-independent node bound:

```cpp
auto status = index.reserve({.entries = 256, .rankedEntries = 256,
    .nodes = 8193, .references = 256, .descriptors = 256});
if (status != QuadTree::Status::Ok)
    return status;
status = workspace.reserve(256);
if (status != QuadTree::Status::Ok)
    return status;
```

`entries` includes dense payload/geometry/metadata and reusable slot capacity;
`rankedEntries` reserves the duplicate-rank directory. Retired slots consume
additional slot space (see the header's exact exhaustion rules).
`references` reserves at least entries (one 12-byte link per tree entry).
`nodes` counts individual pool slots, not blocks. Children are allocated in
blocks of four. `descriptors` reserves **two SpatialKey arrays** for incremental
split/growth rollback preparation; builds use caller workspace instead.

A build's exact topology is counted before commit. Equal n can need different
node capacities. A conservative depth-dependent upper bound is
`1 + 4 * floor(maxDepth * n / (splitThreshold + 1))` for a non-small nonempty
set; calculate it with checked size arithmetic and respect `maxNodes`.
Ordinary geometry needs much less. Growth/recentering can require n mutation
records; a leaf split needs only that leaf's records. Reserve descriptors >= n
for a distribution-independent incremental scratch guarantee. Reservation and
shrink may allocate; hard-capacity mode forbids automatic growth in operations,
not explicit `reserve()`.

With all these capacities and required workspace reserved, changing distributions,
rank checks, build commit, updates, filtering, removal, and queries allocate
nothing. Hard-capacity insufficiency is reported before mutation. Payloads,
visitors, outputs, projections and range operations must themselves be
nonthrowing; their allocations are outside this guarantee. Callable/payload
constraints are checked at compile time where expressible; range/output
nonthrowing behavior and storage aliasing remain client preconditions.

## Storage and complexity

One reference per tree entry. A cell owns an entry by its center and size, not
strict AABB containment: crossing a split line does not pin a small box near
the root. Splits use exact stored `std::midpoint` endpoints; child assignment
uses half-open cells closed at root far edges. Child size tests use exact
unsigned integer spans, or overflow-safe floating size heuristics. Pruning uses
conservative unions of stored boxes, never rounded cell extents. Incremental
own summaries may retain old bounds/filter bits until a periodic tight refit;
geometry and entry filters are always tested exactly before reporting.

The bulk loader derives up to 64 Morton digits by midpoint descent, without
fixed 16-bit quantization or converting integer coordinates to floating point.
Keys have two 64-bit words, level, and a 32-bit entry index. A stable LSD radix
sort skips constant digits; sorted prefixes partition nodes without per-level
entry copying. Contiguous snapshot node entries use index increments instead
of dependent link loads; affected mutations fall back to the retained links.
Splits require more than splitThreshold movable entries and
useful distinct prefixes. Coincident/nonprogressing geometry stays in a leaf.
Large entries stay coarse without blocking subdivision for smaller entries.

Mutations extend cached own bounds/filter ORs in O(1). Replacements/removals
may leave conservative old contributions; own summaries are tightened after
about ownCount/2 changes (immediately for at most eight own entries). Ancestors
combine cached own summaries with their children, never rescanning unrelated
coarse entries. Aggregate counts/bounds are propagated eagerly, so const queries
need no refit or mutable cache. No-op geometry/filter updates change no counters.

An unproductive leaf split is retried only after about ownCount/2 successful
own changes. The retry counter is committed only on success. Splits prepare
sorted leaf records before mutation; motion can still make a formerly coincident
leaf split. Moves preserving placement normally avoid relinking. Merge hysteresis
collapses at mergeThreshold, or frees empty child topology. Child blocks are
recycled before growing; clear retains the pool, shrink compacts it. Traversal
skips empty subtrees; sparse moved calls have no deferred full-tree pass.

Representative targets are O(log n) mutations, O(log n + k) region/partner
queries, O(n log n + p) full pairs and O(|S| log n + p_S) moved pairs. These are
not universal guarantees: bounded overlaps alone do not prevent quadratic
candidate work. A periodic own-summary refit/split retry can still scan a large
changed node, but its own-summary maintenance and unproductive split retries are
amortized O(1) and O(configured depth) per successful own change respectively.
These are not per-call latency bounds; repeated allocation/capacity failures
are outside this amortization. Adaptive extent growth rebuilds O(n)
geometry/topology; max-depth leaves, long-thin geometry and extreme
floating exponent disparities can degenerate. Small sets use O(n²) direct pairs
and O(n) queries. All-overlap unordered traversal is O(n + p). Ordering adds
O(n log n) rank sorting, repeated queries and row sorts, up to O(n² log n) in a
pile. Budgets/fallbacks, not a universal output-sensitive claim, bound exposure.

Active nodes are O(n * configured depth), depth <= 64; rank/slot/reference
bookkeeping is O(live high water + bounded retired slots). Actual memory is
O(retained capacity + caller workspace + pending growth), not O(size) after
clear. No pair-sized internal cache or operation-history directory exists.

LP64 storage sizes (not including enclosing Index/Workspace objects or resource
headers):

- Handle: 16 bytes; reference: 12 bytes; Morton key: 24 bytes.
- `Index<int,int>` / `Index<int,float>` node: 120 bytes; int64/double node: 168.
- Dense entry capacity: `sizeof(Box) + sizeof(Payload) + 32`; slots: 8 bytes.
- Rank cells: 16 bytes, typically 2–4 cells per ranked entry, minimum 8 cells.
- Incremental descriptors: 48 bytes per reserved descriptor (two keys).
- Build workspace: 104 bytes per entry for int32/float; 120 for int64/double
  (captured box, descriptor and two keys). It contains no payload objects.
- Query workspace: 13 bytes per reserved entry plus 4 per reserved moved handle.

`stats()` exposes actual retained bytes/capacities and node/reference/key/handle
sizes. Live bytes count logical records, used slot bookkeeping and reachable
nodes; rank-table spare cells and other unused storage are retained bytes.
`stats()` traverses active nodes for maximum depth; ordinary size/lookups are
O(1). Workspace has separate `retainedBytes()`.

Rebuild peak: old retained buffers + workspace descriptors/keys + pending grown
entry/node/reference buffers. Pending entry buffers are raw capacity, not a
second live payload snapshot. Both transactional/default modes have the same
peak recipe. No replacement buffers are needed within reserved capacities.
Automatic capacity growth may retain newly grown scratch after a later failure,
but indexed values, handles, and output retain the strong guarantee.

## Counters, budgets and failure channels

Per-call counters saturate: node visits, candidate tests, filter rejections,
predicate calls, reports. Candidate tests count each exact entry-level rectangle
overlap evaluation, repeated discovery included, before its result is known.
Filter rejections count rejected entries **and** rejected subtree groups;
predicate calls count custom predicates, not the default accept-all adapter.
Node visits count nonempty nodes entered by region/partner traversal and
nonempty nodes/node pairs entered by self/dual traversal, including pruned
visits and revisits. Bounds-only holder/subtree group pruning before entry
walks is uncharged structural work (at most four groups per self-node visit or
eight per dual node-pair visit). Cell/bounds checks are not candidate tests.

`Index<Payload,T,Policy,false>` omits optional counter maintenance and returns
zero counters; budgets still work. Instrumentation/diagnostics selections are
independent of `_DEBUG` and `NDEBUG`.

`QueryOptions::budget = {candidateTests, nodeVisits}` sets pre-work limits;
`UINT64_MAX` is unlimited and zero is valid. Limits are checked before work.
Exactly using the limit still completes if no further charged work is needed.
`BudgetExhausted`, `Stopped`, and `Ok` are distinct. Preparation, rank/selection
sorting, callbacks and predicates are not candidate-budgeted: budgets are not
wall-clock limits. No continuation cursor exists. Retrying restarts; discard or
reconcile prior partial output rather than blindly appending it.

Failure/status matrix (success is `Ok`; results are `[[nodiscard]]`):

- Factory: `InvalidConfiguration`, `InvalidGeometry`; no allocation, no usable
  index on failure.
- Insert: `InvalidGeometry`, `OutOfBounds`, `DuplicateRank`, `CapacityExhausted`,
  `AllocationFailed`, `IdentityExhausted`, `SizeLimitExceeded`.
- Update: `InvalidGeometry`, `OutOfBounds`, `CapacityExhausted`,
  `AllocationFailed`, `SizeLimitExceeded`.
- Reserve: `AllocationFailed`, `SizeLimitExceeded`, `IdentityExhausted`.
- Shrink: `AllocationFailed`, `SizeLimitExceeded`, `CapacityExhausted`.
- Build: `InvalidGeometry`, `OutOfBounds`, `DuplicateRank`, `InsufficientOutput`,
  `InsufficientWorkspace`, `CapacityExhausted`, `AllocationFailed`,
  `IdentityExhausted`, `SizeLimitExceeded`.
- Query/enumeration: `Stopped`, `BudgetExhausted`; geometry-taking calls also
  `InvalidGeometry`; ordered/rank-bound calls also `MissingRank`; scratch-using
  prepared calls also `InsufficientWorkspace`; convenience calls also
  `AllocationFailed`/`SizeLimitExceeded`.
- Moved sets additionally: `DuplicateHandle`.
- Cross trees additionally: `IncompatiblePolicy`, `InvalidArgument` (same tree).
  Selections additionally: `InvalidArgument` (overlap).
- Remove/filter: `Ok` for valid handles; clear has no failure result.

All mutation failures preserve entries and live handles. Build failures preserve
handle output too. Query validation/preparation errors make no result calls;
selection/accessor callbacks may already have run. Stop/budget exhaustion retain
already delivered output. Invalid handles, active-workspace reuse, callback
mutation and invalid aliasing are programming preconditions, not routine status
errors. Checked handles and active-workspace reuse/moves diagnose and terminate
without throwing; callback mutation/aliasing need not be detected.

## Migration

- `Tree::add(item)` -> `Index::insert(geometry,payload)`, retain its handle.
- Geometry changes -> `update(handle,geometry)`; client geometry may otherwise
  change freely because the index stores its own snapshot.
- Equality-based `remove(item)` -> identity-based `remove(handle)`.
- `query(rect,vector)` -> visitor `query` or append-only `queryTo`.
- `queryAll(vector<pair>)` -> streaming `forEachPair` or explicit `pairsTo`.
- Fixed depth/nodeItems -> occupancy thresholds, bounded depth and separate
  storage/workspace reservation.
- `enumerateNodes` -> always-declared const `forEachNode` / `stats`.
- Custom allocators -> borrowed runtime nonthrowing `Resource`.
- Copy remains deleted; moves now work. Index/Workspace are **runtime-only**,
  not `constexpr`. Supported coordinate types/FP domain are narrower.
- Foreign rectangles convert by value. Layout similarity never permits
  `reinterpret_cast` type-punning.

The old header/demo remain separate legacy sources, not wrappers around Index.
The CMake package installs only the runtime API; see the
[quick start](README.md#use-in-a-project) for installation and linking.
