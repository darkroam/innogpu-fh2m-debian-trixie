#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/030-033-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT
cd "$ROOT"

tar --use-compress-program=zstd -xf \
    docs/planning/evidence/o-stage/5.0.0-i4/o-stage-snapshot.tar.zst -C "$TMP"
mv "$TMP/o-stage" "$TMP/i4"
tar --use-compress-program=zstd -xf \
    docs/planning/evidence/o-stage/5.0.0-i3/o-stage-snapshot.tar.zst -C "$TMP"
mv "$TMP/o-stage" "$TMP/i3"

patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s \
    -d "$TMP/i4" -p1 < patches/030-033.patch

scope="$(sed -n 's|^+++ b/||p' patches/030-033.patch | sort -u)"
[[ "$scope" == $'fantgpu/fantgpu_pci_drv.c\nfantgpu/hal.h' ]]
! rg -q 'fantgpu_pm_probe_state|pm_probe|linux/mutex.h' "$TMP/i4/fantgpu/hal.h"
rg -q 'devres_alloc\(fantgpu_pm_probe_devres_release' "$TMP/i4/fantgpu/fantgpu_pci_drv.c"
rg -q 'devres_find\(dev, fantgpu_pm_probe_devres_release' "$TMP/i4/fantgpu/fantgpu_pci_drv.c"
! rg -q 'pdev_rsrc->pm_probe' "$TMP/i4/fantgpu/fantgpu_pci_drv.c"

python3 - "$TMP/i3/fantgpu/hal.h" "$TMP/i4/fantgpu/hal.h" <<'PY'
import re, sys

def struct(path):
    text = open(path, encoding="utf-8").read()
    match = re.search(r"struct dev_rsrc \{.*?^\};", text, re.M | re.S)
    if not match:
        raise SystemExit(f"dev_rsrc missing: {path}")
    return match.group(0)

if struct(sys.argv[1]) != struct(sys.argv[2]):
    raise SystemExit("dev_rsrc differs from the i3 shipped-object ABI baseline")
PY

actual="$(python3 - "$TMP/i4" <<'PY'
import hashlib, importlib.util, sys
spec = importlib.util.spec_from_file_location("o4", "tools/o4-f0-lock-gen.py")
o4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(o4)
rows = list(o4.walk_rows(sys.argv[1]))
print(hashlib.sha256(o4.manifest_text(rows).encode()).hexdigest())
PY
)"
[[ "$actual" == "4be9ba7509258726b95bd41137da1280868c91c835aa57d4150fd6ba76913300" ]]

echo "PASS: 030-033 restores the shipped-object dev_rsrc ABI and uses independent devres probe state"
