#!/bin/bash
# Enable the Deepin 202504 Innosilicon desktop hardware-GL path after the
# isolated DDX VT test has proven runtime acceleration. This is intentionally
# gated and reversible; do not use it to bypass the DDX test.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
if [[ $# -gt 0 ]]; then
    SRC_ROOT="$1"
elif [[ -x "$ROOT/scripts/prepare-deepin-userspace-root.sh" ]]; then
    SRC_ROOT="$("$ROOT/scripts/prepare-deepin-userspace-root.sh")"
else
    SRC_ROOT="${INNOGPU_DEEPIN_ROOT:-$ROOT/third_party/innogpu-fh2m-deepin-202504/root}"
fi
RESULT_FILE="$ROOT/baselines/latest-ddx-test/result.txt"
STAMP="$(date +%Y%m%d-%H%M%S)"

log() { echo "[innogpu-deepin-hwgl] $*"; }
err() { echo "ERROR: $*" >&2; exit 1; }

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    err "run as root: sudo $0"
fi

cat <<EOF
Recovery if the Deepin desktop hardware-GL trial fails:
  cd "$ROOT"
  sudo scripts/disable-incompatible-userspace.sh
  sudo reboot

Full package rollback to patched-17:
  cd "$ROOT"
  sudo dpkg -i "$ROOT/debs/innogpu-fh2m-trixie_3.3.3.42-patched-17.deb"
  sudo scripts/disable-incompatible-userspace.sh
  sudo reboot

Original fallback remains patched-8 and is not modified by this script.
EOF

[[ -d "$SRC_ROOT" ]] || err "source root not found: $SRC_ROOT"

if [[ "${INNOGPU_ALLOW_UNVERIFIED_DDX:-0}" != "1" ]]; then
    [[ -r "$RESULT_FILE" ]] || err "missing DDX gate result: run scripts/run-local-ddx-vt-test.sh first"
    [[ -r "$ROOT/baselines/latest-ddx-test/summary.txt" ]] || \
        err "missing local DDX gate summary: run scripts/run-local-ddx-vt-test.sh on this device first"
    result="$(cat "$RESULT_FILE")"
    [[ "$result" == "PASS_VENDOR_DDX_RUNTIME_ACCELERATION" ]] || \
        err "DDX gate is '$result', not PASS_VENDOR_DDX_RUNTIME_ACCELERATION; not installing"
fi

require_file() {
    local p="$1"
    [[ -e "$SRC_ROOT/$p" || -L "$SRC_ROOT/$p" ]] || err "missing from source root: $p"
}

require_file usr/lib/x86_64-linux-gnu/innogpu-fh2m/libEGL_inno.so.0.0.0
require_file usr/lib/x86_64-linux-gnu/innogpu-fh2m/libGLX_inno.so.0.0.0
require_file usr/lib/x86_64-linux-gnu/innogpu-fh2m/libgbm.so.1.0.0
require_file usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so
require_file usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so
require_file usr/lib/xorg/modules/drivers/innogpu_drv.so
require_file usr/share/glvnd/egl_vendor.d/00_inno.json
require_file usr/share/X11/xorg.conf.d/10-innogpu.conf

backup_path() {
    local p="$1"
    if [[ -e "$p" || -L "$p" ]]; then
        local b="$p.before-innogpu-deepin-hwgl-$STAMP"
        if [[ ! -e "$b" && ! -L "$b" ]]; then
            mv -f "$p" "$b"
            log "backup $p -> $b"
        fi
    fi
}

log "installing Deepin 202504 userspace libraries from $SRC_ROOT"
install -d \
    /usr/lib/x86_64-linux-gnu/innogpu-fh2m \
    /usr/lib/x86_64-linux-gnu/dri \
    /usr/lib/x86_64-linux-gnu/gbm \
    /usr/lib/xorg/modules/drivers \
    /usr/share/glvnd/egl_vendor.d \
    /etc/X11 \
    /etc/X11/xorg.conf.d \
    /etc/ld.so.conf.d

cp -a "$SRC_ROOT/usr/lib/x86_64-linux-gnu/innogpu-fh2m/." /usr/lib/x86_64-linux-gnu/innogpu-fh2m/
cp -a "$SRC_ROOT/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so" /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so
cp -a "$SRC_ROOT/usr/lib/x86_64-linux-gnu/dri/swrast_inno_dri.so" /usr/lib/x86_64-linux-gnu/dri/swrast_inno_dri.so 2>/dev/null || true
cp -a "$SRC_ROOT/usr/lib/x86_64-linux-gnu/dri/kms_swrast_inno_dri.so" /usr/lib/x86_64-linux-gnu/dri/kms_swrast_inno_dri.so 2>/dev/null || true
cp -a "$SRC_ROOT/usr/lib/x86_64-linux-gnu/dri/innogpu_drv_video.so" /usr/lib/x86_64-linux-gnu/dri/innogpu_drv_video.so 2>/dev/null || true
ln -sfn innogpu_dri.so /usr/lib/x86_64-linux-gnu/dri/inno_dri.so
cp -a "$SRC_ROOT/usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so" /usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so

backup_path /usr/lib/xorg/modules/drivers/innogpu_drv.so
cp -a "$SRC_ROOT/usr/lib/xorg/modules/drivers/innogpu_drv.so" /usr/lib/xorg/modules/drivers/innogpu_drv.so
cp -a "$SRC_ROOT/usr/share/glvnd/egl_vendor.d/00_inno.json" /usr/share/glvnd/egl_vendor.d/00_inno.json

backup_path /usr/share/X11/xorg.conf.d/10-innogpu.conf
backup_path /etc/X11/xorg.conf.d/10-innogpu.conf
cp -a "$SRC_ROOT/usr/share/X11/xorg.conf.d/10-innogpu.conf" /etc/X11/xorg.conf.d/10-innogpu.conf

backup_path /etc/X11/xorg.conf
cat > /etc/X11/xorg.conf <<'XORG'
Section "ServerFlags"
    Option "IgnoreABI" "true"
    Option "BlankTime" "0"
    Option "StandbyTime" "0"
    Option "SuspendTime" "0"
    Option "OffTime" "0"
EndSection

Section "Device"
    Identifier "innogpu"
    Driver "innogpu"
    BusID "PCI:2:0:0"
    Option "PrimaryGPU" "yes"
    Option "GlxVendorLibrary" "inno"
    Option "DRI" "3"
    Option "TearFree" "true"
EndSection

Section "Screen"
    Identifier "screen0"
    Device "innogpu"
EndSection
XORG

backup_path /etc/ld.so.conf.d/0-innogpu-hwgl.conf
cat > /etc/ld.so.conf.d/0-innogpu-hwgl.conf <<'CONF'
/usr/lib/x86_64-linux-gnu/innogpu-fh2m
CONF
ldconfig

if command -v innogpu-repair-dri-nodes >/dev/null 2>&1; then
    innogpu-repair-dri-nodes || true
elif [[ -x "$ROOT/scripts/repair-dri-nodes.sh" ]]; then
    "$ROOT/scripts/repair-dri-nodes.sh" || true
fi

log "Deepin desktop hardware-GL trial files installed."
log "Do not reboot yet. Validate with:"
log "  sudo scripts/test-current-xorg-hwgl-runtime.sh"
log "then from the local TTY start the desktop:"
log "  startx"
log "after the desktop is up:"
log "  scripts/check-desktop-hwgl.sh"
