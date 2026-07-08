#!/bin/bash
# Start the safe Xorg+dwm path on tty1 from SSH.
# Uses Xorg modesetting + Debian Mesa llvmpipe; does not enable vendor GL/GBM.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
LOG=${LOG:-/tmp/innogpu-start-soft-xorg-dwm.log}
USER_NAME="${INNOGPU_X_USER:-${SUDO_USER:-${USER:-}}}"
USER_HOME="${INNOGPU_X_HOME:-$(getent passwd "$USER_NAME" 2>/dev/null | cut -d: -f6)}"
USER_HOME="${USER_HOME:-$HOME}"
DISPLAY_NUM=${INNOGPU_X_DISPLAY:-:0}
VT_NUM=${INNOGPU_X_VT:-1}

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    echo "ERROR: run as root: sudo $0" >&2
    exit 1
fi

process_namespace_unreliable() {
    local init_cmd
    init_cmd="$(tr '\0' ' ' </proc/1/cmdline 2>/dev/null || true)"
    grep -Eq 'bwrap|codex-linux-sandbox' <<<"$init_cmd"
}

list_desktop_processes() {
    ps -eo pid=,tty=,comm=,args= 2>/dev/null |
        awk '$3 ~ /^(Xorg|startx|xinit|dwm|dwmblocks)$/ || $0 ~ /\/Xorg[[:space:]]/ { print }'
}

if process_namespace_unreliable; then
    echo "ERROR: process table is isolated by Codex/bwrap; starting/stopping tty1 Xorg is not safe here." >&2
    echo "Run this script directly from a real SSH session or local recovery TTY." >&2
    exit 2
fi

exec > >(tee "$LOG") 2>&1

section() {
    printf '\n===== %s =====\n' "$*"
}

section "Recovery if this fails"
cat <<RECOVERY
cd $ROOT
sudo scripts/restore-tty1-login.sh
RECOVERY

section "Preflight"
command -v openvt >/dev/null 2>&1 || { echo "ERROR: openvt is missing" >&2; exit 1; }
command -v su >/dev/null 2>&1 || { echo "ERROR: su is missing" >&2; exit 1; }
[[ -d "$USER_HOME" ]] || { echo "ERROR: missing home: $USER_HOME" >&2; exit 1; }
[[ -e /dev/dri/card0 ]] || { echo "ERROR: missing /dev/dri/card0" >&2; exit 1; }

section "Ensure soft Xorg configuration"
if [[ -x "$ROOT/scripts/prepare-soft-xorg-dwm.sh" ]]; then
    "$ROOT/scripts/prepare-soft-xorg-dwm.sh" || true
elif command -v innogpu-prepare-soft-xorg-dwm >/dev/null 2>&1; then
    innogpu-prepare-soft-xorg-dwm || true
fi

section "Stop current tty1/Xorg users"
pkill -TERM -t tty1 Xorg 2>/dev/null || true
pkill -TERM -t tty1 xinit 2>/dev/null || true
pkill -TERM -t tty1 startx 2>/dev/null || true
pkill -TERM -t tty1 dwm 2>/dev/null || true
sleep 1
systemctl stop getty@tty1.service || true
pkill -KILL -t tty1 Xorg 2>/dev/null || true
pkill -KILL -t tty1 xinit 2>/dev/null || true
pkill -KILL -t tty1 startx 2>/dev/null || true
pkill -KILL -t tty1 dwm 2>/dev/null || true

section "Force panel on"
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

section "Start Xorg+dwm on tty1"
start_cmd=$(cat <<'CMD'
cd "$HOME"
export XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
export XINITRC="${XDG_CONFIG_HOME}/x11/xinitrc"
export DISPLAY=:0
exec /usr/bin/startx "$XINITRC" -- :0 vt1 -keeptty
CMD
)

openvt -c "$VT_NUM" -f -- su - "$USER_NAME" -c "$start_cmd" &

sleep 8

section "Runtime status"
processes="$(list_desktop_processes || true)"
printf '%s\n' "$processes"
latest_xlog="$(ls -t "$USER_HOME/.local/share/xorg/Xorg.0.log" /var/log/Xorg.0.log 2>/dev/null | head -1 || true)"
if [[ -n "$latest_xlog" ]]; then
    echo "latest Xorg log: $latest_xlog"
    grep -nE '\((EE|WW)\)|Fatal|failed|Failed|segmentation|no screens|Configure crtc|glamor|modeset|innogpu|Server terminated' "$latest_xlog" | tail -160 || true
fi

if grep -Eq 'Xorg .*(:0| :0)' <<<"$processes" &&
   awk '$3 == "dwm" { found=1 } END { exit found ? 0 : 1 }' <<<"$processes"; then
    echo "RESULT: soft Xorg+dwm is running"
    exit 0
fi

echo "RESULT: soft Xorg+dwm did not stay running"
echo "Restoring tty1 login prompt..."
if [[ -x "$ROOT/scripts/restore-tty1-login.sh" ]]; then
    "$ROOT/scripts/restore-tty1-login.sh" || true
fi
exit 1
