#!/bin/bash
# Prepare the safe Xorg+dwm path: Xorg modesetting + Debian Mesa llvmpipe.
# This intentionally does not enable vendor GL/GBM acceleration.

set -euo pipefail

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    echo "ERROR: run as root: sudo $0" >&2
    exit 1
fi

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
SOURCE_DIR="$ROOT/scripts"
if [[ ! -d "$SOURCE_DIR" ]]; then
    SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
fi
X_USER="${INNOGPU_X_USER:-${SUDO_USER:-${USER:-}}}"
X_HOME="${INNOGPU_X_HOME:-$(getent passwd "$X_USER" 2>/dev/null | cut -d: -f6)}"
X_HOME="${X_HOME:-$HOME}"

echo "Recovery if this makes Xorg worse:"
echo "  cd \"$ROOT\""
echo "  sudo scripts/restore-tty1-login.sh"
echo "  sudo scripts/prepare-soft-xorg-dwm.sh"
echo

echo "[1/6] Disabling vendor GL/GBM and selecting Xorg modesetting..."
if [[ -x "$ROOT/scripts/disable-incompatible-userspace.sh" ]]; then
    "$ROOT/scripts/disable-incompatible-userspace.sh"
elif command -v innogpu-disable-incompatible-userspace >/dev/null 2>&1; then
    innogpu-disable-incompatible-userspace
else
    echo "ERROR: disable-incompatible-userspace helper is missing" >&2
    exit 1
fi

echo "[2/6] Repairing DRM/fbdev nodes..."
if [[ -x "$ROOT/scripts/repair-dri-nodes.sh" ]]; then
    "$ROOT/scripts/repair-dri-nodes.sh" || true
elif command -v innogpu-repair-dri-nodes >/dev/null 2>&1; then
    innogpu-repair-dri-nodes || true
fi

echo "[3/6] Forcing local panel/backlight on..."
for bl in /sys/class/backlight/*; do
    [[ -d "$bl" ]] || continue
    [[ -w "$bl/bl_power" ]] && printf '0\n' > "$bl/bl_power" || true
    if [[ -r "$bl/max_brightness" && -w "$bl/brightness" ]]; then
        max="$(cat "$bl/max_brightness" 2>/dev/null || true)"
        if [[ "$max" =~ ^[0-9]+$ && "$max" -gt 0 ]]; then
            printf '%s\n' "$max" > "$bl/brightness" || true
        fi
    fi
done
[[ -w /sys/class/graphics/fb0/blank ]] && printf '0\n' > /sys/class/graphics/fb0/blank || true
[[ -w /sys/module/kernel/parameters/consoleblank ]] && printf '0\n' > /sys/module/kernel/parameters/consoleblank || true

echo "[4/6] Installing X11 display management for user $X_USER..."
[[ -x "$SOURCE_DIR/install-xdisplay-user.sh" ]] || {
    echo "ERROR: display installer is missing: $SOURCE_DIR/install-xdisplay-user.sh" >&2
    exit 1
}
INNOGPU_DISPLAY_SOURCE_DIR="$SOURCE_DIR" \
INNOGPU_X_USER="$X_USER" \
INNOGPU_X_HOME="$X_HOME" \
    "$SOURCE_DIR/install-xdisplay-user.sh"

echo "[5/6] Keeping tty1 at login prompt if Xorg is currently not requested..."
init_cmd="$(tr '\0' ' ' </proc/1/cmdline 2>/dev/null || true)"
if grep -Eq 'bwrap|codex-linux-sandbox' <<<"$init_cmd"; then
    echo "process namespace is isolated; not changing getty state based on pgrep"
elif ! pgrep -x Xorg >/dev/null 2>&1; then
    systemctl restart getty@tty1.service || true
fi

echo "[6/6] Current soft-path status:"
dpkg -l innogpu-fh2m-trixie || true
ls -l /dev/dri/card0 /dev/dri/renderD128 /dev/fb0 2>/dev/null || true
cat /proc/driver/innogpu/gpu00/status 2>/dev/null || true
printf '\nXorg config:\n'
sed -n '1,120p' /etc/X11/xorg.conf 2>/dev/null || true
printf '\nVendor GL/GBM disabled files:\n'
ls -l /etc/ld.so.conf.d/*innogpu*.disabled /usr/lib/x86_64-linux-gnu/dri/*inno*.disabled 2>/dev/null || true
printf '\nProcesses:\n'
ps -eo pid=,tty=,comm=,args= 2>/dev/null |
    awk '$3 ~ /^(agetty|getty|login|Xorg|startx|xinit|dwm|dwmblocks)$/ || $0 ~ /\/Xorg[[:space:]]/ { print }' || true

cat <<'NEXT'

Soft Xorg+dwm path is prepared.
Next step:
  1. On the local screen, log in on tty1.
  2. The existing profile should automatically run startx on tty1.
  3. If it does not, run: startx

Do not enable hardware GL/GBM yet.
NEXT
