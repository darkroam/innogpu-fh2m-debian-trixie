#!/bin/bash
# Validate current innogpu Xorg candidate config without rebooting or touching the live :0 server.
# Starts a one-shot Xorg on another display/VT with probe-only style checks, captures the log,
# then exits. Success means Xorg loaded innogpu DDX far enough to initialize GLX/DRI without segfault.

set -euo pipefail

LOG=${1:-/tmp/innogpu-xorg-test.log}
DISPLAY_NUM=${INNOGPU_TEST_DISPLAY:-:9}
VT=${INNOGPU_TEST_VT:-8}
XORG=${XORG:-/usr/lib/xorg/Xorg}

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    echo "ERROR: run as root: sudo $0" >&2
    exit 1
fi

rm -f "$LOG"

# Prefer an isolated server. It may still fail if the live X server owns DRM master;
# that is a useful non-reboot signal, but a segfault is a hard failure.
timeout 15 "$XORG" "$DISPLAY_NUM" "vt$VT" -config /etc/X11/xorg.conf -logfile "$LOG" -noreset -retro >/tmp/innogpu-xorg-test.stdout 2>/tmp/innogpu-xorg-test.stderr &
pid=$!
sleep 5
if kill -0 "$pid" 2>/dev/null; then
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
fi

if [[ ! -f "$LOG" ]]; then
    echo "FAIL: no Xorg log produced at $LOG"
    sed -n '1,120p' /tmp/innogpu-xorg-test.stderr 2>/dev/null || true
    exit 1
fi

echo "--- innogpu Xorg test log: $LOG ---"
grep -Ei 'innogpu|IgnoreABI|ABI|AIGLX|GLX|glamor|DRI|EE\)|WW\)|segmentation|fatal|no screens|failed|modeset' "$LOG" | tail -160 || true

if grep -Eiq 'Segmentation fault|Caught signal 11|Fatal server error' "$LOG"; then
    echo "RESULT: FAIL (Xorg crashed)"
    exit 2
fi

if grep -Eiq 'AIGLX: Loaded and initialized inno|GLX: Initialized DRI2 GL provider|innogpu\(0\): innogpu glamor initialized|innogpu\(0\): glamor X acceleration enabled' "$LOG"; then
    echo "RESULT: PASS (vendor innogpu path initialized)"
    exit 0
fi

if grep -Eiq 'Device\(s\) detected, but none match|no screens found|open /dev/dri/card0: No such file|drm.*master|Permission denied|Operation not permitted' "$LOG"; then
    echo "RESULT: INCONCLUSIVE (one-shot Xorg could not acquire/display DRM while live session is running, but no crash observed)"
    exit 3
fi

echo "RESULT: FAIL (vendor innogpu success markers not found)"
exit 1
