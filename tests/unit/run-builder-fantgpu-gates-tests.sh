#!/usr/bin/env bash
# tests/unit/run-builder-fantgpu-gates-tests.sh — builder F 分支早期门禁与静态契约单测
#
# 依据 docs/design/c3-a-4-reproducible-input-plan.md §三（改造点 9-13）与
# §四（postinst/md5sums/trace）：当前版本 5.0.0-i6、血统判定 5.0.0-i*、
# 输入预检分支、PKG_DESC $VERSION 参数化、share/命令前缀血统参数化、
# ld.so.conf fantgpu-fh2m、postinst fh2m_dri.so + 设备门。
# 只测可运行早期门禁与静态契约（不编译内核：KERNELDIR 注入不存在路径使
# builder 在 kernel headers 检查处提前退出——该检查先于 manifest/vendor 预检）。
# 退出码：0=全过 1=用例失败 2=环境错误。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILDER="$ROOT/scripts/build-innogpu-driver.sh"
F_GENERATOR="$ROOT/scripts/generate-fantgpu-maintainer-scripts.sh"
cd "$ROOT"
export LC_ALL=C

[ -f "$BUILDER" ] || { echo "FATAL: builder not found" >&2; exit 2; }

PASS=0; FAILN=0
ok()  { PASS=$((PASS + 1)); echo "ok  $1"; }
bad() { FAILN=$((FAILN + 1)); echo "BAD $1: $2" >&2; }

run_builder() {  # run_builder [env...]（env 值不含空格）；全部注入 KERNELDIR 不存在路径
    set +e
    STAGE_ROOT="$TMP/stage" KERNELDIR="$TMP/no-kernel-headers" \
        env $@ bash "$BUILDER" > "$TMP/b.out" 2> "$TMP/b.err"
    RC=$?
    set -e
    OUTTEXT="$(cat "$TMP/b.out")"
    ERRTEXT="$(cat "$TMP/b.err")"
}

TMP="$(mktemp -d "${TMPDIR:-/tmp}/bfg-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT INT TERM HUP

# t01 未审核版本 → builder_version_review=FAIL rc=1
run_builder VERSION=9.9.9 SOURCE_DATE_EPOCH=1789516800
if [ "$RC" -eq 1 ] && grep -Fq "builder_version_review=FAIL" "$TMP/b.err"; then
    ok t01
else
    bad t01 "rc=$RC"
fi

# t02 5.0.0-i6 通过 allowlist + epoch 门 → 死在 kernel headers（证明版本/血统被接受）
run_builder VERSION=5.0.0-i6 SOURCE_DATE_EPOCH=1789516800
if [ "$RC" -eq 1 ] && grep -Fq "staging_kernel_headers=FAIL" "$TMP/b.out"; then
    ok t02
else
    bad t02 "rc=$RC"
fi

# t03 归档代禁止借用当前 i6 快照重构建
run_builder VERSION=5.0.0-i3 SOURCE_DATE_EPOCH=1788796800
if [ "$RC" -eq 1 ] && grep -Fq "staging_ostage_generation=FAIL" "$TMP/b.out"; then
    ok t03
else
    bad t03 "rc=$RC"
fi

# t04 epoch 错误 → builder_repro=FAIL
run_builder VERSION=5.0.0-i6 SOURCE_DATE_EPOCH=1111111111
if [ "$RC" -eq 1 ] && grep -Fq "builder_repro=FAIL" "$TMP/b.out"; then
    ok t04
else
    bad t04 "rc=$RC"
fi

# t05 静态契约：血统判定 5.0.0-i* 模式（不再写死 == 5.0.0-i1）
if grep -Fq '[[ "$VERSION" == 5.0.0-i* ]] && FANT_LINEAGE=1' "$BUILDER"; then
    ok t05
else
    bad t05 "lineage pattern missing"
fi

# t06 静态契约：PKG_DESC 版本串参数化（无硬编码 5.0.0-i1 文案）
if grep -Fq 'PKG_DESC="Innosilicon Fantasy II-M driver (fantgpu lineage $VERSION, O_stage materialized tree)"' "$BUILDER" \
   && grep -Fq 'O_stage materialized source tree (030 chain of 18,' "$BUILDER" \
   && ! grep -Fq 'fantgpu lineage 5.0.0-i1' "$BUILDER"; then
    ok t06
