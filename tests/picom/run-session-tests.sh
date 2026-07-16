#!/bin/sh

set -eu

test_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
project_root=$(CDPATH= cd -- "$test_dir/../.." && pwd)
session=${PICOM_SESSION_UNDER_TEST:-$project_root/scripts/picom-session.sh}
runtime=$(mktemp -d "${TMPDIR:-/tmp}/innogpu-picom-session.XXXXXX")
fake_bin=$runtime/bin
calls=$runtime/calls.log
tests=0

trap 'rm -rf "$runtime"' EXIT HUP INT TERM
mkdir -p "$fake_bin" "$runtime/home/.config/x11" "$runtime/cache"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

pass() {
    tests=$((tests + 1))
    printf 'ok %02d - %s\n' "$tests" "$1"
}

cat > "$fake_bin/pgrep" <<'SCRIPT'
#!/bin/sh
[ "${FAKE_RUNNING:-}" = "${2:-}" ]
SCRIPT
cat > "$fake_bin/setsid" <<'SCRIPT'
#!/bin/sh
printf '%s\n' "$*" >> "$FAKE_SESSION_CALLS"
SCRIPT
cat > "$fake_bin/picom" <<'SCRIPT'
#!/bin/sh
exit 0
SCRIPT
cat > "$fake_bin/xcompmgr" <<'SCRIPT'
#!/bin/sh
exit 0
SCRIPT
chmod 755 "$fake_bin/pgrep" "$fake_bin/setsid" \
    "$fake_bin/picom" "$fake_bin/xcompmgr"

run_session() {
    : > "$calls"
    PATH=$fake_bin:/usr/bin:/bin \
    HOME=$runtime/home \
    XDG_CACHE_HOME=$runtime/cache \
    XDG_CONFIG_HOME=$runtime/home/.config \
    FAKE_SESSION_CALLS=$calls \
    FAKE_RUNNING=${FAKE_RUNNING:-} \
    PICOM_BIN=${PICOM_BIN:-$fake_bin/picom} \
        "$session"
}

unset FAKE_RUNNING PICOM_BIN
run_session
grep -F -- "-f $fake_bin/picom --config $runtime/home/.config/x11/picom.conf" \
    "$calls" >/dev/null || fail 'T01 patched Picom was not selected'
grep -F -- "--log-file $runtime/cache/picom.log" "$calls" >/dev/null ||
    fail 'T01 Picom log path mismatch'
pass 'Picom is preferred with XDG config and cache paths'

PICOM_BIN=$runtime/missing-picom
export PICOM_BIN
run_session
grep -Fx -- '-f xcompmgr' "$calls" >/dev/null ||
    fail 'T02 xcompmgr fallback was not selected'
pass 'xcompmgr is selected only when Picom is unavailable'

unset PICOM_BIN
FAKE_RUNNING=picom
export FAKE_RUNNING
run_session
[ ! -s "$calls" ] || fail 'T03 duplicate Picom was started'
pass 'running Picom prevents a duplicate compositor'

printf 'PASS: %d Picom session tests\n' "$tests"
