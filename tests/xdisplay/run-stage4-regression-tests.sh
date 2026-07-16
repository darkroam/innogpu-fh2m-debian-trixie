#!/bin/sh

set -eu

fixture_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
project_root=$(CDPATH= cd -- "$fixture_dir/../.." && pwd)
xdisplay=${XDISPLAY_UNDER_TEST:-$project_root/scripts/xdisplay.sh}
fake_bin=$fixture_dir/bin
root=$fixture_dir/test-roots/open
regressions=$fixture_dir/regression
runtime=$(mktemp -d "${TMPDIR:-/tmp}/innogpu-xdisplay-regression.XXXXXX")
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

begin_sequence() {
	name=$1
	initial_state=${2:-0}
	scenario=$regressions/$name
	state=$runtime/$name.state
	calls=$runtime/$name.calls
	mutations=$runtime/$name.mutations
	log=$runtime/$name.log
	printf '%s\n' "$initial_state" > "$state"
	: > "$calls"
	: > "$mutations"
	: > "$log"
}

sequence_status() {
	display=$1
	PATH=$fake_bin:$PATH DISPLAY=$display XDG_RUNTIME_DIR=$runtime \
	XDISPLAY_TEST_MODE=1 XDISPLAY_TEST_ROOT=$root \
	FAKE_XRANDR_SEQUENCE_DIR=$scenario FAKE_XRANDR_SEQUENCE_STATE=$state \
	FAKE_XRANDR_CALLS=$calls FAKE_XRANDR_MUTATIONS=$mutations \
	"$xdisplay" --status
}

sequence_apply() {
	display=$1
	set +e
	PATH=$fake_bin:$PATH DISPLAY=$display XDG_RUNTIME_DIR=$runtime \
	XDISPLAY_TEST_MODE=1 XDISPLAY_TEST_ROOT=$root \
	FAKE_XRANDR_SEQUENCE_DIR=$scenario FAKE_XRANDR_SEQUENCE_STATE=$state \
	FAKE_XRANDR_CALLS=$calls FAKE_XRANDR_MUTATIONS=$mutations \
	FAKE_XRANDR_MUTATION_RESULT=0 \
	"$xdisplay" --apply > "$log" 2>&1
	apply_result=$?
	set -e
}

begin_sequence stale-cleanup
sequence_apply :40
[ "$apply_result" -eq 0 ] || fail 'T01 stale cleanup apply failed'
[ "$(cat "$state")" = 1 ] || fail 'T01 stale cleanup did not reach final state'
[ "$(wc -l < "$mutations")" -eq 1 ] ||
	fail 'T01 stale cleanup emitted more than one mutation'
[ "$(cat "$mutations")" = '--output HDMI-2 --off' ] ||
	fail 'T01 stale cleanup did not explicitly disable HDMI-2'
stale_final=$(sequence_status :40)
assert_contains T01 "$stale_final" 'screen=number:0 minimum:320x200 current:2560x1600'
assert_contains T01 "$stale_final" 'stale_outputs=none'
assert_contains T01 "$stale_final" 'health=ready'
pass 'disconnected active output is disabled and framebuffer contracts'

fixed_calls=$runtime/stale-reread.calls
fixed_mutations=$runtime/stale-reread.mutations
fixed_log=$runtime/stale-reread.log
: > "$fixed_calls"
: > "$fixed_mutations"
set +e
PATH=$fake_bin:$PATH DISPLAY=:41 XDG_RUNTIME_DIR=$runtime \
XDISPLAY_TEST_MODE=1 XDISPLAY_TEST_ROOT=$root \
FAKE_XRANDR_FIXTURE=$fixture_dir/current-stale.xrandr \
FAKE_XRANDR_CALLS=$fixed_calls FAKE_XRANDR_MUTATIONS=$fixed_mutations \
FAKE_XRANDR_MUTATION_RESULT=0 \
"$xdisplay" --apply > "$fixed_log" 2>&1
fixed_result=$?
set -e
[ "$fixed_result" -ne 0 ] ||
	fail 'T02 apply accepted stale geometry after successful mutation'
[ "$(wc -l < "$fixed_mutations")" -eq 1 ] ||
	fail 'T02 stale reread failure was not bounded to one mutation'
[ "$(cat "$fixed_mutations")" = '--output HDMI-2 --off' ] ||
	fail 'T02 stale reread did not use the expected cleanup command'
