#!/bin/bash
# Record one deep suspend/resume. Does not suspend and does not write pm_test.
set -euo pipefail
umask 022
script=$(readlink -f "$0")
root=$(cd "$(dirname "$script")/../.." && pwd)
evidence=${GENTOO_R5_EVIDENCE:-$root/.build/r57-r5-observe}
phase=${1:-}

judge() {
    local before=$1 after=$2 key
    declare -A b a
    while IFS='=' read -r key value; do
        [[ -n $key ]] || continue
        b[$key]=$value
    done <"$before"
    while IFS='=' read -r key value; do
        [[ -n $key ]] || continue
        a[$key]=$value
    done <"$after"
    if [[ ${a[boot_id]:-} != "${b[boot_id]:-}" ]]; then
        echo "r5_judge=FAIL reason=boot_id_changed"
        return 1
    fi
    if [[ ${a[pm_test]:-} != *"[none]"* && ${a[pm_test]:-} != none ]]; then
        echo "r5_judge=FAIL reason=pm_test"
        return 1
    fi
    if [[ ${a[mem_sleep]:-} != *'[deep]'* ]]; then
        echo "r5_judge=FAIL reason=not_deep"
        return 1
    fi
    if (( ${a[suspend_entry]:-0} <= ${b[suspend_entry]:-0} || ${a[suspend_exit]:-0} <= ${b[suspend_exit]:-0} )); then
        echo "r5_judge=FAIL reason=no_new_cycle"
        return 1
    fi
    if [[ ${a[card0]:-} != yes || ${a[counter_missing]:-1} != 0 || ${a[counter_bad]:-1} != 0 || ${a[oops]:-1} != 0 ]]; then
        echo "r5_judge=FAIL reason=driver"
        return 1
    fi
    if (( ${a[temp_work]:-0} > ${b[temp_work]:-0} && ${a[pll]:-0} > ${b[pll]:-0} )); then
        echo "r5_judge=OBSERVED"
        return 0
    fi
    echo "r5_judge=RECORDED_OBSERVATION_ABSENT"
    return 0
}

if [[ $phase == --judge ]]; then
    judge "$2" "$3"
    exit
fi
if [[ $phase != before && $phase != after ]]; then
    echo "Usage: $0 before|after" >&2
    exit 2
fi
if [[ $(id -u) -ne 0 ]]; then
    mkdir -p "$evidence"
    printf '%s uid=%s sudo_exec=1\n' "$(date -Iseconds)" "$(id -u)" >>"$evidence/launcher.log"
    exec sudo -- "$script" "$phase"
fi

run=$(date +%Y%m%d-%H%M%S)
logdir=$evidence/run-$run-$phase
mkdir -p "$logdir"
printf '%s\n' "$logdir" >"$evidence/latest-run.txt"

count_journal() {
    journalctl -b -k --no-pager -o cat | grep -c -E -- "$1" || true
}

status=/proc/driver/fantgpu/gpu00/status
counter_missing=8
counter_bad=0
if [[ -f $status ]]; then
    eval "$(awk '
        BEGIN { split("Server HWR CRR SLR WGP TRP FWF APM", names, " ") }
        $1=="Server" || $1=="HWR" || $1=="CRR" || $1=="SLR" || $1=="WGP" || $1=="TRP" || $1=="FWF" || $1=="APM" {
            seen[$1] = 1
            if ($NF != 0) bad++
        }
        END {
            missing = 0
            for (i = 1; i <= 8; i++) if (!seen[names[i]]) missing++
            printf "counter_missing=%d\ncounter_bad=%d\n", missing, bad + 0
        }
    ' "$status")"
fi

quirks=
for dev in /sys/bus/pci/devices/*; do
    [[ -f $dev/vendor && -f $dev/device ]] || continue
    [[ $(cat "$dev/vendor") == 0x1d94 && $(cat "$dev/device") == 0x148c ]] || continue
    if [[ -f $dev/quirks ]]; then
        quirks+="${dev##*/}:$(cat "$dev/quirks") "
    else
        quirks+="${dev##*/}:ABSENT "
    fi
done
quirks=${quirks:-NONE}

{
    echo "phase=$phase"
    echo "when=$(date -Iseconds)"
    echo "boot_id=$(cat /proc/sys/kernel/random/boot_id)"
    echo "uname=$(uname -r)"
    echo "mem_sleep=$(cat /sys/power/mem_sleep)"
    echo "pm_test=$(cat /sys/power/pm_test 2>/dev/null || echo missing)"
    echo "srcversion=$(cat /sys/module/fantgpu/srcversion 2>/dev/null || echo missing)"
    if [[ -e /dev/dri/card0 ]]; then echo "card0=yes"; else echo "card0=no"; fi
    echo "counter_missing=$counter_missing"
    echo "counter_bad=$counter_bad"
    echo "suspend_entry=$(count_journal 'suspend entry')"
    echo "suspend_exit=$(count_journal 'suspend exit')"
    echo "temp_work=$(count_journal 'hal_temperature_monitor_work')"
    echo "pll=$(count_journal 'recover gpu pll')"
    echo "oops=$(count_journal 'Oops|BUG:')"
    echo "bat0=$(cat /sys/class/power_supply/BAT0/status 2>/dev/null || echo missing)"
    echo "xhci_148c=$quirks"
} >"$logdir/snapshot.txt"
cp -a "$status" "$logdir/status.txt" 2>/dev/null || true
install -m 0644 "$logdir/snapshot.txt" "$evidence/$phase.txt"

if [[ $phase == before ]]; then
    echo "r5_judge=BEFORE_RECORDED"
    echo "next=systemctl suspend, then: $script after"
    cp -a "$logdir/snapshot.txt" "$evidence/before.txt"
    exit 0
fi

[[ -f $evidence/before.txt ]] || { echo "r5_judge=FAIL reason=no_before" | tee "$logdir/result.txt"; exit 1; }
set +e
verdict=$(judge "$evidence/before.txt" "$logdir/snapshot.txt")
rc=$?
set -e
printf '%s\n' "$verdict" | tee "$logdir/result.txt"
if [[ $verdict == r5_judge=RECORDED_OBSERVATION_ABSENT ]]; then
    echo "temp_work and recover gpu pll did not increase. This is not a pass."
fi
exit "$rc"
