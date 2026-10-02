#!/usr/bin/env bash
# Build selected in-tree Linux modules from a package-manager source cache.
set -euo pipefail
umask 022
PATH=/usr/sbin:/usr/bin:/sbin:/bin:$PATH

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
KERNEL_RELEASE=$(uname -r)
EVIDENCE_ROOT=""
SOURCE_VERSION=""
SOURCE_PACKAGE=linux-source-6.12

usage() {
    cat <<'EOF'
Usage: scripts/build-linux-patched-modules.sh [options]

Builds the in-tree xHCI and ACPI battery modules for an installed kernel.
The verified source .deb is kept in debs/; the extracted source is per-run only.

Options:
  --kernel-release RELEASE  Target installed kernel (default: uname -r)
  --source-version VERSION  Exact linux-source package version
  --source-package PACKAGE  Package-manager source package (default: linux-source-6.12)
  --evidence DIR             Output evidence directory
  -h, --help                 Show this help
EOF
}

die() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }
log() { printf '[%s] %s\n' "$(date --iso-8601=ns)" "$*"; }

while (($#)); do
    case $1 in
        --kernel-release) KERNEL_RELEASE=${2:?missing value}; shift 2 ;;
        --source-version) SOURCE_VERSION=${2:?missing value}; shift 2 ;;
        --source-package) SOURCE_PACKAGE=${2:?missing value}; shift 2 ;;
        --evidence) EVIDENCE_ROOT=${2:?missing value}; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) die "unknown option: $1" ;;
    esac
done

[[ $(id -u) -ne 0 ]] || die "run as a normal user; this build does not need root"
command -v apt-get >/dev/null || die "APT source acquisition is not available"
command -v dpkg-query >/dev/null || die "dpkg-query is not available"
command -v dpkg-deb >/dev/null || die "dpkg-deb is not available"
command -v patch >/dev/null || die "patch is not available"
command -v make >/dev/null || die "make is not available"
command -v sha256sum >/dev/null || die "sha256sum is not available"
command -v realpath >/dev/null || die "realpath is not available"
command -v modinfo >/dev/null || die "modinfo is not available"
command -v strip >/dev/null || die "strip is not available"
command -v objcopy >/dev/null || die "objcopy is not available"
command -v strings >/dev/null || die "strings is not available"

BUILD_LINK=/lib/modules/$KERNEL_RELEASE/build
[[ -e $BUILD_LINK ]] || die "matching kernel build tree missing: $BUILD_LINK"
[[ -f "$BUILD_LINK/.config" ]] || die "matching kernel config missing: $BUILD_LINK/.config"

IMAGE_PACKAGE="linux-image-$KERNEL_RELEASE"
PACKAGE_VERSION=$(dpkg-query -W -f='${Version}' "$IMAGE_PACKAGE" 2>/dev/null || true)
[[ -n $PACKAGE_VERSION ]] || \
    die "installed package not found: $IMAGE_PACKAGE"
if [[ -z $SOURCE_VERSION ]]; then
    SOURCE_VERSION=$PACKAGE_VERSION
fi

SOURCE_DEB_DIR="$ROOT/debs"
if [[ -z $EVIDENCE_ROOT ]]; then
    EVIDENCE_ROOT="$ROOT/.build/linux-patched-modules/$KERNEL_RELEASE/$(date +%Y%m%d-%H%M%S)"
fi
EVIDENCE_ROOT=$(realpath -m "$EVIDENCE_ROOT")
WORK_SOURCE="$EVIDENCE_ROOT/source"
BUILD_OUTPUT="$EVIDENCE_ROOT/kernel-build"
mkdir -p "$SOURCE_DEB_DIR" "$EVIDENCE_ROOT"
exec > >(tee "$EVIDENCE_ROOT/build.log") 2>&1

log "kernel_release=$KERNEL_RELEASE"
log "image_package=$IMAGE_PACKAGE"
log "image_package_version=$PACKAGE_VERSION"
log "source_package=$SOURCE_PACKAGE"
log "source_package_version=$SOURCE_VERSION"
log "source_deb_dir=$SOURCE_DEB_DIR"
log "evidence_root=$EVIDENCE_ROOT"

