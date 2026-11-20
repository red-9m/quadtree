# 07 · Make the runtime API the release entry point

**Goal:** new users find a working runtime example before the legacy API. Independent of timing work; use 05 for installed-package instructions. Requirements: `QT-API-02`, `QT-API-07`, `QT-API-08`, `QT-API-15`, `QT-FR-34`, `QT-NFR-10`, [§8 migration](archive/QUADTREE-UPDATE-PLAN.md#8-compatibility-and-versioning).

**Read only:** [README.md](../../README.md); the existing [migration guide](../../INDEX.md#migration), [build ownership rules](../../INDEX.md#build-and-input-ownership), and [persistent cache protocol](../../INDEX.md#persistent-moved-sets-and-cache-replacement). `INDEX.md` already owns the status matrix, reservation recipes and lifetime contracts. Link them; do not write a second specification.

**Deliver:** lead with how to include/link the runtime index and when to choose snapshot vs persistent use. Add a buildable dynamic-vs-static example using `forEachPair(otherIndex, ...)`, different payload types and explicit completion checks. Distinguish current AABB pairs from contacts, handles from stable collider IDs, and partial results from an authoritative cache update. Clearly label or relocate legacy guidance; do not advertise type-punning or a compatibility layer.

**Exit:** build and run the example and check its expected left/right pair identities. Verify README links for migration, usage and measured limitations. Do not claim release performance acceptance without 08 evidence.
