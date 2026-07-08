#!/bin/bash
# Remove the installed innogpu-fh2m-trixie package and disable boot autoload.

set -euo pipefail

EXPECTED_VERSION="${1:-}"

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    echo "ERROR: run as root: sudo $0 [expected-version]" >&2
    exit 1
fi

kernel="$(uname -r)"
installed="$(dpkg-query -W -f='${Version}' innogpu-fh2m-trixie 2>/dev/null || true)"

if [[ -z "$installed" ]]; then
    echo "innogpu-fh2m-trixie is not installed."
    exit 0
fi

if [[ -n "$EXPECTED_VERSION" && "$installed" != "$EXPECTED_VERSION" ]]; then
    echo "ERROR: expected $EXPECTED_VERSION, but installed version is $installed" >&2
    echo "Set FORCE=1 to remove anyway." >&2
    if [[ "${FORCE:-0}" != "1" ]]; then
        exit 1
    fi
fi

cat <<EOF
Removing innogpu-fh2m-trixie $installed.

Recovery if this is not intended:
  sudo dpkg -i innogpu-fh2m-trixie_3.3.3.42-patched-17.deb
  sudo reboot

EOF

if command -v innogpu-disable-incompatible-userspace >/dev/null 2>&1; then
    innogpu-disable-incompatible-userspace || true
fi

rm -f /etc/modules-load.d/innogpu.conf
systemctl disable --now innogpu-repair-dri-nodes.service 2>/dev/null || true
rm -f /etc/systemd/system/innogpu-repair-dri-nodes.service
systemctl daemon-reload 2>/dev/null || true

dpkg -r innogpu-fh2m-trixie

if command -v dkms >/dev/null 2>&1; then
    dkms remove innogpu-kernel/2.2 --all 2>/dev/null || true
fi

depmod -a "$kernel" || true
if command -v update-initramfs >/dev/null 2>&1; then
    update-initramfs -u -k "$kernel" || true
fi

echo
echo "Uninstall complete. Reboot to ensure no old innogpu module remains loaded:"
echo "  sudo reboot"
