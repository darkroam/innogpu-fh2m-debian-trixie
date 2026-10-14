#!/bin/sh
# Source from an X11 xprofile to start the project display watcher.

case ":$PATH:" in
    *:"$HOME/.local/bin":*) ;;
    *) PATH="$HOME/.local/bin:$PATH" ;;
esac
export PATH

# This device has used both names for its internal panel. External outputs are
# always discovered from RandR and must not be listed here.
XDISPLAY_INTERNAL_OUTPUTS=${XDISPLAY_INTERNAL_OUTPUTS:-"eDP-1 DP-1"}
export XDISPLAY_INTERNAL_OUTPUTS

if command -v innogpu-restore-dp1-mode-x11 >/dev/null 2>&1; then
    XDISPLAY_RESTORE_COMMAND=${XDISPLAY_RESTORE_COMMAND:-innogpu-restore-dp1-mode-x11}
    export XDISPLAY_RESTORE_COMMAND
fi

if command -v xdisplay >/dev/null 2>&1; then
    xdisplay watch &
elif command -v xdisplay.sh >/dev/null 2>&1; then
    xdisplay.sh --watch &
fi
