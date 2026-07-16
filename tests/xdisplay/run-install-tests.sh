#!/bin/sh

set -eu

fixture_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
project_root=$(CDPATH= cd -- "$fixture_dir/../.." && pwd)
installer=${XDISPLAY_INSTALLER_UNDER_TEST:-$project_root/scripts/install-xdisplay-user.sh}
source_dir=${XDISPLAY_SOURCE_DIR_UNDER_TEST:-$project_root/scripts}
runtime=$(mktemp -d "${TMPDIR:-/tmp}/innogpu-xdisplay-install.XXXXXX")
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
    INNOGPU_DISPLAY_SOURCE_DIR=$source_dir \
    INNOGPU_X_USER=$(id -un) \
    INNOGPU_X_HOME=$home \
        "$installer" >/dev/null
}

empty_home=$runtime/empty-home
mkdir -p "$empty_home"
run_installer "$empty_home"
cmp -s "$source_dir/xdisplay.sh" "$empty_home/.local/bin/xdisplay.sh" ||
    fail 'T01 xdisplay source mismatch'
cmp -s "$source_dir/displayselect" "$empty_home/.local/bin/displayselect" ||
    fail 'T01 displayselect source mismatch'
cmp -s "$source_dir/xdisplay-session.sh" \
    "$empty_home/.config/x11/innogpu-display-session.sh" ||
    fail 'T01 session source mismatch'
grep -q 'BEGIN INNOGPU DISPLAY SESSION' "$empty_home/.config/x11/xprofile" ||
    fail 'T01 session block missing'
pass 'empty HOME receives exact display tools and one session block'

run_installer "$empty_home"
[ "$(grep -c 'BEGIN INNOGPU DISPLAY SESSION' "$empty_home/.config/x11/xprofile")" -eq 1 ] ||
    fail 'T02 duplicate session block added'
[ ! -e "$empty_home/.local/bin/xdisplay.sh.before-innogpu-soft" ] ||
    fail 'T02 identical managed watcher was backed up as a user version'
pass 'repeated installation is idempotent'

existing_home=$runtime/existing-home
mkdir -p "$existing_home/.local/bin" "$existing_home/.config/x11"
printf '%s\n' '#!/bin/sh' 'echo old-watcher' > "$existing_home/.local/bin/xdisplay.sh"
chmod 755 "$existing_home/.local/bin/xdisplay.sh"
printf '%s\n' '"$HOME/.local/bin/xdisplay.sh" --watch &' > \
    "$existing_home/.config/x11/xprofile"
cp "$existing_home/.config/x11/xprofile" "$runtime/existing-xprofile"
run_installer "$existing_home"
grep -q 'old-watcher' "$existing_home/.local/bin/xdisplay.sh.before-innogpu-soft" ||
    fail 'T03 previous watcher was not preserved'
cmp -s "$runtime/existing-xprofile" "$existing_home/.config/x11/xprofile" ||
    fail 'T03 existing watcher entry was modified'
pass 'existing watcher is preserved once and is not started twice'

symlink_home=$runtime/symlink-home
mkdir -p "$symlink_home/.config/x11"
printf '%s\n' '# existing profile' > "$symlink_home/.config/x11/profile-target"
ln -s profile-target "$symlink_home/.config/x11/xprofile"
run_installer "$symlink_home"
[ -L "$symlink_home/.config/x11/xprofile" ] ||
    fail 'T04 xprofile symlink was replaced'
[ ! -L "$symlink_home/.config/x11/xprofile.before-innogpu-display" ] ||
    fail 'T04 xprofile backup should be a regular snapshot'
grep -qx '# existing profile' \
    "$symlink_home/.config/x11/xprofile.before-innogpu-display" ||
    fail 'T04 xprofile target was not backed up'
grep -q 'BEGIN INNOGPU DISPLAY SESSION' \
    "$symlink_home/.config/x11/profile-target" ||
    fail 'T04 session block was not appended through xprofile symlink'
pass 'xprofile symlink target is updated without replacing the symlink'

printf 'PASS: %d display installer tests\n' "$tests"