DEB=$(find "$SOURCE_DEB_DIR" -maxdepth 1 -type f -name '*.deb' -print | while read -r candidate; do
    [[ $(dpkg-deb -f "$candidate" Package 2>/dev/null || true) == "$SOURCE_PACKAGE" ]] && \
        [[ $(dpkg-deb -f "$candidate" Version 2>/dev/null || true) == "$SOURCE_VERSION" ]] && {
        printf '%s\n' "$candidate"
        break
    }
done)
if [[ -z $DEB ]]; then
    log "source_deb=MISS; downloading exact package without installing it"
    (
        cd "$SOURCE_DEB_DIR"
        apt-get download "$SOURCE_PACKAGE=$SOURCE_VERSION"
    )
    DEB=$(find "$SOURCE_DEB_DIR" -maxdepth 1 -type f -name '*.deb' -print | while read -r candidate; do
        [[ $(dpkg-deb -f "$candidate" Package 2>/dev/null || true) == "$SOURCE_PACKAGE" ]] && \
            [[ $(dpkg-deb -f "$candidate" Version 2>/dev/null || true) == "$SOURCE_VERSION" ]] && {
            printf '%s\n' "$candidate"
            break
        }
    done)
fi
[[ -n $DEB ]] || die "exact linux-source package not found in $SOURCE_DEB_DIR"
DEB_VERSION=$(dpkg-deb -f "$DEB" Version)
[[ $DEB_VERSION == "$SOURCE_VERSION" ]] || die "source package version mismatch: $DEB_VERSION"
DEB_PACKAGE=$(dpkg-deb -f "$DEB" Package)
[[ $DEB_PACKAGE == "$SOURCE_PACKAGE" ]] || die "source package name mismatch: $DEB_PACKAGE"
printf 'file=%s\npackage=%s\nversion=%s\nsha256=%s\n' \
    "$(basename "$DEB")" "$DEB_PACKAGE" "$DEB_VERSION" "$(sha256sum "$DEB" | awk '{print $1}')" \
    >"$EVIDENCE_ROOT/source-package.txt"
log "source_deb=$(basename "$DEB")"

rm -rf "$WORK_SOURCE" "$BUILD_OUTPUT" "$EVIDENCE_ROOT/source-expanded"
mkdir -p "$EVIDENCE_ROOT/source-expanded"
dpkg-deb -x "$DEB" "$EVIDENCE_ROOT/source-expanded"
TAR=$(find "$EVIDENCE_ROOT/source-expanded/usr/src" -maxdepth 1 -type f \
    \( -name 'linux-source-6.12*.tar.*' -o -name 'linux-source-*.tar.*' \) -print -quit)
[[ -n $TAR ]] || die "source archive not found in linux-source package"
mkdir -p "$EVIDENCE_ROOT/source-unpacked"
tar -xf "$TAR" -C "$EVIDENCE_ROOT/source-unpacked"
TOP=$(find "$EVIDENCE_ROOT/source-unpacked" -mindepth 1 -maxdepth 1 -type d -print -quit)
[[ -n $TOP ]] || die "source archive did not produce a top-level directory"
mv "$TOP" "$WORK_SOURCE"
rm -rf "$EVIDENCE_ROOT/source-expanded" "$EVIDENCE_ROOT/source-unpacked"
cp "$BUILD_LINK/.config" "$WORK_SOURCE/.config"

for required in \
    "$ROOT/components/linux/001-hygon-148c-xhci-reset-on-resume.patch" \
    "$ROOT/components/linux/002-kaitian-x7h-battery-notification-delay.patch"; do
    [[ -f $required ]] || die "missing patch: $required"
    sha256sum "$required"
done >"$EVIDENCE_ROOT/patches.sha256"

grep -Fq 'pdev->device == PCI_DEVICE_ID_AMD_RAVEN2_XHCI))' "$WORK_SOURCE/drivers/usb/host/xhci-pci.c" || \
    die "xHCI source anchor missing"
grep -Fq 'battery_notification_delay_quirk' "$WORK_SOURCE/drivers/acpi/battery.c" || \
    die "battery source anchor missing"
[[ $(grep -Fc 'pdev->device == PCI_DEVICE_ID_AMD_RAVEN2_XHCI))' "$WORK_SOURCE/drivers/usb/host/xhci-pci.c") -eq 1 ]] || \
    die "xHCI source anchor is not unique"
[[ $(grep -Fc '/* Point of View mobii wintab p800w */' "$WORK_SOURCE/drivers/acpi/battery.c") -eq 1 ]] || \
    die "battery source anchor is not unique"
grep -qx 'CONFIG_USB_XHCI_PCI=m' "$BUILD_LINK/.config" || die "CONFIG_USB_XHCI_PCI is not a module"
grep -qx 'CONFIG_ACPI_BATTERY=m' "$BUILD_LINK/.config" || die "CONFIG_ACPI_BATTERY is not a module"
sha256sum "$WORK_SOURCE/drivers/usb/host/xhci-pci.c" \
    "$WORK_SOURCE/drivers/acpi/battery.c" >"$EVIDENCE_ROOT/source-before.sha256"

