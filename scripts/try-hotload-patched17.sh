#!/bin/bash
# Try to run the newly installed patched-17 module without rebooting.
# This is conservative by default: it will not stop a live Xorg/dwm session
# unless called with --stop-xorg.

set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
    echo "Run as root: sudo $0" >&2
    exit 1
fi

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

process_namespace_unreliable() {
    local init_cmd
    init_cmd="$(tr '\0' ' ' </proc/1/cmdline 2>/dev/null || true)"
    grep -Eq 'bwrap|codex-linux-sandbox' <<<"$init_cmd"
}

list_desktop_processes() {
    ps -eo pid=,tty=,comm=,args= 2>/dev/null |
        awk '$3 ~ /^(Xorg|startx|xinit|dwm|dwmblocks)$/ || $0 ~ /\/Xorg[[:space:]]/ { print }'
}

stop_xorg=0
if [[ "${1:-}" == "--stop-xorg" ]]; then
    stop_xorg=1
fi

if process_namespace_unreliable; then
    echo "ERROR: process table is isolated by Codex/bwrap; hot reload is not safe here." >&2
    echo "Run this script directly from a real SSH session or local recovery TTY, not from the Codex sandbox." >&2
    exit 2
fi

if [[ "$stop_xorg" == "1" && -z "${SSH_CONNECTION:-}" ]]; then
    current_tty="$(tty 2>/dev/null || true)"
    case "$current_tty" in
        /dev/tty[2-6]) ;;
        *)
            echo "ERROR: --stop-xorg must be run from SSH or a real recovery TTY (/dev/tty2-/dev/tty6)." >&2
            echo "Do not run it inside an Xorg terminal emulator, because stopping Xorg can terminate this script." >&2
            echo "Switch with Ctrl+Alt+F2, log in, then run:" >&2
            echo "  cd \"$ROOT\"" >&2
            echo "  sudo scripts/try-hotload-patched17.sh --stop-xorg" >&2
            exit 2
            ;;
    esac
fi

kernel="$(uname -r)"

echo "Recovery if hot reload makes the display worse:"
cat <<RECOVERY
  cd "$ROOT"
  sudo dpkg -i "$ROOT/debs/innogpu-fh2m-trixie_3.3.3.42-patched-8.deb"
  sudo scripts/disable-incompatible-userspace.sh
  printf '%s\n' innogpu | sudo tee /etc/modules-load.d/innogpu.conf
  sudo depmod -a "$kernel"
  sudo update-initramfs -u -k "$kernel"
  sudo reboot

RECOVERY

installed="$(dpkg-query -W -f='${Version}' innogpu-fh2m-trixie 2>/dev/null || true)"
if [[ "$installed" != "3.3.3.42-patched-17" ]]; then
    echo "ERROR: patched-17 is not installed; current version: ${installed:-unknown}" >&2
    exit 1
fi

if ! /sbin/dkms status innogpu-kernel | grep -F "$kernel" | grep -F installed >/dev/null; then
    echo "ERROR: DKMS module is not installed for $kernel" >&2
    /sbin/dkms status innogpu-kernel || true
    exit 1
fi

desktop_processes="$(list_desktop_processes || true)"
if [[ -n "$desktop_processes" || -S /tmp/.X11-unix/X0 ]]; then
    if [[ "$stop_xorg" != "1" ]]; then
        echo "ERROR: Xorg/dwm/startx is running; not hot-reloading by default." >&2
        echo "If you are ready to restart only the graphical session, run:" >&2
        echo "  sudo scripts/try-hotload-patched17.sh --stop-xorg" >&2
        echo "Otherwise reboot later to load patched-17 safely:" >&2
        echo "  sudo reboot" >&2
        exit 2
    fi

    echo "Stopping Xorg/dwm/startx before module reload..."
    pkill -TERM -t tty1 Xorg 2>/dev/null || true
    pkill -TERM -t tty1 xinit 2>/dev/null || true
    pkill -TERM -t tty1 startx 2>/dev/null || true
    pkill -TERM -t tty1 dwm 2>/dev/null || true
    sleep 2
    pkill -KILL -t tty1 Xorg 2>/dev/null || true
    pkill -KILL -t tty1 xinit 2>/dev/null || true
    pkill -KILL -t tty1 startx 2>/dev/null || true
    pkill -KILL -t tty1 dwm 2>/dev/null || true
fi

echo "Unloading current innogpu module..."
if ! modprobe -r innogpu; then
    echo "ERROR: innogpu could not be unloaded. A reboot is required to run patched-17." >&2
    lsmod | grep '^innogpu' || true
    exit 3
fi

echo "Loading patched-17 innogpu module..."
modprobe innogpu

echo "Repairing DRM/fbdev nodes..."
if command -v innogpu-repair-dri-nodes >/dev/null 2>&1; then
    innogpu-repair-dri-nodes || true
elif [[ -x scripts/repair-dri-nodes.sh ]]; then
    scripts/repair-dri-nodes.sh || true
fi

[[ -w /sys/class/graphics/fb0/blank ]] && printf '0\n' > /sys/class/graphics/fb0/blank || true
systemctl restart getty@tty1.service 2>/dev/null || true

echo
echo "Runtime checks after hot reload:"
dpkg -l innogpu-fh2m-trixie || true
cat /proc/driver/innogpu/gpu00/status 2>/dev/null || true
ls -l /dev/dri /dev/dri/by-path /dev/fb0 2>/dev/null || true
cat /sys/class/drm/card0-DP-1/modes 2>/dev/null || true
list_desktop_processes || true

cat <<NEXT

If tty1 is visible locally, log in as ok and run startx if it does not start
automatically. Keep hardware GL/GBM disabled for now.
NEXT
