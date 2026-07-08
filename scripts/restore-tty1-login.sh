#!/bin/bash
# Restore the local tty1 username/password login prompt without starting Xorg.

set -euo pipefail

LOG=${LOG:-/tmp/innogpu-restore-tty1-login.log}
ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    echo "ERROR: run as root: sudo $0" >&2
    exit 1
fi

exec > >(tee "$LOG") 2>&1

section() {
    printf '\n===== %s =====\n' "$*"
}

try_write() {
    local value="$1"
    local path="$2"

    if [[ -e "$path" && -w "$path" ]]; then
        printf '%s\n' "$value" > "$path" || true
        echo "wrote $value -> $path"
    fi
}

section "Recovery if this is not what you want"
cat <<'RECOVERY'
This script does not install packages or reboot.
If you want Xorg again after tty1 is visible, log in locally and run:
  startx
RECOVERY

section "Stop Xorg/dwm on tty1 only"
pkill -TERM -t tty1 Xorg 2>/dev/null || true
pkill -TERM -t tty1 xinit 2>/dev/null || true
pkill -TERM -t tty1 startx 2>/dev/null || true
pkill -TERM -t tty1 dwm 2>/dev/null || true
sleep 1
pkill -KILL -t tty1 Xorg 2>/dev/null || true
pkill -KILL -t tty1 xinit 2>/dev/null || true
pkill -KILL -t tty1 startx 2>/dev/null || true
pkill -KILL -t tty1 dwm 2>/dev/null || true

section "Repair device nodes"
if [[ -x "$ROOT/scripts/repair-dri-nodes.sh" ]]; then
    "$ROOT/scripts/repair-dri-nodes.sh" || true
elif command -v innogpu-repair-dri-nodes >/dev/null 2>&1; then
    innogpu-repair-dri-nodes || true
fi

section "Force display on"
for bl in /sys/class/backlight/*; do
    [[ -d "$bl" ]] || continue
    try_write 0 "$bl/bl_power"
    if [[ -r "$bl/max_brightness" ]]; then
        max="$(cat "$bl/max_brightness" 2>/dev/null || true)"
        if [[ "$max" =~ ^[0-9]+$ && "$max" -gt 0 ]]; then
            try_write "$max" "$bl/brightness"
        fi
    fi
done

try_write 0 /sys/class/graphics/fb0/blank
try_write 0 /sys/module/kernel/parameters/consoleblank

if command -v setterm >/dev/null 2>&1 && [[ -w /dev/tty1 ]]; then
    setterm -blank 0 -powerdown 0 -powersave off >/dev/tty1 2>/dev/null || true
    setterm -foreground white -background black -store >/dev/tty1 2>/dev/null || true
fi

section "Restart getty@tty1"
systemctl restart getty@tty1.service
sleep 1

section "Status"
systemctl status getty@tty1.service --no-pager || true
ps -ef | grep -E 'agetty|getty|login|startx|Xorg|xinit|dwm' | grep -v grep || true
ls -l /dev/tty1 /dev/fb0 /dev/dri/card0 2>/dev/null || true
cat /sys/class/tty/tty0/active 2>/dev/null || true
find -L /sys/class/backlight -maxdepth 2 -type f -printf '%p=' -exec cat {} \; 2>/dev/null || true
find -L /sys/class/graphics/fb0 -maxdepth 1 -type f -printf '%p=' -exec cat {} \; 2>/dev/null || true

section "Done"
echo "tty1 should now show the username/password login prompt."
echo "Log saved to: $LOG"
