# 05 · CMake package verification

`QuadTree::QuadTree` exports the runtime include directory and `cxx_std_20`, with no compile flags, definitions, link options, or libraries. `find_package(QuadTree 2.0 CONFIG REQUIRED)` finds the version-2.0.0 package. `QUADTREE_INSTALL` defaults on only for top-level builds; enable it explicitly when installing from a parent project. Keep `GNUInstallDirs` destinations relative to preserve relocation.

The package contains two runtime headers under `${CMAKE_INSTALL_INCLUDEDIR}/QuadTree` (including the required `detail/` path), three CMake files under `${CMAKE_INSTALL_LIBDIR}/cmake/QuadTree`, and the license under `${CMAKE_INSTALL_DATADIR}/licenses/QuadTree`. No legacy headers, demo, tests, or Box2D are installed. The target still exposes `<quadtree-index.h>` in every consumption mode.

## Evidence

- [CMake 3.21.4](package-2026-11-20/cmake-3.21.log) and [CMake 4.4.2](package-2026-11-20/cmake-4.4.log): the same [consumer](../../tests/package-consumer/CMakeLists.txt) builds and runs via `add_subdirectory`, source `FetchContent`, and a versioned `find_package` after moving the prefix to a path with a space. The 4.4.2 run uses non-default include/data directories. All consumer runs had networking disabled; the installed runs also hid the checkout with `bwrap --tmpfs` and printed the actual compile command.
- [Install with all optional targets enabled](package-2026-11-20/install-with-optionals.log): same six-file manifest as library-only. [CMake 3.24.3 benchmark build and install](package-2026-11-20/install-with-box2d-cmake-3.24.log): the benchmark still links fetched Box2D on the older CMake path, but none of its files are installed.
- [Release CTest: 20/20](package-2026-11-20/ctest-release.log) · [Tool versions and input hashes](package-2026-11-20/environment.txt).

## Reproduction

Run from the repository root, using CMake and CTest from the same installation. Source-mode checks use the local checkout, not a download.

```sh
root=$PWD
work=$(mktemp -d)
for mode in subdirectory fetchcontent; do
  cmake -S tests/package-consumer -B "$work/$mode" \
    -DQUADTREE_TEST_MODE="$mode" -DQUADTREE_TEST_SOURCE="$root"
  cmake --build "$work/$mode" --config Release
  ctest --test-dir "$work/$mode" -C Release --output-on-failure
done

cmake -S . -B "$work/library" -DQUADTREE_BUILD_DEMO=OFF \
  -DQUADTREE_BUILD_TESTS=OFF -DQUADTREE_BUILD_BENCHMARKS=OFF
cmake --install "$work/library" --config Release --prefix "$work/staged"
mv "$work/staged" "$work/relocated prefix"
cp -R tests/package-consumer "$work/consumer-source"
find "$work/relocated prefix" -type f | sort

cmake -S "$work/consumer-source" -B "$work/installed" \
  -DQUADTREE_TEST_MODE=installed -DQUADTREE_TEST_PREFIX="$work/relocated prefix" \
  -DCMAKE_PREFIX_PATH="$work/relocated prefix" \
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF
cmake --build "$work/installed" --config Release --verbose
ctest --test-dir "$work/installed" -C Release --output-on-failure
```

For independent isolation on Linux, run the last three commands inside `bwrap --unshare-net --ro-bind / / --tmpfs "$root" --bind "$work" "$work" --dev /dev --proc /proc --chdir "$work" -- bash`, with `work` defined or passed to the shell. Repeat the library configure with benchmark options enabled and a pinned local Box2D source to compare the install manifest.

Next: [06 · Portability CI](06-portability-ci.md).
