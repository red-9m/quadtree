# Legacy · Decide and isolate a safety-only maintenance patch

**Decision first:** is the old `quadtree.h` shipping as a supported maintenance release? Record the answer in the [task index](../../QUADTREE-UPDATE-PLAN.md). If not, mark the track deferred, not fixed. This is not a dependency of runtime G1/G2 work.

**If supported:** fix the old API under its existing preconditions. Start with the [two float false-negative fixtures and overflow examples](archive/QUADTREE-UPDATE-PLAN.md#investigation-findings) and the [original defect dispositions](archive/QUADTREE-UPDATE-PLAN.md#7-existing-defects-to-fix). `QT-FIX-02` is missing legacy regression coverage, not a demonstrated invalid erase. `QT-FIX-01` is a documented old-geometry removal precondition; handles solve it in `Index`, not `Tree`.

**Read only:** `Tree::computeBox`, `Tree::doRemoveItem` and `_DEBUG`-dependent enumeration in [quadtree.h](../../quadtree.h); [quadtree-debug.h](../../quadtree-debug.h); `altRectIntersects` and its cast at the call site in [quadtree-test.cpp](../../quadtree-test.cpp); demo options in [CMakeLists.txt](../../CMakeLists.txt). Tests for `Index` do not exercise these sources.

**Scope:** checked arithmetic and invalid geometry, conservative child bounds, ODR-safe declarations, direct header includes, safe foreign rectangle conversion and the legacy demo configuration (`QT-FIX-04`, `QT-FIX-06`, `QT-FIX-10`–`QT-FIX-13`). Coordinate narrowing, ownership redesign and streaming pairs belong to the major API, not this patch.

**Exit if shipping:** compare legacy query/pair identities for both float reproducers in separate trees; cover missing removal, invalid/overflowing geometry, mixed macros with a compatible ABI, and standalone public headers. Run legacy regression tests under ASan/UBSan. Link evidence or explicitly record unresolved defects; do not infer a maintenance pass from the new API tests.
