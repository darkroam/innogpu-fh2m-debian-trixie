#!/bin/bash
# Safe display recovery/diagnostics for innogpu black-screen debugging.

set -euo pipefail

PAINT=0
RUN_XORG_TEST=0
LOG=${LOG:-/tmp/innogpu-display-diagnose.log}
ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
X_USER="${INNOGPU_X_USER:-${SUDO_USER:-${USER:-}}}"
X_HOME="${INNOGPU_X_HOME:-$(getent passwd "$X_USER" 2>/dev/null | cut -d: -f6)}"
X_HOME="${X_HOME:-$HOME}"

usage() {
    cat <<'USAGE'
Usage: sudo scripts/display-recover-and-diagnose.sh [--paint-red] [--xorg-test]

Default actions are non-destructive:
  - repair missing /dev/dri and /dev/fb0 nodes
  - force panel_backlight online at max brightness when present
  - unblank fbcon and disable kernel console blanking
  - collect driver, DRM, backlight, fbdev, Xorg, and kernel-log state

Options:
  --paint-red   write a red test frame to /dev/fb0 so panel visibility is obvious
  --xorg-test   run the isolated one-shot Xorg test on :9/vt8
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --paint-red) PAINT=1 ;;
        --xorg-test) RUN_XORG_TEST=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

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

section "Recovery if this makes the display worse"
cat <<RECOVERY
cd "$ROOT"
sudo dpkg -i innogpu-fh2m-trixie_3.3.3.42-patched-17.deb
sudo reboot
RECOVERY

section "Repair device nodes"
if [[ -x "$ROOT/scripts/repair-dri-nodes.sh" ]]; then
    "$ROOT/scripts/repair-dri-nodes.sh" || true
elif command -v innogpu-repair-dri-nodes >/dev/null 2>&1; then
    innogpu-repair-dri-nodes || true
fi

section "Force backlight and console on"
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

if [[ "$PAINT" == "1" ]]; then
    section "Paint red framebuffer test"
    if [[ -w /dev/fb0 ]]; then
        width="$(cat /sys/class/graphics/fb0/virtual_size 2>/dev/null | cut -d, -f1 || true)"
        height="$(cat /sys/class/graphics/fb0/virtual_size 2>/dev/null | cut -d, -f2 || true)"
        bpp="$(cat /sys/class/graphics/fb0/bits_per_pixel 2>/dev/null || true)"
        if [[ "$width" =~ ^[0-9]+$ && "$height" =~ ^[0-9]+$ && "$bpp" == "32" ]]; then
            perl -e 'print "\x00\x00\xff\x00" x ($ARGV[0] * $ARGV[1])' "$width" "$height" > /dev/fb0
            echo "painted /dev/fb0 red (${width}x${height}x${bpp})"
        else
            echo "skip paint: unsupported fb geometry width=$width height=$height bpp=$bpp"
        fi
    else
        echo "skip paint: /dev/fb0 is not writable"
    fi
fi

section "Runtime status"
date -Is || true
uname -a || true
dpkg -l innogpu-fh2m-trixie || true
lsmod | grep -E '^innogpu|^drm|^drm_kms_helper' || true
cat /proc/driver/innogpu/gpu00/status 2>/dev/null || true

section "Device nodes"
ls -l /dev/dri /dev/dri/by-path /dev/fb* 2>/dev/null || true

section "DRM state"
for f in /sys/class/drm/card0-DP-1/status /sys/class/drm/card0-DP-1/enabled /sys/class/drm/card0-DP-1/dpms; do
    [[ -e "$f" ]] && printf '%s=' "$f" && cat "$f"
done
cat /sys/class/drm/card0-DP-1/modes 2>/dev/null || true

section "Backlight state"
find -L /sys/class/backlight -maxdepth 2 -type f -printf '%p=' -exec cat {} \; 2>/dev/null || true

section "fbcon state"
cat /sys/class/tty/tty0/active 2>/dev/null || true
for vt in /sys/class/vtconsole/vtcon*; do
    [[ -e "$vt/name" ]] || continue
    printf '%s ' "$vt"
    cat "$vt/name" "$vt/bind" 2>/dev/null || true
done
find -L /sys/class/graphics/fb0 -maxdepth 1 -type f -printf '%p=' -exec cat {} \; 2>/dev/null || true

section "Xorg processes and latest log"
ps -ef | grep -E 'Xorg|startx|xinit|dwm' | grep -v grep || true
latest_xlog="$(ls -t "$X_HOME/.local/share/xorg/Xorg.0.log" /var/log/Xorg.0.log 2>/dev/null | head -1 || true)"
if [[ -n "$latest_xlog" ]]; then
    echo "latest Xorg log: $latest_xlog"
    grep -nE '\((EE|WW)\)|Fatal|failed|Failed|segmentation|no screens|Configure crtc|glamor|modeset|innogpu' "$latest_xlog" | tail -120 || true
fi

if [[ "$RUN_XORG_TEST" == "1" ]]; then
    section "One-shot Xorg test"
    if [[ -x "$ROOT/scripts/test-xorg-once.sh" ]]; then
        "$ROOT/scripts/test-xorg-once.sh" /tmp/innogpu-xorg-test.log || true
    else
        echo "missing scripts/test-xorg-once.sh"
    fi
fi

section "Kernel log tail"
journalctl -b -k --no-pager 2>/dev/null \
    | grep -Ei 'innogpu|innodpu|panel|backlight|pwm|bl_en|dp|edp|drm|fbcon|firmware' \
    | tail -300 || true

section "Done"
echo "Log saved to: $LOG"
