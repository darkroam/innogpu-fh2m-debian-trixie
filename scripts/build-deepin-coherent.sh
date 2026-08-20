#!/bin/bash
# Build a Debian Trixie package from the complete Deepin 202504 payload.
# Historical patched packages are never accepted as an input package.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

PATCH_VERSION=${PATCH_VERSION:-}
SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-}
APPLY_DP_FBCON_FALLBACK=${APPLY_DP_FBCON_FALLBACK:-0}
APPLY_PANEL_BACKLIGHT_FALLBACK=${APPLY_PANEL_BACKLIGHT_FALLBACK:-0}
APPLY_PANEL_PLATFORM_FALLBACK=${APPLY_PANEL_PLATFORM_FALLBACK:-0}
APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE=${APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE:-0}
APPLY_LOCAL_CONNECTOR_ACPI_MAP=${APPLY_LOCAL_CONNECTOR_ACPI_MAP:-0}
APPLY_LOCAL_INTERNAL_EDP=${APPLY_LOCAL_INTERNAL_EDP:-0}
APPLY_FBDEV_IO_MMAP=${APPLY_FBDEV_IO_MMAP:-0}
APPLY_PVR_INIT_DIAGNOSTIC=${APPLY_PVR_INIT_DIAGNOSTIC:-0}
APPLY_INVISIBLE_READ_NO_WRITEBACK=${APPLY_INVISIBLE_READ_NO_WRITEBACK:-0}
APPLY_DMA_RESV_USAGE_FIX=${APPLY_DMA_RESV_USAGE_FIX:-0}

if [[ ! "$PATCH_VERSION" =~ ^[0-9]+$ ]] || (( PATCH_VERSION <= 20 )); then
    echo "ERROR: set PATCH_VERSION to a new version greater than 20" >&2
    echo "Historical patched-19/20 artifacts predate the current package ownership boundary and must not be overwritten." >&2
    exit 1
fi
if [[ ! "$SOURCE_DATE_EPOCH" =~ ^[0-9]+$ ]] || (( SOURCE_DATE_EPOCH <= 0 )); then
    echo "ERROR: set SOURCE_DATE_EPOCH to the reviewed release timestamp" >&2
    echo "Fixed release timestamps are required for reproducible package bytes." >&2
    exit 1
fi
export SOURCE_DATE_EPOCH
OUT_DEB=${OUT_DEB:-$ROOT/debs/innogpu-fh2m-trixie_3.3.3.42-patched-${PATCH_VERSION}.deb}

if [[ -z "${INNOGPU_DEEPIN_DEB:-}" ]]; then
    for candidate in \
        "$ROOT/debs/innogpu-fh2m_20250421190503-debug_amd64.deb" \
        "$ROOT/innogpu-fh2m_20250421190503-debug_amd64.deb"; do
        if [[ -f "$candidate" ]]; then
            INNOGPU_DEEPIN_DEB="$candidate"
            break
        fi
    done
fi

DEEPIN_DEB=${INNOGPU_DEEPIN_DEB:-}
[[ -f "$DEEPIN_DEB" ]] || {
    echo "ERROR: missing Deepin 202504 base deb; set INNOGPU_DEEPIN_DEB" >&2
    exit 1
}

base_package=$(dpkg-deb -f "$DEEPIN_DEB" Package)
base_version=$(dpkg-deb -f "$DEEPIN_DEB" Version)
if [[ "$base_package" != "innogpu-fh2m" || "$base_version" != "20250421190503-debug" ]]; then
    echo "ERROR: unsupported base package: $base_package $base_version" >&2
    echo "Expected innogpu-fh2m 20250421190503-debug." >&2
    exit 1
fi

W=$(mktemp -d /tmp/innogpu-deepin-coherent.XXXXXX)
trap 'rm -rf "$W"' EXIT
mkdir -p "$(dirname "$OUT_DEB")"
mkdir -p "$W/root" "$W/verify"
dpkg-deb -x "$DEEPIN_DEB" "$W/root"

DKMS_SRC="$W/root/usr/src/innogpu-kernel-2.2"
[[ -d "$DKMS_SRC" ]] || { echo "ERROR: Deepin DKMS source is missing" >&2; exit 1; }

vendor_files=(
    usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so
    usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so
    usr/lib/x86_64-linux-gnu/innogpu-fh2m/libgbm.so.1.0.0
    usr/lib/x86_64-linux-gnu/innogpu-fh2m/libglapi_inno.so.0.0.0
    usr/lib/xorg/modules/drivers/innogpu_drv.so
    usr/share/glvnd/egl_vendor.d/00_inno.json
)
firmware_files=(
    lib/firmware/innogpu/fh2m.fw
    lib/firmware/innogpu/fh2m.sh
    lib/firmware/innogpu/fh2c.fw
    lib/firmware/innogpu/fh2c.sh
)
for relative in "${vendor_files[@]}"; do
    [[ -e "$W/root/$relative" || -L "$W/root/$relative" ]] || {
        echo "ERROR: Deepin ABI file is missing: $relative" >&2
        exit 1
    }
