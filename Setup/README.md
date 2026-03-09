# Setup

Reproducible setup for the cyberdeck terminal environment on a fresh DietPi install.

## Prerequisites

- DietPi (Debian trixie) on Raspberry Pi 3B
- `~/dotfiles` cloned from the dotfiles repo
- Internet access (for apt packages)

## What it installs

| Component | Purpose |
|-----------|---------|
| `cage` | Wayland kiosk compositor |
| `foot` | Wayland terminal emulator |
| `stow` | Dotfile symlink manager |
| `fastfetch` | System info on session start |
| TerminessNerdFontMono | Nerd Font with icon glyphs |

## What it configures

| Config | Effect |
|--------|--------|
| `~/.bash_profile` | Autostarts cage on tty1 login |
| `~/.local/bin/start-terminal.sh` | Launches cage → foot → tmux |
| `/etc/systemd/system/getty@tty1.service.d/autologin.conf` | Autologin as `dietpi` — no password prompt |
| `~/.hushlogin` | Suppresses DietPi login banner |
| `/etc/motd` | Cleared (blank) |
| `/etc/pam.d/login` | `pam_motd` disabled |
| Dotfiles stowed | nvim, tmux, foot, superfile symlinked into `~/.config/` |

## Usage

```bash
git clone <dotfiles-repo> ~/dotfiles
git clone <cyberdeck-repo> ~/cyberdeck
bash ~/cyberdeck/Setup/setup.sh
sudo reboot
```

## Boot flow

```
Power on
  -> DietPi autologin (tty1, no prompt)
    -> ~/.bash_profile
      -> ~/.local/bin/start-terminal.sh
        -> cage (Wayland kiosk)
          -> foot (terminal)
            -> tmux session "main"
                 window 0: fastfetch, then bash
                 window 1: CoreSerial stats (background)
```

## Files

```
Setup/
├── setup.sh            Main setup script (run this)
├── start-terminal.sh   Cage launch wrapper (copied to ~/.local/bin/)
├── bash_profile        Shell autostart (copied to ~/.bash_profile)
├── fonts/              TerminessNerdFontMono TTF files
└── README.md           This file
```

## Multi-TTY layout

| TTY | Access | Use |
|-----|--------|-----|
| tty1 | autologin | cage + foot + tmux (default) |
| tty2 | `Ctrl+Alt+F2` | retroarch (`ra` alias), then `Ctrl+Alt+F1` to return |
| tty3 | `Ctrl+Alt+F3` | `startx` for MATE desktop |
