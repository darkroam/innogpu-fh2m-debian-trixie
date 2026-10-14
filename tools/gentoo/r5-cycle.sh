#!/bin/bash
# One deep suspend, woken by the RTC. Does not write pm_test.
set -euo pipefail
umask 022
script=$(readlink -f "$0")
root=$(cd "$(dirname "$script")/../.." && pwd)
observe=$root/tools/gentoo/r5-observe.sh
evidence=${GENTOO_R5_EVIDENCE:-$root/.build/r57-r5-observe}
seconds=${1:-45}

if [[ ! $seconds =~ ^[0-9]+$ || $seconds -lt 20 || $seconds -gt 180 ]]; then
    echo "usage: $0 [20-180]" >&2
    exit 2
fi
if [[ $(id -u) -ne 0 ]]; then
    mkdir -p "$evidence"
    printf '%s uid=%s sudo_exec=1 cycle_seconds=%s\n' "$(date -Iseconds)" "$(id -u)" "$seconds" >>"$evidence/launcher.log"
    exec sudo -- "$script" "$seconds"
fi

command -v rtcwake >/dev/null || { echo "rtcwake missing" >&2; exit 1; }
pm_test=$(cat /sys/power/pm_test 2>/dev/null || echo missing)
[[ $pm_test == *"[none]"* || $pm_test == none ]] || { echo "pm_test is not none: $pm_test" >&2; exit 1; }
[[ $(cat /sys/power/mem_sleep) == *'[deep]'* ]] || { echo "mem_sleep is not deep" >&2; exit 1; }

stamp=$(date +%Y%m%d-%H%M%S)
dir=$evidence/cycle-$stamp
mkdir -p "$dir"
printf '%s\n' "$dir" >"$evidence/latest-cycle.txt"

dump_inputs() {
    echo "when=$(date -Iseconds)"
    echo "rtc_date=$(cat /sys/class/rtc/rtc0/date 2>/dev/null || echo missing)"
    echo "rtc_time=$(cat /sys/class/rtc/rtc0/time 2>/dev/null || echo missing)"
    echo "--- input by-id ---"
    ls -l /dev/input/by-id 2>/dev/null || true
    echo "--- usb ---"
    local dev
    for dev in /sys/bus/usb/devices/*; do
        [[ -f $dev/idVendor && -f $dev/idProduct ]] || continue
        printf '%s %s:%s %s\n' "${dev##*/}" "$(cat "$dev/idVendor")" "$(cat "$dev/idProduct")" "$(cat "$dev/product" 2>/dev/null || true)"
    done
}

"$observe" before | tee "$dir/before.out"
dump_inputs >"$dir/inputs-before.txt"
{
    echo "rtcwake_start=$(date -Iseconds)"
    echo "seconds=$seconds"
    echo "mode=mem"
} | tee "$dir/rtcwake.txt"
set +e
rtcwake -m mem -s "$seconds" >>"$dir/rtcwake.txt" 2>&1
wake_rc=$?
set -e
echo "rtcwake_rc=$wake_rc when=$(date -Iseconds)" | tee -a "$dir/rtcwake.txt"
dump_inputs >"$dir/inputs-after.txt"
set +e
"$observe" after | tee "$dir/after.out"
after_rc=$?
set -e
echo "cycle_dir=$dir"
if [[ $wake_rc -ne 0 ]]; then
    exit "$wake_rc"
fi
exit "$after_rc"
