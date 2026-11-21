#!/usr/bin/env bash
# The candidate intentionally violates replacement semantics. Diagnostic only.
set -euo pipefail
here=$(dirname "$(realpath "$0")")
export QT_MATCH_CONTROL_RESULT="$here/../g2-packed-cross-item-2026-11-21"
export QT_MATCH_CANDIDATE_RESULT="$here"
export QT_MATCH_CONTROL_ELF_HASH="$here/../g1-staged-small-2026-11-21/control-executable.sha256"
exec bash "$here/../g9-two-scatter-source-radix-2026-11-21/run.sh" "$1" "$2" --g1
