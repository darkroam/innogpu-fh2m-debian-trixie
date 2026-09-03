#!/bin/bash
# Static tests for patch-024/025/026/028 and current/legacy builder wiring.
# The suite copies tracked source files to /tmp; it does not read local
# payload directories, build a package, install a module, or suspend the host.

set -u -o pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PATCH="$ROOT/patches/024-suspend-resume.patch"
DISPLAY_PATCH="$ROOT/patches/025-suspend-resume-display.patch"
LIFECYCLE_PATCH="$ROOT/patches/026-suspend-resume-dvfs-lifecycle.patch"
TEMP_MONITOR_PATCH="$ROOT/patches/028-suspend-resume-hal-temp-monitor-delay.patch"
BUILDER="$ROOT/scripts/build-deepin-coherent.sh"
WRAPPER="$ROOT/scripts/build-patched28-suspend-resume.sh"
CURRENT_BUILDER="$ROOT/scripts/build-innogpu-driver.sh"
OBSERVER="$ROOT/tools/probe-suspend-resume-state.sh"
OBSERVER_BT="$ROOT/tools/probe-suspend-resume-observer.bt"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/innogpu-suspend-resume-tests.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

tests=0
failures=0
pass() { tests=$((tests + 1)); printf 'suspend_resume_t%02d=PASS # %s\n' "$tests" "$1"; }
fail() { tests=$((tests + 1)); failures=$((failures + 1)); printf 'suspend_resume_t%02d=FAIL reason=%s\n' "$tests" "$1"; }

mkdir -p "$TMP/tree/innogpu" "$TMP/tree/innosrvkm"
cp "$ROOT/drivers/innogpu/hal.h" "$TMP/tree/innogpu/"
cp "$ROOT/drivers/innogpu/innogpu_pci_drv.c" "$TMP/tree/innogpu/"
cp "$ROOT/drivers/innosrvkm/pvr_dvfs_device.c" "$TMP/tree/innosrvkm/"
cp "$ROOT/drivers/innosrvkm/innodpu_drm_pm.c" "$TMP/tree/innosrvkm/"
cp "$ROOT/drivers/innosrvkm/pvr_drm.c" "$TMP/tree/innosrvkm/"

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

if patch --dry-run --batch --forward --fuzz=0 --no-backup-if-mismatch \
    -s -d "$TMP/tree" -p1 < "$LIFECYCLE_PATCH"; then
    pass lifecycle_patch_dry_run
else
    fail lifecycle_patch_dry_run
fi

if patch --batch --forward --fuzz=0 --no-backup-if-mismatch \
    -s -d "$TMP/tree" -p1 < "$LIFECYCLE_PATCH"; then
    pass lifecycle_patch_applies
else
    fail lifecycle_patch_applies
fi

lifecycle_patched="$TMP/tree/innosrvkm/pvr_drm.c"
if [[ "$(grep -c '^diff -ruN ' "$LIFECYCLE_PATCH")" -eq 1 ]] &&
   grep -Fq 'a/innosrvkm/pvr_drm.c b/innosrvkm/pvr_drm.c' "$LIFECYCLE_PATCH" &&
   grep -Fq '#include "pvr_dvfs_device.h"' "$lifecycle_patched"; then
    pass lifecycle_patch_scope_is_single_pvr_pm_file
else
    fail lifecycle_patch_scope_is_single_pvr_pm_file
fi

if python3 - "$lifecycle_patched" <<'PY'
import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8")
suspend = re.search(r"static int pvr_pm_suspend\(.*?\n}\n", text, re.S).group()
dvfs = suspend.index("SuspendDVFS(priv->dev_node)")
pvr = suspend.index("PVRSRVDeviceSuspend(ddev)", dvfs)
rollback = suspend.index("ResumeDVFS(priv->dev_node)", pvr)
raise SystemExit(0 if dvfs < pvr < rollback else 1)
PY
then
    pass lifecycle_suspend_drains_dvfs_before_pvr_poweroff
else
    fail lifecycle_suspend_drains_dvfs_before_pvr_poweroff
fi

if sed -n '/dvfs_err = SuspendDVFS/,/err = PVRSRVDeviceSuspend/p' "$lifecycle_patched" |
       grep -Fq 'rollback_err = ResumeDVFS(priv->dev_node);' &&
   sed -n '/err = PVRSRVDeviceSuspend/,/return err;/p' "$lifecycle_patched" |
       grep -Fq 'if (err)' &&
   sed -n '/err = PVRSRVDeviceSuspend/,/return err;/p' "$lifecycle_patched" |
       grep -Fq 'rollback_err = ResumeDVFS(priv->dev_node);'; then
    pass lifecycle_suspend_failures_restore_dvfs