done
for relative in "${firmware_files[@]}"; do
    [[ -e "$W/root/$relative" ]] || {
        echo "ERROR: Deepin shader firmware is missing: $relative" >&2
        exit 1
    }
done

(
    cd "$DKMS_SRC"
    patch -p6 < "$ROOT/patches/001-kernel-6.12-compat.patch"
    if [[ "$APPLY_DP_FBCON_FALLBACK" == "1" ]]; then
        patch -p1 < "$ROOT/patches/002-dp-fbdev-fallback-mode.patch"
    fi
    if [[ "$APPLY_PANEL_BACKLIGHT_FALLBACK" == "1" ]]; then
        patch -p1 < "$ROOT/patches/003-panel-backlight-fallback.patch"
    fi
    if [[ "$APPLY_PANEL_PLATFORM_FALLBACK" == "1" ]]; then
        patch -p1 < "$ROOT/patches/004-panel-platform-fallback.patch"
    fi
    if [[ "$APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE" == "1" ]]; then
        patch -p1 < "$ROOT/patches/005-backlight-force-initial-enable.patch"
    fi
    if [[ "$APPLY_LOCAL_CONNECTOR_ACPI_MAP" == "1" ]]; then
        patch -p1 < "$ROOT/patches/006-local-connector-acpi-map.patch"
    fi
    if [[ "$APPLY_LOCAL_INTERNAL_EDP" == "1" ]]; then
        patch -p1 < "$ROOT/patches/009-local-internal-edp-connector.patch"
    fi
    if [[ "$APPLY_FBDEV_IO_MMAP" == "1" ]]; then
        patch -p1 < "$ROOT/patches/007-fbdev-io-mmap.patch"
    fi
    if [[ "$APPLY_PVR_INIT_DIAGNOSTIC" == "1" ]]; then
        patch -p1 < "$ROOT/patches/008-pvr-init-diagnostic.patch"
    fi
    if [[ "$APPLY_INVISIBLE_READ_NO_WRITEBACK" == "1" ]]; then
        patch -p1 < "$ROOT/patches/023-invisible-read-no-writeback.patch"
    fi
    if [[ "$APPLY_DMA_RESV_USAGE_FIX" == "1" ]]; then
        patch -p1 < "$ROOT/patches/025-dma-resv-usage-rw.patch"
    fi
    find . \( -name '*.orig' -o -name '*.rej' \) -delete
)

python3 "$ROOT/tools/patch-gpupll-object.py" \
    "$DKMS_SRC/innogpu/innogpu.o_shipped"

install -d \
    "$W/root/etc/ld.so.conf.d" \
    "$W/root/usr/share/innogpu-fh2m-trixie" \
    "$W/root/usr/bin" \
    "$W/root/usr/sbin"
printf '%s\n' '/usr/lib/x86_64-linux-gnu/innogpu-fh2m' \
    > "$W/root/etc/ld.so.conf.d/0-innogpu-hwgl.conf"

helpers=(
    disable-incompatible-userspace.sh
    repair-dri-nodes.sh
    test-xorg-once.sh
    restore-dp1-mode-x11.sh
    xdisplay-session.sh
    install-xdisplay-user.sh
    restore-tty1-login.sh
    display-recover-and-diagnose.sh
    prepare-soft-xorg-dwm.sh
    start-soft-xorg-dwm-from-ssh.sh
    check-soft-xorg-dwm.sh
    install-dri-node-repair-service.sh
)
for helper in "${helpers[@]}"; do
    install -m 0755 "$ROOT/scripts/$helper" \
        "$W/root/usr/share/innogpu-fh2m-trixie/$helper"
done

declare -A command_targets=(
    [innogpu-disable-incompatible-userspace]=disable-incompatible-userspace.sh
    [innogpu-repair-dri-nodes]=repair-dri-nodes.sh
    [innogpu-test-xorg-once]=test-xorg-once.sh
    [innogpu-restore-dp1-mode-x11]=restore-dp1-mode-x11.sh
    [innogpu-restore-tty1-login]=restore-tty1-login.sh
    [innogpu-display-recover-and-diagnose]=display-recover-and-diagnose.sh
    [innogpu-prepare-soft-xorg-dwm]=prepare-soft-xorg-dwm.sh
    [innogpu-start-soft-xorg-dwm]=start-soft-xorg-dwm-from-ssh.sh
    [innogpu-check-soft-xorg-dwm]=check-soft-xorg-dwm.sh
    [innogpu-install-dri-node-repair-service]=install-dri-node-repair-service.sh
)
for command_name in "${!command_targets[@]}"; do
    target=${command_targets[$command_name]}
    ln -sfn "../share/innogpu-fh2m-trixie/$target" "$W/root/usr/bin/$command_name"
    ln -sfn "../share/innogpu-fh2m-trixie/$target" "$W/root/usr/sbin/$command_name"
done

