#!/bin/sh

set -eu

fixture_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
project_root=$(CDPATH= cd -- "$fixture_dir/../.." && pwd)
xdisplay=${XDISPLAY_UNDER_TEST:-$project_root/scripts/xdisplay.sh}
displayselect=${DISPLAYSELECT_UNDER_TEST:-$project_root/scripts/displayselect}
fake_bin=$fixture_dir/bin
runtime=$(mktemp -d "${TMPDIR:-/tmp}/innogpu-xdisplay-stage2.XXXXXX")
calls=$runtime/calls.log
mutations=$runtime/mutations.log
restore_calls=$runtime/restore-calls.log
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

assert_contains() {
	name=$1
	value=$2
	expected=$3
	printf '%s\n' "$value" | grep -F -- "$expected" >/dev/null ||
		fail "$name: missing $expected"
}

run_status() {
	fixture=$1
	root=$2
	display=$3
	: > "$calls"
	: > "$mutations"
	: > "$restore_calls"
	(
		unset XDISPLAY_INTERNAL_OUTPUTS XDISPLAY_RESTORE_COMMAND
		PATH=$fake_bin:$PATH
		export PATH
		DISPLAY=$display
		XDG_RUNTIME_DIR=$runtime
		XDISPLAY_TEST_MODE=1
		XDISPLAY_TEST_ROOT=$fixture_dir/test-roots/$root
		FAKE_XRANDR_FIXTURE=$fixture_dir/$fixture
		FAKE_XRANDR_CALLS=$calls
		FAKE_XRANDR_MUTATIONS=$mutations
		FAKE_RESTORE_CALLS=$restore_calls
		export DISPLAY XDG_RUNTIME_DIR XDISPLAY_TEST_MODE XDISPLAY_TEST_ROOT
		export FAKE_XRANDR_FIXTURE FAKE_XRANDR_CALLS FAKE_XRANDR_MUTATIONS
		export FAKE_RESTORE_CALLS
		"$xdisplay" --status
	)
}

apply_without_mutation() {
	fixture=$1
	root=$2
	: > "$calls"
	: > "$mutations"
	PATH=$fake_bin:$PATH DISPLAY=:9 XDG_RUNTIME_DIR=$runtime \
	XDISPLAY_TEST_MODE=1 XDISPLAY_TEST_ROOT=$fixture_dir/test-roots/$root \
	XDISPLAY_INTERNAL_OUTPUTS='eDP-1 DP-1' \
	FAKE_XRANDR_FIXTURE=$fixture_dir/$fixture FAKE_XRANDR_CALLS=$calls \
	FAKE_XRANDR_MUTATIONS=$mutations "$xdisplay" --apply
	[ ! -s "$mutations" ]
}

single=$(run_status single.xrandr open :9)
assert_contains T01 "$single" 'lid_present=yes'
assert_contains T01 "$single" 'lid_state=open'
assert_contains T01 "$single" 'output=eDP-1 connection:connected primary:1 geometry:1920x1200+0+0'
assert_contains T01 "$single" 'mode_ready:1 first_mode:1920x1200 active:1 stale:0 pending:0'
assert_contains T01 "$single" 'policy=single-output'
[ ! -s "$mutations" ] || fail 'T01 status mutated layout'
pass 'single output parsing and read-only status'

apply_without_mutation single.xrandr open || fail 'T02 single layout mutated'
apply_without_mutation extended.xrandr open || fail 'T02 extended layout mutated'
apply_without_mutation mirror-unknown-internal.xrandr open || fail 'T02 mirror layout mutated'
apply_without_mutation closed-external.xrandr closed || fail 'T02 closed external layout mutated'
pass 'converged apply preserves single, extended, mirror and closed layouts'

extended=$(run_status extended.xrandr open :9)
mirror=$(run_status mirror.xrandr open :9)
negative=$(run_status negative-manual.xrandr open :9)
assert_contains T03 "$extended" 'policy=extend-from-internal'
assert_contains T03 "$extended" 'output=HDMI-1 connection:connected primary:0 geometry:2560x1440+1920+0'
assert_contains T03 "$mirror" 'output=HDMI-1 connection:connected primary:0 geometry:2560x1440+0+0'
assert_contains T03 "$negative" 'output=HDMI-1 connection:connected primary:0 geometry:2560x1440-2560+0 width:2560 height:1440 x:-2560 y:0'
assert_contains T03 "$negative" 'output=eDP-1 connection:connected primary:1 geometry:1920x1200+0-120 width:1920 height:1200 x:0 y:-120'
extended_key=$(printf '%s\n' "$extended" | sed -n 's/^topology_signature=//p')
mirror_key=$(printf '%s\n' "$mirror" | sed -n 's/^topology_signature=//p')
negative_key=$(printf '%s\n' "$negative" | sed -n 's/^topology_signature=//p')
[ "$extended_key" = "$mirror_key" ] || fail 'T03 mirror changed physical topology key'
[ "$extended_key" = "$negative_key" ] || fail 'T03 negative layout changed physical topology key'
pass 'extended, mirrored and signed geometry share one physical key'

