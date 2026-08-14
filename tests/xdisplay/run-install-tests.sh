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

install_fake_engine() {
    home=$1
    mkdir -p "$home/.local/bin"
    printf '%s\n' '#!/bin/sh' 'exit 0' > "$home/.local/bin/xdisplay"
    chmod 755 "$home/.local/bin/xdisplay"
}

run_installer() {
    home=$1
    INNOGPU_DISPLAY_SOURCE_DIR=$source_dir \
    INNOGPU_X_USER=$(id -un) \
    INNOGPU_X_HOME=$home \
        "$installer" >/dev/null
}

missing_home=$runtime/missing-home
mkdir -p "$missing_home"
if run_installer "$missing_home" 2>/dev/null; then
    fail 'T01 integration succeeded without the dotconfig engine'
fi
pass 'missing dotconfig engine is rejected without installing a private copy'

empty_home=$runtime/empty-home
install_fake_engine "$empty_home"
cp "$empty_home/.local/bin/xdisplay" "$runtime/xdisplay-before"
run_installer "$empty_home"
cmp -s "$runtime/xdisplay-before" "$empty_home/.local/bin/xdisplay" ||
    fail 'T02 dotconfig-owned xdisplay was modified'
cmp -s "$source_dir/xdisplay-session.sh" \
    "$empty_home/.config/x11/innogpu-display-session.sh" ||
    fail 'T02 session source mismatch'
cmp -s "$source_dir/restore-dp1-mode-x11.sh" \
    "$empty_home/.local/bin/innogpu-restore-dp1-mode-x11" ||
    fail 'T02 device restore hook mismatch'
grep -q 'BEGIN INNOGPU DISPLAY SESSION' "$empty_home/.config/x11/xprofile" ||
    fail 'T02 session block missing'
pass 'integration installs only the device hook and session entry'

run_installer "$empty_home"
[ "$(grep -c 'BEGIN INNOGPU DISPLAY SESSION' "$empty_home/.config/x11/xprofile")" -eq 1 ] ||
    fail 'T03 duplicate session block added'
pass 'repeated installation is idempotent'

existing_home=$runtime/existing-home
install_fake_engine "$existing_home"
mkdir -p "$existing_home/.config/x11"
printf '%s\n' '"$HOME/.local/bin/xdisplay" watch &' > \
    "$existing_home/.config/x11/xprofile"
cp "$existing_home/.config/x11/xprofile" "$runtime/existing-xprofile"
run_installer "$existing_home"
cmp -s "$runtime/existing-xprofile" "$existing_home/.config/x11/xprofile" ||
    fail 'T04 existing watcher entry was modified'
pass 'existing watcher is preserved once and is not started twice'

symlink_home=$runtime/symlink-home
install_fake_engine "$symlink_home"
mkdir -p "$symlink_home/.config/x11"
printf '%s\n' '# existing profile' > "$symlink_home/.config/x11/profile-target"
ln -s profile-target "$symlink_home/.config/x11/xprofile"
run_installer "$symlink_home"
[ -L "$symlink_home/.config/x11/xprofile" ] ||
    fail 'T05 xprofile symlink was replaced'
[ ! -L "$symlink_home/.config/x11/xprofile.before-innogpu-display" ] ||
    fail 'T05 xprofile backup should be a regular snapshot'
grep -qx '# existing profile' \
    "$symlink_home/.config/x11/xprofile.before-innogpu-display" ||
    fail 'T05 xprofile target was not backed up'
grep -q 'BEGIN INNOGPU DISPLAY SESSION' \
    "$symlink_home/.config/x11/profile-target" ||
    fail 'T05 session block was not appended through xprofile symlink'
pass 'xprofile symlink target is updated without replacing the symlink'

printf 'PASS: %d display installer tests\n' "$tests"
