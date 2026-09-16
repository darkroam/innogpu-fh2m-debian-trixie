#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
PATCH="$ROOT/patches/030-032.patch"
SNAP="$ROOT/docs/planning/evidence/o-stage/5.0.0-i3/o-stage-snapshot.tar.zst"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
PASS=0
FAIL=0

ok() { echo "pm-probe-$1=PASS"; PASS=$((PASS + 1)); }
bad() { echo "pm-probe-$1=FAIL${2:+ reason=$2}"; FAIL=$((FAIL + 1)); }

mkdir -p "$TMP/tree"
tar --use-compress-program=zstd -xf "$SNAP" -C "$TMP/tree"
TREE="$TMP/tree/o-stage"
if patch --batch --forward --fuzz=0 --no-backup-if-mismatch -p1 -d "$TREE" < "$PATCH" >/dev/null; then
    ok t01_strict_patch_apply
else
    bad t01_strict_patch_apply
fi

expected=$'a/fantgpu/fantgpu_pci_drv.c b/fantgpu/fantgpu_pci_drv.c\na/fantgpu/hal.h b/fantgpu/hal.h'
actual=$(awk '/^diff -ruN / { print $3, $4 }' "$PATCH")
[[ "$actual" == "$expected" ]] && ok t02_exact_scope || bad t02_exact_scope

tree_hash=$(cd "$ROOT" && python3 - "$TREE" <<'PY'
import hashlib, importlib.util, sys
spec = importlib.util.spec_from_file_location("o4", "tools/o4-f0-lock-gen.py")
o4 = importlib.util.module_from_spec(spec); spec.loader.exec_module(o4)
print(hashlib.sha256(o4.manifest_text(list(o4.walk_rows(sys.argv[1]))).encode()).hexdigest())
PY
)
[[ "$tree_hash" == 43f63f3fd5a9305734b9749cdfe773bc303726b06bf9f94fc1b55347738e9213 ]] \
    && ok t03_locked_tree_hash || bad t03_locked_tree_hash "$tree_hash"

PCI="$TREE/fantgpu/fantgpu_pci_drv.c"
HAL="$TREE/fantgpu/hal.h"

if rg -q 'debugfs_create_file\("pm_probe", 0600' "$PCI" \
   && rg -q 'capable\(CAP_SYS_ADMIN\)' "$PCI"; then
    ok t04_root_gate
else
    bad t04_root_gate
fi

if rg -q '#define FANTGPU_PM_PROBE_CONFIRM "confirm=R5_I4_DIAGNOSTIC"' "$PCI" \
   && rg -q '"run reg-read " FANTGPU_PM_PROBE_CONFIRM' "$PCI" \
   && rg -q '"run monitor-cycle " FANTGPU_PM_PROBE_CONFIRM' "$PCI" \
   && rg -q 'return -EINVAL;' "$PCI"; then
    ok t05_exact_commands
else
    bad t05_exact_commands
fi

if ! rg -q 'run (dma|pdp|power-sleep|power-wakeup)' "$PCI"; then
    ok t06_no_arbitrary_hal_commands
else
    bad t06_no_arbitrary_hal_commands
fi

if rg -q 'probe->used \|\|' "$PCI" \
   && rg -q 'probe->used = true;' "$PCI" \
   && rg -q 'state, "running"' "$PCI"; then
    ok t07_single_boot_no_reentry
else
    bad t07_single_boot_no_reentry
fi

if python3 - "$PCI" <<'PY'
import re, sys
from pathlib import Path
t = Path(sys.argv[1]).read_text()
b = re.search(r"static ssize_t fantgpu_pm_probe_write\(.*?\n}\n", t, re.S).group()
tokens = ["hal_power_sleep(pdev_rsrc)", "hal_power_wakeup(pdev_rsrc)",
          "raw_rc = sleep_rc ? sleep_rc : wakeup_rc",
          "normalized_rc = fantgpu_pm_propagate(raw_rc)"]
pos = [b.index(x) for x in tokens]
raise SystemExit(0 if pos == sorted(pos) else 1)
PY
then
    ok t08_sleep_wakeup_order
else
    bad t08_sleep_wakeup_order
fi

if rg -q 'return ret < 0 \? ret : ret \? -EIO : 0;' "$PCI" \
   && rg -q 'raw_rc=%d normalized_rc=%d' "$PCI"; then
    ok t09_return_value_contract
else
    bad t09_return_value_contract
fi

if rg -q '\.owner = THIS_MODULE' "$PCI" \
   && [[ $(rg -c 'debugfs_file_get' "$PCI") -ge 1 ]] \
   && [[ $(rg -c 'debugfs_file_put' "$PCI") -ge 2 ]]; then
    ok t10_debugfs_lifetime
else
    bad t10_debugfs_lifetime
fi

if python3 - "$PCI" <<'PY'
import re, sys
from pathlib import Path
t = Path(sys.argv[1]).read_text()
b = re.search(r"static void fantgpu_pm_probe_quiesce\(.*?\n}\n", t, re.S).group()
tokens = ["WRITE_ONCE(probe->removing, true)", "debugfs_remove(probe->dentry)",
          "mutex_lock(&probe->lock)"]
pos = [b.index(x) for x in tokens]
raise SystemExit(0 if pos == sorted(pos) else 1)
PY
then
    ok t11_remove_closes_then_waits
else
    bad t11_remove_closes_then_waits
fi

if [[ $(rg -c 'fantgpu_pm_transition_begin\(dev, &pdev_rsrc\)' "$PCI") -eq 6 ]] \
   && [[ $(rg -c 'fantgpu_pm_transition_end\(pdev_rsrc\)' "$PCI") -eq 7 ]] \
   && rg -q 'fantgpu_pm_transition_begin\(&pdev->dev, &pdev_rsrc\)' "$PCI"; then
    ok t12_all_pm_callbacks_and_shutdown_locked
else
    bad t12_all_pm_callbacks_and_shutdown_locked
fi

if rg -q 'system_state != SYSTEM_RUNNING' "$PCI" \
   && rg -q 'probe->pm_transition' "$PCI" \
   && rg -q 'probe->removing' "$PCI" \
   && rg -q 'fh2m_fant_rsrc_devres_find\(pdev_rsrc->dev\) != pdev_rsrc' "$PCI"; then
    ok t13_pm_and_binding_checks
else
    bad t13_pm_and_binding_checks
fi

if rg -q 'probe=%s\\nstate=%s\\nraw_rc=%d\\nnormalized_rc=%d' "$PCI" \
   && rg -q 'entered_step=%s\\ncompleted_step=%s' "$PCI" \
   && rg -q 'struct fantgpu_pm_probe_state pm_probe;' "$HAL"; then
    ok t14_fixed_status_schema
else
    bad t14_fixed_status_schema
fi

if ! rg -q 'kthread_run|schedule_work|wait_for_completion_timeout|cancel.*timeout|pm_probe_timeout' "$PCI"; then
    ok t15_no_async_timeout
else
    bad t15_no_async_timeout
fi

if ! find "$TREE" -type f \( -name '*.orig' -o -name '*.rej' \) -print -quit | grep -q .; then
    ok t16_no_patch_artifacts
else
    bad t16_no_patch_artifacts
fi

echo "PASS=$PASS FAIL=$FAIL"
[[ "$FAIL" -eq 0 ]]
