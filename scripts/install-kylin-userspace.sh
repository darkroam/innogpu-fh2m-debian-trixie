#!/bin/bash
# Install Innosilicon FH2M userspace GL stack copied from a mounted Kylin/UOS root
# or from $HOME/src/innogpu-kylin-userspace-backup/root.
#
# Debian Trixie warning: Kylin innogpu_drv.so is Xorg video ABI 24, while
# Debian Trixie Xorg is ABI 25. This installer avoids the previous global
# ld.so override and disables the modesetting OutputClass so Xorg does not
# crash in modesetting/glamoregl with vendor libraries.

set -euo pipefail

log() { echo "[innogpu-kylin-userspace] $*"; }
err() { echo "ERROR: $*" >&2; exit 1; }

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    err "run as root: sudo $0 [/mnt/kylin-root|$HOME/src/innogpu-kylin-userspace-backup/root]"
fi

SRC_ROOT="${1:-${INNOGPU_KYLIN_ROOT:-$HOME/src/innogpu-kylin-userspace-backup/root}}"
[[ -d "$SRC_ROOT" ]] || err "source root not found: $SRC_ROOT"

require_file() {
    local p="$1"
    [[ -e "$SRC_ROOT/$p" || -L "$SRC_ROOT/$p" ]] || err "missing from source root: $p"
}

require_file usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so
require_file usr/lib/x86_64-linux-gnu/innogpu-fh2m/libEGL_inno.so.0.0.0
require_file usr/lib/x86_64-linux-gnu/innogpu-fh2m/libGLX_inno.so.0.0.0
require_file usr/lib/x86_64-linux-gnu/innogpu-fh2m/libGL_INNO_MESA.so.1
require_file usr/lib/x86_64-linux-gnu/innogpu-fh2m/libsrv_um_inno.so.1
require_file usr/lib/x86_64-linux-gnu/innogpu-fh2m/libusc_inno.so.1
require_file usr/lib/xorg/modules/drivers/innogpu_drv.so
require_file usr/share/glvnd/egl_vendor.d/00_inno.json

STAMP="$(date +%Y%m%d-%H%M%S)"
backup_path() {
    local p="$1"
    if [[ -e "$p" || -L "$p" ]]; then
        if [[ ! -e "$p.before-innogpu-userspace-$STAMP" && ! -L "$p.before-innogpu-userspace-$STAMP" ]]; then
            mv -f "$p" "$p.before-innogpu-userspace-$STAMP"
            log "backup $p -> $p.before-innogpu-userspace-$STAMP"
        fi
    fi
}

log "copying 64-bit Innosilicon userspace libraries from $SRC_ROOT"
install -d /usr/lib/x86_64-linux-gnu/innogpu-fh2m /usr/lib/x86_64-linux-gnu/dri
cp -a "$SRC_ROOT/usr/lib/x86_64-linux-gnu/innogpu-fh2m/." /usr/lib/x86_64-linux-gnu/innogpu-fh2m/
cp -a "$SRC_ROOT/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so" /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so
cp -a "$SRC_ROOT/usr/lib/x86_64-linux-gnu/dri/swrast_inno_dri.so" /usr/lib/x86_64-linux-gnu/dri/swrast_inno_dri.so 2>/dev/null || true
cp -a "$SRC_ROOT/usr/lib/x86_64-linux-gnu/dri/innogpu_drv_video.so" /usr/lib/x86_64-linux-gnu/dri/innogpu_drv_video.so 2>/dev/null || true
ln -sfn innogpu_dri.so /usr/lib/x86_64-linux-gnu/dri/inno_dri.so
ln -sfn swrast_inno_dri.so /usr/lib/x86_64-linux-gnu/dri/kms_swrast_inno_dri.so 2>/dev/null || true
# Do NOT install vendor GBM backend — it crashes on Debian Trixie libgbm.
# /usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so is deliberately excluded.

if [[ -d "$SRC_ROOT/usr/lib/i386-linux-gnu/innogpu-fh2m" ]]; then
    log "copying optional 32-bit Innosilicon userspace libraries"
    install -d /usr/lib/i386-linux-gnu/innogpu-fh2m /usr/lib/i386-linux-gnu/dri
    cp -a "$SRC_ROOT/usr/lib/i386-linux-gnu/innogpu-fh2m/." /usr/lib/i386-linux-gnu/innogpu-fh2m/
    cp -a "$SRC_ROOT/usr/lib/i386-linux-gnu/dri/innogpu_dri.so" /usr/lib/i386-linux-gnu/dri/innogpu_dri.so 2>/dev/null || true
    cp -a "$SRC_ROOT/usr/lib/i386-linux-gnu/dri/swrast_inno_dri.so" /usr/lib/i386-linux-gnu/dri/swrast_inno_dri.so 2>/dev/null || true
    ln -sfn innogpu_dri.so /usr/lib/i386-linux-gnu/dri/inno_dri.so
    ln -sfn swrast_inno_dri.so /usr/lib/i386-linux-gnu/dri/kms_swrast_inno_dri.so 2>/dev/null || true
    # GBM backend excluded — crashes on Debian Trixie libgbm.