else
    fail lifecycle_suspend_failures_restore_dvfs
fi

if python3 - "$lifecycle_patched" <<'PY'
import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8")
resume = re.search(r"static int pvr_pm_resume\(.*?\n}\n", text, re.S).group()
pvr = resume.index("err = PVRSRVDeviceResume(ddev)")
failure_return = resume.index("if (err)\n\t\treturn err;", pvr)
dvfs = resume.index("ResumeDVFS(priv->dev_node)", failure_return)
raise SystemExit(0 if pvr < failure_return < dvfs else 1)
PY
then
    pass lifecycle_resume_restores_dvfs_only_after_pvr_poweron
else
    fail lifecycle_resume_restores_dvfs_only_after_pvr_poweron
fi

if [[ "$(grep -c 'defined(SUPPORT_LINUX_DVFS) && (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0))' "$lifecycle_patched")" -eq 4 ]] &&
   sed -n '/#else/,/#endif/p' "$lifecycle_patched" | grep -Fq 'return PVRSRVDeviceSuspend(ddev);' &&
   sed -n '/#else/,/#endif/p' "$lifecycle_patched" | grep -Fq 'return PVRSRVDeviceResume(ddev);'; then
    pass lifecycle_old_kernel_uses_existing_callback_path_only
else
    fail lifecycle_old_kernel_uses_existing_callback_path_only
fi

if patch --dry-run --batch --forward --fuzz=0 --no-backup-if-mismatch \
    -s -d "$TMP/tree" -p1 < "$TEMP_MONITOR_PATCH"; then
    pass temperature_monitor_patch_dry_run
else
    fail temperature_monitor_patch_dry_run
fi

if patch --batch --forward --fuzz=0 --no-backup-if-mismatch \
    -s -d "$TMP/tree" -p1 < "$TEMP_MONITOR_PATCH"; then
    pass temperature_monitor_patch_applies_after_024_and_026
else
    fail temperature_monitor_patch_applies_after_024_and_026
fi

temp_pvr="$TMP/tree/innosrvkm/pvr_drm.c"
temp_pci="$TMP/tree/innogpu/innogpu_pci_drv.c"
temp_hal="$TMP/tree/innogpu/hal.h"
if [[ "$(grep -c '^diff -ruN ' "$TEMP_MONITOR_PATCH")" -eq 3 ]] &&
   grep -Fq 'a/innogpu/hal.h b/innogpu/hal.h' "$TEMP_MONITOR_PATCH" &&
   grep -Fq 'a/innogpu/innogpu_pci_drv.c b/innogpu/innogpu_pci_drv.c' "$TEMP_MONITOR_PATCH" &&
   grep -Fq 'a/innosrvkm/pvr_drm.c b/innosrvkm/pvr_drm.c' "$TEMP_MONITOR_PATCH" &&
   ! grep -Eq '(\.o_shipped|vendor/)' "$TEMP_MONITOR_PATCH"; then
    pass temperature_monitor_patch_scope_is_reviewable_source_only
else
    fail temperature_monitor_patch_scope_is_reviewable_source_only
fi

if python3 - "$temp_pvr" <<'PY'
import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8")
resume = re.search(r"static int pvr_pm_resume\(.*?\n}\n", text, re.S).group()
pvr = resume.index("err = PVRSRVDeviceResume(ddev)")
pvr_failure = resume.index("if (err)\n\t\treturn err;", pvr)
dvfs = resume.index("ResumeDVFS(priv->dev_node)", pvr_failure)
dvfs_failure = resume.index("if (err)\n\t\treturn err;", dvfs)
wakeup = resume.index("hal_power_wakeup(pdata->pdev_rsrc)", dvfs_failure)
raise SystemExit(0 if pvr < pvr_failure < dvfs < dvfs_failure < wakeup else 1)
PY
then
    pass temperature_monitor_starts_after_pvr_and_dvfs_resume
else
    fail temperature_monitor_starts_after_pvr_and_dvfs_resume
fi

