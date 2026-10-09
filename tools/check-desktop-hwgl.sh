#!/bin/bash
# Verify whether the currently running desktop Xorg session is using the
# Innosilicon/Fantasy hardware GL path. This script is read-only.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
DISPLAY_NUM="${INNOGPU_X_DISPLAY:-:0}"
X_USER="${INNOGPU_X_USER:-${SUDO_USER:-${USER:-}}}"
USER_HOME="${INNOGPU_X_HOME:-$(getent passwd "$X_USER" 2>/dev/null | cut -d: -f6)}"
USER_HOME="${USER_HOME:-$HOME}"
REPORT_DIR="$ROOT/baselines"
REPORT="$REPORT_DIR/desktop-hwgl-$(date +%Y%m%d-%H%M%S).txt"

section() {
    printf '\n===== %s =====\n' "$*"
}

find_xauthority() {
    local auth first_existing=""
    local candidates=()

    for auth in "${XAUTHORITY:-}" "$HOME/.Xauthority" "$USER_HOME/.Xauthority"; do
        if [[ -n "$auth" && -e "$auth" ]]; then
            candidates+=("$auth")
        fi
    done
    while IFS= read -r auth; do
        [[ -n "$auth" ]] && candidates+=("$auth")
    done < <(ls -t /tmp/serverauth.* 2>/dev/null || true)

    for auth in "${candidates[@]}"; do
        [[ -n "$first_existing" ]] || first_existing="$auth"
        if command -v xdpyinfo >/dev/null 2>&1 &&
           DISPLAY="$DISPLAY_NUM" XAUTHORITY="$auth" xdpyinfo >/dev/null 2>&1; then
            printf '%s\n' "$auth"
            return 0
        fi
    done

    if [[ -n "$first_existing" ]]; then
        printf '%s\n' "$first_existing"
        return 0
    fi

    return 1
}

run_x() {
    local auth="$1"
    shift
    DISPLAY="$DISPLAY_NUM" XAUTHORITY="$auth" "$@"
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

mkdir -p "$REPORT_DIR"

{
    section "Package And Driver"
    dpkg-query -W -f='package=${Package}\nversion=${Version}\nstatus=${db:Status-Abbrev}\n' innogpu-fh2m-trixie 2>/dev/null || true
    cat /proc/driver/innogpu/gpu00/status 2>/dev/null || true

    section "Nodes"
    ls -l /dev/dri /dev/dri/by-path /dev/fb0 2>/dev/null || true

    section "Xorg Processes"
    proc_unreliable=0
    if process_namespace_unreliable; then
        proc_unreliable=1
        echo "process_namespace=CODEX_OR_BWRAP_ISOLATED_UNRELIABLE"
    fi
    xorg_processes="$(list_desktop_processes || true)"
    printf '%s\n' "$xorg_processes"
    if grep -Eq 'Xorg .*(:0| :0)' <<<"$xorg_processes"; then
        desktop_xorg_active=1
    elif [[ "$proc_unreliable" == "1" ]]; then
        desktop_xorg_active=unknown
    else
        desktop_xorg_active=0
    fi

    auth="$(find_xauthority || true)"
    echo "DISPLAY=$DISPLAY_NUM"
    echo "XAUTHORITY=${auth:-missing}"
    if [[ -n "${auth:-}" ]] && command -v xdpyinfo >/dev/null 2>&1 &&
       run_x "$auth" xdpyinfo >/dev/null 2>&1; then
        desktop_xorg_active=1
        echo "x_display_query=OK"
    else
        echo "x_display_query=FAILED_OR_NOT_TESTED"
    fi
    echo "desktop_xorg_active=$desktop_xorg_active"

    section "Desktop GL"
    if [[ -n "${auth:-}" ]] && command -v glxinfo >/dev/null 2>&1; then
        run_x "$auth" glxinfo -B 2>&1 || true
    else
        echo "SKIPPED: missing XAUTHORITY or missing glxinfo"
    fi

    section "X Extensions"
    if [[ -n "${auth:-}" ]] && command -v xdpyinfo >/dev/null 2>&1; then
        run_x "$auth" xdpyinfo -queryExtensions 2>&1 | grep -E 'DRI2|DRI3|GLX|Present|RANDR|Composite|Damage|SYNC|error|failed' || true
    else
        echo "SKIPPED: missing XAUTHORITY or missing xdpyinfo"
    fi

    section "Xrandr"
    if [[ -n "${auth:-}" ]] && command -v xrandr >/dev/null 2>&1; then
        run_x "$auth" xrandr --listproviders 2>&1 || true
        run_x "$auth" xrandr --current 2>&1 || true
    else
        echo "SKIPPED: missing XAUTHORITY or missing xrandr"
    fi

    section "Recent Xorg Log Markers"
    latest_log="$(ls -t "$USER_HOME"/.local/share/xorg/Xorg.*.log /var/log/Xorg*.log 2>/dev/null | head -1 || true)"
    echo "latest_xorg_log=${latest_log:-missing}"
    if [[ -n "${latest_log:-}" ]]; then
        grep -Ein 'LoadModule: "(innogpu|modesetting)"|\((EE|WW)\)|AIGLX|GLX: Initialized|DRI2|DRI3|Present|llvmpipe|swrast|Fantasy|Innosilicon|innogpu\(0\): \[DRI2\]|modeset\(0\): Refusing|fatal|failed|segmentation' "$latest_log" | tail -120 || true
    fi
} | tee "$REPORT"

echo
echo "===== Result ====="
if grep -Eq 'OpenGL vendor string: Innosilicon|OpenGL renderer string: Fantasy II-M' "$REPORT" &&
   grep -q 'DRI3' "$REPORT" &&
   grep -Eiq 'innogpu.*glamor X acceleration enabled|glamor X acceleration enabled on Fantasy II-M|AIGLX: Loaded and initialized inno|GLX: Initialized' "$REPORT"; then
    echo "RESULT: PASS_DESKTOP_HWGL"
    printf '%s\n' "PASS_DESKTOP_HWGL" > "$REPORT_DIR/latest-desktop-hwgl-result.txt"
    exit 0
fi

if grep -q 'desktop_xorg_active=0' "$REPORT"; then
    echo "RESULT: INCONCLUSIVE_NO_DESKTOP_SESSION"
    printf '%s\n' "INCONCLUSIVE_NO_DESKTOP_SESSION" > "$REPORT_DIR/latest-desktop-hwgl-result.txt"
    exit 2
fi

if grep -q 'desktop_xorg_active=unknown' "$REPORT"; then
    echo "RESULT: INCONCLUSIVE_PROCESS_NAMESPACE_UNRELIABLE"
    echo "latest_desktop_hardware_gl_result=NOT_UPDATED"
    exit 2
fi

if grep -Eq 'OpenGL renderer string: llvmpipe|OpenGL vendor string: Mesa' "$REPORT"; then
    echo "RESULT: FAIL_DESKTOP_STILL_SOFTWARE"
    printf '%s\n' "FAIL_DESKTOP_STILL_SOFTWARE" > "$REPORT_DIR/latest-desktop-hwgl-result.txt"
    exit 1
fi

echo "RESULT: INCONCLUSIVE_DESKTOP_HWGL"
printf '%s\n' "INCONCLUSIVE_DESKTOP_HWGL" > "$REPORT_DIR/latest-desktop-hwgl-result.txt"
exit 2