else
    bad t06 "PKG_DESC not parameterized"
fi

# t07 静态契约：postinst F 分支 fh2m_dri.so + lspci 设备门（含不可用 WARNING 分支）
# + C2 生命周期契约（codex re-review P1）：F postinst 不得写 fantgpu.conf（该
# options 文件由 builder 组装期写入 $P 包内确定性 payload、dpkg 管理），O 分支
# 保持历史直写口径
if grep -Fq 'DRI_SO=/usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so' "$F_GENERATOR" \
   && grep -Fq 'lspci -n -d 1ec8:9810' "$F_GENERATOR" \
   && grep -Fq 'lspci unavailable; 1ec8:9810 device gate not executed' "$F_GENERATOR" \
   && grep -Fq "'options innogpu firmware_en=1' > /etc/modprobe.d/innogpu.conf" "$BUILDER" \
   && grep -Fq "'options fantgpu firmware_en=1' > \"\$P/etc/modprobe.d/fantgpu.conf\"" "$BUILDER" \
   && ! grep -Fq "'options fantgpu firmware_en=1' > /etc/modprobe.d/fantgpu.conf" "$F_GENERATOR"; then
    ok t07
else
    bad t07 "postinst F assertions missing"
fi

# t08 静态契约：ld.so.conf 血统分支（0-fantgpu-hwgl.conf + fantgpu-fh2m 目录）
if grep -Fq "'/usr/lib/x86_64-linux-gnu/fantgpu-fh2m' > \"\$P/etc/ld.so.conf.d/0-fantgpu-hwgl.conf\"" "$BUILDER"; then
    ok t08
else
    bad t08 "ld.so.conf F branch missing"
fi

# t09 静态契约：share 目录/命令前缀血统参数化（SHARE_DIR/CMD_PREFIX）
if grep -Fq 'SHARE_DIR="fantgpu-fh2m-trixie"' "$BUILDER" \
   && grep -Fq 'CMD_PREFIX="fantgpu-"' "$BUILDER"; then
    ok t09
else
    bad t09 "share/prefix parameterization missing"
fi

# t10 静态契约：F 分支输入预检只要求 F manifest（不调 extract-vendor-binaries）
if grep -Fq 'tools/validate-binary-manifest-fantgpu.py' "$BUILDER"; then
    ok t10
else
    bad t10 "F input precheck missing"
fi

# t11 静态契约：F 分支 md5sums 重生成调用
if grep -Fq 'tools/gen-package-md5sums.py --root "$P"' "$BUILDER"; then
    ok t11
else
    bad t11 "md5sums generation missing"
fi

# t12 静态契约：materialize-trace 契约（物化 + 逐行 verify + ostage 清单）
if grep -Fq 'builder_materialize_trace=PASS' "$BUILDER" \
	&& grep -Fq -- '--verify-trace' "$BUILDER" \
	&& [[ "$(grep -Fc -- '--ostage-manifest "$OSTAGE_DIR/o-stage.manifest.tsv"' "$BUILDER")" -eq 2 ]]; then
    ok t12
else
    bad t12 "trace assertions missing"
fi

# t13 静态契约：O_stage meta 引用 i6 隔离代（030-034 入链后锁定新树 hash）
if grep -Fq 'OSTAGE_DIR="$ROOT/docs/planning/evidence/o-stage/5.0.0-i6"' "$BUILDER" \
   && grep -Fq '"$OSTAGE_DIR/5.0.0-i6.meta.json"' "$BUILDER" \
   && grep -Fq 'OSTAGE_TREE_HASH="5f6a5347c7e217ba3f7c5b71fdcad520148e0bc71231ab95fb023655865d11da"' "$BUILDER"; then
    ok t13
else
    bad t13 "meta.json reference changed"
fi

# t14 静态契约：release-audit 门禁无条件调用恢复（批 3 C1-②；无跳过行）
if grep -Fq 'scripts/check-release-package.sh "$OUT_DEB" || { echo "builder_package_boundary=FAIL"; exit 1; }' "$BUILDER" \
   && ! grep -Fq 'release-audit gate pending' "$BUILDER"; then
    ok t14
