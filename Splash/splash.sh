#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Use sudo to source the venv
if [ -f "$SCRIPT_DIR/.venv/bin/activate" ]; then
  # shellcheck source=/dev/null
  source "$SCRIPT_DIR/.venv/bin/activate" || sudo bash -c ". $SCRIPT_DIR/.venv/bin/activate && exec $SHELL" || true
elif [ -f "$HOME/.venv/bin/activate" ]; then
  # shellcheck source=/dev/null
  source "$HOME/.venv/bin/activate" || sudo bash -c ". $HOME/.venv/bin/activate && exec $SHELL" || true
fi

if ! command -v tte >/dev/null 2>&1; then
  # Try direct path
  if sudo /root/.venv/bin/tte --help >/dev/null 2>&1; then
    TTE_CMD="sudo /root/.venv/bin/tte"
  else
    echo "terminaltexteffects (tte) is not available." >&2
    exit 1
  fi
else
  TTE_CMD="tte"
fi

clear
cat "/home/dietpi/cyberdeck/Splash/cyberdeck.txt" | $TTE_CMD \
  --canvas-width 0 \
  --canvas-height 0 \
  --anchor-canvas c \
  --anchor-text c \
  wipe \
  --wipe-direction diagonal_top_left_to_bottom_right \
  --final-gradient-stops ff5555 8b0000 \
  --final-gradient-direction horizontal \
  --final-gradient-frames 2