pending=$(run_status pending.xrandr open :9)
assert_contains T04 "$pending" 'output=HDMI-1 connection:connected primary:0 geometry:- width:- height:- x:- y:- mode_ready:0 first_mode:- active:0 stale:0 pending:1'
assert_contains T04 "$pending" 'pending_outputs=HDMI-1'
assert_contains T04 "$pending" 'health=pending'
pass 'pending output parsing'

stale=$(run_status current-stale.xrandr open :9)
assert_contains T05 "$stale" 'output=HDMI-2 connection:disconnected primary:0 geometry:2048x1280+2560+0'
assert_contains T05 "$stale" 'active:1 stale:1 pending:0'
assert_contains T05 "$stale" 'stale_outputs=HDMI-2'
assert_contains T05 "$stale" 'health=stale'
[ ! -s "$mutations" ] || fail 'T05 status tried to clean stale output'
pass 'disconnected geometry is observed but not changed'

unknown=$(run_status single.xrandr unknown :9)
assert_contains T06 "$unknown" 'lid_present=yes'
assert_contains T06 "$unknown" 'lid_state=unknown'
absent=$(run_status mirror-unknown-internal.xrandr absent :9)
assert_contains T06 "$absent" 'lid_present=no'
assert_contains T06 "$absent" 'lid_state=absent'
assert_contains T06 "$absent" 'policy=mirror-fallback'
pass 'lid present-unknown and absent remain distinct'

status_0=$(run_status single.xrandr open :0)
status_00=$(run_status single.xrandr open :0.0)
status_1=$(run_status single.xrandr open :1)
lock_0=$(printf '%s\n' "$status_0" | sed -n 's/^lock_apply=//p')
lock_00=$(printf '%s\n' "$status_00" | sed -n 's/^lock_apply=//p')
lock_1=$(printf '%s\n' "$status_1" | sed -n 's/^lock_apply=//p')
[ "$lock_0" = "$lock_00" ] || fail 'T07 :0 and :0.0 produced different locks'
[ "$lock_0" != "$lock_1" ] || fail 'T07 :0 and :1 produced the same lock'
relative_tmp=$(PATH=$fake_bin:$PATH DISPLAY=:2 XDG_RUNTIME_DIR= TMPDIR=relative-tmp \
	XDISPLAY_TEST_MODE=1 XDISPLAY_TEST_ROOT=$fixture_dir/test-roots/open \
	FAKE_XRANDR_FIXTURE=$fixture_dir/single.xrandr FAKE_XRANDR_CALLS=$calls \
	FAKE_XRANDR_MUTATIONS=$mutations "$xdisplay" --status)
assert_contains T07 "$relative_tmp" "lock_apply=/tmp/xdisplay-$(id -u)/"
pass 'X screen suffix normalization and X server isolation'

: > "$restore_calls"
: > "$mutations"
PATH=$fake_bin:$PATH DISPLAY=:9 XDG_RUNTIME_DIR=$runtime \
XDISPLAY_TEST_MODE=1 XDISPLAY_TEST_ROOT=$fixture_dir/test-roots/open \
XDISPLAY_INTERNAL_OUTPUTS='eDP-1 DP-1' XDISPLAY_RESTORE_COMMAND=restore-probe \
FAKE_XRANDR_FIXTURE=$fixture_dir/pending.xrandr FAKE_XRANDR_CALLS=$calls \
FAKE_XRANDR_MUTATIONS=$mutations FAKE_RESTORE_CALLS=$restore_calls \
"$xdisplay" --status >/dev/null
[ ! -s "$restore_calls" ] || fail 'T08 status invoked legacy restore helper'
[ ! -s "$mutations" ] || fail 'T08 status mutated layout'
pass 'status never invokes restore or layout commands'

symlink_base=$(mktemp -d "$runtime/runtime-symlink.XXXXXX")
victim=$symlink_base/victim
mkdir "$victim"
chmod 755 "$victim"
ln -s "$victim" "$symlink_base/xdisplay-$(id -u)"
set +e
PATH=$fake_bin:$PATH DISPLAY=:9 XDG_RUNTIME_DIR= TMPDIR=$symlink_base \
XDISPLAY_TEST_MODE=1 XDISPLAY_TEST_ROOT=$fixture_dir/test-roots/open \
FAKE_XRANDR_FIXTURE=$fixture_dir/single.xrandr "$xdisplay" --status >/dev/null 2>&1
symlink_result=$?
set -e
[ "$symlink_result" -ne 0 ] || fail 'T09 accepted symlinked fallback runtime directory'
[ "$(stat -c %a "$victim")" = 755 ] || fail 'T09 changed symlink target permissions'
set +e
PATH=$fake_bin:$PATH DISPLAY=:9 XDG_RUNTIME_DIR= TMPDIR=$symlink_base \
FAKE_XRANDR_FIXTURE=$fixture_dir/extended.xrandr \
"$displayselect" >/dev/null 2>&1
displayselect_symlink_result=$?
set -e
[ "$displayselect_symlink_result" -ne 0 ] ||
	fail 'T09 displayselect accepted symlinked fallback runtime directory'
