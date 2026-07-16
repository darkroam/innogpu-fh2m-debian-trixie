#!/bin/bash
# Install the project Picom config and one compositor session entry for a user.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
X_USER="${INNOGPU_X_USER:-${SUDO_USER:-${USER:-}}}"
[[ -n "$X_USER" ]] || { echo "ERROR: cannot determine target user" >&2; exit 1; }

if [[ ${EUID:-$(id -u)} -ne 0 && "$X_USER" != "$(id -un)" ]]; then
    echo "ERROR: only root can install Picom files for another user" >&2
    exit 1
fi

X_HOME="${INNOGPU_X_HOME:-$(getent passwd "$X_USER" 2>/dev/null | cut -d: -f6)}"
X_HOME="${X_HOME:-$HOME}"
X_GROUP="$(id -gn "$X_USER" 2>/dev/null || printf '%s\n' "$X_USER")"
source_config="$ROOT/config/picom.conf"
source_session="$ROOT/scripts/picom-session.sh"

[[ -r "$source_config" ]] || { echo "ERROR: missing $source_config" >&2; exit 1; }
[[ -x "$source_session" ]] || { echo "ERROR: missing $source_session" >&2; exit 1; }

set_owner() {
    if [[ ${EUID:-$(id -u)} -eq 0 ]]; then
        chown "$X_USER:$X_GROUP" "$@"
    fi
}

install -d -m 0755 "$X_HOME/.config/x11"
set_owner "$X_HOME/.config/x11"

picom_config="$X_HOME/.config/x11/picom.conf"
picom_backup="$picom_config.before-innogpu"
if [[ -e "$picom_config" && ! -e "$picom_backup" ]] &&
    ! cmp -s "$source_config" "$picom_config"; then
    cp -L -p -- "$picom_config" "$picom_backup"
    set_owner "$picom_backup"
fi

install -m 0644 "$source_config" "$picom_config"
install -m 0755 "$source_session" \
    "$X_HOME/.config/x11/innogpu-compositor-session.sh"
set_owner "$picom_config" "$X_HOME/.config/x11/innogpu-compositor-session.sh"

xprofile="$X_HOME/.config/x11/xprofile"
if [[ ! -e "$xprofile" ]]; then
    install -m 0644 /dev/null "$xprofile"
    set_owner "$xprofile"
fi

if ! awk '
    /BEGIN INNOGPU COMPOSITOR SESSION/ { found = 1; next }
    /^[[:space:]]*#/ { next }
    /picom|xcompmgr/ { found = 1 }
    END { exit found ? 0 : 1 }
' "$xprofile"; then
    xprofile_backup="$xprofile.before-innogpu-compositor"
    if [[ ! -e "$xprofile_backup" ]]; then
        cp -L -p -- "$xprofile" "$xprofile_backup"
        set_owner "$xprofile_backup"
    fi
    cat >> "$xprofile" <<'XPROFILE'

# BEGIN INNOGPU COMPOSITOR SESSION
if [ -r "${XDG_CONFIG_HOME:-$HOME/.config}/x11/innogpu-compositor-session.sh" ]; then
    . "${XDG_CONFIG_HOME:-$HOME/.config}/x11/innogpu-compositor-session.sh"
fi
# END INNOGPU COMPOSITOR SESSION
XPROFILE
    set_owner "$xprofile"
fi

echo "Installed Picom user configuration for $X_USER in $X_HOME"
