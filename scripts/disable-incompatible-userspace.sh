#!/bin/bash
# Disable Innosilicon vendor userspace GL/GBM pieces that conflict with
# Debian Mesa/GBM. Keep the kernel DRM module and use Xorg modesetting.

set -euo pipefail

log() { echo "[innogpu-userspace] $*"; }
move_if_exists() {
    local src="$1" dst="$2"
    if [[ -e "$src" && ! -e "$dst" ]]; then
        mv -f "$src" "$dst"
        log "disabled $src -> $dst"
    elif [[ -e "$src" && -e "$dst" ]]; then
        rm -f "$src"
        log "removed duplicate $src (backup already exists: $dst)"
    fi
}

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    echo "ERROR: run as root: sudo $0" >&2
    exit 1
fi

if [[ -f /lib/systemd/system/sw-inno-gl.service || -L /etc/systemd/system/sw-inno-gl.service || -f /etc/systemd/system/multi-user.target.wants/sw-inno-gl.service ]]; then
    rm -f /etc/systemd/system/sw-inno-gl.service
    rm -f /etc/systemd/system/multi-user.target.wants/sw-inno-gl.service
    ln -sf /dev/null /etc/systemd/system/sw-inno-gl.service
    systemctl daemon-reload 2>/dev/null || true
    log "masked sw-inno-gl.service"
fi

for conf in /etc/ld.so.conf.d/0-innogpu.conf /etc/ld.so.conf.d/innogpu.conf; do
    move_if_exists "$conf" "${conf}.disabled"
done

for dri in \
    /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so \
    /usr/lib/dri/innogpu_dri.so; do
    move_if_exists "$dri" "${dri}.disabled"
done

if command -v ldconfig >/dev/null 2>&1; then
    ldconfig
elif [[ -x /sbin/ldconfig ]]; then
    /sbin/ldconfig
fi

mkdir -p /etc/X11
cat > /etc/X11/xorg.conf <<'XORG'
Section "Device"
    Identifier "gpu"
    Driver "modesetting"
    Option "kmsdev" "/dev/dri/card0"
EndSection
XORG
log "configured /etc/X11/xorg.conf for modesetting"

if [[ -f /etc/gdm3/daemon.conf ]]; then
    if grep -q '^#\?WaylandEnable=' /etc/gdm3/daemon.conf; then
        sed -i 's/^#\?WaylandEnable=.*/WaylandEnable=false/' /etc/gdm3/daemon.conf
    else
        sed -i '/\[daemon\]/a WaylandEnable=false' /etc/gdm3/daemon.conf
    fi
    log "disabled Wayland in GDM"
fi