pass 'stale cleanup requires a clean post-mutation snapshot'

unsafe_calls=$runtime/stale-unsafe.calls
unsafe_mutations=$runtime/stale-unsafe.mutations
unsafe_log=$runtime/stale-unsafe.log
: > "$unsafe_calls"
: > "$unsafe_mutations"
set +e
PATH=$fake_bin:$PATH DISPLAY=:42 XDG_RUNTIME_DIR=$runtime \
XDISPLAY_TEST_MODE=1 XDISPLAY_TEST_ROOT=$root \
FAKE_XRANDR_FIXTURE=$regressions/stale-no-safe-output.xrandr \
FAKE_XRANDR_CALLS=$unsafe_calls FAKE_XRANDR_MUTATIONS=$unsafe_mutations \
FAKE_XRANDR_MUTATION_RESULT=0 \
"$xdisplay" --apply > "$unsafe_log" 2>&1
unsafe_result=$?
set -e
[ "$unsafe_result" -ne 0 ] ||
	fail 'T02-safe apply disabled the last framebuffer output'
[ ! -s "$unsafe_mutations" ] ||
	fail 'T02-safe emitted a mutation without a safe connected output'
pass 'stale cleanup preserves the last framebuffer output while replacement is pending'

ready_calls=$runtime/stale-ready.calls
ready_mutations=$runtime/stale-ready.mutations
ready_log=$runtime/stale-ready.log
: > "$ready_calls"
: > "$ready_mutations"
set +e
PATH=$fake_bin:$PATH DISPLAY=:49 XDG_RUNTIME_DIR=$runtime \
XDISPLAY_TEST_MODE=1 XDISPLAY_TEST_ROOT=$root \
FAKE_XRANDR_FIXTURE=$regressions/stale-ready-inactive.xrandr \
FAKE_XRANDR_CALLS=$ready_calls FAKE_XRANDR_MUTATIONS=$ready_mutations \
FAKE_XRANDR_MUTATION_RESULT=97 \
"$xdisplay" --apply > "$ready_log" 2>&1
ready_result=$?
set -e
[ "$ready_result" -ne 0 ] ||
	fail 'T02-ready accepted a failed replacement activation'
[ "$(wc -l < "$ready_mutations")" -eq 1 ] ||
	fail 'T02-ready did not stop after replacement activation failed'
ready_mutation=$(cat "$ready_mutations")
assert_contains T02-ready "$ready_mutation" \
	'--output eDP-1 --primary --mode 2560x1600 --rate 90.00 --pos 0x0'
printf '%s\n' "$ready_mutation" | grep -F -- '--output HDMI-2 --off' >/dev/null &&
	fail 'T02-ready disabled stale output before replacement was active'
pass 'ready replacement must activate successfully before stale output is disabled'

retry_calls=$runtime/stale-retry.calls
retry_mutations=$runtime/stale-retry.mutations
retry_times=$runtime/stale-retry.times
retry_log=$runtime/stale-retry.log
: > "$retry_calls"
: > "$retry_mutations"
: > "$retry_times"
set +e
PATH=$fake_bin:$PATH DISPLAY=:47 XDG_RUNTIME_DIR=$runtime \
XDISPLAY_TEST_MODE=1 XDISPLAY_TEST_ROOT=$root \
FAKE_XRANDR_FIXTURE=$fixture_dir/current-stale.xrandr \
FAKE_XRANDR_CALLS=$retry_calls FAKE_XRANDR_MUTATIONS=$retry_mutations \
FAKE_XRANDR_MUTATION_TIMES=$retry_times FAKE_XRANDR_MUTATION_RESULT=0 \
timeout 17 "$xdisplay" --watch > "$retry_log" 2>&1
retry_result=$?
set -e
[ "$retry_result" -eq 124 ] ||
	fail "T03 stale retry watcher returned unexpected status $retry_result"
[ "$(wc -l < "$retry_mutations")" -eq 3 ] ||
	fail 'T03 stale retry count was not capped at three'
awk '
	NR == 1 { previous = $1; next }
	$1 - previous < 4 { exit 1 }
	{ previous = $1 }
' "$retry_times" || fail 'T03 stale retries were not backed off'
pass 'persistent stale state is retried three times with bounded backoff'

