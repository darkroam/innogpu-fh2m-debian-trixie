#!/bin/bash
# Start a one-shot Xorg with the Deepin innogpu DDX from a temporary ModulePath.
# This does not modify /etc or /usr. It is intended to answer one question:
# can the vendor Xorg DDX initialize GLX/DRI on Debian 13 without crashing?

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
SRC_ROOT="${1:-${INNOGPU_DEEPIN_ROOT:-$ROOT/third_party/innogpu-fh2m-deepin-202504/root}}"
WORK="${INNOGPU_DDX_WORK:-/tmp/innogpu-ddx-isolated}"
DISPLAY_NUM="${INNOGPU_TEST_DISPLAY:-:9}"
VT="${INNOGPU_TEST_VT:-8}"
TIMEOUT="${INNOGPU_TEST_TIMEOUT:-15s}"
XORG="${XORG:-/usr/lib/xorg/Xorg}"
LOG="$WORK/Xorg${DISPLAY_NUM#:}.log"
STDOUT="$WORK/xorg.stdout"
STDERR="$WORK/xorg.stderr"
XDPYINFO_OUT="$WORK/xdpyinfo.txt"
GLXINFO_OUT="$WORK/glxinfo.txt"
SWITCH_VT="${INNOGPU_TEST_SWITCH_VT:-0}"
REPORT_DIR="$ROOT/baselines/latest-ddx-test"

err() { echo "ERROR: $*" >&2; exit 1; }
log() { echo "[innogpu-ddx-isolated] $*"; }

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    err "run from the local TTY as root: sudo $0"
fi

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

driver="$(first_existing \
    "$SRC_ROOT/usr/lib/xorg/modules/drivers/innogpu_drv.so" \
    /usr/lib/xorg/modules/drivers/innogpu_drv.so.disabled \
    /usr/lib/xorg/modules/drivers/innogpu_drv.so)" || err "innogpu_drv.so not found"
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
install -d "$WORK/lib" "$WORK/modules/drivers" "$WORK/dri" "$WORK/gbm" "$WORK/glvnd/egl_vendor.d" "$WORK/conf.d"
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
ln -s "$driver" "$WORK/modules/drivers/innogpu_drv.so"
ln -s "$vendor_dri" "$WORK/dri/innogpu_dri.so"
ln -s innogpu_dri.so "$WORK/dri/inno_dri.so"
ln -s "$vendor_gbm" "$WORK/gbm/innogpu_gbm.so"
cp -a "$vendor_egl_json" "$WORK/glvnd/egl_vendor.d/00_inno.json"

cat > "$WORK/xorg.conf" <<EOF_CONF
Section "ServerFlags"
    Option "IgnoreABI" "true"
    Option "AutoAddGPU" "false"
EndSection

Section "Files"
    ModulePath "$WORK/modules"
    ModulePath "/usr/lib/xorg/modules"
EndSection

Section "Module"
    Load "glx"
EndSection

Section "Device"
    Identifier "innogpu"
    Driver "innogpu"
    BusID "PCI:2:0:0"
    Option "PrimaryGPU" "yes"
    Option "GlxVendorLibrary" "inno"
    Option "DRI" "3"
EndSection

Section "Screen"
    Identifier "screen0"
    Device "innogpu"
EndSection
EOF_CONF

log "driver=$driver"
log "vendor_lib=$vendor_lib"
log "vendor_dri=$vendor_dri"
log "vendor_gbm=$vendor_gbm"
log "vendor_egl_json=$vendor_egl_json"
log "filtered_lib=$WORK/lib"
log "config=$WORK/xorg.conf"
log "log=$LOG"
if [[ "$SWITCH_VT" == "1" ]]; then
    vt_args=("vt$VT")
    vt_note="vt$VT with VT switch allowed"
else
    vt_args=("vt$VT" "-novtswitch" "-sharevts")
    vt_note="vt$VT without VT switch"
fi

log "starting $XORG $DISPLAY_NUM $vt_note for up to $TIMEOUT"

set +e
env \
    LD_LIBRARY_PATH="$WORK/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    LIBGL_DRIVERS_PATH="$WORK/dri" \
    GBM_DRIVERS_PATH="$WORK/dri" \
    GBM_BACKENDS_PATH="$WORK/gbm" \
    __EGL_VENDOR_LIBRARY_FILENAMES="$WORK/glvnd/egl_vendor.d/00_inno.json" \
    __GLX_VENDOR_LIBRARY_NAME=inno \
    timeout -k 2s "$TIMEOUT" "$XORG" "$DISPLAY_NUM" "${vt_args[@]}" \
        -config "$WORK/xorg.conf" \
        -logfile "$LOG" \
        -noreset \
        -retro \
        >"$STDOUT" 2>"$STDERR" &
xorg_pid=$!

client_probe_rc=99
for _ in $(seq 1 50); do
    if ! kill -0 "$xorg_pid" 2>/dev/null; then
        break
    fi
    if [[ -S "/tmp/.X11-unix/X${DISPLAY_NUM#:}" ]]; then
        client_probe_rc=0
        break
    fi
    sleep 0.1
done

