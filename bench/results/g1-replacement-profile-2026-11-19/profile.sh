#!/usr/bin/env bash
# Run at the repository root with the Release binary and its -g twin.
set -eu
export LC_ALL=C
exe=$(realpath "$1") twin=$(realpath "$2") out=$3 cpu=$4
here=$(realpath "$(dirname "$0")")
mkdir "$out"
out=$(realpath "$out")
for kind in release debug; do
  binary=$exe
  if [[ $kind == debug ]]; then binary=$twin; fi
  objcopy --dump-section ".text=$out/$kind.text" "$binary" "$out/$kind.copy"
  objdump -h "$binary" | awk '$2 == ".text" {print $3, $4, $6}' > "$out/$kind.layout"
done
cmp "$out/release.text" "$out/debug.text"
cmp "$out/release.layout" "$out/debug.layout"
# The post-identity boundary in summarize.py depends on this exact binary.
(cd "$out" && sha256sum -c "$here/text.sha256")
rm "$out/"{release,debug}{.text,.copy,.layout}

for event in cycles instructions; do
  for scene in uniform clustered; do
    for n in 2 4; do
      p="$out/$event-$scene-$n"
      taskset -c "$cpu" perf record -q -e "$event:u" -F 5477 -o "$p.data" -- "$exe" \
        --model=snapshot --types=int32 --scenes="$scene" --engines=index --sizes= --small-sizes="$n" \
        --batch=256 --rounds=9 --min-time=0.8 --max-time=1.1 --max-reps=400000 --csv="$p.csv" > "$p.log"
      perf script -i "$p.data" -F period,ip,dso,dsoff > "$p.samples"
    done
  done
done
python3 "$here/summarize.py" "$out" "$exe" "$twin"