begin_sequence connector-switch
direct=$(sequence_status :43)
assert_contains T04 "$direct" 'output=HDMI-1 connection:connected'
assert_contains T04 "$direct" 'current_mode:3840x2160'
printf '%s\n' 1 > "$state"
: > "$mutations"
sequence_apply :43
[ "$apply_result" -eq 0 ] || fail 'T04 connector switch apply failed'
[ "$(cat "$state")" = 3 ] || fail 'T04 connector switch did not converge'
[ "$(wc -l < "$mutations")" -eq 2 ] ||
	fail 'T04 connector switch did not use two bounded mutations'
[ "$(sed -n '1p' "$mutations")" = '--output HDMI-1 --off' ] ||
	fail 'T04 old direct connector was not disabled first'
second_mutation=$(sed -n '2p' "$mutations")
assert_contains T04 "$second_mutation" '--output HDMI-2 --mode 2560x1440 --rate 59.95'
switched=$(sequence_status :43)
assert_contains T04 "$switched" 'screen=number:0 minimum:320x200 current:5120x1600'
assert_contains T04 "$switched" 'output=HDMI-1 connection:disconnected'
assert_contains T04 "$switched" 'output=HDMI-2 connection:connected'
assert_contains T04 "$switched" 'current_mode:2560x1440'
assert_contains T04 "$switched" 'health=ready'
pass 'direct connector residue is cleared before dock mode normalization'

begin_sequence no-preferred
unpreferred=$(sequence_status :44)
assert_contains T05 "$unpreferred" 'output=HDMI-2 connection:connected'
assert_contains T05 "$unpreferred" 'preferred_mode:-'
assert_contains T05 "$unpreferred" 'target_mode:2560x1440'
assert_contains T05 "$unpreferred" 'target_rate:59.95'
assert_contains T05 "$unpreferred" 'mode_count:3'
sequence_apply :44
[ "$apply_result" -eq 0 ] || fail 'T05 no-preferred apply failed'
mode_mutation=$(cat "$mutations")
assert_contains T05 "$mode_mutation" '--output HDMI-2 --mode 2560x1440 --rate 59.95'
printf '%s\n' "$mode_mutation" | grep -F -- '--auto' >/dev/null &&
	fail 'T05 no-preferred mode used --auto'
[ "$(cat "$state")" = 1 ] || fail 'T05 no-preferred mode did not converge'
pass 'first advertised mode is explicit target when preferred is absent'

begin_sequence standalone-preferred
standalone_preferred=$(sequence_status :50)
assert_contains T05-standalone "$standalone_preferred" \
	'preferred_mode:1920x1200'
assert_contains T05-standalone "$standalone_preferred" \
	'preferred_rate:59.95'
assert_contains T05-standalone "$standalone_preferred" \
	'target_mode:1920x1200'
assert_contains T05-standalone "$standalone_preferred" \
	'target_rate:59.95'
assert_contains T05-standalone "$standalone_preferred" \
	'mode_signature:1920x1200@59.95+;1920x1080@60.00;'
pass 'standalone preferred marker is associated with its preceding mode rate'

begin_sequence capability-delay
set +e
PATH=$fake_bin:$PATH DISPLAY=:45 XDG_RUNTIME_DIR=$runtime \
XDISPLAY_TEST_MODE=1 XDISPLAY_TEST_ROOT=$root \
FAKE_XRANDR_SEQUENCE_DIR=$scenario FAKE_XRANDR_SEQUENCE_STATE=$state \
FAKE_XRANDR_CALLS=$calls FAKE_XRANDR_MUTATIONS=$mutations \
FAKE_XRANDR_MUTATION_RESULT=0 \
timeout 8 "$xdisplay" --watch > "$log" 2>&1
watch_result=$?
set -e
[ "$watch_result" -eq 124 ] ||
	fail "T06 settling watcher returned unexpected status $watch_result"
[ "$(cat "$state")" = 3 ] ||
	fail 'T06 watcher stopped probing before delayed capabilities appeared'
[ "$(wc -l < "$mutations")" -eq 1 ] ||
	fail 'T06 delayed capability change was not normalized exactly once'
