#!/usr/bin/env bash
# R51 task 2: recover BAT0 without reboot, then capture one AC transition.
set -u
umask 022

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
STAMP=$(date +%Y%m%d-%H%M%S)
EVIDENCE_ROOT="$ROOT/.build/r51-battery-observation-20261002-01"
LOGDIR=${R51_LOG_DIR:-$EVIDENCE_ROOT/task2-run-${STAMP}}
OBSERVE_SECONDS=${R51_OBSERVE_SECONDS:-60}
DEVICE=PNP0C0A:00
DRIVER=/sys/bus/acpi/drivers/battery
LAUNCH_ROOT="$ROOT/.build/r51-battery-observation-20261002-01"

if [[ $(id -u) -ne 0 ]]; then
    mkdir -p "$LAUNCH_ROOT" 2>/dev/null || true
    {
        printf '%s\n' '--- pre-sudo ---'
        date --iso-8601=ns
        id
        tty || true
        printf 'sudo_exec=1\n'
    } >>"$LAUNCH_ROOT/launcher.log" 2>&1
    exec sudo -- "$0" "$@"
fi

{
    printf '%s\n' '--- post-sudo ---'
    date --iso-8601=ns
    id
    tty || true
    printf 'sudo_root=1\n'
} >>"$LAUNCH_ROOT/launcher.log" 2>&1

mkdir -p "$LOGDIR" || exit 1
printf '%s\n' "$LOGDIR" >"$EVIDENCE_ROOT/latest-run.txt"
exec > >(tee "$LOGDIR/console.log") 2>&1

log() {
    printf '[%s] %s\n' "$(date --iso-8601=ns)" "$*" | tee -a "$LOGDIR/summary.log"
}

write_result() {
    local result=$1
    {
        printf 'result=%s\n' "$result"
        printf 'timestamp=%s\n' "$(date --iso-8601=ns)"
        printf 'logdir=%s\n' "$LOGDIR"
        printf 'uid=%s\n' "$(id -u)"
        printf 'kernel=%s\n' "$(uname -r)"
        if [[ -d /sys/class/power_supply/BAT0 ]]; then
            printf 'bat0=present\n'
            [[ -r /sys/class/power_supply/BAT0/capacity ]] && \
                printf 'capacity=%s\n' "$(cat /sys/class/power_supply/BAT0/capacity)"
        else
            printf 'bat0=absent\n'
        fi
    } >"$LOGDIR/result.txt"
}

snapshot() {
    local name=$1
    {
        printf '%s\n' "--- $name ---"
        date --iso-8601=ns
        id
        uname -a
        printf '%s\n' '[power_supply]'
        for p in /sys/class/power_supply/*; do
            [[ -d "$p" ]] || continue
            printf '%s ' "$p"
            for f in online present status capacity type; do
                [[ -r "$p/$f" ]] && printf '%s=%s ' "$f" "$(cat "$p/$f")"
            done
            echo
        done
        printf '%s\n' '[acpi]'
        if [[ -e "/sys/bus/acpi/devices/$DEVICE/status" ]]; then
            printf 'device=%s status=%s driver=%s\n' "$DEVICE" \
                "$(cat "/sys/bus/acpi/devices/$DEVICE/status")" \
                "$(readlink "/sys/bus/acpi/devices/$DEVICE/driver" 2>/dev/null || echo none)"
        fi
        printf '%s\n' '[modules]'
        lsmod | awk '$1 == "battery" || $1 == "ac"'
    } >"$LOGDIR/${name}.txt" 2>&1
}

cleanup() {
    local pid
    for pid in "${UDEV_PID:-}" "${JOURNAL_PID:-}" "${POLL_PID:-}"; do
        [[ -n "$pid" ]] && kill "$pid" 2>/dev/null || true
    done
}
trap cleanup EXIT INT TERM

log "R51 task2 start logdir=$LOGDIR"
snapshot before

if [[ -r /sys/firmware/acpi/tables/DSDT ]]; then
    cp /sys/firmware/acpi/tables/DSDT "$LOGDIR/DSDT.bin" \
        >"$LOGDIR/dsdt-copy.log" 2>&1
    DSDT_RC=$?
    log "dsdt_copy_rc=$DSDT_RC"
    if [[ $DSDT_RC -eq 0 ]] && command -v iasl >/dev/null 2>&1; then
        iasl -d "$LOGDIR/DSDT.bin" >"$LOGDIR/iasl.log" 2>&1
        log "iasl_rc=$?"
    fi
else
    log "dsdt_copy_rc=1 (unreadable)"
fi

if [[ ! -d /sys/class/power_supply/BAT0 ]]; then
    log "BAT0 missing; attempting ACPI battery unbind/bind"
    printf '%s' "$DEVICE" >"$DRIVER/unbind" 2>"$LOGDIR/unbind.err"
    UNBIND_RC=$?
    log "unbind_rc=$UNBIND_RC"

    if [[ $UNBIND_RC -eq 0 ]]; then
        sleep 1
        printf '%s' "$DEVICE" >"$DRIVER/bind" 2>"$LOGDIR/bind.err"
        BIND_RC=$?
        log "bind_rc=$BIND_RC"
    else
        BIND_RC=1
    fi

    sleep 2
    snapshot after-bind
fi

if [[ -d /sys/class/power_supply/BAT0 && -r /sys/class/power_supply/BAT0/capacity ]]; then
    log "recovery=PASS BAT0 capacity=$(cat /sys/class/power_supply/BAT0/capacity)"
else
    log "recovery=FAIL BAT0 not restored; no unplug/replug test started"
    snapshot final
    write_result RECOVERY_FAIL
    exit 3
fi

log "starting udev, journal, and 100ms sysfs monitors"
timeout "$OBSERVE_SECONDS" udevadm monitor --kernel --udev --property \
    --subsystem-match=power_supply >"$LOGDIR/udevadm.log" 2>&1 &
UDEV_PID=$!
timeout "$OBSERVE_SECONDS" journalctl -kf -o short-monotonic \
    >"$LOGDIR/journal-kernel.log" 2>&1 &
JOURNAL_PID=$!
(
    while :; do
        printf '%s\n' "--- $(date --iso-8601=ns) ---"
        for p in /sys/class/power_supply/*; do
            [[ -d "$p" ]] || continue
            printf '%s ' "$p"
            for f in online present status capacity; do
                [[ -r "$p/$f" ]] && printf '%s=%s ' "$f" "$(cat "$p/$f")"
            done
            echo
        done
        sleep 0.1
    done
) >"$LOGDIR/sysfs.log" 2>&1 &
POLL_PID=$!

printf '\n恢复成功。现在拔掉电源线，等状态栏电池消失后按 Enter。\n'
read -r _
log "user_unplug_confirmed"
snapshot unplug-confirmed

printf '现在插回电源线，保持系统运行 20 秒后按 Enter。\n'
read -r _
log "user_plug_confirmed"
snapshot plug-confirmed
sleep 20

cleanup
snapshot final
log "observation_complete"
write_result OBSERVATION_COMPLETE
printf '\nR51 log directory: %s\n' "$LOGDIR"
printf 'Review summary.log, sysfs.log, udevadm.log, journal-kernel.log, and DSDT files.\n'
