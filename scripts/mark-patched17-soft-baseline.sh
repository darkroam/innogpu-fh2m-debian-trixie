#!/bin/bash
# Mark patched-17 as the new rollback/baseline point only after the safe
# software-display stack is proven on the real system.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
X_USER="${INNOGPU_X_USER:-${SUDO_USER:-${USER:-}}}"
USER_HOME="${INNOGPU_X_HOME:-$(getent passwd "$X_USER" 2>/dev/null | cut -d: -f6)}"
USER_HOME="${USER_HOME:-$HOME}"
STAMP_DIR="$ROOT/baselines"
STAMP="$STAMP_DIR/patched17-soft-baseline.txt"
REPORT="$STAMP_DIR/patched17-soft-baseline-report.txt"

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

process_namespace_unreliable() {
    local init_cmd
    init_cmd="$(tr '\0' ' ' </proc/1/cmdline 2>/dev/null || true)"
    grep -Eq 'bwrap|codex-linux-sandbox' <<<"$init_cmd"
}

list_desktop_processes() {
    ps -eo pid=,tty=,comm=,args= 2>/dev/null |
        awk '$3 ~ /^(Xorg|startx|xinit|dwm|dwmblocks)$/ || $0 ~ /\/Xorg[[:space:]]/ { print }'
}

find_xauthority() {
    local auth
    auth="$(ls -t /tmp/serverauth.* 2>/dev/null | head -1 || true)"
    if [[ -n "$auth" ]]; then
        printf '%s\n' "$auth"
        return 0
    fi
    if [[ -e "$USER_HOME/.Xauthority" ]]; then
        printf '%s\n' "$USER_HOME/.Xauthority"
        return 0
    fi
    return 1
}

cd "$ROOT"

installed="$(dpkg-query -W -f='${Version}' innogpu-fh2m-trixie 2>/dev/null || true)"
[[ "$installed" == "3.3.3.42-patched-17" ]] || fail "patched-17 is not installed (current: ${installed:-unknown})"

status="$(cat /proc/driver/innogpu/gpu00/status 2>/dev/null || true)"
grep -q 'Driver Status:[[:space:]]*OK' <<<"$status" || fail "Driver Status is not OK"
grep -q 'Firmware Status:[[:space:]]*OK' <<<"$status" || fail "Firmware Status is not OK"

if ! process_namespace_unreliable; then
    [[ -e /dev/dri/card0 ]] || fail "missing /dev/dri/card0"
    [[ -e /dev/dri/renderD128 ]] || fail "missing /dev/dri/renderD128"
    [[ -e /dev/fb0 ]] || fail "missing /dev/fb0"

    processes="$(list_desktop_processes || true)"
    grep -Eq 'Xorg .*(:0| :0)' <<<"$processes" || fail "Xorg is not running"
    awk '$3 == "dwm" { found=1 } END { exit found ? 0 : 1 }' <<<"$processes" || fail "dwm is not running"
else
    echo "WARNING: process/device namespace is isolated; relying on X display and GL checks." >&2
fi

if [[ -e /etc/ld.so.conf.d/0-innogpu.conf || -e /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so ]]; then
    fail "vendor GL/DRI is enabled; expected safe llvmpipe baseline"
fi

auth="$(find_xauthority || true)"
[[ -n "$auth" ]] || fail "Xauthority not found"

if [[ "$(id -un)" == "ok" ]]; then
    glx="$(DISPLAY=:0 XAUTHORITY="$auth" glxinfo -B 2>/dev/null || true)"
else
    glx="$(su - ok -c "DISPLAY=:0 XAUTHORITY='$auth' glxinfo -B" 2>/dev/null || true)"
fi
grep -qi 'llvmpipe' <<<"$glx" || fail "GL renderer is not llvmpipe or glxinfo failed"

mkdir -p "$STAMP_DIR"
{
    echo "patched17_soft_baseline=OK"
    echo "timestamp=$(date -Is)"
    echo "kernel=$(uname -r)"
    echo "package=$installed"
    echo
    echo "Recovery to patched-17 soft baseline:"
    echo "  cd $ROOT"
    echo "  sudo dpkg -i innogpu-fh2m-trixie_3.3.3.42-patched-17.deb"
    echo "  sudo scripts/disable-incompatible-userspace.sh"
    echo "  printf '%s\\n' innogpu | sudo tee /etc/modules-load.d/innogpu.conf"
    echo "  sudo depmod -a \"\$(uname -r)\""
    echo "  sudo update-initramfs -u -k \"\$(uname -r)\""
    echo "  sudo reboot"
    echo
    echo "Fallback to patched-8:"
    echo "  cd $ROOT"
    echo "  sudo dpkg -i innogpu-fh2m-trixie_3.3.3.42-patched-8.deb"
    echo "  sudo scripts/disable-incompatible-userspace.sh"
    echo "  printf '%s\\n' innogpu | sudo tee /etc/modules-load.d/innogpu.conf"
    echo "  sudo depmod -a \"\$(uname -r)\""
    echo "  sudo update-initramfs -u -k \"\$(uname -r)\""
    echo "  sudo reboot"
} | tee "$STAMP"

{
    echo "===== patched17 baseline report ====="
    cat "$STAMP"
    echo
    echo "===== driver status ====="
    cat /proc/driver/innogpu/gpu00/status 2>/dev/null || true
    echo
    echo "===== nodes ====="
    ls -l /dev/dri /dev/dri/by-path /dev/fb0 2>/dev/null || true
    echo
    echo "===== processes ====="
    list_desktop_processes || true
    echo
    echo "===== glxinfo ====="
    echo "$glx"
} > "$REPORT"

echo
echo "Marked patched-17 soft baseline:"
echo "  $STAMP"
echo "Report:"
echo "  $REPORT"
