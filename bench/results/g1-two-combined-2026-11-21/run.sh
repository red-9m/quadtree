#!/usr/bin/env bash
# Matched G1 screen: aligned (-falign-functions=64) packed cross-item control vs aligned combined candidate.
set -euo pipefail
here=$(dirname "$(realpath "$0")")
export QT_MATCH_CONTROL_RESULT="$here/../g2-packed-cross-item-2026-11-21"
export QT_MATCH_CANDIDATE_RESULT="$here"
export QT_MATCH_CONTROL_ELF_HASH="$here/../g1-layout-control-2026-11-21/control-executable.sha256"
exec bash "$here/../g9-two-scatter-source-radix-2026-11-21/run.sh" "$1" "$2" --g1
