#!/usr/bin/env bash
# Static contract tests for the F-only 030-031 PM diagnostic patch.
# These checks inspect source control flow only; they do not model closed HAL
# completion, build/install a module, or suspend the host.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PATCH="$ROOT/patches/030-031.patch"
SNAP="$ROOT/docs/planning/evidence/o-stage/o-stage-snapshot.tar.zst"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/fantgpu-030031-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT

PASS=0
FAIL=0
ok() { PASS=$((PASS + 1)); printf '030031_t%02d=PASS # %s\n' "$PASS" "$1"; }
bad() { PASS=$((PASS + 1)); FAIL=$((FAIL + 1)); printf '030031_t%02d=FAIL reason=%s\n' "$PASS" "$1"; }

mkdir -p "$TMP/tree"
tar --use-compress-program=zstd -xf "$SNAP" -C "$TMP/tree"
TREE="$TMP/tree/o-stage"

if patch --batch --forward --fuzz=0 --no-backup-if-mismatch --dry-run -s \
    -d "$TREE" -p1 < "$PATCH"; then
    ok strict_patch_dry_run
else
    bad strict_patch_dry_run
fi

if patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s \
    -d "$TREE" -p1 < "$PATCH"; then
    ok strict_patch_apply
else
    bad strict_patch_apply
fi

expected=$'a/fantgpu/fantgpu_pci_drv.c b/fantgpu/fantgpu_pci_drv.c\na/fantsrvkm/fantdpu_drm_pm.c b/fantsrvkm/fantdpu_drm_pm.c\na/fantsrvkm/ft_drm.c b/fantsrvkm/ft_drm.c'
actual="$(awk '/^diff -ruN / { print $7, $8 }' "$PATCH")"
if [[ "$actual" == "$expected" ]]; then
    ok exact_three_file_scope
else
    bad exact_three_file_scope
fi

if ! find "$TREE" -type f \( -name '*.orig' -o -name '*.rej' \) -print -quit | grep -q .; then
    ok no_patch_artifacts
else
    bad patch_artifacts_present
fi

tree_hash="$(cd "$ROOT" && python3 - "$TREE" <<'PY'
import hashlib
import importlib.util
import sys

spec = importlib.util.spec_from_file_location("o4", "tools/o4-f0-lock-gen.py")
o4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(o4)
rows = list(o4.walk_rows(sys.argv[1]))
print(hashlib.sha256(o4.manifest_text(rows).encode()).hexdigest())
PY
)"
if [[ "$tree_hash" == acfe80d1cff9437f8d4a77ee71640d1a4c0698614656f8312cdf662d8366361c ]]; then
    ok locked_after_tree_hash
else
    bad locked_after_tree_hash
fi

PCI="$TREE/fantgpu/fantgpu_pci_drv.c"
DRM="$TREE/fantsrvkm/fantdpu_drm_pm.c"
FT="$TREE/fantsrvkm/ft_drm.c"

if rg -q 'fantgpu_pm_stage=%s event=%s rc=%d mode=%s reason=%s propagated=%d' "$PCI" "$DRM" \
   && rg -q 'propagated=na' "$PCI" "$DRM" "$FT" \
   && rg -q 'reason=condition_false propagated=na index=%d' "$FT"; then
    ok marker_required_fields_and_order
else
    bad marker_required_fields_and_order
fi

if python3 - "$PCI" <<'PY'
import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8")
body = re.search(r"static int fantgpu_device_suspend\(.*?\n}\n", text, re.S).group()
calls = [
    "fh2m_fant_rsrc_devres_find", "fh2m_hal_dma_suspend(pdev_rsrc)",
    "hal_power_sleep(pdev_rsrc)", "atomic_set(&pdev_rsrc->pvr_resume_count",
    "hal_check_reg_accessiable(pdev_rsrc)", "disable_irq(pdev->irq)",
    "hal_pdp_restore_default_cfg(dev)", "pci_save_state(pdev)",
    "pci_disable_device(pdev)", "fh2m_hal_get_chiptype(dev)",
    "pci_set_power_state(pdev, PCI_D3hot)",
    "fantgpu_pci_dump_cfgspace_regs", "fh2m_fant_msleep",
]
stages = [
    '"device_suspend"', '"dma_suspend"', '"power_sleep"',
    '"resume_counter_reset"', '"reg_access_check"', '"disable_irq"',
    '"pdp_restore"', '"pci_save_state"', '"pci_disable_device"',
    '"get_chip_type"', '"pci_set_d3hot"', '"cfgspace_dump"',
    '"debug_delay"', '"device_suspend_complete"',
]
call_pos = [body.index(token) for token in calls]
stage_pos = [body.index(token) for token in stages]
raise SystemExit(0 if call_pos == sorted(call_pos) and
                 stage_pos == sorted(stage_pos) else 1)
