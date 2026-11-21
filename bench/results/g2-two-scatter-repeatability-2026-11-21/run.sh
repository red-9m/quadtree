#!/usr/bin/env bash
# Repeat the frozen 15-round slice twice on the unchanged two-stream control.
set -euo pipefail
export LC_ALL=C
here=$(dirname "$(realpath "$0")")
w=$(realpath "$1")
out=$(realpath -m "$2")
thermal=${3:-}
[[ -z $thermal || $thermal == --matched-temperature ]]
exe="$w/control-build/bench/quadtree-bench"
runner="$here/../g1-flat-input-2026-11-19/reproduce.sh"
expected="$here/../g2-two-scatter-2026-11-21"
mkdir "$out"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
(cd "$w/control" && sha256sum -c --quiet "$expected/candidate-inputs.sha256")
[[ $(git -C "$w/box2d" rev-parse HEAD) == 178495ba35695aefe9c321e569d2277f06db3b90 ]]
objcopy --dump-section .text="$tmp/candidate.text" "$exe" "$tmp/exe-copy"
(cd "$tmp" && grep '  candidate.text$' "$expected/text.sha256" | sha256sum -c) > "$out/text-check.log"
sha256sum < "$exe" | awk '{print $1 "  quadtree-bench"}' > "$out/executable.sha256"

mode=/sys/firmware/acpi/platform_profile
clock=/sys/devices/system/clocksource/clocksource0/current_clocksource
siblings=/sys/devices/system/cpu/cpu6/topology/thread_siblings_list
files=("$mode" /sys/devices/system/cpu/cpufreq/policy*/energy_performance_preference)
[[ $(<"$clock") == hpet && $(<"$siblings") == 6-7 ]]
original=()
for f in "${files[@]}"; do [[ -w $f ]]; original+=("$(<"$f")"); done
grep -H . "${files[@]}" > "$out/power-before.txt"
monitor=
restore() {
    if [[ -n $monitor ]]; then kill "$monitor" 2>/dev/null || true; wait "$monitor" 2>/dev/null || true; fi
    for i in "${!files[@]}"; do printf '%s\n' "${original[i]}" > "${files[i]}"; done
    grep -H . "${files[@]}" > "$out/power-after.txt"
    rm -rf "$tmp"
    cmp "$out/power-before.txt" "$out/power-after.txt"
}
trap restore EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
for f in "${files[@]}"; do printf 'performance\n' > "$f"; done
for f in "${files[@]}"; do [[ $(<"$f") == performance ]]; done
{
    git -C "$here" rev-parse HEAD
    uname -a
    lscpu | grep '^Model name:'
    c++ --version | head -1
    git -C "$w/box2d" rev-parse HEAD
    printf 'Benchmark CPU affinity: 6; monitor CPU affinity: 0\n'
    grep -H . "${files[@]}" "$clock" "$siblings" /sys/class/power_supply/*/online
} > "$out/environment.txt"

# jiffies: total minus idle and iowait gives CPU busy time. No guest double count.
temp=
for f in /sys/class/hwmon/hwmon*/name; do
    if [[ $(<"$f") == k10temp ]]; then temp="${f%/name}/temp1_input"; break; fi
done
freq=/sys/devices/system/cpu/cpufreq/policy6/scaling_cur_freq
header=unix_seconds,stage,cpu,busy_jiffies,total_jiffies,tctl_millidegrees
if [[ -n $thermal ]]; then
    [[ -n $temp && -r $temp && -r $freq ]]
    header+=,cpu6_scaling_khz
    printf '%s\n' "$header" > "$out/run-boundaries.csv"
fi
snapshot() {
    local cpu user nice system idle iowait irq softirq steal rest tctl= frequency=
    if [[ -n $temp && -r $temp ]]; then tctl=$(<"$temp"); fi
    if [[ -n $thermal ]]; then frequency=,$(<"$freq"); fi
    while read -r cpu user nice system idle iowait irq softirq steal rest; do
        case $cpu in
            cpu|cpu6|cpu7)
                local busy=$((user + nice + system + irq + softirq + steal))
                printf '%s,%s,%s,%d,%d,%s%s\n' "$EPOCHREALTIME" "$1" "$cpu" "$busy" "$((busy + idle + iowait))" "$tctl" "$frequency" ;;
            cpu*) ;;
            *) break ;;
        esac
    done < /proc/stat
}
printf '%s\n' "$header" > "$out/load.csv"
if [[ -z $thermal ]]; then
{
    snapshot preflight-start
    sleep 10
    snapshot preflight-end
} >> "$out/load.csv"
awk -F, '
    $2 == "preflight-start" { busy[$3] = $4; total[$3] = $5 }
    $2 == "preflight-end" {
        pct = 100 * ($4 - busy[$3]) / ($5 - total[$3]);
        printf "%s: %.2f%% busy\n", $3, pct;
        ++n; if (pct > 5) failed = 1;
    }
    END { exit (n != 3 || failed) }
' "$out/load.csv" > "$out/idle-check.log" || { printf 'Host not idle; see %s/idle-check.log\n' "$out" >&2; exit 1; }
fi
(
    taskset -pc 0 "$BASHPID" >/dev/null
    while :; do snapshot monitor; sleep 5; done
) >> "$out/load.csv" &
monitor=$!
printf 'run,start,finish\n' > "$out/run-order.csv"
target=()
for run in 1 2; do
    if [[ -n $thermal ]]; then
        accepted=$(taskset -c 0 python3 "$here/quiet-start.py" "$temp" "$out/quiet-start-$run.csv" "${target[@]}")
        if [[ $run == 1 ]]; then target=("$accepted"); fi
    fi
    start=$(date -Is)
    if [[ -n $thermal ]]; then snapshot "start-$run" >> "$out/run-boundaries.csv"; fi
    taskset -c 6 bash "$runner" "$exe" "$out/$run" 15 full
    if [[ -n $thermal ]]; then snapshot "finish-$run" >> "$out/run-boundaries.csv"; fi
    printf '%s,%s,%s\n' "$run" "$start" "$(date -Is)" >> "$out/run-order.csv"
    gzip -n "$out/$run/snapshot-raw.csv"
done
kill "$monitor"
wait "$monitor" 2>/dev/null || true
monitor=
snapshot end >> "$out/load.csv"
"$exe" --diff="$out/1/snapshot.csv,$out/2/snapshot.csv" > "$out/repeatability-diff.txt"
