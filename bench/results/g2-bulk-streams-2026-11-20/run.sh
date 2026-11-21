#!/usr/bin/env bash
# Run the factor screen, or the frozen slice after selecting WORK/candidate-build.
set -euo pipefail
export LC_ALL=C
work=$(realpath "$1")
out=$(realpath -m "$2")
stage=${3:?pilot or full}
here=$(dirname "$(realpath "$0")")
mkdir -p "$out"
if [[ -e "$out/power-before.txt" ]]; then
    printf 'Output already contains a run: %s\n' "$out" >&2
    exit 1
fi
mode=/sys/firmware/acpi/platform_profile
files=("$mode" /sys/devices/system/cpu/cpufreq/policy*/energy_performance_preference)
original=()
for f in "${files[@]}"; do original+=("$(<"$f")"); done
restore() {
    for i in "${!files[@]}"; do printf '%s\n' "${original[i]}" > "${files[i]}"; done
    grep -H . "${files[@]}" > "$out/power-after.txt"
    cmp "$out/power-before.txt" "$out/power-after.txt"
}
grep -H . "${files[@]}" > "$out/power-before.txt"
trap restore EXIT
for f in "${files[@]}"; do printf 'performance\n' > "$f"; done
for f in "${files[@]}"; do [[ $(<"$f") == performance ]]; done
case $stage in
    full) bash "$here/../g2-cross-screen-2026-11-20/run.sh" "$work" "$out" ;;
    pilot)
        grep -H . "${files[@]}" > "$out/power-during.txt"
        printf 'block,kind,start,finish\n' > "$out/run-order.csv"
        for block in 1 2; do
            order=(control centers narrow)
            if [[ $block == 2 ]]; then order=(narrow centers control); fi
            for kind in "${order[@]}"; do
                start=$(date -Is)
                run="$out/$block-$kind"
                taskset -c 6 "$work/$kind-build/bench/quadtree-bench" --model=snapshot --types=int32 \
                    --scenes=uniform,split-line,split-line-translated,mixed-size --engines=index,index-ordered \
                    --sizes=100000 --small-sizes= --rounds=9 --min-time=0.03 --min-reps=3 --max-reps=12 \
                    --csv="$run.csv" --raw="$run-raw.csv" > "$run.log"
                printf '%s,%s,%s,%s\n' "$block" "$kind" "$start" "$(date -Is)" >> "$out/run-order.csv"
                gzip -n "$run-raw.csv"
            done
        done
        ;;
    *) printf 'Expected pilot or full, got %s\n' "$stage" >&2; exit 1 ;;
esac
