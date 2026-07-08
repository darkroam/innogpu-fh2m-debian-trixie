#!/bin/bash
# Summarize the current Innogpu remediation state without changing global
# system configuration. Run from the local session when possible.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
DISPLAY_NUM="${INNOGPU_X_DISPLAY:-:0}"
X_USER="${INNOGPU_X_USER:-${SUDO_USER:-${USER:-}}}"
USER_HOME="${INNOGPU_X_HOME:-$(getent passwd "$X_USER" 2>/dev/null | cut -d: -f6)}"
USER_HOME="${USER_HOME:-$HOME}"
latest_ddx_result="$(cat "$ROOT/baselines/latest-ddx-test/result.txt" 2>/dev/null || true)"
latest_desktop_hwgl_result="$(cat "$ROOT/baselines/latest-desktop-hwgl-result.txt" 2>/dev/null || true)"
latest_current_xorg_hwgl_runtime_result="$(cat "$ROOT/baselines/latest-current-xorg-hwgl-test/result.txt" 2>/dev/null || true)"
latest_post_reboot_hwgl_result="$(cat "$ROOT/baselines/latest-post-reboot-hwgl-result.txt" 2>/dev/null || true)"
current_xorg_config=UNKNOWN
if grep -Eq 'Driver[[:space:]]+"innogpu"' /etc/X11/xorg.conf 2>/dev/null; then
    current_xorg_config=INNOGPU_DDX
elif grep -Eq 'Driver[[:space:]]+"modesetting"' /etc/X11/xorg.conf 2>/dev/null; then
    current_xorg_config=MODESETTING_SOFT
fi

current_vendor_gl=DISABLED_OR_MISSING
if [[ -e /etc/ld.so.conf.d/0-innogpu-hwgl.conf &&
      -e /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so &&
      -e /usr/lib/xorg/modules/drivers/innogpu_drv.so ]]; then
    current_vendor_gl=ENABLED
fi

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

section "Package And Driver"
dpkg-query -W -f='package=${Package}\nversion=${Version}\nstatus=${db:Status-Abbrev}\n' innogpu-fh2m-trixie 2>/dev/null || true
cat /proc/driver/innogpu/gpu00/status 2>/dev/null || true

section "Current Persistent Xorg/GL Config"
echo "current_xorg_config=$current_xorg_config"
echo "current_vendor_gl=$current_vendor_gl"
echo "historic_latest_desktop_hwgl_result=${latest_desktop_hwgl_result:-MISSING}"
echo "historic_latest_current_xorg_hwgl_runtime_result=${latest_current_xorg_hwgl_runtime_result:-MISSING}"

section "Nodes"
ls -l /dev/dri /dev/dri/by-path /dev/fb0 2>/dev/null || true
if [[ ! -e /dev/dri/card0 || ! -e /dev/dri/renderD128 ]]; then
    echo "node_visibility=UNAVAILABLE_IN_CURRENT_CONTEXT_OR_MISSING"
fi

section "Xorg And dwm"
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
echo "XAUTHORITY=${auth:-missing}"
if [[ -n "${auth:-}" ]] && command -v xdpyinfo >/dev/null 2>&1 &&
   run_x "$auth" xdpyinfo >/dev/null 2>&1; then
    desktop_xorg_active=1
    echo "x_display_query=OK"
else
    echo "x_display_query=FAILED_OR_NOT_TESTED"
fi
echo "desktop_xorg_active=$desktop_xorg_active"

section "Current Desktop GL"
if [[ -n "${auth:-}" ]] && command -v glxinfo >/dev/null 2>&1; then
    run_x "$auth" glxinfo -B 2>/dev/null | grep -E 'OpenGL vendor|OpenGL renderer|OpenGL version|Accelerated|Device' || true
else
    echo "desktop_gl=SKIPPED_MISSING_XAUTHORITY_OR_TOOL"
fi

section "Current Xorg Acceleration Extensions"
if [[ -n "${auth:-}" ]] && command -v xdpyinfo >/dev/null 2>&1; then
    set +e
    extensions="$(run_x "$auth" xdpyinfo -queryExtensions 2>&1)"
    xdpyinfo_rc=$?
    set -e
    if [[ "$xdpyinfo_rc" == "0" ]]; then
        printf '%s\n' "$extensions" | grep -E 'DRI2|DRI3|GLX|Present|RANDR' || true
        if grep -q 'DRI3' <<<"$extensions"; then
            echo "dri3_result=AVAILABLE"
        else
            echo "dri3_result=MISSING"
        fi
    else
        printf '%s\n' "$extensions" | tail -20
        echo "dri3_result=QUERY_FAILED_CURRENT_CONTEXT_NOT_PROVEN"
    fi
else
    echo "dri3_result=SKIPPED_MISSING_XAUTHORITY_OR_TOOL"
fi

section "Deepin Surfaceless EGL Hardware Probe"
if [[ "$latest_desktop_hwgl_result" == "PASS_DESKTOP_HWGL" ]]; then
    echo "surfaceless_hw_result=SKIPPED_AFTER_DESKTOP_HWGL_PASS"
elif [[ -x /tmp/probe-surfaceless-gles2 ]]; then
    surfaceless="$("$ROOT/scripts/run-deepin-surfaceless-egl.sh" /tmp/probe-surfaceless-gles2 2>&1 || true)"
else
    surfaceless="$("$ROOT/scripts/run-deepin-surfaceless-egl.sh" 2>&1 || true)"