if sed -n '/static int pvr_pm_resume/,/^}/p' "$temp_pvr" |
       grep -Fq 'if (err)' &&
   sed -n '/static int pvr_pm_resume/,/^}/p' "$temp_pvr" |
       grep -Fq 'return err;' &&
   sed -n '/static int pvr_pm_resume/,/^}/p' "$temp_pvr" |
       grep -Fq 'hal_power_wakeup(pdata->pdev_rsrc);'; then
    pass temperature_monitor_is_not_started_on_resume_failure
else
    fail temperature_monitor_is_not_started_on_resume_failure
fi

if grep -Fq 'atomic_t pvr_resume_count;' "$temp_hal" &&
   sed -n '/static int innogpu_device_suspend/,/^}/p' "$temp_pci" |
       grep -Fq 'atomic_set(&pdev_rsrc->pvr_resume_count, 0);' &&
   sed -n '/static int pvr_pm_resume/,/^}/p' "$temp_pvr" |
       grep -Fq 'atomic_inc_return(&pdata->pdev_rsrc->pvr_resume_count)' &&
   sed -n '/static int pvr_pm_resume/,/^}/p' "$temp_pvr" |
       grep -Fq 'if (resumed == pdata->dev_nums)'; then
    pass temperature_monitor_waits_for_all_pvr_children_per_parent
else
    fail temperature_monitor_waits_for_all_pvr_children_per_parent
fi

if python3 - "$ROOT/drivers/innogpu/hal.h" "$temp_hal" <<'PY'
import sys
from pathlib import Path

before = Path(sys.argv[1]).read_text(encoding="utf-8")
after = Path(sys.argv[2]).read_text(encoding="utf-8")
marker = "\tstruct vpuinfo_s vpuinfo;\n"
prefix, suffix = before.split(marker, 1)
expected = prefix + marker + (
    "\n\t/* Queue the parent temperature work only after every PVR child resumes. */\n"
    "\tatomic_t pvr_resume_count;\n"
) + suffix
raise SystemExit(0 if after == expected else 1)
PY
then
    pass temperature_counter_is_appended_without_shifting_existing_hal_fields
else
    fail temperature_counter_is_appended_without_shifting_existing_hal_fields
fi

if grep -Fq 'inno_rsrc_devres_alloc(sizeof(struct dev_rsrc))' "$temp_pci"; then
    pass temperature_counter_storage_uses_updated_source_struct_size
else
    fail temperature_counter_storage_uses_updated_source_struct_size
fi

if sed -n '/static int pvr_pm_resume/,/^}/p' "$temp_pvr" |
       grep -Fq 'pdata->dev_idx >= (unsigned int)pdata->dev_nums' &&
   sed -n '/static int pvr_pm_resume/,/^}/p' "$temp_pvr" |
       grep -Fq 'return -ENODEV;' &&
   sed -n '/static int pvr_pm_resume/,/^}/p' "$temp_pvr" |
       grep -Fq 'return -EOVERFLOW;'; then
    pass temperature_monitor_rejects_invalid_or_overflow_resume_state
else
    fail temperature_monitor_rejects_invalid_or_overflow_resume_state
fi

