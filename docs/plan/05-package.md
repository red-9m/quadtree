# 05 · Ship a relocatable CMake package

**Goal:** a downstream C++20 project can consume the runtime header via `QuadTree::QuadTree` from source or an installation. Independent of benchmarks. Requirements: `QT-NFR-10`, [§9 packaging](archive/QUADTREE-UPDATE-PLAN.md#9-build-packaging-and-documentation).

**Read only:** the `quadtree` INTERFACE target, `QuadTree::QuadTree` alias and `QUADTREE_BUILD_*` options in [CMakeLists.txt](../../CMakeLists.txt); the include of `detail/quadtree-index-impl.h` from [quadtree-index.h](../../quadtree-index.h). The build-tree alias exists; its checkout include path cannot be exported as an install path.

**Deliver:** build/install include interfaces, an exported target with the **same** consumer name, package config/version files, the runtime header and its required `detail/` header at the correct relative path, and [LICENSE](../../LICENSE). Keep CMake 3.21 for library consumers, optional targets off as a subproject, and test/demo flags and Box2D off the interface. No legacy wrappers, release benchmark, or CI matrix in this task.

**Exit:** one minimal consumer compiles and runs `Index::create` with `add_subdirectory`, source `FetchContent`, and `find_package` after moving the installed prefix. The installed test must not see checkout include paths or need network access. No test/demo helpers in the install tree.
