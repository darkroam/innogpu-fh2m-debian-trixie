#!/bin/bash
# Enable the full vendor GBM/EGL/GLX path for hardware-GL testing.
# This is intentionally separate from the safe userspace installer because the
# Deepin 202504 GBM backend has crashed on Debian Trixie in prior tests.

set -euo pipefail

log() { echo "[innogpu-experimental-hwgl] $*"; }
err() { echo "ERROR: $*" >&2; exit 1; }

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    err "run as root: sudo $0 [third_party/innogpu-fh2m-deepin-202504/root]"
fi

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
SRC_ROOT="${1:-${INNOGPU_DEEPIN_ROOT:-$ROOT/third_party/innogpu-fh2m-deepin-202504/root}}"
[[ -d "$SRC_ROOT" ]] || err "source root not found: $SRC_ROOT"

require_file() {
    local p="$1"
    [[ -e "$SRC_ROOT/$p" || -L "$SRC_ROOT/$p" ]] || err "missing from source root: $p"
}

require_file usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so
require_file usr/lib/x86_64-linux-gnu/innogpu-fh2m/libEGL_inno.so.0.0.0
require_file usr/lib/x86_64-linux-gnu/innogpu-fh2m/libGLX_inno.so.0.0.0
require_file usr/share/glvnd/egl_vendor.d/00_inno.json
require_file usr/share/X11/xorg.conf.d/10-innogpu.conf

STAMP="$(date +%Y%m%d-%H%M%S)"
backup_path() {
    local p="$1"
    if [[ -e "$p" || -L "$p" ]]; then
        mv -f "$p" "$p.before-innogpu-hwgl-$STAMP"
        log "backup $p -> $p.before-innogpu-hwgl-$STAMP"
    fi
}

if command -v innogpu-install-kylin-userspace >/dev/null 2>&1; then
    innogpu-install-kylin-userspace "$SRC_ROOT"
else
    "$(dirname "$0")/install-kylin-userspace.sh" "$SRC_ROOT"
fi

if command -v innogpu-repair-dri-nodes >/dev/null 2>&1; then
    innogpu-repair-dri-nodes
else
    "$(dirname "$0")/repair-dri-nodes.sh"
fi

log "installing vendor GBM backend and GLVND EGL vendor file"
install -d /usr/lib/x86_64-linux-gnu/gbm /usr/share/glvnd/egl_vendor.d /usr/share/X11/xorg.conf.d
backup_path /usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so
cp -a "$SRC_ROOT/usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so" /usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so
cp -a "$SRC_ROOT/usr/share/glvnd/egl_vendor.d/00_inno.json" /usr/share/glvnd/egl_vendor.d/00_inno.json
cp -a "$SRC_ROOT/usr/share/X11/xorg.conf.d/10-innogpu.conf" /usr/share/X11/xorg.conf.d/10-innogpu.conf

log "adding vendor library directory to a dedicated ld.so.conf.d entry"
cat > /etc/ld.so.conf.d/0-innogpu-hwgl.conf <<'CONF'
/usr/lib/x86_64-linux-gnu/innogpu-fh2m
CONF
ldconfig

log "experimental hardware-GL path enabled. Validate before reboot:"
log "  sudo innogpu-test-xorg-once"
log "Recover if Xorg/EGL crashes:"
log "  sudo innogpu-disable-incompatible-userspace"
