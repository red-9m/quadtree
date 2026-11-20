#!/usr/bin/env bash
set -euo pipefail

mode=${1:?specify check, asan-ubsan, stl-debug or tsan}
config=${2:?specify Debug or Release}
case "$config" in
    Debug|Release) ;;
    *) printf 'Unknown configuration: %s\n' "$config" >&2; exit 2 ;;
esac
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
# Use native absolute paths for -D arguments on Windows, including spaces.
native_work=$work
if [[ ${OS:-} == Windows_NT ]]; then
    native_work=$(cd -- "$work" && pwd -W)
fi

export CMAKE_GENERATOR=${CMAKE_GENERATOR:-Ninja}
flags=()
case "$mode" in
    check) ;;
    asan-ubsan)
        flags=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
        export ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1
        ;;
    stl-debug) flags=(-D_GLIBCXX_DEBUG) ;;
    tsan)
        flags=(-fsanitize=thread -fno-omit-frame-pointer)
        export TSAN_OPTIONS=halt_on_error=1
        ;;
    *) printf 'Unknown portability mode: %s\n' "$mode" >&2; exit 2 ;;
esac
export CXXFLAGS="${CXXFLAGS:-} ${flags[*]}"

build=$native_work/build
generator_args=()
case "$CMAKE_GENERATOR" in
    'Visual Studio '*) generator_args+=(-A x64) ;;
    'Ninja Multi-Config'|Xcode) ;;
    *) generator_args+=(-DCMAKE_BUILD_TYPE="$config") ;;
esac
configure=("${generator_args[@]}")
parallel=${CMAKE_BUILD_PARALLEL_LEVEL:-2}
if [[ $mode == tsan ]]; then
    targets=(index-test)
    tests='^index-test$' # Includes concurrent and nested const queries.
    configure+=(-DQUADTREE_BUILD_DEMO=OFF -DQUADTREE_BUILD_BENCHMARKS=OFF)
else
    targets=(quadtree-test quadtree-public-headers quadtree-no-exceptions-check
        index-test index-test-noexcept index-death entry-store-test entry-store-test-noexcept
        entry-store-death quadtree-index-fuzz quadtree-dynamic-static)
    tests='^(index-test(-noexcept)?|index-death-.*|entry-store-.*|index-fuzz-quick|index-example-dynamic-static)$'
    configure+=(-DQUADTREE_BUILD_DEMO=ON -DQUADTREE_BUILD_BENCHMARKS=ON -DQUADTREE_BENCH_BOX2D=OFF)
fi

set -x
cmake --version
cmake -S "$root" -B "$build" "${configure[@]}" -DQUADTREE_BUILD_TESTS=ON -DQUADTREE_INSTALL=ON
cmake --build "$build" --config "$config" --parallel "$parallel" --verbose --target "${targets[@]}"
ctest --test-dir "$build" -C "$config" --output-on-failure --no-tests=error --timeout 180 -R "$tests"

if [[ $mode == check ]]; then
    cmake --install "$build" --config "$config" --prefix "$native_work/staged"
    prefix="$native_work/relocated prefix"
    mv -- "$work/staged" "$prefix"
    cp -R -- "$root/tests/package-consumer" "$work/consumer-source"
    cmake -S "$native_work/consumer-source" -B "$native_work/consumer" \
        "${generator_args[@]}" -DQUADTREE_TEST_MODE=installed \
        -DQUADTREE_TEST_PREFIX="$prefix" -DCMAKE_PREFIX_PATH="$prefix" \
        -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF
    cmake --build "$native_work/consumer" --config "$config" --parallel "$parallel" --verbose
    ctest --test-dir "$native_work/consumer" -C "$config" --output-on-failure --no-tests=error --timeout 180
fi
