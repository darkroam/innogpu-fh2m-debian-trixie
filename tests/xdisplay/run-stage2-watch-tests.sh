#!/bin/sh

set -eu

fixture_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
project_root=$(CDPATH= cd -- "$fixture_dir/../.." && pwd)
xdisplay=${XDISPLAY_UNDER_TEST:-$project_root/scripts/xdisplay.sh}
fake_bin=$fixture_dir/bin
root=$fixture_dir/test-roots/open
fixture=$fixture_dir/single.xrandr
runtime=$(mktemp -d "${TMPDIR:-/tmp}/innogpu-xdisplay-watch.XXXXXX")
user_id=$(id -u)
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

start_watch() {
	display=$1
	mode=$2
	log=$3
	calls=$4
	PATH=$fake_bin:$PATH DISPLAY=$display XDG_RUNTIME_DIR=$runtime \
	XDISPLAY_TEST_MODE=1 XDISPLAY_TEST_ROOT=$root \
	FAKE_XRANDR_MODE=$mode FAKE_XRANDR_FIXTURE=$fixture \
	FAKE_XRANDR_CALLS=$calls "$xdisplay" --watch >"$log" 2>&1 &
	WATCH_PID=$!
}

wait_for_file() {
	file=$1
	i=0
	while [ ! -s "$file" ] && [ "$i" -lt 50 ]; do
		sleep 0.1
		i=$((i + 1))
	done
	[ -s "$file" ]
}

stop_watch() {
	pid=$1
	signal=$2
	kill -"$signal" "$pid" 2>/dev/null || :
	set +e
	wait "$pid" 2>/dev/null
	set -e
}

for spec in '20 HUP' '22 TERM'; do
	set -- $spec
	display=:$1
	signal=$2
	key=_$1
	generation=$runtime/xdisplay-$user_id-$key.generation
	watch_lock=$runtime/xdisplay-$user_id-$key.watch.lock
	start_watch "$display" fixture "$runtime/signal-$signal.log" "$runtime/signal-$signal.calls"
	pid=$WATCH_PID
	wait_for_file "$generation" || fail "generation missing before $signal"
	stop_watch "$pid" "$signal"
	[ ! -e "$generation" ] || fail "$signal left generation behind"
	flock -n "$watch_lock" true || fail "$signal did not release watch lock"
done

# A POSIX shell may inherit SIGINT as ignored when its caller starts it with
# `&`. GNU timeout starts the command in the foreground, so this separately
# verifies the INT trap without that inherited process-state limitation.
generation=$runtime/xdisplay-$user_id-_21.generation
watch_lock=$runtime/xdisplay-$user_id-_21.watch.lock
set +e
PATH=$fake_bin:$PATH DISPLAY=:21 XDG_RUNTIME_DIR=$runtime \
XDISPLAY_TEST_MODE=1 XDISPLAY_TEST_ROOT=$root \
FAKE_XRANDR_MODE=fixture FAKE_XRANDR_FIXTURE=$fixture \
FAKE_XRANDR_CALLS=$runtime/signal-INT.calls \
timeout --preserve-status -s INT 1 "$xdisplay" --watch \
    >"$runtime/signal-INT.log" 2>&1
int_result=$?
set -e
[ "$int_result" -ne 0 ] || fail 'INT watcher returned success'
[ ! -e "$generation" ] || fail 'INT left generation behind'
flock -n "$watch_lock" true || fail 'INT did not release watch lock'
pass 'HUP, INT and TERM clean generation and release watch locks'

display=:23
key=_23
generation=$runtime/xdisplay-$user_id-$key.generation
first_log=$runtime/duplicate-first.log
second_log=$runtime/duplicate-second.log
start_watch "$display" fixture "$first_log" "$runtime/duplicate-first.calls"
first_pid=$WATCH_PID
wait_for_file "$generation" || fail 'first duplicate watcher did not start'
start_time=$(date +%s)
set +e
PATH=$fake_bin:$PATH DISPLAY=:23.0 XDG_RUNTIME_DIR=$runtime \
XDISPLAY_TEST_MODE=1 XDISPLAY_TEST_ROOT=$root \
FAKE_XRANDR_MODE=fixture FAKE_XRANDR_FIXTURE=$fixture \
FAKE_XRANDR_CALLS=$runtime/duplicate-second.calls \
"$xdisplay" --watch >"$second_log" 2>&1
second_result=$?
set -e
elapsed=$(($(date +%s) - start_time))
[ "$second_result" -eq 0 ] || fail 'duplicate watcher returned failure'
[ "$elapsed" -ge 7 ] && [ "$elapsed" -le 10 ] ||
	fail "duplicate watcher wait was ${elapsed}s"
grep -F 'watcher is already running' "$second_log" >/dev/null ||
	fail 'duplicate watcher did not report existing instance'
stop_watch "$first_pid" TERM
pass 'duplicate watcher waits for the bounded lock window'

start_watch :24 fixture "$runtime/server-24.log" "$runtime/server-24.calls"
pid_24=$WATCH_PID
start_watch :25 fixture "$runtime/server-25.log" "$runtime/server-25.calls"
pid_25=$WATCH_PID
wait_for_file "$runtime/xdisplay-$user_id-_24.generation" || fail ':24 watcher missing'
wait_for_file "$runtime/xdisplay-$user_id-_25.generation" || fail ':25 watcher missing'
kill -0 "$pid_24" 2>/dev/null || fail ':24 watcher exited unexpectedly'
kill -0 "$pid_25" 2>/dev/null || fail ':25 watcher exited unexpectedly'
stop_watch "$pid_24" TERM
stop_watch "$pid_25" TERM
pass 'different X servers hold independent watcher locks'

generation=$runtime/xdisplay-$user_id-_26.generation
start_watch :26 fail "$runtime/handoff-old.log" "$runtime/handoff-old.calls"
old_pid=$WATCH_PID
wait_for_file "$generation" || fail 'handoff old watcher did not start'
start_watch :26.0 fixture "$runtime/handoff-new.log" "$runtime/handoff-new.calls"
new_pid=$WATCH_PID
set +e
wait "$old_pid"
old_result=$?
set -e
[ "$old_result" -ne 0 ] || fail 'failing old watcher returned success'
wait_for_file "$generation" || fail 'new watcher did not acquire handed-off lock'
kill -0 "$new_pid" 2>/dev/null || fail 'new watcher exited during handoff'
grep -F 'watcher is already running' "$runtime/handoff-new.log" >/dev/null &&
	fail 'new watcher timed out instead of acquiring released lock'
stop_watch "$new_pid" TERM
[ ! -e "$generation" ] || fail 'handoff watcher left generation behind'
pass 'new watcher acquires lock after old X server failure exit'

printf 'PASS: %s watcher lifecycle tests\n' "$tests"
