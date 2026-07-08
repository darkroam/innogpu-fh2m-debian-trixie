#!/bin/bash
# Local-only wrapper for the isolated Deepin innogpu Xorg DDX VT test.
# It does not install files and does not change persistent Xorg configuration.

set -u -o pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
LOG_DIR="$ROOT/baselines"
RUN_LOG="$LOG_DIR/ddx-vt-test-$(date +%Y%m%d-%H%M%S).log"

log() { printf '[innogpu-ddx-vt] %s\n' "$*"; }
err() { printf '[innogpu-ddx-vt] ERROR: %s\n' "$*" >&2; exit 1; }

[[ -d "$ROOT" ]] || err "project root not found: $ROOT"
[[ -x "$ROOT/scripts/test-isolated-deepin-xorg-ddx.sh" ]] || err "missing test script"
mkdir -p "$LOG_DIR"

if [[ -z "${INNOGPU_DEEPIN_ROOT:-}" && -x "$ROOT/scripts/prepare-deepin-userspace-root.sh" ]]; then
    INNOGPU_DEEPIN_ROOT="$("$ROOT/scripts/prepare-deepin-userspace-root.sh")"
    export INNOGPU_DEEPIN_ROOT
fi

cat <<'EOF'
Recovery if the temporary Xorg test misbehaves:
  sudo pkill -f 'Xorg :9'
  sudo chvt 1

This test starts a temporary Xorg :9 on vt8 with the Deepin innogpu DDX.
It does not modify /etc, /usr, module autoload, initramfs, or patched-8/patched-17 packages.
EOF

if [[ -n "${SSH_TTY:-}" || -n "${SSH_CONNECTION:-}" ]]; then
    log "WARN: SSH session detected. Prefer running this from the local tty/login session."
fi

log "refreshing sudo credentials"
sudo -v || exit $?

log "running VT-switch DDX test; full output will be saved to $RUN_LOG"
set +e
sudo env INNOGPU_TEST_SWITCH_VT=1 INNOGPU_DEEPIN_ROOT="${INNOGPU_DEEPIN_ROOT:-}" "$ROOT/scripts/test-isolated-deepin-xorg-ddx.sh" 2>&1 | tee "$RUN_LOG"
rc=${PIPESTATUS[0]}
set -e

archive="$LOG_DIR/ddx-test-$(date +%Y%m%d-%H%M%S)"
if [[ -d "$LOG_DIR/latest-ddx-test" ]]; then
    mkdir -p "$archive"
    cp -a "$LOG_DIR/latest-ddx-test/." "$archive/" 2>/dev/null || true
    log "archived latest-ddx-test to $archive"
fi

echo
echo "===== summary ====="
echo "test_exit_code=$rc"
echo "run_log=$RUN_LOG"
if [[ -f "$LOG_DIR/latest-ddx-test/summary.txt" ]]; then
    sed -n '1,80p' "$LOG_DIR/latest-ddx-test/summary.txt"
fi

echo
echo "===== key markers ====="
grep -Ehi 'RESULT:|glamor X acceleration|DRI3|GLX:|AIGLX|OpenGL vendor|OpenGL renderer|drmSetMaster|Device or resource busy|Segmentation|Fatal server error' \
    "$RUN_LOG" \
    "$LOG_DIR/latest-ddx-test/Xorg.log" \
    "$LOG_DIR/latest-ddx-test/glxinfo.txt" \
    "$LOG_DIR/latest-ddx-test/xdpyinfo.txt" 2>/dev/null | tail -120 || true

exit "$rc"