PY
then
    ok pci_suspend_stage_order
else
    bad pci_suspend_stage_order
fi

if rg -q '"pdp_restore", "skip"' "$PCI" \
   && rg -q '"pci_set_d3hot", "skip"' "$PCI" \
   && rg -q '"resize_resume", "skip"' "$PCI" \
   && rg -q '"debug_delay", "skip"' "$PCI"; then
    ok conditional_stage_skip_contract
else
    bad conditional_stage_skip_contract
fi

if rg -q 'return ret < 0 \? ret : ret \? -EIO : 0;' "$PCI" "$DRM" "$FT" \
   && rg -q 'return fantgpu_pm_callback_result' "$PCI" \
   && rg -q 'propagated=%d' "$PCI" "$DRM" "$FT"; then
    ok negative_passthrough_positive_eio
else
    bad negative_passthrough_positive_eio
fi

if rg -q 'drm_crtc_backup.*index|index=%u' "$DRM" \
   && rg -q '"drm_resume_complete"' "$DRM" \
   && rg -q '"drm_gem_recover"' "$DRM" \
   && rg -q '"drm_crtc_restore"' "$DRM"; then
    ok drm_symmetric_indexed_boundaries
else
    bad drm_symmetric_indexed_boundaries
fi

if rg -q '"ft_suspend"' "$FT" \
   && rg -q '"pvr_device_suspend"' "$FT" \
   && rg -q '"ft_resume"' "$FT" \
   && rg -q '"pvr_device_resume"' "$FT" \
   && rg -q '"dvfs_resume"' "$FT" \
   && rg -q '"ft_resume_complete"' "$FT"; then
    ok ft_pvr_dvfs_symmetric_boundaries
else
    bad ft_pvr_dvfs_symmetric_boundaries
fi

if rg -q 'fantgpu_pm_stage=pvr_resume_counter_inc.*index=%d resumed=%d dev_nums=%d' "$FT" \
   && rg -q 'atomic_inc_return\(&pdata->pdev_rsrc->pvr_resume_count\)' "$FT" \
   && rg -q '"pvr_power_wakeup", "skip"|ft_pm_skip_marker\(dev, "pvr_power_wakeup"' "$FT"; then
    ok pvr_resume_counter_observable
else
    bad pvr_resume_counter_observable
fi

if python3 - "$PCI" "$DRM" "$FT" <<'PY'
import sys
from pathlib import Path

pci, drm, ft = [Path(p).read_text(encoding="utf-8") for p in sys.argv[1:]]
checks = [
    "ret = fh2m_hal_dma_suspend" in pci,
    "ret = hal_power_sleep" in pci,
    "ret = hal_pdp_restore_default_cfg" in pci,
    "ret = pci_save_state" in pci,
    "ret = pci_set_power_state" in pci,
    "ret = fantgpu_resize_resume" in pci,
    "dev_priv->pm_state = drm_atomic_helper_suspend" in drm,
    "ret = drm_atomic_helper_resume" in drm,
    "dvfs_err = SuspendDVFS" in ft,
    "err = PVRSRVDeviceSuspend" in ft,
    "err = PVRSRVDeviceResume" in ft,
    "dvfs_err = ResumeDVFS" in ft,
]
raise SystemExit(0 if all(checks) else 1)
PY
then
    ok integer_returns_are_captured
else
    bad integer_returns_are_captured
fi

if python3 - "$PCI" <<'PY'
import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8")
body = re.search(r"static int fantgpu_device_suspend\(.*?\n}\n", text, re.S).group()
calls = ["fh2m_hal_dma_suspend", "hal_power_sleep", "hal_check_reg_accessiable",
         "disable_irq", "hal_pdp_restore_default_cfg", "pci_save_state",
         "pci_disable_device"]
positions = [body.index(call) for call in calls]
forbidden = ("kthread_run", "schedule_work", "wait_for_completion_timeout",
             "skip_hal", "pm_stage_timeout")
raise SystemExit(0 if positions == sorted(positions) and
                 not any(token in body for token in forbidden) else 1)
PY
then
    ok hal_order_preserved_and_no_timeout_bypass
else
    bad hal_order_or_timeout_bypass
fi

if rg -q '"resize_resume", "enter"' "$PCI" \
   && rg -q 'ret = fantgpu_resize_resume\(pdev_rsrc\);' "$PCI" \
   && rg -q '"hal_pci_irq_resume", "enter"' "$PCI" \
   && rg -q '"enable_irq", "enter"' "$PCI" \
   && rg -q '"device_resume_complete"' "$PCI"; then
    ok pci_resume_boundaries_include_resize
else
    bad pci_resume_boundaries_include_resize
fi

printf 'PASS=%d FAIL=%d\n' "$((PASS - FAIL))" "$FAIL"
[[ "$FAIL" -eq 0 ]]