delayed_mutation=$(cat "$mutations")
assert_contains T06 "$delayed_mutation" '--output HDMI-2 --mode 2560x1440 --rate 59.95'
query_count=$(awk -F '\t' '$2 == "--query" { count++ } END { print count + 0 }' "$calls")
[ "$query_count" -ge 2 ] || fail 'T06 watcher did not repeat active probing'
delayed_final=$(sequence_status :45)
assert_contains T06 "$delayed_final" 'current_mode:2560x1440'
assert_contains T06 "$delayed_final" 'mode_count:3'
assert_contains T06 "$delayed_final" 'health=ready'
pass 'successful layout keeps settling until delayed capabilities converge'

begin_sequence capability-preferred-delay
preferred_initial=$(sequence_status :48)
assert_contains T06-full "$preferred_initial" 'first_mode:2048x1280'
assert_contains T06-full "$preferred_initial" 'preferred_mode:-'
assert_contains T06-full "$preferred_initial" 'target_mode:2048x1280'
assert_contains T06-full "$preferred_initial" \
	'mode_signature:2048x1280@59.98;2560x1440@50.00;'
set +e
PATH=$fake_bin:$PATH DISPLAY=:48 XDG_RUNTIME_DIR=$runtime \
XDISPLAY_TEST_MODE=1 XDISPLAY_TEST_ROOT=$root \
FAKE_XRANDR_SEQUENCE_DIR=$scenario FAKE_XRANDR_SEQUENCE_STATE=$state \
FAKE_XRANDR_CALLS=$calls FAKE_XRANDR_MUTATIONS=$mutations \
FAKE_XRANDR_MUTATION_RESULT=0 \
timeout 8 "$xdisplay" --watch > "$log" 2>&1
preferred_watch_result=$?
set -e
[ "$preferred_watch_result" -eq 124 ] ||
	fail "T06-full settling watcher returned unexpected status $preferred_watch_result"
[ "$(cat "$state")" = 3 ] ||
	fail 'T06-full watcher ignored preferred/rate capability changes'
[ "$(wc -l < "$mutations")" -eq 1 ] ||
	fail 'T06-full capability change was not normalized exactly once'
preferred_mutation=$(cat "$mutations")
assert_contains T06-full "$preferred_mutation" \
	'--output HDMI-2 --mode 2560x1440 --rate 59.95'
preferred_query_count=$(awk -F '\t' \
	'$2 == "--query" { count++ } END { print count + 0 }' "$calls")
[ "$preferred_query_count" -ge 2 ] ||
	fail 'T06-full watcher did not repeat active probing'
preferred_final=$(sequence_status :48)
assert_contains T06-full "$preferred_final" 'first_mode:2048x1280'
assert_contains T06-full "$preferred_final" 'current_mode:2560x1440'
assert_contains T06-full "$preferred_final" 'preferred_mode:2560x1440'
assert_contains T06-full "$preferred_final" 'preferred_rate:59.95'
assert_contains T06-full "$preferred_final" 'target_mode:2560x1440'
assert_contains T06-full "$preferred_final" \
	'mode_signature:2048x1280@59.98;2560x1440@59.95+,50.00;'
pass 'full capability signature catches preferred and rate changes with stable first mode'

begin_sequence missing-capabilities
missing_inactive=$(sequence_status :46)
assert_contains T07 "$missing_inactive" 'output=HDMI-2 connection:connected'
assert_contains T07 "$missing_inactive" 'mode_count:0'
assert_contains T07 "$missing_inactive" 'target_mode:-'
assert_contains T07 "$missing_inactive" 'active:0 stale:0 pending:1'
sequence_apply :46
[ ! -s "$mutations" ] ||
	fail 'T07 inactive output without capabilities was guessed or enabled'

printf '%s\n' 1 > "$state"
: > "$mutations"
missing_active=$(sequence_status :46)
assert_contains T07 "$missing_active" 'mode_count:0'
assert_contains T07 "$missing_active" 'target_mode:-'
assert_contains T07 "$missing_active" 'active:1 stale:0 pending:0'
sequence_apply :46
[ "$apply_result" -eq 0 ] ||
	fail 'T07 active output without capabilities was not preserved'
[ ! -s "$mutations" ] ||
	fail 'T07 active output without capabilities was reconfigured'
[ "$(cat "$state")" = 1 ] ||
	fail 'T07 missing-capability apply changed fixture state'
pass 'missing EDID or mode data preserves active output and leaves inactive pending'

printf 'PASS: %s stage 4 display regression tests\n' "$tests"
