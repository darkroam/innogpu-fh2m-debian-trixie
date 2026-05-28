#!/bin/bash
# Apply the runtime workaround used on some Fantasy II-M machines with
# Debian Trixie kernel 6.12.88, where innogpu loads but Oopses during
# g0m_soc_hw_init -> g0m_soc_setpll -> set_pll_reg.
#
# This script patches the installed innogpu.ko by replacing the first
# g0m_soc_hw_init -> g0m_soc_setpll call with five NOPs.
#
# Usage:
#   sudo ./scripts/patch-skip-first-gpupll.sh [kernel-version]
#
# Default kernel-version: uname -r

set -euo pipefail

KVER="${1:-$(uname -r)}"
KO="/lib/modules/${KVER}/kernel/drivers/gpu/drm/innogpu/innogpu.ko"
BACKUP_DIR="/lib/modules/${KVER}/kernel/drivers/gpu/drm/innogpu/backup"
BACKUP="${BACKUP_DIR}/innogpu.ko.pre-skip-first-gpupll"

if [[ $EUID -ne 0 ]]; then
    echo "ERROR: run as root: sudo $0 [kernel-version]" >&2
    exit 1
fi

if [[ ! -f "$KO" ]]; then
    echo "ERROR: module not found: $KO" >&2
    exit 1
fi

mkdir -p "$BACKUP_DIR"
if [[ ! -f "$BACKUP" ]]; then
    cp -a "$KO" "$BACKUP"
    echo "Backup written: $BACKUP"
else
    echo "Backup already exists: $BACKUP"
fi

python3 - "$KO" <<'PY'
from pathlib import Path
import sys

p = Path(sys.argv[1])
b = bytearray(p.read_bytes())

# innogpu-fh2m 3.3.3.42 on Debian 6.12.88:
#   nm -n innogpu.ko:
#     00000000000460a0 t g0m_soc_setpll
#     0000000000046280 t g0m_soc_hw_init
#   first call from g0m_soc_hw_init to g0m_soc_setpll is at .text+0x46392
#   .text section file offset is 0x40
#   file offset = 0x40 + 0x46392 = 0x463d2
off = 0x40 + 0x46392
old = bytes.fromhex('e8 09 fd ff ff')
new = bytes.fromhex('90 90 90 90 90')
cur = bytes(b[off:off+5])

if cur == new:
    print(f'already patched at {off:#x}')
elif cur == old:
    b[off:off+5] = new
    p.write_bytes(b)
    print(f'patched {p} at {off:#x}: {old.hex(" ")} -> {new.hex(" ")}')
else:
    raise SystemExit(f'ERROR: unexpected bytes at {off:#x}: {cur.hex(" ")}')
PY

depmod -a "$KVER"
echo "depmod complete for $KVER"
echo "Next: test manually before enabling boot autoload: sudo modprobe innogpu"
