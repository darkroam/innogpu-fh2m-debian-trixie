#!/bin/bash
# Probe Deepin/Innosilicon EGL on GBM/surfaceless without Xorg DDX and without
# changing system configuration. This targets single-application acceleration
# via /dev/dri/renderD128 as a lower-risk path than replacing the live Xorg DDX.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
SRC_ROOT="${1:-${INNOGPU_DEEPIN_ROOT:-$ROOT/third_party/innogpu-fh2m-deepin-202504/root}}"
WORK="${INNOGPU_EGL_WORK:-/tmp/innogpu-egl-gbm-isolated}"
TIMEOUT="${INNOGPU_EGL_TIMEOUT:-8s}"

log() { printf '[innogpu-egl-gbm] %s\n' "$*"; }
err() { printf '[innogpu-egl-gbm] ERROR: %s\n' "$*" >&2; exit 1; }

require() {
    command -v "$1" >/dev/null 2>&1 || err "missing command: $1"
}

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

run_probe() {
    local name="$1"
    shift

    printf '\n===== %s =====\n' "$name"
    set +e
    timeout -k 2s "$TIMEOUT" "$@"
    local rc=$?
    set -e
    echo "exit_code=$rc"
}

require timeout
require eglinfo
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
mesa_swrast="$(first_existing \
    /usr/lib/x86_64-linux-gnu/dri/swrast_dri.so \
    /usr/lib/dri/swrast_dri.so)" || true

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
ln -s "$vendor_dri" "$WORK/gbm/innogpu_dri.so"
if [[ -n "${mesa_swrast:-}" ]]; then
    ln -s "$mesa_swrast" "$WORK/dri/swrast_dri.so"
fi
cp -a "$vendor_egl_json" "$WORK/glvnd/egl_vendor.d/00_inno.json"

log "vendor_lib=$vendor_lib"
log "vendor_dri=$vendor_dri"
log "vendor_gbm=$vendor_gbm"
log "work=$WORK"
log "filtered_lib=$WORK/lib"
ls -l /dev/dri /dev/dri/by-path /dev/fb0 2>/dev/null || true

common_env=(
    env
    LD_LIBRARY_PATH="$WORK/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    LIBGL_DRIVERS_PATH="$WORK/dri"
    GBM_DRIVERS_PATH="$WORK/dri"
    GBM_BACKENDS_PATH="$WORK/gbm"
    __EGL_VENDOR_LIBRARY_FILENAMES="$WORK/glvnd/egl_vendor.d/00_inno.json"
)

run_probe "eglinfo gbm with Inno EGL vendor" \
    "${common_env[@]}" \
    eglinfo -B -p gbm

run_probe "eglinfo surfaceless with Inno EGL vendor" \
    "${common_env[@]}" \
    eglinfo -B -p surfaceless

if [[ -x /tmp/probe-egl-gbm ]]; then
    run_probe "minimal GBM GLES2 render probe" \
        "${common_env[@]}" \
        /tmp/probe-egl-gbm /dev/dri/card0 /dev/dri/renderD128
else
    echo
    echo "===== minimal GBM GLES2 render probe ====="
    echo "SKIPPED: compile with:"
    echo "  gcc -Wall -Wextra -O2 tools/probe-egl-gbm.c -ldl -o /tmp/probe-egl-gbm"
fi

run_probe "eglinfo gbm debug with Inno EGL vendor" \
    "${common_env[@]}" \
    EGL_LOG_LEVEL=debug \
    eglinfo -B -p gbm

printf '\n===== summary hints =====\n'
echo "Use the minimal GBM GLES2 render probe as the GBM pass/fail gate."
echo "eglinfo -p gbm is currently unreliable with the Deepin 202504 stack and may crash even when the minimal GBM probe passes."
echo "Desktop Xorg acceleration still depends on the isolated DDX test."
