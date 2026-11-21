#!/usr/bin/env bash
set -euo pipefail
here=$(dirname "$(realpath "$0")")
w=$(realpath "$1")
out=$(realpath -m "$2")
export QT_MATCH_CONTROL_RESULT="$here/../g2-packed-cross-item-2026-11-21"
export QT_MATCH_CONTROL_ELF_HASH="$here/../g1-staged-small-2026-11-21/control-executable.sha256"
if [[ ${3:-} == --retry-singleton-117 && $# == 3 ]]; then
  export QT_MATCH_CANDIDATE_RESULT="$here/singleton"
  exec bash "$here/../g9-two-scatter-source-radix-2026-11-21/run.sh" "$w/singleton" "$out/singleton-retry" --g1-117
fi
[[ $# == 2 ]]
mkdir "$out"
for kind in empty singleton; do
  export QT_MATCH_CANDIDATE_RESULT="$here/$kind"
  bash "$here/../g9-two-scatter-source-radix-2026-11-21/run.sh" "$w/$kind" "$out/$kind" --g1
done
