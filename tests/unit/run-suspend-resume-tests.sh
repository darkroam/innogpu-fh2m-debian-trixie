#!/bin/bash
# Static tests for patch-024/025 and current/legacy builder wiring.
# The suite copies two tracked source files to /tmp; it does not read local
# payload directories, build a package, install a module, or suspend the host.

set -u -o pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PATCH="$ROOT/patches/024-suspend-resume.patch"
DISPLAY_PATCH="$ROOT/patches/025-suspend-resume-display.patch"
BUILDER="$ROOT/scripts/build-deepin-coherent.sh"
WRAPPER="$ROOT/scripts/build-patched28-suspend-resume.sh"
CURRENT_BUILDER="$ROOT/scripts/build-innogpu-driver.sh"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/innogpu-suspend-resume-tests.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

tests=0
failures=0
pass() { tests=$((tests + 1)); printf 'suspend_resume_t%02d=PASS # %s\n' "$tests" "$1"; }
fail() { tests=$((tests + 1)); failures=$((failures + 1)); printf 'suspend_resume_t%02d=FAIL reason=%s\n' "$tests" "$1"; }

mkdir -p "$TMP/tree/innosrvkm"
cp "$ROOT/drivers/innosrvkm/pvr_dvfs_device.c" "$TMP/tree/innosrvkm/"
cp "$ROOT/drivers/innosrvkm/innodpu_drm_pm.c" "$TMP/tree/innosrvkm/"

if patch --dry-run -s -d "$TMP/tree" -p1 < "$PATCH"; then
    pass patch_dry_run
else
    fail patch_dry_run
fi

if patch -s -d "$TMP/tree" -p1 < "$PATCH"; then
    pass patch_applies
else
    fail patch_applies
fi

patched="$TMP/tree/innosrvkm/pvr_dvfs_device.c"
if python3 - "$patched" <<'PY'
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8")
enabled = text.index("if (!psDVFSDevice->bEnabled)")
power = text.index("PVRSRVDefaultDomainPower(psDeviceNode)", enabled)
opp = text.index("devfreq_recommended_opp", enabled)
preclock = text.index("PVRSRVDevicePreClockSpeedChange", enabled)
raise SystemExit(0 if enabled < power < opp < preclock else 1)
PY
then
    pass power_guard_precedes_opp_and_preclock
else
    fail power_guard_precedes_opp_and_preclock
fi

if sed -n '/PVRSRVDefaultDomainPower/,/^[[:space:]]*}/p' "$patched" |
    grep -Fq 'PVRSRV_SYS_POWER_STATE_ON' &&
   sed -n '/PVRSRVDefaultDomainPower/,/^[[:space:]]*}/p' "$patched" |
    grep -Fq '*requested_freq = psRGXTimingInfo->ui32CoreClockSpeed;' &&
   sed -n '/PVRSRVDefaultDomainPower/,/^[[:space:]]*}/p' "$patched" |
    grep -Fq 'return 0;'; then
    pass powered_off_request_keeps_current_frequency
else
    fail powered_off_request_keeps_current_frequency
fi

if [[ "$(grep -c '^diff -ruN ' "$PATCH")" -eq 1 ]] &&
   grep -Fq 'a/innosrvkm/pvr_dvfs_device.c b/innosrvkm/pvr_dvfs_device.c' "$PATCH"; then
    pass patch_scope_is_single_driver_file
else
    fail patch_scope_is_single_driver_file
fi

if patch --dry-run --batch --forward --fuzz=0 --no-backup-if-mismatch \
    -s -d "$TMP/tree" -p1 < "$DISPLAY_PATCH"; then
    pass display_patch_dry_run
else
    fail display_patch_dry_run
fi

if patch --batch --forward --fuzz=0 --no-backup-if-mismatch \
    -s -d "$TMP/tree" -p1 < "$DISPLAY_PATCH"; then
    pass display_patch_applies
else
    fail display_patch_applies
fi

display_patched="$TMP/tree/innosrvkm/innodpu_drm_pm.c"
if sed -n '/static int innodpu_drm_resume/,/^}/p' "$display_patched" |
       grep -Fq 'ret = innodpu_drm_wakeup(dev);' &&
   ! sed -n '/static int innodpu_drm_resume/,/^}/p' "$display_patched" |
       grep -Fq 'innodpu_pdp0_wakeup(crtc);'; then
    pass display_patch_removes_only_post_atomic_cursor_restore
else
    fail display_patch_removes_only_post_atomic_cursor_restore
fi

if [[ "$(grep -c '^diff -ruN ' "$DISPLAY_PATCH")" -eq 1 ]] &&
   grep -Fq 'a/innosrvkm/innodpu_drm_pm.c b/innosrvkm/innodpu_drm_pm.c' "$DISPLAY_PATCH" &&
   grep -Fq 'innodpu_pdp0_wakeup(crtc);' "$DISPLAY_PATCH"; then
    pass display_patch_scope_is_single_driver_file
else
    fail display_patch_scope_is_single_driver_file
fi

if grep -Fq '@@ -288,17 +288,11 @@' "$DISPLAY_PATCH" &&
   [[ "$(grep -c '^ ' "$DISPLAY_PATCH")" -ge 6 ]]; then
    pass display_patch_has_three_line_context
else
    fail display_patch_has_three_line_context
fi

if ! find "$TMP/tree" -type f \( -name '*.orig' -o -name '*.rej' \) -print -quit |
       grep -q .; then
    pass strict_patch_application_leaves_no_artifacts
else
    fail strict_patch_application_leaves_no_artifacts
fi

if bash -n "$BUILDER" "$WRAPPER" "$CURRENT_BUILDER"; then
    pass builder_shell_syntax
else
    fail builder_shell_syntax
