#!/usr/bin/env bash
# Install previously built in-tree module overrides without rebooting.
set -euo pipefail
umask 022
PATH=/usr/sbin:/usr/bin:/sbin:/bin:$PATH

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
EVIDENCE_ROOT=""
KERNEL_RELEASE=""
PREFLIGHT_ONLY=0

usage() {
    cat <<'EOF'
Usage: scripts/install-linux-patched-modules.sh --evidence DIR [--preflight]

Installs battery.ko and xhci-pci.ko into updates/r51 for the exact kernel
recorded by the build evidence. It backs up the original modules and initramfs,
refreshes depmod/initramfs, and never reboots.
With --preflight it performs all read-only admission checks and stops.
EOF
}

die() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }
result_value() { awk -F= -v key="$1" '$1 == key {sub(/^[^=]*=/, ""); print; exit}' "$2"; }
while (($#)); do
    case $1 in
        --evidence) EVIDENCE_ROOT=${2:?missing value}; shift 2 ;;
        --preflight) PREFLIGHT_ONLY=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) die "unknown option: $1" ;;
    esac
done
[[ -n $EVIDENCE_ROOT ]] || { usage >&2; exit 2; }
EVIDENCE_ROOT=$(realpath "$EVIDENCE_ROOT")
[[ -d $EVIDENCE_ROOT ]] || die "evidence directory not found: $EVIDENCE_ROOT"
[[ -r "$EVIDENCE_ROOT/result.txt" ]] || die "build result missing"
KERNEL_RELEASE=$(awk -F= '$1 == "kernelrelease" {print $2}' "$EVIDENCE_ROOT/result.txt")
[[ -n $KERNEL_RELEASE ]] || die "kernelrelease missing from build result"
[[ -f "$EVIDENCE_ROOT/battery.ko" && -f "$EVIDENCE_ROOT/xhci-pci.ko" ]] || \
    die "built modules missing from evidence"

MODULE_ROOT=/lib/modules/$KERNEL_RELEASE
BUILD_LINK=$MODULE_ROOT/build
[[ -e $BUILD_LINK ]] || die "matching kernel build tree missing: $BUILD_LINK"
dpkg-query -W -f='${Status}' "linux-image-$KERNEL_RELEASE" 2>/dev/null | \
    grep -qx 'install ok installed' || die "target kernel image package is not installed"
[[ -f "/boot/vmlinuz-$KERNEL_RELEASE" && -f "/boot/initrd.img-$KERNEL_RELEASE" ]] || \
    die "target kernel boot files are incomplete"

EXPECTED_BATTERY=$(result_value battery_sha256 "$EVIDENCE_ROOT/result.txt")
EXPECTED_XHCI=$(result_value xhci_pci_sha256 "$EVIDENCE_ROOT/result.txt")
[[ $(sha256sum "$EVIDENCE_ROOT/battery.ko" | awk '{print $1}') == "$EXPECTED_BATTERY" ]] || \
    die "battery module hash differs from build result"
[[ $(sha256sum "$EVIDENCE_ROOT/xhci-pci.ko" | awk '{print $1}') == "$EXPECTED_XHCI" ]] || \
    die "xHCI module hash differs from build result"
[[ $(modinfo -F vermagic "$EVIDENCE_ROOT/battery.ko" | awk '{print $1}') == "$KERNEL_RELEASE" ]] || \
    die "battery vermagic mismatch"
[[ $(modinfo -F vermagic "$EVIDENCE_ROOT/xhci-pci.ko" | awk '{print $1}') == "$KERNEL_RELEASE" ]] || \
    die "xHCI vermagic mismatch"

SOURCE_FILE=$(result_value file "$EVIDENCE_ROOT/source-package.txt")
SOURCE_SHA=$(result_value sha256 "$EVIDENCE_ROOT/source-package.txt")
[[ -f "$ROOT/debs/$SOURCE_FILE" ]] || die "source package archive is missing from debs/"
[[ $(sha256sum "$ROOT/debs/$SOURCE_FILE" | awk '{print $1}') == "$SOURCE_SHA" ]] || \
    die "source package archive hash mismatch"

