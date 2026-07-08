#!/bin/bash
# Run a single command with the Deepin/Innosilicon surfaceless EGL stack.
# This avoids enabling vendor GL/GBM globally and does not touch /etc or /usr.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
SRC_ROOT="${INNOGPU_DEEPIN_ROOT:-$ROOT/third_party/innogpu-fh2m-deepin-202504/root}"
WORK="${INNOGPU_SURFACELESS_WORK:-/tmp/innogpu-surfaceless-egl}"

err() { printf '[innogpu-surfaceless] ERROR: %s\n' "$*" >&2; exit 1; }
log() { printf '[innogpu-surfaceless] %s\n' "$*"; }

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
vendor_egl_json="$(first_existing \
    "$SRC_ROOT/usr/share/glvnd/egl_vendor.d/00_inno.json" \
    /usr/share/glvnd/egl_vendor.d/00_inno.json.disabled \
    /usr/share/glvnd/egl_vendor.d/00_inno.json)" || err "00_inno.json not found"

rm -rf "$WORK"
install -d "$WORK/lib" "$WORK/dri" "$WORK/glvnd/egl_vendor.d"
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
cp -a "$vendor_egl_json" "$WORK/glvnd/egl_vendor.d/00_inno.json"

if [[ "$#" -eq 0 ]]; then
    set -- eglinfo -B -p surfaceless
fi

log "vendor_lib=$vendor_lib"
log "vendor_dri=$vendor_dri"
log "work=$WORK"
env \
    LD_LIBRARY_PATH="$WORK/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    LIBGL_DRIVERS_PATH="$WORK/dri" \
    __EGL_VENDOR_LIBRARY_FILENAMES="$WORK/glvnd/egl_vendor.d/00_inno.json" \
    EGL_PLATFORM=surfaceless \
    "$@"
