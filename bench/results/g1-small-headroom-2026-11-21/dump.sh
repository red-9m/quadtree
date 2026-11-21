#!/usr/bin/env bash
# Addresses are for the recorded .text hashes, not arbitrary rebuilds.
set -euo pipefail
w=$(realpath "$1")
for kind in empty singleton; do (cd "$w/$kind" && sha256sum -c --quiet "$2/$kind/text.sha256"); done
old="$w/empty/control-build/bench/quadtree-bench"
empty="$w/empty/candidate-build/bench/quadtree-bench"
single="$w/singleton/candidate-build/bench/quadtree-bench"
part() {
  objdump -d --start-address="$2" --stop-address="$3" "$1" | cut -c1-160
}
printf '# Control: adapter stores candidate and reference in separate qwords\n'
part "$old" 0xece20 0xece63
printf '\n# Control: runner calls adapter then reloads both qwords with movdqa\n'
part "$old" 0xeda3d 0xeda99
printf '\n# Empty probe: adapter is inlined; zero query counters go through one SIMD store/load\n'
part "$empty" 0xeffbf 0xf001a
printf '\n# Empty probe: call to nonempty path\n'
part "$empty" 0xf02d0 0xf031f
printf '\n# Singleton probe: the retained capacity/size predicates and unranked flat path\n'
part "$single" 0xd16f0 0xd1783
printf '\n# Singleton probe: skip work and return Status::Ok if no output and Snapshot\n'
part "$single" 0xd21f2 0xd220b
