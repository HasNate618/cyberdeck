#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -f "$SCRIPT_DIR/.venv/bin/activate" ]; then
  # Preferred unified cyberdeck virtualenv.
  # shellcheck source=/dev/null
  source "$SCRIPT_DIR/.venv/bin/activate"
elif [ -f "$HOME/.venv/bin/activate" ]; then
  # Backward-compatible fallback.
  # shellcheck source=/dev/null
  source "$HOME/.venv/bin/activate"
fi

if ! command -v tte >/dev/null 2>&1; then
  echo "terminaltexteffects (tte) is not available in the active environment." >&2
  exit 1
fi

clear
cat "$SCRIPT_DIR/logo.txt" | tte \
  --canvas-width 0 \
  --canvas-height 0 \
  --anchor-canvas c \
  --anchor-text c \
  wipe \
  --final-gradient-stops 008000 00dd00 \
  --final-gradient-frames 2
