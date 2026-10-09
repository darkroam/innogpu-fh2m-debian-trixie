#!/bin/bash
# Probe the Deepin/Innosilicon userspace acceleration stack without enabling it
# globally. This must not install files under /etc or /usr and is safe to run
# while the patched-17 llvmpipe Xorg session is the rollback point.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
SRC_ROOT="${1:-${INNOGPU_DEEPIN_ROOT:-$ROOT/third_party/innogpu-fh2m-deepin-202504/root}}"
WORK="${INNOGPU_HWGL_WORK:-/tmp/innogpu-hwgl-isolated}"
DISPLAY_NUM="${DISPLAY:-:0}"
TIMEOUT="${INNOGPU_HWGL_TIMEOUT:-8s}"

log() { printf '[innogpu-hwgl-isolated] %s\n' "$*"; }
warn() { printf '[innogpu-hwgl-isolated] WARN: %s\n' "$*" >&2; }
err() { printf '[innogpu-hwgl-isolated] ERROR: %s\n' "$*" >&2; exit 1; }

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

find_xauthority() {
    local auth
    auth="$(ls -t /tmp/serverauth.* 2>/dev/null | head -1 || true)"
    if [[ -n "$auth" ]]; then
        printf '%s\n' "$auth"
        return 0
    fi
    local x_user user_home
    x_user="${INNOGPU_X_USER:-${SUDO_USER:-${USER:-}}}"
    user_home="${INNOGPU_X_HOME:-$(getent passwd "$x_user" 2>/dev/null | cut -d: -f6)}"
    user_home="${user_home:-$HOME}"
    for auth in "$HOME/.Xauthority" "$user_home/.Xauthority"; do
        if [[ -e "$auth" ]]; then
            printf '%s\n' "$auth"
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
    return 0
}

require timeout
require glxinfo
[[ -d "$SRC_ROOT" ]] || err "source root not found: $SRC_ROOT"

auth="$(find_xauthority || true)"
[[ -n "$auth" ]] || err "Xauthority not found; start Xorg first"

vendor_lib="$(first_existing \
    "$SRC_ROOT/usr/lib/x86_64-linux-gnu/innogpu-fh2m" \
    /usr/lib/x86_64-linux-gnu/innogpu-fh2m)" || err "vendor library directory not found"
vendor_dri="$(first_existing \
    "$SRC_ROOT/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so" \
    /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so.disabled \
    /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so)" || err "innogpu_dri.so not found"
vendor_swrast="$(first_existing \
    "$SRC_ROOT/usr/lib/x86_64-linux-gnu/dri/swrast_inno_dri.so" \
    /usr/lib/x86_64-linux-gnu/dri/swrast_inno_dri.so.disabled \
    /usr/lib/x86_64-linux-gnu/dri/swrast_inno_dri.so \
    /usr/lib/x86_64-linux-gnu/dri/swrast_dri.so)" || true
mesa_swrast="$(first_existing \
    /usr/lib/x86_64-linux-gnu/dri/swrast_dri.so \
    /usr/lib/dri/swrast_dri.so)" || true
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
if [[ -n "${vendor_swrast:-}" ]]; then
    ln -s "$vendor_swrast" "$WORK/dri/swrast_inno_dri.so"
    ln -s swrast_inno_dri.so "$WORK/dri/kms_swrast_inno_dri.so"
fi
if [[ -n "${mesa_swrast:-}" ]]; then
    ln -s "$mesa_swrast" "$WORK/dri/swrast_dri.so"
fi
ln -s "$vendor_gbm" "$WORK/gbm/innogpu_gbm.so"
ln -s "$vendor_dri" "$WORK/gbm/innogpu_dri.so"
cp -a "$vendor_egl_json" "$WORK/glvnd/egl_vendor.d/00_inno.json"

log "DISPLAY=$DISPLAY_NUM"
log "XAUTHORITY=$auth"
log "vendor_lib=$vendor_lib"
log "vendor_dri=$vendor_dri"
log "vendor_gbm=$vendor_gbm"
log "work=$WORK"
log "filtered_lib=$WORK/lib"

common_env=(
    env
    DISPLAY="$DISPLAY_NUM"
    XAUTHORITY="$auth"
    LD_LIBRARY_PATH="$WORK/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    LIBGL_DRIVERS_PATH="$WORK/dri"
    GBM_DRIVERS_PATH="$WORK/dri"
    GBM_BACKENDS_PATH="$WORK/gbm"
)

run_probe "current safe renderer" \
    env DISPLAY="$DISPLAY_NUM" XAUTHORITY="$auth" \
    glxinfo -B

run_probe "mesa loader forced to innogpu_dri" \
    "${common_env[@]}" \
    MESA_LOADER_DRIVER_OVERRIDE=innogpu \
    glxinfo -B

run_probe "glvnd forced to inno GLX vendor" \
    "${common_env[@]}" \
    __GLX_VENDOR_LIBRARY_NAME=inno \
    glxinfo -B

if command -v eglinfo >/dev/null 2>&1; then
    run_probe "egl glvnd forced to inno vendor json" \
        "${common_env[@]}" \
        __EGL_VENDOR_LIBRARY_FILENAMES="$WORK/glvnd/egl_vendor.d/00_inno.json" \
        eglinfo -B
else
    warn "eglinfo is not installed; skipping EGL probe"
fi

printf '\n===== summary hints =====\n'
echo "Hardware GL is promising only if a probe reports the Innosilicon/Inno renderer and Accelerated: yes."
echo "If every probe stays llvmpipe or exits non-zero, keep patched-17 as soft baseline and inspect the failing loader path next."
