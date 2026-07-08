#!/bin/bash
# Run a command with the Deepin/Innosilicon GBM/EGL stack in isolation.
# This avoids enabling vendor GL/GBM globally and does not touch /etc or /usr.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
SRC_ROOT="${INNOGPU_DEEPIN_ROOT:-$ROOT/third_party/innogpu-fh2m-deepin-202504/root}"
WORK="${INNOGPU_GBM_WORK:-/tmp/innogpu-egl-gbm-isolated}"

err() { printf '[innogpu-gbm] ERROR: %s\n' "$*" >&2; exit 1; }
log() { printf '[innogpu-gbm] %s\n' "$*"; }

first_existing() {
    local p
    for p in "$@"; do
        if [[ -e "$p" || -L "$p" ]]; then
            printf '%s\n' "$p"
            return 0
        fi
    done
    return 1
}

[[ -d "$SRC_ROOT" ]] || err "source root not found: $SRC_ROOT"

vendor_lib="$(first_existing \
    "$SRC_ROOT/usr/lib/x86_64-linux-gnu/innogpu-fh2m" \
    /usr/lib/x86_64-linux-gnu/innogpu-fh2m)" || err "vendor library directory not found"
vendor_dri="$(first_existing \
    "$SRC_ROOT/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so" \
    /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so.disabled \
    /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so)" || err "innogpu_dri.so not found"
vendor_gbm="$(first_existing \
    "$SRC_ROOT/usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so" \
    /usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so.disabled \
    /usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so)" || err "innogpu_gbm.so not found"
vendor_egl_json="$(first_existing \
    "$SRC_ROOT/usr/share/glvnd/egl_vendor.d/00_inno.json" \
    /usr/share/glvnd/egl_vendor.d/00_inno.json.disabled \
    /usr/share/glvnd/egl_vendor.d/00_inno.json)" || err "00_inno.json not found"

rm -rf "$WORK"
install -d "$WORK/lib" "$WORK/dri" "$WORK/gbm" "$WORK/glvnd/egl_vendor.d"
for lib in "$vendor_lib"/*; do
    base="$(basename "$lib")"
    case "$base" in
        libEGL.so*|libGLX.so*|libGLdispatch.so*|libGL.so*|libGLESv1_CM.so*|libGLESv2.so*)
            continue
            ;;
    esac
    ln -s "$lib" "$WORK/lib/$base"
done
if [[ -e "$WORK/lib/libglapi_inno.so.0" && ! -e "$WORK/lib/libglapi.so.0" ]]; then
    ln -s libglapi_inno.so.0 "$WORK/lib/libglapi.so.0"
fi
ln -s "$vendor_dri" "$WORK/dri/innogpu_dri.so"
ln -s innogpu_dri.so "$WORK/dri/inno_dri.so"
ln -s "$vendor_gbm" "$WORK/gbm/innogpu_gbm.so"
cp -a "$vendor_egl_json" "$WORK/glvnd/egl_vendor.d/00_inno.json"

if [[ "$#" -eq 0 ]]; then
    [[ -x /tmp/probe-egl-gbm ]] || err "compile first: gcc -Wall -Wextra -O2 tools/probe-egl-gbm.c -ldl -o /tmp/probe-egl-gbm"
    set -- /tmp/probe-egl-gbm /dev/dri/card0 /dev/dri/renderD128
fi

log "vendor_lib=$vendor_lib"
log "vendor_dri=$vendor_dri"
log "vendor_gbm=$vendor_gbm"
log "work=$WORK"
env \
    LD_LIBRARY_PATH="$WORK/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    LIBGL_DRIVERS_PATH="$WORK/dri" \
    GBM_DRIVERS_PATH="$WORK/dri" \
    GBM_BACKENDS_PATH="$WORK/gbm" \
    __EGL_VENDOR_LIBRARY_FILENAMES="$WORK/glvnd/egl_vendor.d/00_inno.json" \
    "$@"
