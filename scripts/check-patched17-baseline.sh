#!/bin/bash
# Read-only status check for the patched-17 Deepin-kernel soft-display baseline.
# This script does not require root and does not change system state.

set -euo pipefail

section() {
    printf '\n===== %s =====\n' "$*"
}

section "Package"
dpkg -l innogpu-fh2m-trixie 2>/dev/null || true
installed="$(dpkg-query -W -f='${Version}' innogpu-fh2m-trixie 2>/dev/null || true)"
if [[ "$installed" == "3.3.3.42-patched-17" ]]; then
    echo "package_result=OK"
else
    echo "package_result=NOT_PATCHED17 current=${installed:-unknown}"
fi

section "DKMS"
if [[ -x /sbin/dkms ]]; then
    /sbin/dkms status innogpu-kernel || true
else
    echo "dkms not found"
fi

section "Loaded Driver"
lsmod | grep '^innogpu' || true
cat /proc/driver/innogpu/gpu00/status 2>/dev/null || true
if [[ -r /sys/module/innogpu/parameters/s_bl_en_ctrl ]]; then
    printf 's_bl_en_ctrl='
    cat /sys/module/innogpu/parameters/s_bl_en_ctrl
fi

section "Device Nodes"
ls -l /dev/dri /dev/dri/by-path /dev/fb0 2>/dev/null || true
missing_nodes=0
for node in /dev/dri/card0 /dev/dri/renderD128 /dev/fb0; do
    if [[ -e "$node" ]]; then
        echo "node_ok=$node"
    else
        echo "node_missing=$node"
        missing_nodes=1
    fi
done
for f in /sys/class/drm/card0/dev /sys/class/drm/renderD128/dev /sys/class/graphics/fb0/dev; do
    [[ -r "$f" ]] && printf '%s=' "$f" && cat "$f"
done
if [[ "$missing_nodes" == "1" ]] &&
   [[ -r /sys/class/drm/card0/dev ]] &&
   [[ -r /sys/class/drm/renderD128/dev ]] &&
   [[ -r /sys/class/graphics/fb0/dev ]]; then
    echo "node_note=sysfs_minors_exist_but_dev_nodes_missing_or_not_visible"
    echo "node_note=run_on_real_tty: sudo scripts/repair-dri-nodes.sh"
fi

section "DRM Connectors"
for c in /sys/class/drm/card0-*; do
    [[ -d "$c" ]] || continue
    name="$(basename "$c")"
    status="$(cat "$c/status" 2>/dev/null || true)"
    enabled="$(cat "$c/enabled" 2>/dev/null || true)"
    dpms="$(cat "$c/dpms" 2>/dev/null || true)"
    printf '%s status=%s enabled=%s dpms=%s\n' "$name" "$status" "$enabled" "$dpms"
    if [[ -r "$c/modes" ]]; then
        sed 's/^/  mode=/' "$c/modes" | head -20
    fi
done

section "TTY And Xorg"
cat /sys/class/tty/tty0/active 2>/dev/null || true
ps -ef | grep -E 'agetty|getty|login|startx|Xorg|xinit|dwm|dwmblocks' | grep -v grep || true

section "Node Repair Service"
systemctl is-enabled innogpu-repair-dri-nodes.service 2>/dev/null || true
systemctl is-active innogpu-repair-dri-nodes.service 2>/dev/null || true
systemctl --no-pager --full status innogpu-repair-dri-nodes.service 2>/dev/null | sed -n '1,80p' || true

section "Soft Userspace Guard"
ls -l /etc/ld.so.conf.d/*innogpu* /usr/lib/x86_64-linux-gnu/dri/*inno* /usr/lib/x86_64-linux-gnu/gbm/*inno* 2>/dev/null || true
if [[ -e /etc/ld.so.conf.d/0-innogpu.conf || -e /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so ]]; then
    echo "soft_userspace_result=FAIL_VENDOR_GL_ENABLED"
else
    echo "soft_userspace_result=OK_VENDOR_GL_DISABLED"
fi

section "Summary"
if [[ "$installed" == "3.3.3.42-patched-17" ]] &&
   [[ -e /dev/dri/card0 ]] &&
   [[ -e /dev/dri/renderD128 ]] &&
   [[ -e /dev/fb0 ]]; then
    echo "RESULT: PATCHED17_BASELINE_NODES_OK"
else
    echo "RESULT: INCOMPLETE"
fi
