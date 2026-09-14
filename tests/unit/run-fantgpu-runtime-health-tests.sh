#!/usr/bin/env bash
# Unit tests for the pre-R-item F display health gate.
# All inputs are synthetic; no module, device, or privileged log is touched.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
GATE="$ROOT/scripts/check-fantgpu-runtime-health.sh"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/fantgpu-health-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT INT TERM HUP

PASS=0
FAIL=0
ok() { PASS=$((PASS + 1)); echo "ok $1"; }
bad() { FAIL=$((FAIL + 1)); echo "BAD $1: $2" >&2; }

setup_fixture() {
    local name=$1
    local base="$TMP/$name"
    mkdir -p "$base/sys/class/drm/card0" "$base/dev/dri"
    : > "$base/dev/dri/card0"
    printf '%s\n' 'Driver Status:   OK' 'Firmware Status: OK' > "$base/status"
    printf '%s\n' '[  1.000] fantgpu: module loaded' > "$base/dmesg"
    printf '%s\n' "$base"
}

run_case() {
    local base=$1 out=$2
    set +e
    "$GATE" --dmesg "$base/dmesg" --status-file "$base/status" \
        --sysfs-root "$base/sys" --dev-root "$base/dev" > "$out" 2>&1
    RC=$?
    set -e
}

base=$(setup_fixture pass)
run_case "$base" "$TMP/pass.out"
if [[ $RC -eq 0 ]] && grep -Fq 'fantgpu_runtime_health=PASS' "$TMP/pass.out"; then
    ok t01_pass
else
    bad t01_pass "rc=$RC output=$(cat "$TMP/pass.out")"
fi

base=$(setup_fixture no-card)
rm -rf "$base/sys/class/drm/card0" "$base/dev/dri/card0"
run_case "$base" "$TMP/no-card.out"
if [[ $RC -eq 1 ]] && grep -Fq 'drm_sysfs_card_missing' "$TMP/no-card.out" &&
   grep -Fq 'drm_dev_card_missing' "$TMP/no-card.out"; then
    ok t02_card_required
else
    bad t02_card_required "rc=$RC output=$(cat "$TMP/no-card.out")"
fi

base=$(setup_fixture firmware-fail)
printf '%s\n' '[  2.000] fantgpu: firmware: failed to load fantgpu/fh2m/fh2m.fw (-2)' >> "$base/dmesg"
run_case "$base" "$TMP/firmware-fail.out"
if [[ $RC -eq 1 ]] && grep -Fq 'firmware_request_failed' "$TMP/firmware-fail.out"; then
    ok t03_firmware_failure_blocks
else
    bad t03_firmware_failure_blocks "rc=$RC output=$(cat "$TMP/firmware-fail.out")"
fi

base=$(setup_fixture optional-hwinfo)
printf '%s\n' '[  2.000] fantgpu: firmware: failed to load fantgpu/hwinfo_g0m.bin (-2)' >> "$base/dmesg"
run_case "$base" "$TMP/optional-hwinfo.out"
if [[ $RC -eq 0 ]] && grep -Fq 'optional_hwinfo_g0m=missing_allowed' "$TMP/optional-hwinfo.out"; then
    ok t04_optional_hwinfo_allowed
else
    bad t04_optional_hwinfo_allowed "rc=$RC output=$(cat "$TMP/optional-hwinfo.out")"
fi

base=$(setup_fixture oops)
printf '%s\n' '[  2.000] BUG: kernel NULL pointer dereference' >> "$base/dmesg"
run_case "$base" "$TMP/oops.out"
if [[ $RC -eq 1 ]] && grep -Fq 'kernel_fault' "$TMP/oops.out"; then
    ok t05_kernel_fault_blocks
else
    bad t05_kernel_fault_blocks "rc=$RC output=$(cat "$TMP/oops.out")"
fi

base=$(setup_fixture no-dmesg)
rm -f "$base/dmesg"
run_case "$base" "$TMP/no-dmesg.out"
if [[ $RC -eq 3 ]] && grep -Fq 'reason=dmesg_unavailable' "$TMP/no-dmesg.out"; then
    ok t06_dmesg_is_hard_prerequisite
else
    bad t06_dmesg_is_hard_prerequisite "rc=$RC output=$(cat "$TMP/no-dmesg.out")"
fi

base=$(setup_fixture no-status)
rm -f "$base/status"
run_case "$base" "$TMP/no-status.out"
if [[ $RC -eq 1 ]] && grep -Fq 'status_unavailable' "$TMP/no-status.out"; then
    ok t07_status_is_hard_prerequisite
else
    bad t07_status_is_hard_prerequisite "rc=$RC output=$(cat "$TMP/no-status.out")"
fi

echo "PASS=$PASS FAIL=$FAIL"
(( FAIL == 0 ))
