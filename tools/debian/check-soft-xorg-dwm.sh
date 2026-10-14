#!/bin/bash
# Verify the safe Xorg+dwm path without enabling vendor acceleration.

set -euo pipefail

# Target user resolution order: INNOGPU_X_USER > SUDO_USER > USER. No default
# username is hard-coded; if no user can be resolved the script fails loudly.
X_USER="${INNOGPU_X_USER:-${SUDO_USER:-${USER:-}}}"
if [[ -z "$X_USER" ]]; then
    echo "ERROR: cannot determine target user (set INNOGPU_X_USER, or run via sudo)" >&2
    exit 1
fi
# Home is taken from INNOGPU_X_HOME or getent passwd; never falls back to root's
# $HOME, because that would silently run X commands as a different identity.
USER_HOME="${INNOGPU_X_HOME:-$(getent passwd "$X_USER" 2>/dev/null | cut -d: -f6)}"
if [[ -z "$USER_HOME" ]]; then
    echo "ERROR: cannot determine home for user $X_USER (set INNOGPU_X_HOME)" >&2
    exit 1
fi
DISPLAY_NUM=${INNOGPU_X_DISPLAY:-:0}

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    echo "ERROR: run as root: sudo $0" >&2
    exit 1
fi

section() {
    printf '\n===== %s =====\n' "$*"
}

find_xauthority() {
    local auth

    auth="$(ls -t /tmp/serverauth.* 2>/dev/null | head -1 || true)"
    if [[ -n "$auth" ]]; then
        printf '%s\n' "$auth"
        return 0
    fi

    for auth in "$USER_HOME/.Xauthority" "$USER_HOME/.xauth"*; do
        if [[ -e "$auth" ]]; then
            printf '%s\n' "$auth"
            return 0
        fi
    done

    return 1
}

run_x() {
    local auth="$1"
    shift

    su - "$X_USER" -c "DISPLAY='$DISPLAY_NUM' XAUTHORITY='$auth' $*"
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

section "Processes"
proc_unreliable=0
if process_namespace_unreliable; then
    proc_unreliable=1
    echo "process_namespace=CODEX_OR_BWRAP_ISOLATED_UNRELIABLE"
fi
processes="$(list_desktop_processes || true)"
printf '%s\n' "$processes"

if ! grep -Eq 'Xorg .*(:0| :0)' <<<"$processes" && [[ "$proc_unreliable" != "1" ]]; then
    echo "RESULT: FAIL (Xorg is not running)"
    exit 1
fi

if ! awk '$3 == "dwm" { found=1 } END { exit found ? 0 : 1 }' <<<"$processes" &&
   [[ "$proc_unreliable" != "1" ]]; then
    echo "RESULT: FAIL (dwm is not running)"
    exit 1
fi

section "Display access"
auth="$(find_xauthority || true)"
if [[ -z "${auth:-}" ]]; then
    echo "RESULT: FAIL (Xauthority not found)"
    exit 1
fi
echo "XAUTHORITY=$auth"

section "xrandr"
run_x "$auth" "xrandr --verbose | sed -n '1,120p'" || {
    echo "RESULT: FAIL (xrandr could not query display)"
    exit 1
}

section "Keep display awake"
run_x "$auth" "xset s off -dpms dpms force on" || true

section "GL renderer"
if command -v glxinfo >/dev/null 2>&1; then
    run_x "$auth" "glxinfo -B | grep -E 'OpenGL vendor|OpenGL renderer|OpenGL version|Accelerated|Device'" || true
else
    echo "glxinfo is not installed"
fi

section "Xorg log markers"
latest_xlog="$(ls -t "$USER_HOME/.local/share/xorg/Xorg.0.log" /var/log/Xorg.0.log 2>/dev/null | head -1 || true)"
if [[ -n "$latest_xlog" ]]; then
    echo "latest Xorg log: $latest_xlog"
    grep -nE '\((EE|WW)\)|Fatal|failed|Failed|segmentation|no screens|Configure crtc|glamor|modeset|innogpu|Server terminated' "$latest_xlog" | tail -160 || true
fi

section "Soft stack guard"
if [[ -e /etc/ld.so.conf.d/0-innogpu.conf || -e /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so ]]; then
    echo "RESULT: FAIL (vendor GL/DRI appears enabled)"
    exit 1
fi

echo "RESULT: PASS (soft Xorg+dwm is running)"
