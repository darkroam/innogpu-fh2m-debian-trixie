#!/bin/bash
# Staging build: assemble drivers/ + verified vendor black-box into an isolated
# build/<unique>/source tree and compile the DKMS module offline.
# Read-only on drivers/ and vendor/ (the deterministic transform is applied to
# the staging copy, never to vendor/). No install, no hot-swap, no reboot.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

KERNEL="${KERNELDIR_VER:-$(uname -r)}"
KERNELDIR="${KERNELDIR:-/lib/modules/$KERNEL/build}"
[[ -d "$KERNELDIR" ]] || { echo "staging_kernel_headers=FAIL $KERNELDIR"; exit 1; }

# 1) manifest 与 vendor 就位检查
[[ -f binary-manifest.json ]] || { echo "staging_manifest=FAIL"; exit 1; }
bash scripts/extract-vendor-binaries.sh --check-only | grep -q 'vendor_extraction_overall=PASS' || {
    echo "staging_vendor_check=FAIL"; exit 1; }

# 2) 隔离 staging 树
mkdir -p "$ROOT/build"
STAGE="$(mktemp -d "$ROOT/build/stage.XXXXXX")"
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/source"

# 3) 复制 drivers/ 源码
cp -r drivers/. "$STAGE/source/"
# 4) 放置 vendor 内核黑盒对象
for obj in vendor/kernel/*/*.o_shipped; do
    mod=$(basename "$(dirname "$obj")")
    cp "$obj" "$STAGE/source/$mod/"
done
echo "staging_objects=$(ls "$STAGE/source"/*/*.o_shipped | wc -l)"

# 5) 确定性二进制变换（作用于 staging 副本）
python3 tools/patch-gpupll-object.py "$STAGE/source/innogpu/innogpu.o_shipped" >/dev/null && echo "staging_deterministic_transform=PASS"

# 6) 离线编译
cd "$STAGE/source"
jobs=$(nproc); (( jobs > 16 )) && jobs=16
make -j"$jobs" KERNELDIR="$KERNELDIR" > "$ROOT/build/staging-build.log" 2>&1 || {
    echo "staging_dkms_build=FAIL"; tail -15 "$ROOT/build/staging-build.log"; exit 1; }
echo "staging_dkms_build=PASS"

# 7) 模块校验
if [[ ! -s innogpu.ko ]]; then echo "staging_module=FAIL (no innogpu.ko)"; exit 1; fi
NAME=$(/usr/sbin/modinfo -F name innogpu.ko)
VERMAGIC=$(/usr/sbin/modinfo -F vermagic innogpu.ko)
echo "staging_module_name=$NAME"
echo "staging_module_vermagic=$VERMAGIC"
[[ "$NAME" == "innogpu" ]] || { echo "staging_module=FAIL name=$NAME"; exit 1; }
[[ "$VERMAGIC" == "$KERNEL "* ]] || { echo "staging_module_vermagic=FAIL got=$VERMAGIC"; exit 1; }
echo "staging_module_vermagic=PASS"
echo "staging_overall=PASS"
echo "staging_log=$ROOT/build/staging-build.log"