RECOVERY_TTY=$(who | awk '$2 ~ /^tty[0-9]+$/ {print $2; exit}')
[[ -n $RECOVERY_TTY ]] || die "no logged-in physical TTY recovery session"
AC_ONLINE=0
for supply in /sys/class/power_supply/*; do
    [[ -r "$supply/type" && -r "$supply/online" ]] || continue
    [[ $(cat "$supply/type") == Mains && $(cat "$supply/online") == 1 ]] && AC_ONLINE=1
done
((AC_ONLINE)) || die "trusted AC power is not online"

TARGET="$MODULE_ROOT/updates/r51"
BACKUP="$EVIDENCE_ROOT/install-backup"
[[ ! -e "$TARGET/battery.ko" && ! -e "$TARGET/xhci-pci.ko" ]] || \
    die "r51 override already exists; refuse to overwrite"

{
    printf 'preflight=PASS\n'
    printf 'timestamp=%s\n' "$(date --iso-8601=ns)"
    printf 'kernel_release=%s\n' "$KERNEL_RELEASE"
    printf 'recovery_tty=%s\n' "$RECOVERY_TTY"
    printf 'ac_online=1\n'
    printf 'source_file=%s\n' "$SOURCE_FILE"
    printf 'source_sha256=%s\n' "$SOURCE_SHA"
    printf 'battery_sha256=%s\n' "$EXPECTED_BATTERY"
    printf 'xhci_pci_sha256=%s\n' "$EXPECTED_XHCI"
} >"$EVIDENCE_ROOT/install-preflight.txt"

if ((PREFLIGHT_ONLY)); then
    printf 'PREFLIGHT_PASS no_host_mutation=1 evidence=%s\n' "$EVIDENCE_ROOT"
    exit 0
fi
if [[ $(id -u) -ne 0 ]]; then
    exec sudo -- "$(realpath "$0")" --evidence "$EVIDENCE_ROOT"
fi

mkdir -p "$TARGET" "$BACKUP"
exec > >(tee "$EVIDENCE_ROOT/install.log") 2>&1

printf 'kernel_release=%s\n' "$KERNEL_RELEASE"
printf 'target=%s\n' "$TARGET"
printf 'recovery_tty=%s\n' "$RECOVERY_TTY"
printf 'ac_online=1\n'
printf 'started=%s\n' "$(date --iso-8601=ns)"

BATTERY_BEFORE=$(modinfo -k "$KERNEL_RELEASE" -n battery)
XHCI_BEFORE=$(modinfo -k "$KERNEL_RELEASE" -n xhci_pci)
GRUB_DEFAULT_BEFORE=$(sha256sum /etc/default/grub | awk '{print $1}')
GRUB_CFG_BEFORE=$(sha256sum /boot/grub/grub.cfg | awk '{print $1}')
{
    printf 'battery_before=%s\n' "$BATTERY_BEFORE"
    printf 'xhci_before=%s\n' "$XHCI_BEFORE"
    sha256sum "$BATTERY_BEFORE" "$XHCI_BEFORE" "/boot/initrd.img-$KERNEL_RELEASE" \
        /etc/default/grub /boot/grub/grub.cfg
} >"$EVIDENCE_ROOT/preinstall-state.txt"

for original in \
    "$MODULE_ROOT/kernel/drivers/acpi/battery.ko" \
    "$MODULE_ROOT/kernel/drivers/acpi/battery.ko.xz" \
    "$MODULE_ROOT/kernel/drivers/usb/host/xhci-pci.ko" \
    "$MODULE_ROOT/kernel/drivers/usb/host/xhci-pci.ko.xz"; do
    [[ -e $original ]] || continue
    cp -a "$original" "$BACKUP/$(basename "$original")"
done
cp -a "/boot/initrd.img-$KERNEL_RELEASE" "$BACKUP/initrd.img-$KERNEL_RELEASE.before"

MUTATED=0
rollback_on_exit() {
    local rc=$?
    trap - EXIT
    if ((rc != 0 && MUTATED)); then
        rm -f "$TARGET/battery.ko" "$TARGET/xhci-pci.ko"
        rmdir "$TARGET" 2>/dev/null || true
        depmod -a "$KERNEL_RELEASE" || true
        cp -a "$BACKUP/initrd.img-$KERNEL_RELEASE.before" "/boot/initrd.img-$KERNEL_RELEASE" || true
        printf 'INSTALL_FAIL_ROLLED_BACK rc=%s\n' "$rc" >&2
    fi
    exit "$rc"
}
trap rollback_on_exit EXIT

MUTATED=1
install -m 0644 "$EVIDENCE_ROOT/battery.ko" "$TARGET/battery.ko"
install -m 0644 "$EVIDENCE_ROOT/xhci-pci.ko" "$TARGET/xhci-pci.ko"
SIGN_FILE="$BUILD_LINK/scripts/sign-file"
if [[ -x $SIGN_FILE && -r /var/lib/dkms/mok.key && -r /var/lib/dkms/mok.pub ]]; then
    "$SIGN_FILE" sha256 /var/lib/dkms/mok.key /var/lib/dkms/mok.pub "$TARGET/battery.ko"
    "$SIGN_FILE" sha256 /var/lib/dkms/mok.key /var/lib/dkms/mok.pub "$TARGET/xhci-pci.ko"
    printf 'module_signing=dkms_key\n'
elif mokutil --sb-state 2>/dev/null | grep -qi enabled; then
    die "Secure Boot enabled but no usable module signing key"
else
    printf 'module_signing=unsigned_secure_boot_disabled\n'
fi
depmod -a "$KERNEL_RELEASE"
[[ $(modinfo -k "$KERNEL_RELEASE" -n battery) == "$TARGET/battery.ko" ]] || \
    die "depmod did not select r51 battery module"
[[ $(modinfo -k "$KERNEL_RELEASE" -n xhci_pci) == "$TARGET/xhci-pci.ko" ]] || \
    die "depmod did not select r51 xHCI module"
update-initramfs -u -k "$KERNEL_RELEASE"
lsinitramfs "/boot/initrd.img-$KERNEL_RELEASE" >"$EVIDENCE_ROOT/initramfs.list"
grep -Fqx "usr/lib/modules/$KERNEL_RELEASE/updates/r51/battery.ko" \
    "$EVIDENCE_ROOT/initramfs.list" || \
    die "r51 battery module missing from initramfs"
grep -Fqx "usr/lib/modules/$KERNEL_RELEASE/updates/r51/xhci-pci.ko" \
    "$EVIDENCE_ROOT/initramfs.list" || \
    die "r51 xHCI module missing from initramfs"

[[ $(sha256sum /etc/default/grub | awk '{print $1}') == "$GRUB_DEFAULT_BEFORE" ]] || \
    die "/etc/default/grub changed unexpectedly"
[[ $(sha256sum /boot/grub/grub.cfg | awk '{print $1}') == "$GRUB_CFG_BEFORE" ]] || \
    die "grub.cfg changed unexpectedly"

sha256sum "$TARGET/battery.ko" "$TARGET/xhci-pci.ko" \
    "/boot/initrd.img-$KERNEL_RELEASE" >"$EVIDENCE_ROOT/install-artifacts.sha256"
trap - EXIT
printf 'install_rc=0\nfinished=%s\n' "$(date --iso-8601=ns)" | tee -a "$EVIDENCE_ROOT/install.log"
printf 'INSTALL_PASS no_reboot=1 evidence=%s\n' "$EVIDENCE_ROOT"
