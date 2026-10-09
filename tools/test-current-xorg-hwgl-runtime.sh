#!/bin/bash
# Runtime-test the currently installed Xorg hardware-GL configuration.
# This starts a temporary Xorg and probes real GLX clients without changing
# persistent configuration.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
WORK="${INNOGPU_CURRENT_HWGL_WORK:-/tmp/innogpu-current-hwgl-test}"
DISPLAY_NUM="${INNOGPU_TEST_DISPLAY:-:9}"
VT="${INNOGPU_TEST_VT:-8}"
TIMEOUT="${INNOGPU_TEST_TIMEOUT:-15s}"
XORG="${XORG:-/usr/lib/xorg/Xorg}"
REPORT_DIR="$ROOT/baselines/latest-current-xorg-hwgl-test"
LOG="$WORK/Xorg${DISPLAY_NUM#:}.log"
STDOUT="$WORK/xorg.stdout"
STDERR="$WORK/xorg.stderr"
XDPYINFO_OUT="$WORK/xdpyinfo.txt"
GLXINFO_OUT="$WORK/glxinfo.txt"

err() { echo "ERROR: $*" >&2; exit 1; }
log() { echo "[innogpu-current-hwgl] $*"; }

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    err "run as root: sudo $0"
fi

cat <<'RECOVERY'
Recovery if the temporary Xorg test misbehaves:
  sudo pkill -f 'Xorg :9'
  sudo chvt 1

This test starts a temporary Xorg :9 on vt8 and does not change persistent config.
RECOVERY

rm -rf "$WORK"
install -d "$WORK"

log "starting $XORG $DISPLAY_NUM vt$VT for up to $TIMEOUT using current /etc Xorg config"

set +e
timeout -k 2s "$TIMEOUT" "$XORG" "$DISPLAY_NUM" "vt$VT" \
    -config /etc/X11/xorg.conf \
    -logfile "$LOG" \
    -noreset \
    -retro \
    >"$STDOUT" 2>"$STDERR" &
xorg_pid=$!

socket_ready=0
for _ in $(seq 1 80); do
    if ! kill -0 "$xorg_pid" 2>/dev/null; then
        break
    fi
    if [[ -S "/tmp/.X11-unix/X${DISPLAY_NUM#:}" ]]; then
        socket_ready=1
        break
    fi
    sleep 0.1
done

if [[ "$socket_ready" == "1" ]]; then
    DISPLAY="$DISPLAY_NUM" XAUTHORITY= xdpyinfo -queryExtensions >"$XDPYINFO_OUT" 2>&1
    xdpyinfo_rc=$?

    DISPLAY="$DISPLAY_NUM" XAUTHORITY= glxinfo -B >"$GLXINFO_OUT" 2>&1
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
xorg_rc=$?
set -e

install -d "$REPORT_DIR"
cp -a "$LOG" "$REPORT_DIR/Xorg.log" 2>/dev/null || true
cp -a "$STDOUT" "$REPORT_DIR/xorg.stdout" 2>/dev/null || true
cp -a "$STDERR" "$REPORT_DIR/xorg.stderr" 2>/dev/null || true
cp -a "$XDPYINFO_OUT" "$REPORT_DIR/xdpyinfo.txt" 2>/dev/null || true
cp -a "$GLXINFO_OUT" "$REPORT_DIR/glxinfo.txt" 2>/dev/null || true
{
    echo "timestamp=$(date -Is)"
    echo "xorg_exit_code=$xorg_rc"
    echo "xdpyinfo_rc=$xdpyinfo_rc"
    echo "glxinfo_rc=$glxinfo_rc"
    echo "display=$DISPLAY_NUM"
    echo "vt=vt$VT"
} > "$REPORT_DIR/summary.txt"

echo "xorg_exit_code=$xorg_rc"
echo "xdpyinfo_rc=$xdpyinfo_rc"
echo "glxinfo_rc=$glxinfo_rc"
echo "report_dir=$REPORT_DIR"

echo
echo "===== Xorg markers ====="
grep -Ein 'innogpu|glamor|AIGLX|GLX|DRI2|DRI3|Present|Fantasy|Innosilicon|EE\)|WW\)|fatal|failed|segmentation|drmSetMaster' "$LOG" 2>/dev/null | tail -180 || true

echo
echo "===== xdpyinfo markers ====="
grep -E 'DRI2|DRI3|GLX|Present|RANDR|Composite|Damage|SYNC|error|failed' "$XDPYINFO_OUT" 2>/dev/null || true

echo
echo "===== glxinfo markers ====="
grep -E 'direct rendering|OpenGL vendor|OpenGL renderer|OpenGL version|Accelerated|Device|error|failed|BadValue|Segmentation' "$GLXINFO_OUT" 2>/dev/null || true

if [[ "$xdpyinfo_rc" == "0" ]] &&
   [[ "$glxinfo_rc" == "0" ]] &&
   grep -q 'DRI3' "$XDPYINFO_OUT" 2>/dev/null &&
   grep -Eiq 'OpenGL renderer string: Fantasy II-M|OpenGL vendor string: innosilicon|OpenGL vendor string: Innosilicon' "$GLXINFO_OUT" 2>/dev/null &&
   grep -Eiq 'Accelerated: yes|direct rendering: Yes' "$GLXINFO_OUT" 2>/dev/null; then
    echo "PASS_CURRENT_XORG_HWGL_RUNTIME" > "$REPORT_DIR/result.txt"
    echo "RESULT: PASS_CURRENT_XORG_HWGL_RUNTIME"
    exit 0
fi

if grep -Eiq 'Segmentation fault|Caught signal 11|Fatal server error' "$LOG" "$GLXINFO_OUT" 2>/dev/null; then
    echo "FAIL_CURRENT_XORG_HWGL_CRASH" > "$REPORT_DIR/result.txt"
    echo "RESULT: FAIL_CURRENT_XORG_HWGL_CRASH"
    exit 2
fi

echo "FAIL_CURRENT_XORG_HWGL_NO_ACCEL" > "$REPORT_DIR/result.txt"
echo "RESULT: FAIL_CURRENT_XORG_HWGL_NO_ACCEL"
exit 1
