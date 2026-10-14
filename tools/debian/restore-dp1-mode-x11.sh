#!/bin/sh
# Recover the internal panel when innogpu exposes it without a usable mode.

set -u

OUTPUT="${1:-auto}"
MODE="1920x1200R"

if ! command -v xrandr >/dev/null 2>&1; then
    exit 0
fi

get_output_line() {
    xrandr 2>/dev/null | awk -v output="$OUTPUT" '$1 == output && $2 == "connected" { print; exit }'
}

detect_output() {
    if [ "$OUTPUT" != "auto" ]; then
        return
    fi

    for candidate in eDP-1 DP-1; do
        if xrandr 2>/dev/null | awk -v output="$candidate" '$1 == output && $2 == "connected" { found=1 } END { exit found ? 0 : 1 }'; then
            OUTPUT="$candidate"
            return
        fi
    done
}

has_current_mode() {
    line="$1"

    # A healthy line contains geometry after "connected", for example:
    # DP-1 connected primary 1920x1200+0+0 ...
    echo "$line" | grep -Eq "^${OUTPUT} connected( primary)? [0-9]+x[0-9]+[+-][0-9]+[+-][0-9]+"
}

is_connected_without_mode() {
    line=$(xrandr 2>/dev/null | awk -v output="$OUTPUT" '$1 == output && $2 == "connected" { print; exit }')
    [ -n "$line" ] || return 1
    has_current_mode "$line" && return 1
    return 0
}

# On this machine EDID/panel readiness appears several seconds after Xorg starts.
detect_output
[ "$OUTPUT" != "auto" ] || exit 0

i=0
while [ "$i" -lt 15 ]; do
    line=$(get_output_line)
    if [ -n "$line" ] && has_current_mode "$line"; then
        exit 0
    fi

    if is_connected_without_mode; then
        xset dpms force on >/dev/null 2>&1 || true

        if ! xrandr 2>/dev/null | grep -q "^  ${MODE} "; then
            xrandr --newmode "$MODE" 154.00 1920 1968 2000 2080 1200 1203 1209 1235 +hsync -vsync >/dev/null 2>&1 || true
        fi

        xrandr --addmode "$OUTPUT" "$MODE" >/dev/null 2>&1 || true
        xrandr --output "$OUTPUT" --mode "$MODE" --primary >/dev/null 2>&1 || true
    fi

    sleep 1
    i=$((i + 1))
done