rm -rf "$W/root/DEBIAN"
install -d "$W/root/DEBIAN"
installed_size=$(du -sk --exclude=DEBIAN "$W/root" | awk '{print $1}')
cat > "$W/root/DEBIAN/control" <<EOF
Package: innogpu-fh2m-trixie
Version: 3.3.3.42-patched-${PATCH_VERSION}
Section: graphics
Priority: optional
Architecture: amd64
Installed-Size: ${installed_size}
Depends: dkms, build-essential, libdrm2, libepoxy0, libpixman-1-0, libwayland-server0, libxcb-randr0
Recommends: linux-headers-amd64, libegl1, libgles2, libgl1, libglx0, libgles1, libglvnd0
Conflicts: innogpu-fh2m, innogpu-fh2m-kernel-dkms, innogpu-kernel-dkms
Replaces: innogpu-fh2m, innogpu-fh2m-kernel-dkms, innogpu-kernel-dkms
Maintainer: Tim Hant <tthantclaw@outlook.com>
Homepage: https://github.com/timhant/innogpu-fh2m-debian-trixie
Description: Innosilicon Fantasy II-M driver (coherent Deepin 202504 baseline)
 Complete Deepin 202504 kernel and graphics userspace payload, patched for
 Debian Trixie kernel 6.12. DRI, GBM, GLAPI, GLVND and Xorg DDX are kept from
 one vendor release to preserve their private ABI.
EOF

cat > "$W/root/DEBIAN/postinst" <<EOF
#!/bin/bash
set -e

PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export PATH

case "\${1:-}" in
    configure)
        ;;
    abort-upgrade|abort-remove|abort-deconfigure)
        exit 0
        ;;
    *)
        echo "postinst called with unknown argument \${1:-}" >&2
        exit 1
        ;;
esac

kernel_ver=\$(uname -r)
echo "Configuring innogpu-fh2m-trixie patched-${PATCH_VERSION} from Deepin 202504..."

install -d /etc/modprobe.d
printf '%s\n' 'options innogpu firmware_en=1' > /etc/modprobe.d/innogpu.conf

dkms add -m innogpu-kernel -v 2.2 2>/dev/null || true
dkms build -m innogpu-kernel -v 2.2 -k "\$kernel_ver" --force
dkms install -m innogpu-kernel -v 2.2 -k "\$kernel_ver" --force

# The package ships a complete, matching Deepin DRI/GBM/GLAPI/DDX set.
# Never move or restore an individual vendor library here.
if [ ! -f /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so ]; then
    echo "ERROR: coherent Deepin innogpu_dri.so is missing" >&2
    exit 1
fi

ldconfig
depmod -a "\$kernel_ver"
if command -v update-initramfs >/dev/null 2>&1; then
    update-initramfs -u -k "\$kernel_ver"
fi

echo "Installed coherent Deepin 202504 userspace; module autoload policy was not changed."
exit 0
EOF

cat > "$W/root/DEBIAN/prerm" <<'EOF'
#!/bin/bash
set -e

case "${1:-}" in
    remove|upgrade|deconfigure)
        dkms remove -m innogpu-kernel -v 2.2 --all 2>/dev/null || true
        ;;
    failed-upgrade)
        ;;
    *)
        echo "prerm called with unknown argument ${1:-}" >&2
        exit 1
        ;;
esac

exit 0
EOF

cat > "$W/root/DEBIAN/postrm" <<'EOF'
#!/bin/bash
set -e

case "${1:-}" in
    remove|purge|upgrade|failed-upgrade|abort-install|abort-upgrade|disappear)
        ldconfig
        ;;
    *)
        echo "postrm called with unknown argument ${1:-}" >&2
        ;;
esac

exit 0
EOF

chmod 0755 "$W/root/DEBIAN/postinst" "$W/root/DEBIAN/prerm" "$W/root/DEBIAN/postrm"

dpkg-deb --root-owner-group --build "$W/root" "$OUT_DEB"
"$ROOT/scripts/check-release-package.sh" "$OUT_DEB"
dpkg-deb -x "$OUT_DEB" "$W/verify"

for relative in "${vendor_files[@]}"; do
    if ! cmp -s "$W/root/$relative" "$W/verify/$relative"; then
        echo "ERROR: built package changed Deepin ABI file: $relative" >&2
        exit 1
    fi
done
for relative in "${firmware_files[@]}"; do
    if ! cmp -s "$W/root/$relative" "$W/verify/$relative"; then
        echo "ERROR: built package changed Deepin firmware payload: $relative" >&2
        exit 1
    fi
done

if ! LD_LIBRARY_PATH="$W/verify/usr/lib/x86_64-linux-gnu/innogpu-fh2m" \
    ldd -r "$W/verify/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so" \
    > "$W/ldd-r.txt" 2>&1; then
    cat "$W/ldd-r.txt" >&2
    echo "ERROR: built Deepin DRI has unresolved runtime symbols" >&2
    exit 1
fi
if grep -q 'undefined symbol:' "$W/ldd-r.txt"; then
    cat "$W/ldd-r.txt" >&2
    echo "ERROR: built Deepin DRI has unresolved runtime symbols" >&2
    exit 1
fi

echo "Built coherent Deepin package: $OUT_DEB"
echo "Base: $base_package $base_version"
sha256sum "$OUT_DEB"
