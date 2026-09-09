#!/usr/bin/env bash
# scripts/materialize-d-stage.sh — 阶段一 D_stage 物化（D → D_stage = 4.0.2-i3 源树配方）
#
# 输出根：/tmp/r16-d-stage/（v24 唯一合法 D_stage 物化根，系统 $TMPDIR，
# 不在仓库任何路径下）。只读 D 源树（third_party/innogpu-fh2m-deepin-202504，
# 绝对保护区，绝不写入）；只写 /tmp/r16-d-stage/；不修改 drivers/、
# 不修改 patches/、不修改任何保护区路径。
#
# 配方（与 scripts/build-deepin-coherent.sh + scripts/build-innogpu-driver.sh
# VERSION=4.0.2-i3 的源树 staging 完全一致）：
#   D = third_party/innogpu-fh2m-deepin-202504/root/usr/src/innogpu-kernel-2.2
#   1) 拷贝 innodma/innogpu/innopmbus/innopower/innosmmu/innosrvkm/innovpu/tools
#      + 顶层文件（Makefile Kbuild dkms.conf dkms.post_remove dkms.pre_install
#      cfg_detect.sh modules_config.sh test_item.sh）
#   2) patched-27 启用集（builder 序）：001(-p6) → 002 → 006 → 009 → 007 →
#      024 → 025 → 023(rebased) → 026 → 027（其余 -p1）。023 与 025
#      共同修改 CPU_PREP 调用点，物化时保留原补丁只读并重基 023 上下文。
#   3) 4.0.2-i3 suspend 组：026-lifecycle → 028 → 029（-p1，--forward --fuzz=0）
#      （025-display 仅 4.0.1-i4，i3 关闭，不应用）
#   4) patch-000 构建期对象变换：tools/patch-gpupll-object.py 作用于
#      物化树 innogpu/innogpu.o_shipped
#   5) 校验：无 .orig/.rej 残留；应用清单回显 machine-readable
#
# 之后由 tools/d-stage-audit-gen.py gen-manifest / snapshot 对
# /tmp/r16-d-stage/ 生成 tree-manifest 与审计产物。
#
# 用法：scripts/materialize-d-stage.sh（无参数；不接受 --keep 等任何参数）
# 退出码：0=物化成功；1=输入缺失/补丁失败/残留物；2=参数错误。

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"
LC_ALL=C
export LC_ALL

# codex 复审 P2：不接受任何参数（--keep 已移除）
[[ $# -eq 0 ]] || { echo "Usage: $0" >&2; exit 2; }

D_ROOT="$ROOT/third_party/innogpu-fh2m-deepin-202504/root/usr/src/innogpu-kernel-2.2"
OUT="/tmp/r16-d-stage"

[[ -d "$D_ROOT" ]] || { echo "materialize=FAIL (missing D root: $D_ROOT)"; exit 1; }

# codex 初审 P2：--keep 会残留旧文件污染 D_stage，已移除；每次物化全量重建
rm -rf -- "$OUT"
mkdir -p -- "$OUT"

for d in innodma innogpu innopmbus innopower innosmmu innosrvkm innovpu tools; do
    cp -r -- "$D_ROOT/$d" "$OUT/"
done
for f in Makefile Kbuild dkms.conf dkms.post_remove dkms.pre_install cfg_detect.sh modules_config.sh test_item.sh; do
    [[ -f "$D_ROOT/$f" ]] && cp -- "$D_ROOT/$f" "$OUT/"
done

apply_p() {
    local level="$1" file="$2"
    patch -p"$level" --batch --forward --fuzz=0 --no-backup-if-mismatch -s \
        -d "$OUT" < "$ROOT/$file"
}

apply_rebased_023() {
    local rebased
    rebased="$(mktemp "${TMPDIR:-/tmp}/innogpu-rebased-023.XXXXXX")"
    # 025 and 023 overlap in CPU_PREP. Rebase only 023's context against
    # the already-applied 025 result; the repository patch files stay immutable.
    sed \
        -e 's/, write, true/, innodpu_dma_resv_usage_rw(write), true/' \
        -e 's/, write))/, innodpu_dma_resv_usage_rw(write)))/' \
        "$ROOT/patches/023-invisible-read-no-writeback.patch" > "$rebased"
    if patch -p1 --batch --forward --fuzz=0 --no-backup-if-mismatch -s \
        -d "$OUT" < "$rebased"; then
        rm -f -- "$rebased"
    else
        local rc=$?
        rm -f -- "$rebased"
        return "$rc"
    fi
}

apply_p 6 "patches/001-kernel-6.12-compat.patch"
# codex 初审 P1：glob 会误匹配同前缀补丁（025-dma 与 025-suspend-resume-display、
# 两个 026-*）——改为显式文件名清单（builder 序）
for p in \
    "patches/002-dp-fbdev-fallback-mode.patch" \
    "patches/006-local-connector-acpi-map.patch" \
    "patches/009-local-internal-edp-connector.patch" \
    "patches/007-fbdev-io-mmap.patch" \
    "patches/024-suspend-resume.patch" \
    "patches/025-dma-resv-usage-rw.patch"; do
    apply_p 1 "$p"
done
apply_rebased_023
for p in \
    "patches/026-inactive-crtc-vblank-guard.patch" \
    "patches/027-foreign-dmabuf-lifecycle.patch"; do
    apply_p 1 "$p"
done
# 4.0.2-i3 suspend 组（026-lifecycle → 028 → 029；025-display 不应用）
apply_p 1 "patches/026-suspend-resume-dvfs-lifecycle.patch"
apply_p 1 "patches/028-suspend-resume-hal-temp-monitor-delay.patch"
apply_p 1 "patches/029-suspend-resume-ddcci-panel.patch"

# patch-000 构建期对象变换（单点字节替换；只接受唯一旧序列或已变换状态）
python3 "$ROOT/tools/patch-gpupll-object.py" "$OUT/innogpu/innogpu.o_shipped"

artifact="$(find "$OUT" -type f \( -name '*.orig' -o -name '*.rej' \) -print -quit)"
[[ -z "$artifact" ]] || {
    echo "materialize=FAIL (patch artifact: ${artifact#"$OUT"/})" >&2
    exit 1
}

echo "materialize=OK"
echo "d_stage_root=$OUT"
echo "recipe=Deepin_20250421190503 + patched-27(001,002,006,009,007,024,025,023-rebased,026,027) + 026-lifecycle + 028 + 029 + patch-000"
echo "excluded=003,004,005,008(closed),025-display(i4-only)"
echo "next=tools/d-stage-audit-gen.py gen-manifest --d-root $D_ROOT --d-stage-root $OUT --out-dir docs/planning/evidence --label D"
exit 0
