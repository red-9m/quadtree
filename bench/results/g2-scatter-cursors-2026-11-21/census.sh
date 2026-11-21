#!/usr/bin/env bash
set -euo pipefail
here=$(dirname "$(realpath "$0")")
src=$(realpath "$1")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
(cd "$src" && sha256sum --check --quiet "$here/../g2-topology-cost-2026-11-20/candidate-inputs.sha256")
cp -a "$src"/. "$stage/"
(cd "$stage" && patch -s -p1 < "$here/census.patch")
"${CXX:-c++}" -std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror \
    -fno-access-control -I"$stage" -I"$stage/bench" "$here/census.cpp" "$stage/bench/bench-memory.cpp" -o "$stage/census"
"$stage/census"
