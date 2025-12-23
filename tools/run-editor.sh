#!/usr/bin/env bash
set -euo pipefail

# Cross-platform-ish wrapper (macOS/Linux). Requires python3.
# Usage:
#   ./tools/run-editor.sh
#   ./tools/run-editor.sh --log tools/logs/editor.log

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3 "$SCRIPT_DIR/run_editor.py" "$@"