for patch_file in \
    "$ROOT/components/linux/001-hygon-148c-xhci-reset-on-resume.patch" \
    "$ROOT/components/linux/002-kaitian-x7h-battery-notification-delay.patch"; do
    log "dry_run=$(basename "$patch_file")"
    patch --dry-run --fuzz=0 -d "$WORK_SOURCE" -p1 <"$patch_file"
    patch --fuzz=0 -d "$WORK_SOURCE" -p1 <"$patch_file"
done

printf '%s\n' "$KERNEL_RELEASE" >"$EVIDENCE_ROOT/requested-kernel-release"
sha256sum "$WORK_SOURCE/drivers/usb/host/xhci-pci.c" \
    "$WORK_SOURCE/drivers/acpi/battery.c" >"$EVIDENCE_ROOT/source-after.sha256"

build_modules() {
    local output=$1
    mkdir -p "$output/battery" "$output/xhci"
    cp "$WORK_SOURCE/drivers/acpi/battery.c" "$output/battery/"
    printf 'obj-m += battery.o\n' >"$output/battery/Makefile"
    cp "$WORK_SOURCE/drivers/usb/host/xhci-pci.c" "$output/xhci/"
    cp "$WORK_SOURCE/drivers/usb/host/"*.h "$output/xhci/"
    printf 'obj-m += xhci-pci.o\n' >"$output/xhci/Makefile"
    make -C "$BUILD_LINK" M="$output/battery" \
        KCFLAGS="-ffile-prefix-map=$output/battery=drivers/acpi" modules
    make -C "$BUILD_LINK" M="$output/xhci" \
        KCFLAGS="-ffile-prefix-map=$output/xhci=drivers/usb/host" modules
    objcopy --remove-section=.BTF --remove-section=.BTF.base "$output/battery/battery.ko"
    objcopy --remove-section=.BTF --remove-section=.BTF.base "$output/xhci/xhci-pci.ko"
    strip --strip-debug "$output/battery/battery.ko" "$output/xhci/xhci-pci.ko"
}

MODULE_BUILD="$BUILD_OUTPUT/modules"
build_modules "$MODULE_BUILD"
cp "$MODULE_BUILD/battery/battery.ko" "$BUILD_OUTPUT/battery-a.ko"
cp "$MODULE_BUILD/xhci/xhci-pci.ko" "$BUILD_OUTPUT/xhci-pci-a.ko"
make -C "$BUILD_LINK" M="$MODULE_BUILD/battery" clean
make -C "$BUILD_LINK" M="$MODULE_BUILD/xhci" clean
build_modules "$MODULE_BUILD"
cmp "$BUILD_OUTPUT/battery-a.ko" "$MODULE_BUILD/battery/battery.ko" || \
    die "battery module A/B mismatch"
cmp "$BUILD_OUTPUT/xhci-pci-a.ko" "$MODULE_BUILD/xhci/xhci-pci.ko" || \
    die "xHCI module A/B mismatch"

BATTERY_MODULE="$MODULE_BUILD/battery/battery.ko"
XHCI_MODULE="$MODULE_BUILD/xhci/xhci-pci.ko"
[[ -f $BATTERY_MODULE ]] || die "battery.ko was not built"
[[ -f $XHCI_MODULE ]] || die "xhci-pci.ko was not built"
[[ $(modinfo -F vermagic "$BATTERY_MODULE" | awk '{print $1}') == "$KERNEL_RELEASE" ]] || \
    die "battery vermagic does not match target"
[[ $(modinfo -F vermagic "$XHCI_MODULE" | awk '{print $1}') == "$KERNEL_RELEASE" ]] || \
    die "xHCI vermagic does not match target"
if strings "$BATTERY_MODULE" "$XHCI_MODULE" | grep -Fq "$ROOT"; then
    die "built modules contain the local repository path"
fi
{
    printf 'kernelrelease=%s\n' "$KERNEL_RELEASE"
    printf 'config_sha256=%s\n' "$(sha256sum "$BUILD_LINK/.config" | awk '{print $1}')"
    printf 'battery_sha256=%s\n' "$(sha256sum "$BATTERY_MODULE" | awk '{print $1}')"
    printf 'xhci_pci_sha256=%s\n' "$(sha256sum "$XHCI_MODULE" | awk '{print $1}')"
    printf 'battery_vermagic=%s\n' "$(modinfo -F vermagic "$BATTERY_MODULE")"
    printf 'xhci_pci_vermagic=%s\n' "$(modinfo -F vermagic "$XHCI_MODULE")"
    printf 'ab_identical=1\n'
    printf 'private_paths=0\n'
} | tee "$EVIDENCE_ROOT/result.txt"

cp "$BATTERY_MODULE" "$EVIDENCE_ROOT/battery.ko"
cp "$XHCI_MODULE" "$EVIDENCE_ROOT/xhci-pci.ko"
log "BUILD_PASS evidence=$EVIDENCE_ROOT"
