#!/bin/bash

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CHECK="$ROOT/tools/check-hygon-xhci-resume-fix.sh"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
PCI="$TMP/sys/bus/pci/devices/0000:06:00.1"
mkdir -p "$PCI"
printf '0x1d94\n' >"$PCI/vendor"
printf '0x148c\n' >"$PCI/device"

printf '%s\n' 'xhci_hcd 0000:06:00.1: hcc params 0x1 quirks 0x0000000000000090' >"$TMP/pass.log"
"$CHECK" --sysfs-root "$TMP/sys" --kernel-log "$TMP/pass.log" | grep -q '=PASS '

printf '%s\n' 'xhci_hcd 0000:06:00.1: hcc params 0x1 quirks 0x0000000000000010' >"$TMP/fail.log"
set +e
"$CHECK" --sysfs-root "$TMP/sys" --kernel-log "$TMP/fail.log" >"$TMP/out"
rc=$?
set -e
[[ "$rc" == 1 ]] && grep -q '=FAIL ' "$TMP/out"

set +e
"$CHECK" --sysfs-root "$TMP/sys" --kernel-log /dev/null >"$TMP/out"
rc=$?
set -e
[[ "$rc" == 3 ]] && grep -q '=UNVERIFIED ' "$TMP/out"

printf '0x1234\n' >"$PCI/device"
"$CHECK" --sysfs-root "$TMP/sys" --kernel-log /dev/null | grep -q '=NOT_APPLICABLE '

printf 'hygon_xhci_resume_fix_tests=PASS\n'
printf 'tests_total=4 tests_passed=4 tests_failed=0 tests_skipped=0\n'
