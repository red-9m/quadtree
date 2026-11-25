#!/usr/bin/env bash
# screen.sh Release-benchmark /new/output-directory
set -euo pipefail
here=$(dirname "$(realpath "$0")"); exe=$(realpath "$1"); d=$(realpath -m "$2"); mkdir "$d"
files=(/sys/firmware/acpi/platform_profile /sys/devices/system/cpu/cpufreq/policy*/energy_performance_preference)
original=(); for f in "${files[@]}"; do original+=("$(<"$f")"); done
grep -H . "${files[@]}" > "$d/power-before.txt"
restore() { for i in "${!files[@]}"; do printf '%s\n' "${original[i]}" > "${files[i]}"; done; grep -H . "${files[@]}" > "$d/power-after.txt"; cmp "$d/power-before.txt" "$d/power-after.txt"; }
trap restore EXIT; trap 'exit 130' INT; trap 'exit 143' TERM
for f in "${files[@]}"; do printf 'performance\n' > "$f"; done
for f in /sys/class/hwmon/hwmon*/name; do if [[ $(<"$f") == k10temp ]]; then temp=${f%/name}/temp1_input; break; fi; done
sha256sum "$exe" > "$d/binary.sha256"; git diff HEAD -- detail/quadtree-index-impl.h > "$d/source.patch"; git rev-parse HEAD > "$d/revision.txt"
for stage in matrix first second; do
    rounds=45; [[ $stage != matrix ]] || rounds=5
    taskset -c 0 python3 "$here/../g2-two-scatter-repeatability-2026-11-21/quiet-start.py" "$temp" "$d/$stage.quiet.csv" >/dev/null
    taskset -c 6 "$exe" --model=snapshot --types=int64 --scenes=uniform --sizes=100000 --small-sizes= --engines=index,index-ordered,brute,sweep,grid --rounds=$rounds --csv="$d/$stage.csv" --raw="$d/$stage.raw.csv" > "$d/$stage.log"
    gzip -n "$d/$stage.raw.csv"; rc=0; "$exe" --gate-files="$d/$stage.csv" > "$d/$stage.gates" || rc=$?; [[ $rc == 0 || $rc == 3 || $rc == 4 ]]
done
