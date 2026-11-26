# Optional benchmark dependencies/options need newer CMake than the library:
# Box2D requires 3.22; DOWNLOAD_EXTRACT_TIMESTAMP requires 3.24.
cmake_minimum_required(VERSION 3.24)

# Benchmark harness. Optimization comes from the build type
# (e.g. -DCMAKE_BUILD_TYPE=Release); nothing here is an interface requirement.
add_executable(quadtree-bench quadtree-bench.cpp bench-memory.cpp)
target_link_libraries(quadtree-bench PRIVATE QuadTree::QuadTree)
target_include_directories(quadtree-bench PRIVATE ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
if (MSVC)
    target_compile_options(quadtree-bench PRIVATE /W4 /permissive-)
else()
    target_compile_options(quadtree-bench PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)
endif()

option(QUADTREE_BENCH_NATIVE "Tune the benchmark for the build machine (-march=native)" OFF)
if (QUADTREE_BENCH_NATIVE AND NOT MSVC)
    target_compile_options(quadtree-bench PRIVATE -march=native)
endif()

# Box2D >= 3.2 dynamic-tree baselines. Default: the pinned v3.2.0 source built
# with this project's configuration (static, NDEBUG in release builds, no
# validation). Installed packages may enable asserts and validation.
option(QUADTREE_BENCH_BOX2D "Build the Box2D dynamic-tree baselines" ON)
option(QUADTREE_BENCH_SYSTEM_BOX2D "Use an installed Box2D >= 3.2 instead of the pinned source" OFF)
if (QUADTREE_BENCH_BOX2D)
    if (QUADTREE_BENCH_SYSTEM_BOX2D)
        find_package(box2d 3.2 REQUIRED)
        set(QT_BENCH_BOX2D_DESCRIPTION "installed ${box2d_VERSION}")
    else()
        enable_language(C)
        include(FetchContent)
        FetchContent_Declare(box2d
            URL https://github.com/erincatto/box2d/archive/refs/tags/v3.2.0.tar.gz
            URL_HASH SHA256=ffec747b27a2267016f8d0240d658c82d187fb2271fa0f69fcbe53fbc067180d
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_MakeAvailable(box2d)
        # Build the linked baseline, but ignore its install rules. Setting the
        # directory property also works before FetchContent's 3.28 keyword.
        get_property(bench_subdirectories DIRECTORY PROPERTY SUBDIRECTORIES)
        if (box2d_SOURCE_DIR IN_LIST bench_subdirectories)
            set_property(DIRECTORY "${box2d_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)
        endif()
        set(QT_BENCH_BOX2D_DESCRIPTION "pinned v3.2.0 source, built as $<CONFIG>")
    endif()
    target_link_libraries(quadtree-bench PRIVATE box2d::box2d)
    target_compile_definitions(quadtree-bench PRIVATE QT_BENCH_HAVE_BOX2D)
else()
    set(QT_BENCH_BOX2D_DESCRIPTION "not built")
endif()

# Effective flags recorded in every result file.
set(QT_BENCH_CONFIG_FLAGS
    "$<$<CONFIG:Debug>:${CMAKE_CXX_FLAGS_DEBUG}>$<$<CONFIG:Release>:${CMAKE_CXX_FLAGS_RELEASE}>$<$<CONFIG:RelWithDebInfo>:${CMAKE_CXX_FLAGS_RELWITHDEBINFO}>$<$<CONFIG:MinSizeRel>:${CMAKE_CXX_FLAGS_MINSIZEREL}>")
set(QT_BENCH_TARGET_FLAGS "$<JOIN:$<TARGET_PROPERTY:quadtree-bench,COMPILE_OPTIONS>, >")
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/bench-build-info.h CONTENT
"#pragma once
#define QT_BENCH_CONFIG \"$<CONFIG>\"
#define QT_BENCH_BOX2D \"${QT_BENCH_BOX2D_DESCRIPTION}\"
#define QT_BENCH_FLAGS \"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}: ${CMAKE_CXX_FLAGS} ${QT_BENCH_CONFIG_FLAGS} ${QT_BENCH_TARGET_FLAGS}\"
")

# Smoke run: standard scenes, every type and engine; fails if engines disagree.
add_test(NAME bench-smoke COMMAND quadtree-bench --quick)

# Opt-in game fixtures: cross-check both snapshot modes against brute force.
add_test(NAME bench-small-world-check COMMAND quadtree-bench --model=snapshot --types=int32
    --scenes=small-world-8,small-world-mixed --engines=brute,index,index-ordered
    --small-sizes= --sizes=1,64,4096 --rounds=1 --min-time=0 --min-reps=1 --max-reps=1)

# Persistent cross-check over full 64-step passes (split, merge and refit paths):
# every engine against brute force on all workloads.
add_test(NAME bench-persistent-check COMMAND quadtree-bench --model=persistent --types=int32,float
    --persistent-sizes=200,800 --steps=64 --rounds=1 --max-time=1000)

# Rank-set collision benchmark (entry store duplicate-rank bookkeeping).
add_executable(quadtree-rank-bench rank-bench.cpp)
target_link_libraries(quadtree-rank-bench PRIVATE QuadTree::QuadTree)
target_include_directories(quadtree-rank-bench PRIVATE ${PROJECT_SOURCE_DIR})
if (MSVC)
    target_compile_options(quadtree-rank-bench PRIVATE /W4 /permissive-)
else()
    target_compile_options(quadtree-rank-bench PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)
endif()
add_test(NAME rank-bench-smoke COMMAND quadtree-rank-bench --quick)

# Targeted mutation and ordered moved-set regressions; batched timing, with
# allocation/result checks but no machine-dependent timing assertions in CTest.
add_executable(quadtree-index-regression-bench index-regression-bench.cpp)
target_link_libraries(quadtree-index-regression-bench PRIVATE QuadTree::QuadTree)
if (MSVC)
    target_compile_options(quadtree-index-regression-bench PRIVATE /W4 /permissive- /EHs-c-)
    target_compile_definitions(quadtree-index-regression-bench PRIVATE _HAS_EXCEPTIONS=0)
else()
    target_compile_options(quadtree-index-regression-bench PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror -fno-exceptions)
endif()
add_test(NAME index-regression-bench-smoke COMMAND quadtree-index-regression-bench)

# Randomized differential fuzzer: internal tree invariants after every step and
# a brute-force oracle for entries, pairs, moved sets and queries. The full run
# (no arguments) takes minutes; configure with sanitizer flags for memory checks.
add_executable(quadtree-index-fuzz index-fuzz.cpp)
set_target_properties(quadtree-index-fuzz PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(quadtree-index-fuzz PRIVATE QuadTree::QuadTree)
quadtree_strict_warnings(quadtree-index-fuzz PRIVATE)
add_test(NAME index-fuzz-quick COMMAND quadtree-index-fuzz --quick)

# Gate evaluator self-test on a fixture with known verdicts.
add_test(NAME bench-gate-check COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:quadtree-bench>
    -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/gate-fixture.csv -P ${CMAKE_CURRENT_SOURCE_DIR}/gate-check.cmake)