fi

if grep -Fq 'APPLY_SUSPEND_RESUME_FIX=${APPLY_SUSPEND_RESUME_FIX:-0}' "$BUILDER" &&
   grep -Fq 'patches/024-suspend-resume.patch' "$BUILDER"; then
    pass coherent_builder_switch
else
    fail coherent_builder_switch
fi

if grep -Fq 'PATCH_VERSION=28' "$WRAPPER" &&
   grep -Fq 'APPLY_SUSPEND_RESUME_FIX=1' "$WRAPPER" &&
   grep -Fq 'APPLY_FOREIGN_DMABUF_LIFECYCLE_FIX=1' "$WRAPPER" &&
   grep -Fq 'patched-28.deb' "$WRAPPER"; then
    pass patched28_inherits_p27_and_enables_fix
else
    fail patched28_inherits_p27_and_enables_fix
fi

if grep -Fq 'VERSION="${VERSION:-4.0.1-i4}"' "$CURRENT_BUILDER" &&
   grep -Fq '4.0.1-i3|4.0.1-i4) EXPECTED_SOURCE_DATE_EPOCH=1788451200' "$CURRENT_BUILDER" &&
   ! grep -Eq '4\.0\.1-i[12]\) EXPECTED_SOURCE_DATE_EPOCH=' "$CURRENT_BUILDER" &&
   grep -Fq 'builder_version_review=FAIL unreviewed package version' "$CURRENT_BUILDER"; then
    pass current_builder_version_is_fail_closed
else
    fail current_builder_version_is_fail_closed
fi

if [[ "$(grep -c 'apply_reviewed_source_fixes "\$' "$CURRENT_BUILDER")" -eq 2 ]] &&
   grep -Fq 'patches/024-suspend-resume.patch' "$CURRENT_BUILDER" &&
   grep -Fq 'patches/025-suspend-resume-display.patch' "$CURRENT_BUILDER" &&
   grep -Fq '[[ "$VERSION" == "4.0.1-i4" ]]' "$CURRENT_BUILDER"; then
    pass current_builder_patches_compile_and_package_trees
else
    fail current_builder_patches_compile_and_package_trees
fi

if [[ "$(grep -c -- '--fuzz=0 --no-backup-if-mismatch' "$CURRENT_BUILDER")" -eq 2 ]] &&
   grep -Fq -- "-name '*.orig' -o -name '*.rej'" "$CURRENT_BUILDER" &&
   grep -Fq 'apply_reviewed_source_fixes "$STAGE/source" compile-staging' "$CURRENT_BUILDER" &&
   grep -Fq 'apply_reviewed_source_fixes "$P/usr/src/innogpu-kernel-2.2" packaged-dkms' "$CURRENT_BUILDER" &&
   grep -Fq 'reject_patch_artifacts "$P" package-payload' "$CURRENT_BUILDER"; then
    pass current_builder_patch_application_is_fail_closed
else
    fail current_builder_patch_application_is_fail_closed
fi

mkdir -p "$TMP/reject-i2" "$TMP/reject-i4"
if output=$(INNOGPU_ROOT="$TMP/reject-i4" VERSION=4.0.1-i4 \
    SOURCE_DATE_EPOCH=1788364800 bash "$CURRENT_BUILDER" 2>&1); then
    fail current_builder_rejects_old_epoch_for_i4
elif grep -Fq 'builder_repro=FAIL 4.0.1-i4 requires SOURCE_DATE_EPOCH=1788451200' <<<"$output" &&
     [[ ! -e "$TMP/reject-i4/build" ]]; then
    pass current_builder_rejects_old_epoch_for_i4
else
    fail current_builder_rejects_old_epoch_for_i4
fi

if output=$(INNOGPU_ROOT="$TMP/reject-i2" VERSION=4.0.1-i2 \
    SOURCE_DATE_EPOCH=1788364800 bash "$CURRENT_BUILDER" 2>&1); then
    fail current_builder_rejects_retired_iteration
elif grep -Fq 'builder_version_review=FAIL unreviewed package version: 4.0.1-i2' <<<"$output" &&
     [[ ! -e "$TMP/reject-i2/build" ]]; then
    pass current_builder_rejects_retired_iteration
else
    fail current_builder_rejects_retired_iteration
fi

mkdir -p "$TMP/reject-root"
if output=$(INNOGPU_ROOT="$TMP/reject-root" VERSION=4.0.0-i1 \
    SOURCE_DATE_EPOCH=1787342400 bash "$CURRENT_BUILDER" 2>&1); then
    fail current_builder_rejects_reused_4_0_0_version
elif grep -Fq 'builder_version_review=FAIL unreviewed package version: 4.0.0-i1' <<<"$output" &&
     [[ ! -e "$TMP/reject-root/build" ]]; then
    pass current_builder_rejects_reused_4_0_0_version
else
    fail current_builder_rejects_reused_4_0_0_version
fi

if output=$(INNOGPU_ROOT="$TMP/reject-root" VERSION=4.0.1-i3 \
    SOURCE_DATE_EPOCH=1787342400 bash "$CURRENT_BUILDER" 2>&1); then
    fail current_builder_rejects_unreviewed_epoch
elif grep -Fq 'builder_repro=FAIL 4.0.1-i3 requires SOURCE_DATE_EPOCH=1788451200' <<<"$output" &&
     [[ ! -e "$TMP/reject-root/build" ]]; then
    pass current_builder_rejects_unreviewed_epoch
else
    fail current_builder_rejects_unreviewed_epoch
fi

printf 'tests_total=%d tests_passed=%d tests_failed=%d tests_skipped=0\n' \
    "$tests" "$((tests - failures))" "$failures"
[[ "$failures" -eq 0 ]]