if sed -n '/static int innogpu_device_resume/,/^}/p' "$temp_pci" |
       grep -Fq 'if (!is_suspend)' &&
   [[ "$(sed -n '/static int innogpu_device_resume/,/^}/p' "$temp_pci" |
       grep -c 'hal_power_wakeup(pdev_rsrc);')" -eq 1 ]] &&
   sed -n '/static int pvr_pm_resume/,/^}/p' "$temp_pvr" |
       grep -Fq '#if defined(NO_HARDWARE)'; then
    pass parent_s3_wakeup_removed_and_nonhardware_path_preserved
else
    fail parent_s3_wakeup_removed_and_nonhardware_path_preserved
fi

if sed -n '/#else/,/#endif/p' "$temp_pvr" |
       grep -Fq 'err = PVRSRVDeviceResume(ddev);' &&
   sed -n '/static int pvr_pm_resume/,/^}/p' "$temp_pvr" |
       grep -Fq 'resumed = atomic_inc_return'; then
    pass old_kernel_path_starts_monitor_only_after_successful_pvr_resume
else
    fail old_kernel_path_starts_monitor_only_after_successful_pvr_resume
fi

if sed -n '/static int innogpu_device_suspend/,/^}/p' "$temp_pci" |
       grep -Fq 'hal_power_sleep(pdev_rsrc);' &&
   sed -n '/static int pvr_pm_suspend/,/^}/p' "$temp_pvr" |
       grep -Fq 'SuspendDVFS(priv->dev_node)' &&
   sed -n '/static int pvr_pm_suspend/,/^}/p' "$temp_pvr" |
       grep -Fq 'PVRSRVDeviceSuspend(ddev)'; then
    pass temperature_patch_preserves_suspend_cancel_and_dvfs_drain
else
    fail temperature_patch_preserves_suspend_cancel_and_dvfs_drain
fi

if ! find "$TMP/tree" -type f \( -name '*.orig' -o -name '*.rej' \) -print -quit |
       grep -q .; then
    pass temperature_patch_application_leaves_no_artifacts
else
    fail temperature_patch_application_leaves_no_artifacts
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

if grep -Fq 'VERSION="${VERSION:-4.0.2-i2}"' "$CURRENT_BUILDER" &&
   grep -Fq '4.0.1-i3|4.0.1-i4) EXPECTED_SOURCE_DATE_EPOCH=1788451200' "$CURRENT_BUILDER" &&
   grep -Fq '4.0.2-i1) EXPECTED_SOURCE_DATE_EPOCH=1788624000' "$CURRENT_BUILDER" &&
   grep -Fq '4.0.2-i2) EXPECTED_SOURCE_DATE_EPOCH=1788710400' "$CURRENT_BUILDER" &&
   ! grep -Eq '4\.0\.1-i[12]\) EXPECTED_SOURCE_DATE_EPOCH=' "$CURRENT_BUILDER" &&
   grep -Fq 'builder_version_review=FAIL unreviewed package version' "$CURRENT_BUILDER"; then
    pass current_builder_version_is_fail_closed
else
    fail current_builder_version_is_fail_closed
fi

if [[ "$(grep -c 'apply_reviewed_source_fixes "\$' "$CURRENT_BUILDER")" -eq 2 ]] &&
   grep -Fq 'patches/024-suspend-resume.patch' "$CURRENT_BUILDER" &&
   grep -Fq 'patches/025-suspend-resume-display.patch' "$CURRENT_BUILDER" &&
   grep -Fq 'patches/026-suspend-resume-dvfs-lifecycle.patch' "$CURRENT_BUILDER" &&
   grep -Fq 'patches/028-suspend-resume-hal-temp-monitor-delay.patch' "$CURRENT_BUILDER" &&
   grep -Fq '[[ "$VERSION" == "4.0.1-i4" ]]' "$CURRENT_BUILDER" &&
   grep -Fq '[[ "$VERSION" == "4.0.2-i2" ]]' "$CURRENT_BUILDER"; then
    pass current_builder_patches_compile_and_package_trees
else
    fail current_builder_patches_compile_and_package_trees
fi

if [[ "$(grep -c -- '--fuzz=0 --no-backup-if-mismatch' "$CURRENT_BUILDER")" -eq 4 ]] &&
   grep -Fq -- "-name '*.orig' -o -name '*.rej'" "$CURRENT_BUILDER" &&
   grep -Fq 'apply_reviewed_source_fixes "$STAGE/source" compile-staging' "$CURRENT_BUILDER" &&
   grep -Fq 'apply_reviewed_source_fixes "$P/usr/src/innogpu-kernel-2.2" packaged-dkms' "$CURRENT_BUILDER" &&
   grep -Fq 'reject_patch_artifacts "$P" package-payload' "$CURRENT_BUILDER"; then
    pass current_builder_patch_application_is_fail_closed
else
    fail current_builder_patch_application_is_fail_closed
fi

mkdir -p "$TMP/reject-402-epoch" "$TMP/reject-402-version"
if output=$(INNOGPU_ROOT="$TMP/reject-402-epoch" VERSION=4.0.2-i1 \
    SOURCE_DATE_EPOCH=1788537600 bash "$CURRENT_BUILDER" 2>&1); then
    fail current_builder_rejects_old_epoch_for_4_0_2_i1
elif grep -Fq 'builder_repro=FAIL 4.0.2-i1 requires SOURCE_DATE_EPOCH=1788624000' <<<"$output" &&
     [[ ! -e "$TMP/reject-402-epoch/build" ]]; then
    pass current_builder_rejects_old_epoch_for_4_0_2_i1
else
    fail current_builder_rejects_old_epoch_for_4_0_2_i1
fi

if output=$(INNOGPU_ROOT="$TMP/reject-402-version" VERSION=4.0.2-i2 \
    SOURCE_DATE_EPOCH=1788624000 bash "$CURRENT_BUILDER" 2>&1); then
    fail current_builder_rejects_old_epoch_for_4_0_2_i2
elif grep -Fq 'builder_repro=FAIL 4.0.2-i2 requires SOURCE_DATE_EPOCH=1788710400' <<<"$output" &&
     [[ ! -e "$TMP/reject-402-version/build" ]]; then
    pass current_builder_rejects_old_epoch_for_4_0_2_i2
else
    fail current_builder_rejects_old_epoch_for_4_0_2_i2
fi

if output=$(INNOGPU_ROOT="$TMP/reject-402-version" VERSION=4.0.2-i3 \
    SOURCE_DATE_EPOCH=1788710400 bash "$CURRENT_BUILDER" 2>&1); then
    fail current_builder_rejects_unreviewed_4_0_2_iteration
elif grep -Fq 'builder_version_review=FAIL unreviewed package version: 4.0.2-i3' <<<"$output"; then
    pass current_builder_rejects_unreviewed_4_0_2_iteration
else
    fail current_builder_rejects_unreviewed_4_0_2_iteration
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

if bash -n "$OBSERVER" &&
   ! grep -Fq '/sys/power/state' "$OBSERVER" &&
   ! grep -Eq '(rtcwake|systemctl[[:space:]]+(suspend|hibernate)|printf .*mem_sleep)' "$OBSERVER" &&
   grep -Fq "trap 'exit 129' HUP" "$OBSERVER" &&
   grep -Fq "trap 'exit 130' INT" "$OBSERVER" &&
   grep -Fq "trap 'exit 143' TERM" "$OBSERVER"; then
    pass observer_is_non_suspend_and_has_valid_syntax
else
    fail observer_is_non_suspend_and_has_valid_syntax
fi

if grep -Fq 'EXPECTED_OBJECT_SHA=30c594629d1d0e32674e793f2f4235afd4efd3f1e92ee4e4ed1920b315618c2b' "$OBSERVER" &&
   grep -Fq 'EXPECTED_KERNEL=6.12.101+deb13-amd64' "$OBSERVER" &&
   grep -Fq 'EXPECTED_VERSION=4.0.1-i3' "$OBSERVER" &&
   grep -Fq 'EXPECTED_MODULE_BUILD_ID=be315ad1dc8de5248bb4d29f84e0a98fbc1978ab' "$OBSERVER" &&
   grep -Fq 'loaded_module_build_id_mismatch' "$OBSERVER" &&
   grep -Fq 'pahole -F btf -C "$type" /sys/kernel/btf/innogpu' "$OBSERVER" &&
   grep -Fq '[[ -n $layout ]] || fail "missing_${type}_layout"' "$OBSERVER" &&
   grep -Fq '"$BUILD_OUTPUT_ROOT"/*|/tmp/*)' "$OBSERVER" &&
   grep -Fq '[[ ! -e $OUTPUT && ! -L $OUTPUT ]]' "$OBSERVER" &&
   grep -Fq 'chown -R "$DESKTOP_UID:$DESKTOP_GID" "$OUTPUT"' "$OBSERVER" &&
   grep -Fq 'cursor_enable_offset_mismatch' "$OBSERVER" &&
   grep -Fq 'crtc_active_offset_mismatch' "$OBSERVER" &&
   grep -Fq 'framebuffer_gem_offset_mismatch' "$OBSERVER" &&
   grep -Fq 'plane_state_fb_offset_mismatch' "$OBSERVER"; then
    pass observer_abi_is_fail_closed
else
    fail observer_abi_is_fail_closed
fi

if grep -Fq 'kprobe:innogpu:pdp0_cursor_move' "$OBSERVER_BT" &&
   grep -Fq 'kprobe:innogpu:innodpu_pdp0_wakeup' "$OBSERVER_BT" &&
   grep -Fq 'active_valid=' "$OBSERVER_BT" &&
   grep -Fq 'kprobe:innogpu:fh2m_hal_reg_read32' "$OBSERVER_BT" &&
   grep -Fq 'kprobe:innogpu:fh2m_hal_reg_write32' "$OBSERVER_BT" &&
   grep -Fq '/arg2 >= 0x258 && arg2 <= 0x25a/' "$OBSERVER_BT" &&
   ! grep -Eq 'system\(|override\(|signal\(' "$OBSERVER_BT"; then
    pass observer_records_natural_driver_activity_only
else
    fail observer_records_natural_driver_activity_only
fi

if grep -Fq 'kprobe:innogpu:pdp0_get_fb_dev_paddr' "$OBSERVER_BT" &&
   grep -Fq 'kretprobe:innogpu:pdp0_get_fb_dev_paddr' "$OBSERVER_BT" &&
   grep -Fq 'kprobe:innogpu:pdp0_get_fbaddr' "$OBSERVER_BT" &&
   grep -Fq 'kprobe:innogpu:fh2m_innodpu_gem_get_dev_paddr' "$OBSERVER_BT" &&
   grep -Fq 'kprobe:innogpu:innodpu_gem_object_free' "$OBSERVER_BT" &&
   grep -Fq 'kprobe:innogpu:pdp0_fd_plane_update' "$OBSERVER_BT" &&
   grep -Fq 'r08_drm_primary_fb_snapshot=' "$OBSERVER"; then
    pass observer_correlates_primary_fb_gem_and_scanout
else
    fail observer_correlates_primary_fb_gem_and_scanout
fi

if grep -Fq 'kprobe:innogpu:pdp0_enter_config_mode' "$OBSERVER_BT" &&
   grep -Fq 'kprobe:innogpu:pdp0_leave_config_mode' "$OBSERVER_BT" &&
   grep -Fq 'kprobe:innogpu:pdp0_set_and_wait_config_valid' "$OBSERVER_BT" &&
   grep -Fq '/arg2 == 0x1f9/' "$OBSERVER_BT" &&
   grep -Fq 'r08_event=shadow_sequence time_ns=' "$OBSERVER_BT" &&
   grep -Fq 'r08_shadow_sequence_events=' "$OBSERVER" &&
   grep -Fq 'active_valid=%u active=%u value=%u' "$OBSERVER_BT"; then
    pass observer_records_shadow_config_valid_order
else
    fail observer_records_shadow_config_valid_order
fi

if grep -Fq 'xrandr --query --props' "$OBSERVER" &&
   grep -Fq 'r08_hdmi_connector_snapshot=' "$OBSERVER" &&
   grep -Fq 'r08_hdmi_format_snapshot=' "$OBSERVER" &&
   grep -Fq 'output_format=%s:output_bpc=%s:broadcast_rgb=%s' "$OBSERVER" &&
   grep -Fq 'sub(/:$/, "", plane)' "$OBSERVER" &&
   grep -Fq 'r08_primary_fb_content_crc=UNAVAILABLE_NO_READ_ONLY_KERNEL_INTERFACE' "$OBSERVER"; then
    pass observer_snapshots_connector_format_and_reports_unavailable_fields
else
    fail observer_snapshots_connector_format_and_reports_unavailable_fields
fi

if grep -Fq 'r08_fb_dev_paddr_entries=' "$OBSERVER" &&
   grep -Fq 'r08_fb_dev_paddr_completed=' "$OBSERVER" &&
   grep -Fq 'r08_fb_dev_paddr_unmatched_at_stop=' "$OBSERVER" &&
   grep -Fq 'r08_scanout_unmatched_at_stop=' "$OBSERVER" &&
   grep -Fq 'r08_gem_paddr_unmatched_at_stop=' "$OBSERVER" &&
   grep -Fq 'r08_return_capture=$RETURN_CAPTURE' "$OBSERVER" &&
   grep -Fq 'PARTIAL_UNMATCHED_AT_STOP' "$OBSERVER" &&
   grep -Fq 'fb_dev_return_count_exceeds_entries' "$OBSERVER"; then
    pass observer_reports_unmatched_calls_at_sample_stop
else
    fail observer_reports_unmatched_calls_at_sample_stop
fi

if grep -Fq 'reg_module=%lu reg_entity=0x%lx' "$OBSERVER_BT" &&
   ! grep -Eq 'hal_(read|write).*dpu=%lu' "$OBSERVER_BT"; then
    pass observer_labels_hal_abi_arguments_accurately
else
    fail observer_labels_hal_abi_arguments_accurately
fi

if ! grep -Eq '(/dev/mem|devmem|ioremap|bpf_probe_write_user|copyout|system\(|override\(|signal\()' \
        "$OBSERVER_BT" "$OBSERVER" &&
   ! grep -Eq '(xrandr.*--output|modprobe|rmmod|insmod|/sys/power/state|rtcwake)' "$OBSERVER"; then
    pass observer_has_no_active_driver_or_display_operations
else
    fail observer_has_no_active_driver_or_display_operations
fi

printf 'tests_total=%d tests_passed=%d tests_failed=%d tests_skipped=0\n' \
    "$tests" "$((tests - failures))" "$failures"
[[ "$failures" -eq 0 ]]
