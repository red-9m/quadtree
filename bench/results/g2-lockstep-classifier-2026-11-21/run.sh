#!/usr/bin/env bash
set -euo pipefail
here=$(dirname "$(realpath "$0")")
export QT_MATCH_CONTROL_RESULT="$here/../g9-two-scatter-source-radix-2026-11-21"
export QT_MATCH_CANDIDATE_RESULT="$here"
export QT_MATCH_CONTROL_ELF_HASH="$here/control-executable.sha256"
exec bash "$QT_MATCH_CONTROL_RESULT/run.sh" "$@"
