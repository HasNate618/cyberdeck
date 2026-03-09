#!/usr/bin/env bash
# setup.sh — Cyberdeck terminal environment setup
#
# Installs and configures:
#   - cage + foot (Wayland kiosk terminal)
#   - TerminessNerdFontMono
#   - fastfetch on session start
#   - tty1 autologin (no login prompt)
#   - tmux session with CoreSerial stats in background window
#   - dotfiles stowed via GNU Stow
#
# Idempotent: safe to re-run.
#
# Usage:
#   git clone <this-repo> ~/cyberdeck
#   git clone <dotfiles-repo> ~/dotfiles
#   bash ~/cyberdeck/Setup/setup.sh

set -euo pipefail

DOTFILES_DIR="$HOME/dotfiles"
FONT_DIR="$HOME/.local/share/fonts/TerminessNerdFont"
BIN_DIR="$HOME/.local/bin"
SETUP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

step() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }
ok()   { printf '    \033[1;32m✓\033[0m %s\n' "$*"; }
info() { printf '    \033[0;37m%s\033[0m\n' "$*"; }

# ── 1. Packages ──────────────────────────────────────────────────────────────
step "Installing packages: cage foot stow fastfetch"
sudo apt-get install -y cage foot stow fastfetch
ok "Packages installed"

# ── 2. Font ───────────────────────────────────────────────────────────────────
step "Installing TerminessNerdFontMono"
mkdir -p "$FONT_DIR"
cp "$SETUP_DIR"/fonts/TerminessNerdFontMono-*.ttf "$FONT_DIR/"
fc-cache -f
ok "Font installed and cache updated"

# ── 3. Dotfiles ───────────────────────────────────────────────────────────────
step "Stowing dotfiles (nvim tmux foot superfile)"
if [ ! -d "$DOTFILES_DIR" ]; then
    echo "ERROR: $DOTFILES_DIR not found. Clone your dotfiles repo there first." >&2
    exit 1
fi
cd "$DOTFILES_DIR"
for pkg in nvim tmux foot superfile; do
    stow -t "$HOME" "$pkg" 2>/dev/null && ok "Stowed $pkg" || info "$pkg already stowed or conflict (check manually)"
done
cd - > /dev/null

# ── 4. Cage launch script ─────────────────────────────────────────────────────
step "Installing start-terminal.sh"
mkdir -p "$BIN_DIR"
cp "$SETUP_DIR/start-terminal.sh" "$BIN_DIR/start-terminal.sh"
chmod +x "$BIN_DIR/start-terminal.sh"
ok "start-terminal.sh installed to $BIN_DIR"

# ── 5. bash_profile (autostart on tty1) ───────────────────────────────────────
step "Writing ~/.bash_profile"
if grep -q 'start-terminal' "$HOME/.bash_profile" 2>/dev/null; then
    info "~/.bash_profile already configured — skipping"
else
    cp "$SETUP_DIR/bash_profile" "$HOME/.bash_profile"
    ok "~/.bash_profile written"
fi

# ── 6. Autologin on tty1 ─────────────────────────────────────────────────────
step "Enabling autologin on tty1"
OVERRIDE_DIR=/etc/systemd/system/getty@tty1.service.d
sudo mkdir -p "$OVERRIDE_DIR"
sudo tee "$OVERRIDE_DIR/autologin.conf" > /dev/null << 'EOF'
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin dietpi --noclear %I $TERM
EOF
sudo systemctl daemon-reload
ok "tty1 autologin enabled"

# ── 7. Suppress DietPi login banner ──────────────────────────────────────────
step "Suppressing DietPi login banner"
touch "$HOME/.hushlogin"
ok "~/.hushlogin created — DietPi banner suppressed"

# ── 8. Suppress MOTD ─────────────────────────────────────────────────────────
step "Clearing MOTD"
sudo truncate -s 0 /etc/motd
sudo sed -i 's/^session\s\+optional\s\+pam_motd.so/# &/' /etc/pam.d/login
ok "MOTD cleared and pam_motd disabled"

# ── Done ─────────────────────────────────────────────────────────────────────
printf '\n\033[1;32mSetup complete.\033[0m\n'
info "Reboot to start the cage+foot+tmux session automatically."
info "  Retroarch : Ctrl+Alt+F2 -> ra -> Ctrl+Alt+F1"
info "  MATE      : Ctrl+Alt+F3 -> login -> startx"