[ "$(stat -c %a "$victim")" = 755 ] ||
	fail 'T09 displayselect changed symlink target permissions'
pass 'fallback runtime directory rejects symlinks without chmod side effects'

failure_runtime=$(mktemp -d "$runtime/failure.XXXXXX")
: > "$calls"
set +e
PATH=$fake_bin:$PATH DISPLAY=:7 XDG_RUNTIME_DIR=$failure_runtime \
XDISPLAY_TEST_MODE=1 XDISPLAY_TEST_ROOT=$fixture_dir/test-roots/open \
FAKE_XRANDR_MODE=fail FAKE_XRANDR_CALLS=$calls \
timeout 8 "$xdisplay" --watch >/dev/null 2>&1
failure_result=$?
set -e
[ "$failure_result" -ne 0 ] || fail 'T10 failing watcher returned success'
[ "$failure_result" -ne 124 ] || fail 'T10 failing watcher did not exit before timeout'
[ "$(wc -l < "$calls")" -ge 6 ] || fail 'T10 watcher exited before failure threshold'
find "$failure_runtime" -name '*.generation' -print -quit | grep . >/dev/null &&
	fail 'T10 watcher left its generation file behind'
pass 'watcher exits after bounded snapshot failures and cleans generation'

display_runtime=$(mktemp -d "$runtime/displayselect.XXXXXX")
for display in :0 :0.0 :1 localhost:10.0 host.example:2.3 host:foo.1; do
	expected_lock=$(PATH=$fake_bin:$PATH DISPLAY=$display \
		XDG_RUNTIME_DIR=$display_runtime XDISPLAY_TEST_MODE=1 \
		XDISPLAY_TEST_ROOT=$fixture_dir/test-roots/open \
		FAKE_XRANDR_FIXTURE=$fixture_dir/extended.xrandr \
		FAKE_XRANDR_CALLS=$calls FAKE_XRANDR_MUTATIONS=$mutations \
		"$xdisplay" --status | sed -n 's/^lock_apply=//p')
	set +e
	PATH=$fake_bin:$PATH DISPLAY=$display XDG_RUNTIME_DIR=$display_runtime \
	FAKE_XRANDR_FIXTURE=$fixture_dir/extended.xrandr FAKE_XRANDR_CALLS=$calls \
	FAKE_XRANDR_MUTATIONS=$mutations "$displayselect" >/dev/null 2>&1
	set -e
	[ -e "$expected_lock" ] ||
		fail "T11 lock mismatch for DISPLAY=$display"
done
user_id=$(id -u)
[ -e "$display_runtime/xdisplay-$user_id-_0.apply.lock" ] ||
	fail 'T11 displayselect did not use normalized :0 lock'
[ -e "$display_runtime/xdisplay-$user_id-_1.apply.lock" ] ||
	fail 'T11 displayselect did not isolate :1 lock'
[ "$(find "$display_runtime" -name '*.apply.lock' | wc -l)" -eq 5 ] ||
	fail 'T11 displayselect created inconsistent lock paths'
pass 'displayselect uses the same per-server lock naming'

optional_bin=$runtime/optional-bin
mkdir -p "$optional_bin"
for command_name in setbg remaps dunst notify-send; do
	printf '%s\n' '#!/bin/sh' 'exit 1' > "$optional_bin/$command_name"
	chmod 755 "$optional_bin/$command_name"
done
: > "$calls"
: > "$mutations"
PATH=$optional_bin:$fake_bin:/usr/bin:/bin DISPLAY=:8 \
XDG_RUNTIME_DIR=$display_runtime \
FAKE_XRANDR_FIXTURE=$fixture_dir/single.xrandr FAKE_XRANDR_CALLS=$calls \
FAKE_XRANDR_MUTATIONS=$mutations FAKE_XRANDR_MUTATION_RESULT=0 \
"$displayselect" >/dev/null 2>&1 ||
	fail 'T12 optional desktop helper failure changed displayselect result'
grep -F -- '--output eDP-1 --primary --auto --scale 1.0x1.0' "$mutations" >/dev/null ||
	fail 'T12 single-output layout was not applied'
pass 'optional desktop helper failures do not override a successful layout'

printf 'PASS: %s stage 2 fixture tests\n' "$tests"
