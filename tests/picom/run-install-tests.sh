#!/bin/sh

set -eu

test_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
project_root=$(CDPATH= cd -- "$test_dir/../.." && pwd)
installer=${PICOM_INSTALLER_UNDER_TEST:-$project_root/scripts/install-picom-user.sh}
runtime=$(mktemp -d "${TMPDIR:-/tmp}/innogpu-picom-install.XXXXXX")
tests=0

trap 'rm -rf "$runtime"' EXIT HUP INT TERM

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

pass() {
    tests=$((tests + 1))
    printf 'ok %02d - %s\n' "$tests" "$1"
}

run_installer() {
    home=$1
    INNOGPU_ROOT=$project_root INNOGPU_X_USER=$(id -un) \
    INNOGPU_X_HOME=$home "$installer" >/dev/null
}

empty_home=$runtime/empty-home
mkdir -p "$empty_home"
run_installer "$empty_home"
cmp -s "$project_root/config/picom.conf" \
    "$empty_home/.config/x11/picom.conf" || fail 'T01 config mismatch'
cmp -s "$project_root/scripts/picom-session.sh" \
    "$empty_home/.config/x11/innogpu-compositor-session.sh" ||
    fail 'T01 session mismatch'
grep -q 'BEGIN INNOGPU COMPOSITOR SESSION' \
    "$empty_home/.config/x11/xprofile" || fail 'T01 session block missing'
pass 'empty HOME receives config and one compositor session block'

run_installer "$empty_home"
[ "$(grep -c 'BEGIN INNOGPU COMPOSITOR SESSION' \
    "$empty_home/.config/x11/xprofile")" -eq 1 ] ||
    fail 'T02 duplicate session block added'
[ ! -e "$empty_home/.config/x11/picom.conf.before-innogpu" ] ||
    fail 'T02 identical managed config was backed up'
pass 'repeated installation is idempotent'

existing_home=$runtime/existing-home
mkdir -p "$existing_home/.config/x11"
printf '%s\n' 'backend = "xrender";' > "$existing_home/.config/x11/picom.conf"
printf '%s\n' 'picom --config "$HOME/.config/x11/picom.conf" &' > \
    "$existing_home/.config/x11/xprofile"
cp "$existing_home/.config/x11/xprofile" "$runtime/existing-xprofile"
run_installer "$existing_home"
grep -q 'backend = "xrender";' \
    "$existing_home/.config/x11/picom.conf.before-innogpu" ||
    fail 'T03 previous config was not preserved'
cmp -s "$runtime/existing-xprofile" "$existing_home/.config/x11/xprofile" ||
    fail 'T03 existing compositor entry was modified'
pass 'existing config is backed up and existing startup remains authoritative'

printf 'PASS: %d Picom installer tests\n' "$tests"
