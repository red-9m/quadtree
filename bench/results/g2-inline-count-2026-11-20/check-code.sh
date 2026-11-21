#!/usr/bin/env bash
# Keep target flags fixed; inspect the actual gate ELF, not an isolated helper.
set -euo pipefail
export LC_ALL=C
work=$(realpath "$1")
for kind in control inline candidate; do
    exe="$work/$kind-build/bench/quadtree-bench"
    printf '\n%s\n' "$kind"
    count=0
    while IFS= read -r symbol; do
        printf '%s\n' "$symbol" | c++filt
        hits=$(objdump -d --no-show-raw-insn --disassemble="$symbol" "$exe" | awk '
            /call.*__popcountdi2/ || /4332322132212110/ || /[[:space:]]popcnt/ {print}')
        printf '%s\n' "$hits"
        if [[ $kind == control ]]; then
            [[ $(grep -c 'call.*__popcountdi2' <<< "$hits") == 2 ]]
        else
            [[ $hits == *4332322132212110* && $hits != *__popcountdi2* ]]
        fi
        (( count += 1 ))
    done < <(nm "$exe" | awk '$3 ~ /11walkOrdered/ && $2 == "t" {print $3}')
    [[ $count == 2 ]]
done
for kind in lookahead candidate; do
    exe="$work/$kind-build/bench/quadtree-bench"
    symbol=$(nm "$exe" | awk '$3 ~ /^_ZNK.*8planBulk/ && $3 !~ /same_as/ {print $3}')
    printf '\n%s planBulk\n' "$kind"
    hits=$(objdump -d --no-show-raw-insn --disassemble="$symbol" "$exe" | awk '/[[:space:]]prefetch/ {print}')
    [[ -n $hits ]]
    printf '%s\n' "$hits"
done
