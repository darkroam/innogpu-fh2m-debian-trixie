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

deb="${INNOGPU_PATCHED8_DEB:-$(first_existing \
    "$ROOT/innogpu-fh2m-trixie_3.3.3.42-patched-8.deb" || true)}"
fallback_deb="${INNOGPU_PATCHED17_DEB:-$(first_existing \
    "$ROOT/innogpu-fh2m-trixie_3.3.3.42-patched-17.deb" || true)}"
kernel="$(uname -r)"

[[ -f "$deb" ]] || { echo "Missing package: patched-8 deb. Put it in the repo root or set INNOGPU_PATCHED8_DEB." >&2; exit 1; }
[[ -f "$fallback_deb" ]] || { echo "Missing fallback package: patched-17 deb. Put it in the repo root or set INNOGPU_PATCHED17_DEB." >&2; exit 1; }

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
echo "Restoring safe soft userspace configuration after patched-8 downgrade..."
if [[ -x scripts/disable-incompatible-userspace.sh ]]; then
    scripts/disable-incompatible-userspace.sh || true
elif command -v innogpu-disable-incompatible-userspace >/dev/null 2>&1; then
    innogpu-disable-incompatible-userspace || true
fi

echo
echo "Enabling innogpu boot autoload for the baseline test..."
install -d -m 0755 /etc/modules-load.d
printf '%s\n' innogpu > /etc/modules-load.d/innogpu.conf

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
echo "Current safe userspace state:"
sed -n '1,80p' /etc/X11/xorg.conf 2>/dev/null || true
ls -l /etc/ld.so.conf.d/*innogpu* /usr/lib/x86_64-linux-gnu/dri/*inno* 2>/dev/null || true

echo
echo "Current runtime state before reboot:"
cat /proc/driver/innogpu/gpu00/status 2>/dev/null || true
ls -l /dev/dri /dev/dri/by-path /dev/fb0 2>/dev/null || true
find -L /sys/class/backlight -maxdepth 2 -type f -printf '%p=' -exec cat {} \; 2>/dev/null || true
ps -ef | grep -E 'agetty|getty|login|startx|Xorg|xinit|dwm' | grep -v grep || true

cat <<NEXT

patched-8 is installed and boot autoload is enabled.
Reboot is required to run the patched-8 kernel module:
  sudo reboot

After reboot, verify only the baseline first:
  dpkg -l innogpu-fh2m-trixie
  cat /proc/driver/innogpu/gpu00/status
  ls -l /dev/dri /dev/fb0
  ps -ef | grep -E 'agetty|getty|login|startx|Xorg|xinit|dwm' | grep -v grep
NEXT
