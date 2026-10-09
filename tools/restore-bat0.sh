#!/usr/bin/env bash
# Recover ACPI BAT0 without reboot, PM, or module unload.
set -u -o pipefail
umask 022

SCRIPT_PATH=$(readlink -f "$0")
ROOT=$(cd "$(dirname "$SCRIPT_PATH")/.." && pwd)
STAMP=$(date +%Y%m%d-%H%M%S)
EVIDENCE_ROOT=${BAT0_RECOVERY_ROOT:-$ROOT/.build/bat0-recovery}
LOGDIR=${BAT0_RECOVERY_LOG_DIR:-$EVIDENCE_ROOT/run-$STAMP}
DRIVER=/sys/bus/acpi/drivers/battery

if [[ $(id -u) -ne 0 ]]; then
    mkdir -p "$EVIDENCE_ROOT" 2>/dev/null || true
    {
        printf '%s\n' '--- pre-sudo ---'
        date --iso-8601=ns
        id
        tty || true
        printf 'sudo_exec=1\n'
    } >>"$EVIDENCE_ROOT/launcher.log" 2>&1
    exec sudo -- "$SCRIPT_PATH" "$@"
fi

mkdir -p "$LOGDIR" || exit 1
printf '%s\n' "$LOGDIR" >"$EVIDENCE_ROOT/latest-run.txt"
exec > >(tee "$LOGDIR/console.log") 2>&1

log() {
    printf '[%s] %s\n' "$(date --iso-8601=ns)" "$*" | tee -a "$LOGDIR/summary.log"
}

bat0_present() {
    [[ -d /sys/class/power_supply/BAT0 ]]
}

snapshot() {
    local name=$1
    {
        printf '%s\n' "--- $name ---"
        date --iso-8601=ns
        id
        uname -a
        printf '%s\n' '[power_supply]'
        for supply in /sys/class/power_supply/*; do
            [[ -d "$supply" ]] || continue
            printf '%s ' "$supply"
            for field in online present status capacity type; do
                [[ -r "$supply/$field" ]] && printf '%s=%s ' "$field" "$(cat "$supply/$field")"
            done
            printf '\n'
        done
        printf '%s\n' '[acpi_battery_devices]'
        local count=0 device_path device driver_link
        for device_path in /sys/bus/acpi/devices/PNP0C0A:*; do
            [[ -d "$device_path" ]] || continue
            count=$((count + 1))
            device=${device_path##*/}
            driver_link=$(readlink "$device_path/driver" 2>/dev/null || printf 'none')
            printf 'device=%s status=%s driver=%s\n' "$device" \
                "$(cat "$device_path/status" 2>/dev/null || printf unknown)" "$driver_link"
        done
        printf 'count=%s\n' "$count"
        printf '%s\n' '[modules]'
        lsmod | awk '$1 == "battery" || $1 == "ac"'
    } >"$LOGDIR/${name}.txt" 2>&1
}

write_result() {
    local result=$1
    {
        printf 'result=%s\n' "$result"
        printf 'timestamp=%s\n' "$(date --iso-8601=ns)"
        printf 'logdir=%s\n' "$LOGDIR"
        printf 'uid=%s\n' "$(id -u)"
        printf 'kernel=%s\n' "$(uname -r)"
        if bat0_present; then
            printf 'bat0=present\n'
            [[ -r /sys/class/power_supply/BAT0/capacity ]] && \
                printf 'capacity=%s\n' "$(cat /sys/class/power_supply/BAT0/capacity)"
        else
            printf 'bat0=absent\n'
        fi
    } >"$LOGDIR/result.txt"
}

run_step() {
    local name=$1
    shift
    "$@" >"$LOGDIR/$name.out" 2>&1
    local rc=$?
    printf '%s rc=%s\n' "$name" "$rc" >>"$LOGDIR/steps.log"
    log "$name rc=$rc"
    return "$rc"
}

unbind_device() {
    local device=$1
    printf '%s' "$device" >"$DRIVER/unbind"
}

bind_device() {
    local device=$1
    printf '%s' "$device" >"$DRIVER/bind"
}

wait_for_bat0() {
    local attempt
    for ((attempt = 1; attempt <= 20; attempt++)); do
        bat0_present && return 0
        log "BAT0 probe attempt=$attempt/20"
        sleep 0.25
    done
    return 1
}

log "BAT0 recovery start logdir=$LOGDIR"
snapshot before

if [[ ! -d "$DRIVER" ]]; then
    log "recovery=FAIL battery driver path missing: $DRIVER"
    write_result NO_BATTERY_DRIVER
    exit 3
fi

devices=()
for device_path in /sys/bus/acpi/devices/PNP0C0A:*; do
    [[ -d "$device_path" ]] || continue
    devices+=("${device_path##*/}")
done

if [[ ${#devices[@]} -eq 0 ]]; then
    log "recovery=FAIL no ACPI PNP0C0A device exposed"
    write_result NO_ACPI_BATTERY_DEVICE
    exit 3
fi

log "ACPI battery devices: ${devices[*]}"
for device in "${devices[@]}"; do
    if [[ -e "$DRIVER/$device" ]]; then
        run_step "unbind-$device" unbind_device "$device" || true
    else
        log "unbind-$device skipped (not currently bound)"
    fi
done

sleep 1
for device in "${devices[@]}"; do
    run_step "bind-$device" bind_device "$device" || true
done

if ! wait_for_bat0; then
    log "BAT0 still absent; requesting non-destructive battery module probe"
    run_step modprobe-battery modprobe battery || true
    wait_for_bat0 || true
fi

snapshot final
if bat0_present; then
    log "recovery=PASS BAT0 restored"
    write_result RECOVERY_PASS
    printf 'BAT0 recovery succeeded. Evidence: %s\n' "$LOGDIR"
    exit 0
fi

log "recovery=FAIL BAT0 was not restored; ACPI/EC did not expose a battery object"
write_result RECOVERY_FAIL
printf 'BAT0 recovery failed. Evidence: %s\n' "$LOGDIR"
exit 3
