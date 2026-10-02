#!/usr/bin/env bash
# Verify the installed R51 battery/xHCI modules with one AC unplug/replug cycle.
set -u -o pipefail
umask 022

SCRIPT=$(readlink -f "$0")
ROOT=$(cd "$(dirname "$SCRIPT")/.." && pwd)
EVIDENCE_ROOT="$ROOT/.build/r51-module-deploy-20261002-10/runtime"
STAMP=$(date +%Y%m%d-%H%M%S)
LOGDIR="$EVIDENCE_ROOT/run-$STAMP"
KERNEL=6.12.107+deb13-amd64

if [[ $(id -u) -ne 0 ]]; then
    mkdir -p "$EVIDENCE_ROOT"
    printf '[%s] launcher_uid=%s tty=%s\n' "$(date --iso-8601=ns)" \
        "$(id -u)" "$(tty 2>/dev/null || printf unknown)" >>"$EVIDENCE_ROOT/launcher.log"
    exec sudo -- "$SCRIPT"
fi

mkdir -p "$LOGDIR"
printf '%s\n' "$LOGDIR" >"$EVIDENCE_ROOT/latest-run.txt"
exec > >(tee "$LOGDIR/console.log") 2>&1

log() { printf '[%s] %s\n' "$(date --iso-8601=ns)" "$*"; }

cleanup() {
    local pid
    for pid in "${UDEV_PID:-}" "${JOURNAL_PID:-}" "${POLL_PID:-}"; do
        if [[ -n $pid ]]; then
            kill "$pid" 2>/dev/null || true
        fi
    done
}
trap cleanup EXIT INT TERM

snapshot() {
    local name=$1 supply field
    {
        date --iso-8601=ns
        for supply in /sys/class/power_supply/*; do
            [[ -d $supply ]] || continue
            printf '%s ' "${supply##*/}"
            for field in type online present status capacity; do
                [[ -r $supply/$field ]] && printf '%s=%s ' "$field" "$(cat "$supply/$field")"
            done
            printf '\n'
        done
    } >"$LOGDIR/$name.txt"
}

loaded_build_id() {
    od -An -tx1 -j16 -N20 "/sys/module/$1/notes/.note.gnu.build-id" 2>/dev/null | tr -d ' \n'
}

file_build_id() {
    readelf -n "$1" | awk '/Build ID:/ {print $3}'
}

fail() {
    log "VERIFY_FAIL $*"
    printf 'result=FAIL\nreason=%s\nlogdir=%s\n' "$*" "$LOGDIR" >"$LOGDIR/result.txt"
    exit 1
}

log "R51 task3 verification start logdir=$LOGDIR"
[[ $(uname -r) == "$KERNEL" ]] || fail "unexpected_kernel=$(uname -r)"

BATTERY_FILE=$(/usr/sbin/modinfo -k "$KERNEL" -n battery)
XHCI_FILE=$(/usr/sbin/modinfo -k "$KERNEL" -n xhci_pci)
[[ $BATTERY_FILE == "/lib/modules/$KERNEL/updates/r51/battery.ko" ]] || fail "wrong_battery_path=$BATTERY_FILE"
[[ $XHCI_FILE == "/lib/modules/$KERNEL/updates/r51/xhci-pci.ko" ]] || fail "wrong_xhci_path=$XHCI_FILE"
[[ $(loaded_build_id battery) == "$(file_build_id "$BATTERY_FILE")" ]] || fail 'loaded_battery_build_id_mismatch'
[[ $(loaded_build_id xhci_pci) == "$(file_build_id "$XHCI_FILE")" ]] || fail 'loaded_xhci_build_id_mismatch'

"$ROOT/scripts/check-hygon-xhci-resume-fix.sh" >"$LOGDIR/xhci-quirk.txt" || fail 'xhci_quirk_check_failed'
snapshot before
[[ -r /sys/class/power_supply/ADP1/online && $(cat /sys/class/power_supply/ADP1/online) == 1 ]] || \
    fail 'AC_not_online_at_start'
[[ -r /sys/class/power_supply/BAT0/capacity ]] || fail 'BAT0_missing_at_start'

timeout 90 udevadm monitor --kernel --udev --property --subsystem-match=power_supply \
    >"$LOGDIR/udevadm.log" 2>&1 &
UDEV_PID=$!
timeout 90 journalctl -kf -o short-monotonic >"$LOGDIR/journal-kernel.log" 2>&1 &
JOURNAL_PID=$!
(
    while :; do
        snapshot_line="$(date --iso-8601=ns)"
        for path in /sys/class/power_supply/*; do
            [[ -d $path ]] || continue
            snapshot_line+=" ${path##*/}"
            for attr in online present capacity; do
                [[ -r $path/$attr ]] && snapshot_line+=" $attr=$(cat "$path/$attr")"
            done
        done
        printf '%s\n' "$snapshot_line"
        sleep 0.1
    done
) >"$LOGDIR/sysfs-timeline.log" 2>&1 &
POLL_PID=$!

printf '\n请拔掉电源线；拔掉后按 Enter。\n'
read -r _
log 'user_unplug_confirmed'
sleep 5
snapshot after-unplug

printf '请插回电源线；插回后按 Enter。脚本随后自动观察 20 秒。\n'
read -r _
log 'user_replug_confirmed'
sleep 20
snapshot after-replug
cleanup

FINAL_CAPACITY=$(cat /sys/class/power_supply/BAT0/capacity 2>/dev/null || true)
FINAL_ONLINE=$(cat /sys/class/power_supply/ADP1/online 2>/dev/null || true)
[[ $FINAL_ONLINE == 1 ]] || fail "ADP1_not_online_final=$FINAL_ONLINE"
[[ $FINAL_CAPACITY =~ ^[0-9]+$ ]] || fail 'BAT0_not_automatically_restored'

ERROR_HITS=$(grep -Eic 'battery.*(error|fail)|ACPI.*battery.*(error|fail)' \
    "$LOGDIR/journal-kernel.log" 2>/dev/null || true)
{
    printf 'result=PASS\n'
    printf 'kernel=%s\n' "$KERNEL"
    printf 'battery_module=%s\n' "$BATTERY_FILE"
    printf 'xhci_module=%s\n' "$XHCI_FILE"
    printf 'final_ac_online=%s\n' "$FINAL_ONLINE"
    printf 'final_bat0_capacity=%s\n' "$FINAL_CAPACITY"
    printf 'kernel_battery_error_hits=%s\n' "$ERROR_HITS"
    printf 'logdir=%s\n' "$LOGDIR"
} >"$LOGDIR/result.txt"
log "VERIFY_PASS BAT0 capacity=$FINAL_CAPACITY AC_online=$FINAL_ONLINE kernel_battery_error_hits=$ERROR_HITS"
