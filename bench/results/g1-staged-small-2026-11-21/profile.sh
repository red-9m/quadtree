#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C
here=$(dirname "$(realpath "$0")")
w=$(realpath "$1") out=$(realpath -m "$2")
profile="$here/../g1-runstep-profile-2026-11-19/profile.sh"
quiet="$here/../g2-two-scatter-repeatability-2026-11-21/quiet-start.py"
ref="$here/../g2-packed-cross-item-2026-11-21"
mkdir "$out"
(cd "$w/candidate" && sha256sum -c --quiet "$ref/candidate-inputs.sha256")
objcopy --dump-section ".text=$out/candidate.text" "$w/candidate-build/bench/quadtree-bench" "$out/exe.copy"
grep '  candidate.text$' "$ref/text.sha256" | (cd "$out" && sha256sum -c -)
rm "$out/candidate.text" "$out/exe.copy"
[[ $(git -C "$w/box2d" rev-parse HEAD) == 178495ba35695aefe9c321e569d2277f06db3b90 ]]
files=(/sys/firmware/acpi/platform_profile /sys/devices/system/cpu/cpufreq/policy*/energy_performance_preference)
original=()
for f in "${files[@]}"; do [[ -w $f ]]; original+=("$(<"$f")"); done
grep -H . "${files[@]}" > "$out/power-before.txt"
restore() {
  for i in "${!files[@]}"; do printf '%s\n' "${original[i]}" > "${files[i]}"; done
  grep -H . "${files[@]}" > "$out/power-after.txt"
  cmp "$out/power-before.txt" "$out/power-after.txt"
}
trap restore EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
for f in "${files[@]}"; do printf 'performance\n' > "$f"; done
{
  uname -a; c++ --version | head -1; perf --version; git -C "$w/box2d" rev-parse HEAD
  grep -H . "${files[@]}" /sys/devices/system/clocksource/clocksource0/current_clocksource
} > "$out/environment.txt"
# Run the original profile against the staged source/ELF. Gate timings are separate.
(cd "$w/candidate" && QT_PROFILE_QUIET="$quiet" bash "$profile" "$w/candidate-build" "$w/box2d" "$out/captures" 6)