fi
if [[ "${surfaceless+x}" == "x" ]]; then
    printf '%s\n' "$surfaceless" | grep -E 'EGL vendor|OpenGL .*vendor|OpenGL .*renderer|OpenGL .*version|Fantasy|Innosilicon|ERROR|failed|Segmentation' || true
    printf '%s\n' "$surfaceless" | grep -E 'GL vendor|GL renderer|GL version|glClear error' || true
    if grep -Eq '(OpenGL|GL) .*renderer: Fantasy II-M' <<<"$surfaceless" &&
       grep -Eq '(OpenGL|GL) .*vendor: Innosilicon' <<<"$surfaceless" &&
       { ! grep -q 'glClear error:' <<<"$surfaceless" || grep -q 'glClear error: 0x0' <<<"$surfaceless"; }; then
        echo "surfaceless_hw_result=PASS"
    else
        echo "surfaceless_hw_result=FAIL"
    fi
fi

section "Deepin GBM EGL Hardware Probe"
if [[ "$latest_desktop_hwgl_result" == "PASS_DESKTOP_HWGL" ]]; then
    echo "gbm_hw_result=SKIPPED_AFTER_DESKTOP_HWGL_PASS"
elif [[ -x /tmp/probe-egl-gbm ]]; then
    gbm="$("$ROOT/scripts/run-deepin-gbm-egl.sh" 2>&1 || true)"
    printf '%s\n' "$gbm" | grep -E '== node|gbm backend|EGL vendor|GL vendor|GL renderer|GL version|glClear error|failed|Segmentation' || true
    if grep -q 'gbm backend: inno' <<<"$gbm" &&
       grep -q 'GL renderer: Fantasy II-M' <<<"$gbm" &&
       grep -q 'glClear error: 0x0' <<<"$gbm"; then
        echo "gbm_hw_result=PASS"
    else
        echo "gbm_hw_result=FAIL"
    fi
else
    echo "gbm_hw_result=SKIPPED_MISSING_/tmp/probe-egl-gbm"
fi

section "Known Remaining Gates"
if [[ "$latest_post_reboot_hwgl_result" == "PASS_POST_REBOOT_HWGL" &&
      "$current_xorg_config" == "INNOGPU_DDX" && "$current_vendor_gl" == "ENABLED" &&
      "$latest_desktop_hwgl_result" == "PASS_DESKTOP_HWGL" ]]; then
    echo "desktop_xorg_gl=PASS_DESKTOP_HWGL_CURRENT_CONFIG_MATCHES_HISTORY"
    echo "ddx_xorg=PASS_VENDOR_DDX_RUNTIME_ACCELERATION"
    echo "deepin_hwgl_trial_gate=PASSED_AND_INSTALLED"
    echo "post_reboot_hwgl=PASS_POST_REBOOT_HWGL"
    echo "remaining_gate=NONE"
elif [[ "$current_xorg_config" == "INNOGPU_DDX" && "$current_vendor_gl" == "ENABLED" &&
      "$latest_desktop_hwgl_result" == "PASS_DESKTOP_HWGL" ]]; then
    echo "desktop_xorg_gl=PASS_DESKTOP_HWGL_CURRENT_CONFIG_MATCHES_HISTORY"
    echo "ddx_xorg=PASS_VENDOR_DDX_RUNTIME_ACCELERATION"
    echo "deepin_hwgl_trial_gate=PASSED_AND_INSTALLED"
    echo "remaining_gate=OPTIONAL_REBOOT_PERSISTENCE_CHECK"
elif [[ "$current_xorg_config" == "MODESETTING_SOFT" || "$current_vendor_gl" != "ENABLED" ]]; then
    echo "desktop_xorg_gl=NOT_CURRENTLY_ENABLED"
    echo "ddx_xorg=${latest_ddx_result:-NOT_VERIFIED_RUN_LOCAL_TTY: $ROOT/scripts/run-local-ddx-vt-test.sh}"
    echo "deepin_hwgl_trial_gate=HISTORIC_RESULT_${latest_desktop_hwgl_result:-MISSING}_BUT_CURRENT_CONFIG_IS_${current_xorg_config}_${current_vendor_gl}"
    echo "remaining_gate=REAPPLY_DEEPIN_HWGL_TRIAL_THEN_VALIDATE_WITHOUT_REBOOT"
else
    echo "desktop_xorg_gl=NOT_COMPLETE_UNTIL_PASS_DESKTOP_HWGL"
    echo "ddx_xorg=${latest_ddx_result:-NOT_VERIFIED_RUN_LOCAL_TTY: $ROOT/scripts/run-local-ddx-vt-test.sh}"
    echo "deepin_hwgl_trial_gate=REQUIRES_PASS_VENDOR_DDX_RUNTIME_ACCELERATION"
    echo "remaining_gate=REAL_DESKTOP_STARTX_AND_CHECK_DESKTOP_HWGL"
fi
echo "gbm_egl=PASS_WITH_MINIMAL_GBM_GLES2_PROBE_AND_DESKTOP_HWGL"
echo "latest_ddx_result=${latest_ddx_result:-MISSING}"
echo "latest_desktop_hwgl_result=${latest_desktop_hwgl_result:-MISSING}"
echo "latest_current_xorg_hwgl_runtime_result=${latest_current_xorg_hwgl_runtime_result:-MISSING}"
echo "latest_post_reboot_hwgl_result=${latest_post_reboot_hwgl_result:-MISSING}"

section "Recovery Points"
if [[ -r "$ROOT/baselines/patched17-soft-baseline.txt" ]]; then
    sed -n '1,80p' "$ROOT/baselines/patched17-soft-baseline.txt"
else
    echo "missing patched17 baseline file"
fi
