#!/usr/bin/env bash
set -euo pipefail
here=$(dirname "$(realpath "$0")")
src=$(realpath "$1") out=$(realpath -m "$2")
flags=() details=cross-shapes.csv
if (($# == 3)); then
    [[ $3 == --item ]]
    flags=(-DQT_ITEM_CENSUS); details=item-outcomes.csv
else
    [[ $# == 2 ]]
fi
(cd "$src" && sha256sum -c --quiet "$here/../g9-two-scatter-source-radix-2026-11-21/candidate-inputs.sha256")
mkdir "$out"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
"${CXX:-c++}" -std=c++20 -O3 -DNDEBUG -fno-access-control -Wall -Wextra -Wpedantic \
    -Wconversion -Wsign-conversion -Wshadow -Werror -I"$src" -I"$src/bench" \
    "${flags[@]}" "$here/census.cpp" "$src/bench/bench-memory.cpp" -o "$stage/census"
"$stage/census" > "$out/work.csv" 2> "$out/$details"
