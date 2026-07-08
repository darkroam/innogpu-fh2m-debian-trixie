#!/bin/bash
# Recreate missing DRM/fbdev nodes from sysfs. This covers systems where the
# innogpu minors exist but udev/devtmpfs did not expose the device nodes.

set -euo pipefail

log() { echo "[innogpu-dri-nodes] $*"; }
err() { echo "ERROR: $*" >&2; exit 1; }

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    err "run as root: sudo $0"
fi

[[ -d /sys/class/drm ]] || err "/sys/class/drm is missing"

install -d -m 0755 /dev/dri /dev/dri/by-path

group_or_root() {
    local group="$1"
    if getent group "$group" >/dev/null 2>&1; then
        printf '%s' "$group"
    else
        printf 'root'
    fi
}

make_node() {
    local name="$1"
    local mode="$2"
    local group="$3"
    local sysdev="/sys/class/drm/${name}/dev"
    local node="/dev/dri/${name}"
    local major minor pci

    [[ -r "$sysdev" ]] || return 0
    IFS=: read -r major minor < "$sysdev"
    [[ -n "$major" && -n "$minor" ]] || err "invalid major/minor in $sysdev"

    if [[ -e "$node" && ! -c "$node" ]]; then
        mv -f "$node" "${node}.not-char-before-innogpu"
        log "moved non-device $node aside"
    fi

    if [[ ! -e "$node" ]]; then
        mknod "$node" c "$major" "$minor"
        log "created $node ($major:$minor)"
    fi

    chown "root:$(group_or_root "$group")" "$node" 2>/dev/null || true
    chmod "$mode" "$node"

    if [[ -e "/sys/class/drm/${name}/device" ]]; then
        pci="$(basename "$(readlink -f "/sys/class/drm/${name}/device")")"
        case "$name" in
            card*) ln -sfn "../${name}" "/dev/dri/by-path/pci-${pci}-card" ;;
            renderD*) ln -sfn "../${name}" "/dev/dri/by-path/pci-${pci}-render" ;;
        esac
    fi
}

for card in /sys/class/drm/card[0-9]*; do
    [[ -e "$card/dev" ]] || continue
    make_node "$(basename "$card")" 0660 video
done

for render in /sys/class/drm/renderD*; do
    [[ -e "$render/dev" ]] || continue
    # Use 0666 so non-logind sessions and recovery shells can still validate
    # render-node acceleration. A real desktop session may later tighten this
    # with udev ACLs or the render group.
    make_node "$(basename "$render")" 0666 render
done

if ! compgen -G "/dev/dri/card*" >/dev/null; then
    err "no DRM card node could be created"
fi

for fb in /sys/class/graphics/fb[0-9]*; do
    [[ -e "$fb/dev" ]] || continue

    name="$(basename "$fb")"
    node="/dev/$name"
    IFS=: read -r major minor < "$fb/dev"
    [[ -n "$major" && -n "$minor" ]] || err "invalid major/minor in $fb/dev"

    if [[ -e "$node" && ! -c "$node" ]]; then
        mv -f "$node" "${node}.not-char-before-innogpu"
        log "moved non-device $node aside"
    fi

    if [[ ! -e "$node" ]]; then
        mknod "$node" c "$major" "$minor"
        log "created $node ($major:$minor)"
    fi

    chown "root:$(group_or_root video)" "$node" 2>/dev/null || true
    chmod 0660 "$node"
done

log "DRM nodes:"
ls -l /dev/dri /dev/dri/by-path 2>/dev/null || true

log "fbdev nodes:"
ls -l /dev/fb* 2>/dev/null || true
