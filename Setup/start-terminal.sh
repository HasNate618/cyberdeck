#!/usr/bin/env bash
# start-terminal.sh — Cage + foot + tmux session launcher
#
# Called from ~/.bash_profile on tty1 autologin.
# Creates a persistent tmux session "main" with two windows:
#   window 0 (shell): fastfetch system info, then interactive bash
#   window 1 (stats): CoreSerial stats sender (runs in background)
#
# Exit all tmux windows to return to the TTY.
# To run retroarch: Ctrl+Alt+F2 -> ra -> Ctrl+Alt+F1 to return.

STATS_SCRIPT="$HOME/cyberdeck/CoreSerial/send_stats.py"

if ! tmux has-session -t main 2>/dev/null; then
    tmux new-session -d -s main -n shell "fastfetch; exec bash"
    if [ -f "$STATS_SCRIPT" ]; then
        tmux new-window -t main:1 -n stats \
            "python3 $STATS_SCRIPT --port /dev/ttyACM0 --interval 1.0"
    fi
    tmux select-window -t main:0
fi

exec cage -- foot -e tmux attach-session -t main
