#!/usr/bin/env bash
set -euo pipefail
here=$(dirname "$(realpath "$0")")
export QT_MATCH_CONTROL_RESULT="$here/../g2-packed-cross-item-2026-11-21"
export QT_MATCH_CANDIDATE_RESULT="$here"
export QT_MATCH_CONTROL_ELF_HASH="$here/../g1-staged-small-2026-11-21/control-executable.sha256"
if [[ ${3:-} == --full && $# == 3 ]]; then
  exec bash "$here/../g9-two-scatter-source-radix-2026-11-21/run.sh" "$1" "$2"
fi
[[ $# == 2 ]]
exec bash "$here/../g9-two-scatter-source-radix-2026-11-21/run.sh" "$1" "$2" --g1
