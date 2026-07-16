#!/bin/bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
    echo "Run as root: sudo $0" >&2
    exit 1
fi

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

first_existing() {
    local p
    for p in "$@"; do
        if [[ -f "$p" ]]; then
            printf '%s\n' "$p"
            return 0
        fi
    done
    return 1
}

deb="${INNOGPU_PATCHED17_DEB:-$(first_existing \
    "$ROOT/innogpu-fh2m-trixie_3.3.3.42-patched-17.deb" || true)}"
fallback_deb="${INNOGPU_PATCHED8_DEB:-$(first_existing \
    "$ROOT/innogpu-fh2m-trixie_3.3.3.42-patched-8.deb" || true)}"
kernel="$(uname -r)"

[[ -f "$deb" ]] || { echo "Missing package: patched-17 deb. Put it in the repo root or set INNOGPU_PATCHED17_DEB." >&2; exit 1; }
[[ -f "$fallback_deb" ]] || { echo "Missing fallback package: patched-8 deb. Put it in the repo root or set INNOGPU_PATCHED8_DEB." >&2; exit 1; }

cat <<RECOVERY
Recovery if this fails or the next reboot is worse:
  cd "$ROOT"
  sudo dpkg -i $fallback_deb
  sudo scripts/disable-incompatible-userspace.sh
  printf '%s\n' innogpu | sudo tee /etc/modules-load.d/innogpu.conf
  sudo depmod -a "$kernel"
  sudo update-initramfs -u -k "$kernel"
  sudo reboot

RECOVERY

echo "Installing $deb ..."
dpkg -i "$deb"

echo
echo "Keeping safe soft userspace path enabled (modesetting + Mesa llvmpipe)..."
if [[ -x scripts/disable-incompatible-userspace.sh ]]; then
    scripts/disable-incompatible-userspace.sh || true
elif command -v innogpu-disable-incompatible-userspace >/dev/null 2>&1; then
    innogpu-disable-incompatible-userspace || true
fi

echo
echo "Installing X11 display management for the desktop user..."
if [[ -x "$ROOT/scripts/install-xdisplay-user.sh" ]]; then
    "$ROOT/scripts/install-xdisplay-user.sh"
elif [[ -x /usr/share/innogpu-fh2m-trixie/install-xdisplay-user.sh ]]; then
    INNOGPU_DISPLAY_SOURCE_DIR=/usr/share/innogpu-fh2m-trixie \
        /usr/share/innogpu-fh2m-trixie/install-xdisplay-user.sh
else
    echo "ERROR: install-xdisplay-user.sh is missing" >&2
    exit 1
fi

echo
echo "Enabling boot autoload..."
install -d -m 0755 /etc/modules-load.d
printf '%s\n' innogpu > /etc/modules-load.d/innogpu.conf

echo
echo "Installing boot-time DRM/fbdev node repair service..."
if command -v innogpu-install-dri-node-repair-service >/dev/null 2>&1; then
    innogpu-install-dri-node-repair-service || true
elif [[ -x scripts/install-dri-node-repair-service.sh ]]; then
    scripts/install-dri-node-repair-service.sh || true
fi

echo
echo "Refreshing module metadata/initramfs..."
depmod -a "$kernel" || true
if command -v update-initramfs >/dev/null 2>&1; then
    update-initramfs -u -k "$kernel" || true
fi

echo
echo "DKMS status:"
/sbin/dkms status innogpu-kernel || true

if ! /sbin/dkms status innogpu-kernel | grep -F "installed" | grep -F "$kernel" >/dev/null; then
    echo "ERROR: DKMS did not install innogpu for $kernel." >&2
    echo "Inspect: /var/lib/dkms/innogpu-kernel/2.2/build/make.log" >&2
    exit 1
fi

echo
echo "Installed package:"
dpkg -l innogpu-fh2m-trixie || true

echo
echo "Current runtime state before reboot:"
cat /proc/driver/innogpu/gpu00/status 2>/dev/null || true
ls -l /dev/dri /dev/dri/by-path /dev/fb0 2>/dev/null || true
systemctl --no-pager --full status innogpu-repair-dri-nodes.service 2>/dev/null || true
ps -ef | grep -E 'agetty|getty|login|startx|Xorg|xinit|dwm' | grep -v grep || true

cat <<NEXT

patched-17 is installed. It uses the Deepin 202504 DKMS kernel baseline plus
local connector/tty fallback fixes, while keeping soft Xorg userspace.

The installed package and DKMS build are now verified without rebooting.
The running kernel module may still be the previously loaded one until innogpu
is reloaded.

Next non-reboot step, only if no important graphical work is running:
  sudo scripts/try-hotload-patched17.sh

If hot reload reports that innogpu is busy or cannot be replaced safely, then a
reboot is required to run patched-17:
  sudo reboot

After patched-17 is actually loaded, first verify local username/password tty1
appears. Do not enable hardware GL/GBM yet.
NEXT
