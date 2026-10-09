#!/bin/sh
# Source from xprofile to start patched Picom, or xcompmgr when Picom is absent.

is_running() {
    if command -v pgrep >/dev/null 2>&1; then
        pgrep -x "$1" >/dev/null 2>&1
    elif command -v pidof >/dev/null 2>&1; then
        pidof -s "$1" >/dev/null 2>&1
    else
        return 1
    fi
}

start_detached() {
    if command -v setsid >/dev/null 2>&1; then
        setsid -f "$@" >/dev/null 2>&1
    else
        nohup "$@" >/dev/null 2>&1 &
    fi
}

picom_cmd=${PICOM_BIN:-$(command -v picom 2>/dev/null || true)}
if [ -n "$picom_cmd" ] && [ -x "$picom_cmd" ]; then
    if ! is_running picom; then
        cache_dir=${XDG_CACHE_HOME:-$HOME/.cache}
        config_file=${XDG_CONFIG_HOME:-$HOME/.config}/x11/picom.conf
        mkdir -p "$cache_dir"
        start_detached "$picom_cmd" --config "$config_file" \
            --log-level=warn --log-file "$cache_dir/picom.log"
    fi
elif command -v xcompmgr >/dev/null 2>&1 && ! is_running xcompmgr; then
    start_detached xcompmgr
fi