if [[ "$client_probe_rc" == "0" ]]; then
    DISPLAY="$DISPLAY_NUM" XAUTHORITY= xdpyinfo -queryExtensions >"$XDPYINFO_OUT" 2>&1
    xdpyinfo_rc=$?

    env \
        DISPLAY="$DISPLAY_NUM" \
        XAUTHORITY= \
        LD_LIBRARY_PATH="$WORK/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
        LIBGL_DRIVERS_PATH="$WORK/dri" \
        GBM_DRIVERS_PATH="$WORK/dri" \
        GBM_BACKENDS_PATH="$WORK/gbm" \
        __EGL_VENDOR_LIBRARY_FILENAMES="$WORK/glvnd/egl_vendor.d/00_inno.json" \
        __GLX_VENDOR_LIBRARY_NAME=inno \
        glxinfo -B >"$GLXINFO_OUT" 2>&1
    glxinfo_rc=$?
else
    xdpyinfo_rc=99
    glxinfo_rc=99
    echo "X socket did not become ready" >"$XDPYINFO_OUT"
    echo "X socket did not become ready" >"$GLXINFO_OUT"
fi

if kill -0 "$xorg_pid" 2>/dev/null; then
    kill "$xorg_pid" 2>/dev/null || true
fi
wait "$xorg_pid" 2>/dev/null
rc=$?
set -e

install -d "$REPORT_DIR"
cp -a "$WORK/xorg.conf" "$REPORT_DIR/xorg.conf" 2>/dev/null || true
cp -a "$LOG" "$REPORT_DIR/Xorg.log" 2>/dev/null || true
cp -a "$STDOUT" "$REPORT_DIR/xorg.stdout" 2>/dev/null || true
cp -a "$STDERR" "$REPORT_DIR/xorg.stderr" 2>/dev/null || true
cp -a "$XDPYINFO_OUT" "$REPORT_DIR/xdpyinfo.txt" 2>/dev/null || true
cp -a "$GLXINFO_OUT" "$REPORT_DIR/glxinfo.txt" 2>/dev/null || true
{
    echo "timestamp=$(date -Is)"
    echo "xorg_exit_code=$rc"
    echo "xdpyinfo_rc=$xdpyinfo_rc"
    echo "glxinfo_rc=$glxinfo_rc"
    echo "driver=$driver"
    echo "vendor_lib=$vendor_lib"
    echo "vendor_dri=$vendor_dri"
    echo "vendor_gbm=$vendor_gbm"
    echo "vendor_egl_json=$vendor_egl_json"
    echo "display=$DISPLAY_NUM"
    echo "vt_note=$vt_note"
} > "$REPORT_DIR/summary.txt"

write_result() {
    local result="$1"
    printf '%s\n' "$result" > "$REPORT_DIR/result.txt"
    echo "RESULT: $result"
}

echo "xorg_exit_code=$rc"
echo "xdpyinfo_rc=$xdpyinfo_rc"
echo "glxinfo_rc=$glxinfo_rc"
echo "report_dir=$REPORT_DIR"

echo
echo "===== stdout ====="
sed -n '1,120p' "$STDOUT" 2>/dev/null || true

echo
echo "===== stderr ====="
sed -n '1,160p' "$STDERR" 2>/dev/null || true

echo
echo "===== log markers ====="
if [[ -f "$LOG" ]]; then
    grep -Ein 'innogpu|IgnoreABI|ABI|AIGLX|GLX|glamor|DRI|EE\)|WW\)|segmentation|fatal|no screens|failed|modeset|gbm|inno' "$LOG" | tail -220 || true
else
    echo "missing log: $LOG"
fi

echo
echo "===== xdpyinfo markers ====="
grep -E 'DRI2|DRI3|GLX|Present|RANDR|MIT-SHM|Composite|Damage|SYNC|unable|error|failed' "$XDPYINFO_OUT" 2>/dev/null || true

echo
echo "===== glxinfo markers ====="
grep -E 'OpenGL vendor|OpenGL renderer|OpenGL version|Accelerated|Device|direct rendering|error|failed|BadValue|Segmentation' "$GLXINFO_OUT" 2>/dev/null || true

if [[ -f "$LOG" ]] && grep -Eiq 'drm.*master|Device or resource busy|Permission denied|Operation not permitted|Cannot run in framebuffer mode|no screens found' "$LOG"; then
    write_result "INCONCLUSIVE_DRM_BUSY_OR_NO_SCREEN"
    exit 3
fi

if [[ -f "$LOG" ]] && grep -Eiq 'Segmentation fault|Caught signal 11' "$LOG"; then
    write_result "FAIL_CRASH"
    exit 2
fi

if [[ "$xdpyinfo_rc" == "0" ]] &&
   grep -q 'DRI3' "$XDPYINFO_OUT" 2>/dev/null &&
   grep -Eq 'OpenGL renderer.*Fantasy II-M|OpenGL vendor.*Innosilicon' "$GLXINFO_OUT" 2>/dev/null; then
    write_result "PASS_VENDOR_DDX_RUNTIME_ACCELERATION"
    exit 0
fi

if [[ -f "$LOG" ]] && grep -Eiq 'AIGLX: Loaded and initialized inno|glamor X acceleration enabled|DRI3 enabled|DRI2.*enabled|GLX: Initialized' "$LOG"; then
    write_result "PASS_VENDOR_DDX_INITIALIZED"
    exit 0
fi

write_result "FAIL_NO_VENDOR_SUCCESS_MARKER"
exit 1