else
    bad t14 "gate call not restored or skip line remains"
fi

# t15 印证门禁恢复（dsh 返工裁决）：builder 不得在任何 post-trace 位置打
# 补丁——F 包内 DKMS 源 = 18 链 O_stage 快照本身（030-034 已随链入树），
# 补丁后状态由锁定 o_stage_tree_hash 覆盖；无 apply_fantgpu_runtime_fixes、
# 无顶层非法补丁名引用。
if grep -Fq 'APPLIED_SOURCE_FIXES="o-stage-materialized-030-chain-18' "$BUILDER" \
   && ! grep -Fq 'apply_fantgpu_runtime_fixes' "$BUILDER" \
   && ! grep -Fq 'fantgpu-hwinfo-audio-fallback.patch' "$BUILDER"; then
    ok t15
else
    bad t15 "post-trace patching not removed or 17-chain wiring missing"
fi

# t16 fallback semantics：先检查 hwinfo，再进入 HAL OS matcher；缺失 hwinfo
# 时必须选择 NORMAL 模式并立即返回，不能只检查存在性而仍调用危险路径。
guard_line=$(grep -nF 'if (!fh2m_hal_get_hwinfo_finished_status(chip->parent))' \
    "$ROOT/patches/030-030.patch" | cut -d: -f1 || true)
normal_line=$(grep -nF 'chip->dma_pointer_mode = DMA_POINTER_NORMAL;' \
    "$ROOT/patches/030-030.patch" | head -1 | cut -d: -f1 || true)
return_line=$(grep -nF $'+\t\treturn;' "$ROOT/patches/030-030.patch" |
    head -1 | cut -d: -f1 || true)
matcher_line=$(grep -nF 'fh2m_hal_os_release_match' \
    "$ROOT/patches/030-030.patch" | head -1 | cut -d: -f1 || true)
if [[ "$guard_line" =~ ^[0-9]+$ && "$normal_line" =~ ^[0-9]+$ &&
      "$return_line" =~ ^[0-9]+$ && "$matcher_line" =~ ^[0-9]+$ &&
      "$guard_line" -lt "$normal_line" && "$normal_line" -lt "$return_line" &&
      "$return_line" -lt "$matcher_line" ]]; then
    ok t16
else
    bad t16 "fallback guard/normal-return/matcher ordering is not fail-safe"
fi

# t17 入链印证：锁定的 O_stage 快照必须已含 030-034——reverse dry-run 成功
# 证明补丁已随链入树且可干净反转（正向 dry-run 必然失败：已应用）。
PATCH_TREE="$TMP/patch-tree"
mkdir -p "$PATCH_TREE"
if tar --use-compress-program=zstd -xf \
       "$ROOT/docs/planning/evidence/o-stage/5.0.0-i6/o-stage-snapshot.tar.zst" -C "$PATCH_TREE" \
   && patch --batch --forward --fuzz=0 --no-backup-if-mismatch --dry-run -R -s \
       -d "$PATCH_TREE/o-stage" -p1 < "$ROOT/patches/030-034.patch"; then
    ok t17
else
    bad t17 "030-034 not present in locked O_stage snapshot (reverse dry-run failed)"
fi

# t18 编译后必须按 i3 基线检查 shipped object 共享结构的实际 BTF 布局。
if grep -Fq 'builder_shipped_abi=PASS size=140536 members=115' "$BUILDER" \
   && grep -Fq '"pvr_resume_count": 140528' "$BUILDER" \
   && grep -Fq 'pahole -C dev_rsrc fantgpu.ko' "$BUILDER"; then
    ok t18
else
    bad t18 "compiled dev_rsrc ABI gate missing"
fi

run_builder VERSION=5.0.0-i5 SOURCE_DATE_EPOCH=1789516800
if [ "$RC" -eq 1 ] && grep -Fq "staging_ostage_generation=FAIL" "$TMP/b.out"; then
    ok t19_archived_i5
else
    bad t19_archived_i5 "rc=$RC"
fi

echo "PASS=$PASS FAIL=$FAILN"
[ "$FAILN" -eq 0 ] || exit 1
exit 0
