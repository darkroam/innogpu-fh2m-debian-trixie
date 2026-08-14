#!/bin/bash
# Connect the dotconfig-owned xdisplay engine to this Innogpu device.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
SOURCE_DIR="${INNOGPU_DISPLAY_SOURCE_DIR:-$ROOT/scripts}"
if [[ ! -d "$SOURCE_DIR" ]]; then
    SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
fi

X_USER="${INNOGPU_X_USER:-${SUDO_USER:-${USER:-}}}"
[[ -n "$X_USER" ]] || { echo "ERROR: cannot determine target user" >&2; exit 1; }

if [[ ${EUID:-$(id -u)} -ne 0 && "$X_USER" != "$(id -un)" ]]; then
    echo "ERROR: only root can install display files for another user" >&2
    exit 1
fi

X_HOME="${INNOGPU_X_HOME:-$(getent passwd "$X_USER" 2>/dev/null | cut -d: -f6)}"
X_HOME="${X_HOME:-$HOME}"
X_GROUP="$(id -gn "$X_USER" 2>/dev/null || printf '%s\n' "$X_USER")"

for file in restore-dp1-mode-x11.sh xdisplay-session.sh; do
    [[ -x "$SOURCE_DIR/$file" ]] || {
        echo "ERROR: missing display source: $SOURCE_DIR/$file" >&2
        exit 1
    }
done

if [[ ! -x "$X_HOME/.local/bin/xdisplay" && ! -x "$X_HOME/.local/bin/xdisplay.sh" ]]; then
    echo "ERROR: xdisplay is not installed for $X_USER; install it from the dotconfig project first" >&2
    exit 1
fi

set_owner() {
    if [[ ${EUID:-$(id -u)} -eq 0 ]]; then
        chown "$X_USER:$X_GROUP" "$@"
    fi
}

install -d -m 0755 "$X_HOME/.local/bin" "$X_HOME/.config/x11"
set_owner "$X_HOME/.local/bin" "$X_HOME/.config/x11"

install -m 0755 "$SOURCE_DIR/restore-dp1-mode-x11.sh" \
    "$X_HOME/.local/bin/innogpu-restore-dp1-mode-x11"
install -m 0755 "$SOURCE_DIR/xdisplay-session.sh" \
    "$X_HOME/.config/x11/innogpu-display-session.sh"
set_owner \
    "$X_HOME/.local/bin/innogpu-restore-dp1-mode-x11" \
    "$X_HOME/.config/x11/innogpu-display-session.sh"

xprofile="$X_HOME/.config/x11/xprofile"
if [[ ! -e "$xprofile" ]]; then
    install -m 0644 /dev/null "$xprofile"
    set_owner "$xprofile"
fi

if ! grep -Eq "xdisplay(['\"]?[[:space:]]+watch|\\.sh['\"]?[[:space:]]+--watch)|BEGIN INNOGPU DISPLAY SESSION" "$xprofile"; then
    xprofile_backup="$xprofile.before-innogpu-display"
    if [[ ! -e "$xprofile_backup" ]]; then
        cp -L -p -- "$xprofile" "$xprofile_backup"
        set_owner "$xprofile_backup"
    fi
    cat >> "$xprofile" <<'XPROFILE'

# BEGIN INNOGPU DISPLAY SESSION
if [ -r "${XDG_CONFIG_HOME:-$HOME/.config}/x11/innogpu-display-session.sh" ]; then
    . "${XDG_CONFIG_HOME:-$HOME/.config}/x11/innogpu-display-session.sh"
fi
# END INNOGPU DISPLAY SESSION
XPROFILE
    set_owner "$xprofile"
fi

echo "Installed Innogpu X11 display integration for $X_USER in $X_HOME"