fi

log "installing GLVND/Vulkan/OpenCL config"
install -d /usr/share/glvnd/egl_vendor.d /etc/vulkan/icd.d /etc/OpenCL/vendors /usr/share/drirc.d
# EGL vendor JSON (00_inno.json) is deliberately excluded — vendor libEGL crashes on Debian Trixie.
# Vulkan and OpenCL ICDs are installed for apps that request them explicitly.
cp -a "$SRC_ROOT/etc/vulkan/icd.d/innoconf.json" /etc/vulkan/icd.d/innoconf.json 2>/dev/null || true
cp -a "$SRC_ROOT/etc/OpenCL/vendors/INNO.icd" /etc/OpenCL/vendors/INNO.icd 2>/dev/null || true
cp -a "$SRC_ROOT/usr/share/drirc.d/01-innogpu_drv.conf" /usr/share/drirc.d/01-innogpu_drv.conf 2>/dev/null || true

log "installing vendor Xorg DDX"
install -d /usr/lib/xorg/modules/drivers /etc/X11/xorg.conf.d /usr/lib/x86_64-linux-gnu
backup_path /usr/lib/xorg/modules/drivers/innogpu_drv.so
cp -a "$SRC_ROOT/usr/lib/xorg/modules/drivers/innogpu_drv.so" /usr/lib/xorg/modules/drivers/innogpu_drv.so

# Do NOT add /usr/lib/x86_64-linux-gnu/innogpu-fh2m to ld.so.conf.d.
# That makes Debian's modesetting/glamor load vendor libgbm and can segfault.
rm -f /etc/ld.so.conf.d/0-innogpu.conf /etc/ld.so.conf.d/0-innogpu-i386.conf

log "creating allowlisted vendor-library symlinks in the default linker path"
# EGL and GLX vendor libs (libEGL_inno, libGLX_inno) are deliberately excluded —
# they crash on Debian Trixie. Apps use Mesa swrast via DRI instead.
for lib in \
    libGL_INNO_MESA.so libGLESv1_CM_INNO_MESA.so libGLESv2_INNO_MESA.so \
    libglapi_inno.so libglapi_inno.so.0 libinno_dri_support.so \
    libinno_mesa_wsi.so libsrv_um_inno.so libufwriter_inno.so \
    libusc_inno.so libVK_INNO.so libINNOOCL.so; do
    if [[ -e "/usr/lib/x86_64-linux-gnu/innogpu-fh2m/$lib" || -L "/usr/lib/x86_64-linux-gnu/innogpu-fh2m/$lib" ]]; then
        ln -sfn "innogpu-fh2m/$lib" "/usr/lib/x86_64-linux-gnu/$lib"
    fi
done

# Disable the package's safe modesetting OutputClass for the hardware-GL attempt.
# If it remains active, Debian may pick modesetting first and crash before innogpu DDX owns the screen.
for f in /usr/share/X11/xorg.conf.d/10-innogpu.conf /etc/X11/xorg.conf.d/10-innogpu.conf; do
    if [[ -e "$f" || -L "$f" ]]; then
        backup_path "$f"
    fi
done

backup_path /etc/X11/xorg.conf
cat > /etc/X11/xorg.conf <<'XORG'
Section "ServerFlags"
    Option "IgnoreABI" "true"
EndSection

Section "Device"
    Identifier "innogpu"
    Driver "innogpu"
    Option "PrimaryGPU" "yes"
    Option "TearFree" "true"
EndSection

Section "Screen"
    Identifier "screen0"
    Device "innogpu"
EndSection
XORG

ldconfig

if command -v innogpu-repair-dri-nodes >/dev/null 2>&1; then
    innogpu-repair-dri-nodes || true
elif [[ -x "$(dirname "$0")/repair-dri-nodes.sh" ]]; then
    "$(dirname "$0")/repair-dri-nodes.sh" || true
fi

log "installed candidate config. Validate without reboot with:"
log "  sudo innogpu-test-xorg-once"
log "If it fails, recover with: sudo innogpu-disable-incompatible-userspace"
