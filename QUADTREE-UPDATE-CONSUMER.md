# Proposal: make `QuadTree::Index` easy to adopt as a game broadphase

Status: proposal. This proposal covers `quadtree-index.h` only; the legacy `quadtree.h` is out of scope.

## Target consumer

A single-threaded, fixed-step 2D game engine:

- `int32` axis-aligned boxes in a small world (about 600×400 units), y down.
- At most a few thousand boxed objects. Each object has a stable, never-reused
  64-bit ID.
- Objects change position by direct writes. Nothing reports which objects moved,
  so the engine cannot supply a "dirty" set.
- Collision callbacks are ordinary engine code. They may allocate and throw.

The intended pattern: each step, rebuild a snapshot from every boxed object, collect all overlapping pairs, then pass them to the engine's own callbacks. A local probe measured snapshot rebuild plus the full unordered pair pass at about 0.73 ms for 4096 8×8 boxes (GCC `-O2`). The current API is already fast and correct enough. The gap is edge-of-API ergonomics.

## P1: API additions for this usage

5. **Bounded collection for throwing callers.** Visitors and output iterators
   must not throw: they run inside `noexcept` functions, so
   `std::back_inserter(vector)` terminates on `bad_alloc`. Add overloads that
   write into a span:

   ```cpp
   struct PairIds { Handle a, b; };
   CollectResult pairsTo(std::span<PairIds> out, QueryOptions<T> = {}) const noexcept;
   CollectResult queryTo(const G&, std::span<Handle> out, QueryOptions<T> = {}) const noexcept;
   // CollectResult{status, count, counters}; status == InsufficientOutput when out
   // is too small. Report it before any write, or document the prefix as unusable.
   ```

   Callers can then collect without allocating and dispatch to throwing code
   outside the index.
6. **Guaranteed rebuild capacity.**
   - Add `static Expected<Reservation> Index::reservationForBuild(std::size_t n,
     const Config<T>&)` and `Index::reserveForBuild(n)`. They return a node,
     reference and workspace size for which any `build` of at most `n` entries
     cannot return `CapacityExhausted`.
   - The size bound may depend on `maxDepth`.
   - Document a recommended configuration for small worlds (for example
     `maxDepth = 10`) so the bound stays in kilobytes, not megabytes. This lets
     the engine combine `hardCapacity = true` with a fixed per-step rebuild.
7. **Skip entries during an accessor build.** Let the `buildWith` accessor return
   `std::optional<Rect<T>>` or `std::optional<Box<T>>`. A `nullopt` skips the
   entry and writes an invalid handle to the output, if one is given. Engines
   can then build directly from object lists in which some objects have no
   bounds, or have coordinates that are not representable, without first
   copying them into a staging vector.
8. **Optional begin/end tracking** (`quadtree-contacts.h`, header-only, outside
   `Index`):
   - Input is the current step's pairs, keyed by stable payload IDs (not
     handles, because every rebuild invalidates handles).
   - Sort and deduplicate the pairs, diff them against the previous step's
     pairs, and emit `Begin` / `End` events in deterministic order.
   - Storage is reserved up front, there are no allocations per step, and
     running out of capacity is reported as a status.

## P2: evidence and documentation

9. **Add a small-world benchmark row.** Use `int32`, n = 1…4096, boxes of 4–64
   units in a 600×400 world, and measure rebuild plus full pairs per step, both
   unordered and ordered by unique ranks. Track the time against a per-step
   target, for example ≤ 1 ms at 4096.
10. **Add a "per-step broadphase" recipe to `README.md`:**
    - Snapshot `build` from stable IDs, using the payload as the ID.
    - Choose `Boundary::Default` or `TouchInclusive` deliberately: the default
      treats edge-touching boxes as not overlapping.
    - Use bounded `pairsTo`, then dispatch outside the index.
    - Note that results are AABB candidates, not contacts.
    - Note that `-ffast-math` affects only floating coordinates.

## Non-goals

- Narrow phase, physics, and any knowledge of scene graphs or hierarchies.
- Thread safety beyond what the API already provides.
- Change-notification integration: consumers without dirty tracking use
  snapshot rebuilds.
- Fixing or extending the legacy `Tree`.
