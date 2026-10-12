#!/bin/bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
bash -n "$root/tools/gentoo-fh2m.sh"
bash -n "$root/tools/gentoo-install-kernel-patches.sh"
bash -n "$root/tools/gentoo-check-kernel-patches.sh"
"$root/tools/gentoo-fh2m.sh" check --offline

work=$(mktemp -d)
fix=$(mktemp)
out=$(mktemp)
trap 'rm -rf "$work" "$fix" "$out"' EXIT
DESTDIR=$work/dest "$root/tools/gentoo-install-kernel-patches.sh" >/dev/null
PORTAGE_PATCH_BASE=$work/dest/etc/portage/patches/sys-kernel "$root/tools/gentoo-check-kernel-patches.sh" --files-only >/dev/null
if PORTAGE_PATCH_BASE=$work/missing "$root/tools/gentoo-check-kernel-patches.sh" --files-only >"$out" 2>&1; then
    echo "absent portage patches were accepted" >&2
    exit 1
fi
grep -q 'portage_patches=ABSENT' "$out"
cat > "$fix" << 'EOF'
Server Errors:   0
HWR Event Count: 0
CRR Event Count: 0
SLR Event Count: 0
WGP Error Count: 0
TRP Error Count: 0
FWF Event Count: 0
APM Event Count: 0
EOF
GENTOO_COUNTERS_ONLY=1 GENTOO_STATUS_FILE=$fix "$root/tools/gentoo-fh2m.sh" verify >/dev/null
sed -i '/^APM /d' "$fix"
if GENTOO_COUNTERS_ONLY=1 GENTOO_STATUS_FILE=$fix "$root/tools/gentoo-fh2m.sh" verify >"$out" 2>&1; then
    echo "missing row was accepted" >&2
    exit 1
fi
grep -q counters_rows_missing "$out"
printf '%s\n' 'Server Errors:   1' 'HWR Event Count: 0' 'CRR Event Count: 0' 'SLR Event Count: 0' \
    'WGP Error Count: 0' 'TRP Error Count: 0' 'FWF Event Count: 0' 'APM Event Count: 0' > "$fix"
if GENTOO_COUNTERS_ONLY=1 GENTOO_STATUS_FILE=$fix "$root/tools/gentoo-fh2m.sh" verify >"$out" 2>&1; then
    echo "nonzero counter was accepted" >&2
    exit 1
fi
if grep -q counters_rows_missing "$out"; then
    echo "nonzero reported missing rows" >&2
    exit 1
fi

bash -n "$root/tools/gentoo-r5-observe.sh"
bash -n "$root/tools/gentoo-r5-cycle.sh"
if "$root/tools/gentoo-r5-cycle.sh" 5 >"$out" 2>&1; then
    echo "short wake was accepted" >&2
    exit 1
fi
grep -q "usage:" "$out"
fixdir=$(mktemp -d)
trap 'rm -rf "$work" "$fix" "$out" "$fixdir"' EXIT
printf '%s\n' \
    'boot_id=abc' 'mem_sleep=s2idle [deep]' 'pm_test=[none]' 'card0=yes' \
    'counter_missing=0' 'counter_bad=0' 'suspend_entry=1' 'suspend_exit=1' \
    'temp_work=0' 'pll=0' 'oops=0' >"$fixdir/before"
sed -e 's/suspend_entry=1/suspend_entry=2/' -e 's/suspend_exit=1/suspend_exit=2/' \
    "$fixdir/before" >"$fixdir/after"
got=$("$root/tools/gentoo-r5-observe.sh" --judge "$fixdir/before" "$fixdir/after")
[[ $got == r5_judge=RECORDED_OBSERVATION_ABSENT ]]
if "$root/tools/gentoo-r5-observe.sh" --judge "$fixdir/before" "$fixdir/before" >"$out"; then
    echo "unchanged cycle was accepted" >&2
    exit 1
fi
grep -q 'reason=no_new_cycle' "$out"
sed -e 's/^pm_test=.*/pm_test=[none] core processors platform devices freezer/' \
    -e 's/suspend_entry=1/suspend_entry=0/' -e 's/suspend_exit=1/suspend_exit=0/' \
    "$fixdir/before" >"$fixdir/before-real"
sed -e 's/suspend_entry=0/suspend_entry=1/' -e 's/suspend_exit=0/suspend_exit=1/' \
    "$fixdir/before-real" >"$fixdir/after-real"
got=$("$root/tools/gentoo-r5-observe.sh" --judge "$fixdir/before-real" "$fixdir/after-real")
[[ $got == r5_judge=RECORDED_OBSERVATION_ABSENT ]]
