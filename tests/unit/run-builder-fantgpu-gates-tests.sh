#!/usr/bin/env bash
# tests/unit/run-builder-fantgpu-gates-tests.sh — builder F 分支早期门禁与静态契约单测
#
# 依据 docs/design/c3-a-4-reproducible-input-plan.md §三（改造点 9-13）与
# §四（postinst/md5sums/trace）：R29 候选 5.0.0-i10、血统判定 5.0.0-i*、
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
        env "$@" bash "$BUILDER" > "$TMP/b.out" 2> "$TMP/b.err"
    RC=$?
    set -e
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

# t02 i10 仅通过版本/epoch门，未通过源身份门，更不代表实际构建通过。
run_builder VERSION=5.0.0-i10 SOURCE_DATE_EPOCH=1790121600
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
run_builder VERSION=5.0.0-i10 SOURCE_DATE_EPOCH=1111111111
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

# t15 i9 用户授权cfg_detect顺序与fbdev兼容派生；父树trace与派生整树门分别标明，
# 不把i6原trace当作修改后证明。下面的实际派生回归补充本静态契约。
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
   && grep -Fq '"pvr_resume_count": 140528' "$ROOT/tools/check-fantgpu-shipped-abi.py" \
   && grep -Fq 'python3 "$ROOT/tools/check-fantgpu-shipped-abi.py"' "$BUILDER" \
   && grep -Fq -- '--module fantgpu.ko' "$BUILDER"; then
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

for version in 5.0.0-i{1..9}; do
    epoch=1788796800
    [[ "$version" != 5.0.0-i5 && "$version" != 5.0.0-i6 ]] || epoch=1789516800
    [[ "$version" != 5.0.0-i7 ]] || epoch=1790035200
    [[ "$version" != 5.0.0-i8 && "$version" != 5.0.0-i9 ]] || epoch=1790121600
    run_builder VERSION="$version" SOURCE_DATE_EPOCH="$epoch"
    if [[ "$RC" == 1 ]] && ! [[ -d "$TMP/stage" ]]; then
        ok "archived-$version"
    else
        bad "archived-$version" "rc=$RC or staging was written"
    fi
done
run_builder VERSION=5.0.0-i10 SOURCE_DATE_EPOCH=
if [[ "$RC" == 1 ]] && grep -Fq builder_repro=FAIL "$TMP/b.out"; then
    ok empty-epoch
else
    bad empty-epoch "rc=$RC"
fi
# Present headers but changed metadata must fail before manifest/compilation.
mkdir -p "$TMP/root/headers" "$TMP/root/docs/planning/evidence/o-stage/5.0.0-i6"
printf '%s\n' '{}' > "$TMP/root/docs/planning/evidence/o-stage/5.0.0-i6/5.0.0-i6.meta.json"
set +e
INNOGPU_ROOT="$TMP/root" VERSION=5.0.0-i10 SOURCE_DATE_EPOCH=1790121600 \
    KERNELDIR="$TMP/root/headers" STAGE_ROOT="$TMP/stage" \
    bash "$BUILDER" > "$TMP/b.out" 2> "$TMP/b.err"
RC=$?
set -e
if [[ "$RC" == 1 && ! -d "$TMP/stage" ]] && grep -Fq 'source meta identity drift' "$TMP/b.out"; then
    ok source-drift
else
    bad source-drift "rc=$RC"
fi

if python3 - "$BUILDER" "$ROOT" "$PATCH_TREE/o-stage" "$TMP" <<'PY'
import hashlib, os, pathlib, resource, subprocess, sys
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
builder, repo, tree, tmp = map(pathlib.Path, sys.argv[1:])
text = builder.read_text()
func = text[text.index("derive_fantgpu_source() {"):text.index("\nAPPLIED_SOURCE_FIXES=")]
names = ["cfg_detect.sh", "test_item.sh", "fantsrvkm/fantdpu_drm_fb.c", "fantpower/fant_input_event.c"]
originals = {name: (tree / name).read_bytes() for name in names}
before = originals["cfg_detect.sh"]
env = dict(os.environ, ROOT=str(repo))
def derive():
    return subprocess.run(["bash", "-ec", func + '\nderive_fantgpu_source "$1"', "fixture", str(tree)], env=env, capture_output=True, text=True)
p = derive()
assert p.returncode == 0, p.stderr
after = (tree / "cfg_detect.sh").read_bytes()
line = after[len(before):]
assert line == b'LC_ALL=C sort -o "$CFG_FILE_DIR/$CFG_FILE" "$CFG_FILE_DIR/$CFG_FILE"\n'
assert hashlib.sha256(after).hexdigest() == "f096890ea5662301f6eb805c2a5554aa9d027aebd1b2b8c28d31f84ae7c431ca"
fb_source = (tree / "fantsrvkm/fantdpu_drm_fb.c").read_text()
input_source = (tree / "fantpower/fant_input_event.c").read_text()
# Reject second application and unrelated parent drift before mutation.
assert derive().returncode != 0
for name, data in originals.items():
    (tree / name).write_bytes(data)
(tree / "unexpected").write_text("drift\n")
assert derive().returncode != 0 and (tree / "cfg_detect.sh").read_bytes() == before
header = tmp / "header.h"
env.update(CFG_FILE_DIR=str(tmp), CFG_FILE=header.name)
for content in [b'#define B 2\n#define A 1\n#define A 1\n', b'#define A 1\n#define B 2\n#define A 1\n']:
    header.write_bytes(content)
    subprocess.run(["bash", "-c", line.decode()], env=env, check=True)
    assert header.read_bytes() == b'#define A 1\n#define A 1\n#define B 2\n'
header.unlink()
header.mkdir()  # output cannot be replaced; original directory survives.
p = subprocess.run(["bash", "-c", line.decode()], env=env, capture_output=True)
assert p.returncode != 0 and header.is_dir()
# Execute the production helper/caller fragment, not a copied implementation.
alloc = fb_source[fb_source.index("static struct fb_info *fant_fbdev_helper_alloc("):fb_source.index("static inline void\nfant_fbdev_helper_fill_info")]
start = fb_source.index("\tinfo = fant_fbdev_helper_alloc(helper);")
call = fb_source[start:fb_source.index("\n\tfant_drm_info", start)]
harness = r'''#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#define KERNEL_VERSION(a,b,c) (((a)<<16)+((b)<<8)+(c))
#define DRM_VERSION KERNEL_VERSION(6,12,0)
#define ERR_PTR(x) ((void *)(intptr_t)(x))
#define PTR_ERR(x) ((intptr_t)(x))
#define IS_ERR_OR_NULL(x) (!(x) || (uintptr_t)(x) >= (uintptr_t)-4095)
struct fb_info {int dummy;};
struct drm_fb_helper {struct fb_info *info;};
static int calls;
#ifdef FANTGPU_DRM_FB_HELPER_ALLOC_INFO_PRESENT
static struct fb_info *drm_fb_helper_alloc_info(struct drm_fb_helper *h) {calls++; return h->info;}
#endif
''' + alloc + "\nstatic int probe(struct drm_fb_helper *helper) {struct fb_info *info; int err;\n" + call + "\nreturn 0; err_unlock_dev: return err; }\n" + r'''
int main(void) {
 struct fb_info info; struct drm_fb_helper helper = {&info};
 assert(probe(&helper) == 0);
 helper.info = ERR_PTR(-EIO); assert(probe(&helper) == -EIO);
 helper.info = NULL; assert(probe(&helper) == -ENOMEM);
#ifdef FANTGPU_DRM_FB_HELPER_ALLOC_INFO_PRESENT
 assert(calls == 3);
#else
 assert(calls == 0);
#endif
}
'''
source = tmp / "fb-helper.c"; source.write_text(harness)
for defines in [[], ["-DFANTGPU_DRM_FB_HELPER_ALLOC_INFO_PRESENT"]]:
    exe = tmp / "fb-helper"
    subprocess.run(["cc", "-Wall", "-Werror", *defines, str(source), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
# Run unchanged extracted production connect/disconnect on both buses, including
# every fallible acquisition. A stack checks reverse unwinding and exact owners.
def lifecycle(source):
    return source[source.index("static int fant_input_connect("):source.index("static const struct input_device_id fant_input_ids[]")]

original = originals["fantpower/fant_input_event.c"].decode()
start, end = original.index("static int fant_input_connect("), original.index("static void fant_input_disconnect(")
assert original[:start] == input_source[:input_source.index("static int fant_input_connect(")]
assert original[end:] == input_source[input_source.index("static void fant_input_disconnect("):]
input_harness = r'''#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#define GFP_KERNEL 0
#define BUS_BLUETOOTH 5
#define PWRD_DBG_INPUT 1
#define fantpwr_info(...) ((void)0)
#define fantpwr_notice(...) ((void)0)
struct notifier_block { int (*notifier_call)(struct notifier_block *, unsigned long, void *); int priority; };
struct input_handle_private { struct notifier_block pm_nb; unsigned long sys_stat; };
struct input_dev { struct { int bustype; } id; };
struct input_handler { int unused; };
struct input_device_id { int unused; };
struct input_handle { struct input_dev *dev; struct input_handler *handler; const char *name; void *private; };
static int fail, allocs, depth, stack[5], pm_calls;
static void *owners[2];
static struct notifier_block *notifier;
static struct input_handle *registered, *opened;
static void acquire(int id) { assert(depth < 5); stack[depth++] = id; }
static void release(int id) { assert(depth > 0 && stack[depth-1] == id); --depth; }
static void *kzalloc(size_t n, int flags) {
    (void)flags; ++allocs; assert(allocs <= 2);
    if (fail == allocs) return NULL;
    void *p = calloc(1, n); assert(p); owners[allocs-1] = p; acquire(allocs); return p;
}
static void kfree(void *p) {
    assert(p && (p == owners[0] || p == owners[1]));
    int i = p == owners[0] ? 0 : 1; release(i+1); owners[i] = NULL; free(p);
}
static int fant_input_pm_notifier(struct notifier_block *n, unsigned long a, void *d) { (void)n; (void)a; (void)d; return 0; }
static int register_pm_notifier(struct notifier_block *n) {
    ++pm_calls; assert(!notifier && n == &((struct input_handle_private *)owners[1])->pm_nb);
    if (fail == 3) return -EIO; /* contract injection, not evidence of kernel reachability */
    assert(n->notifier_call == fant_input_pm_notifier && n->priority == 0);
    notifier = n; acquire(3); return 0;
}
static int unregister_pm_notifier(struct notifier_block *n) {
    assert(n == notifier); release(3); notifier = NULL; return 0;
}
static int input_register_handle(struct input_handle *h) {
    assert(!registered && h == owners[0] && h->private == owners[1]);
    if (fail == 4) return -EIO;
    registered = h; acquire(4); return 0;
}
static int input_open_device(struct input_handle *h) {
    assert(h == registered && !opened);
    if (fail == 5) return -ENODEV;
    opened = h; acquire(5); return 0;
}
static void input_unregister_handle(struct input_handle *h) { assert(h == registered && !opened); release(4); registered = NULL; }
static void input_close_device(struct input_handle *h) { assert(h == opened); release(5); opened = NULL; }
static int fh2m_get_pwr_debug_lvl(void) { return 0; }
#include "input-functions.inc"
int main(int argc, char **argv) {
    assert(argc == 3); int bluetooth = atoi(argv[1]); fail = atoi(argv[2]);
    struct input_dev dev = {{bluetooth ? BUS_BLUETOOTH : 3}};
    struct input_handler handler = {0}; struct input_device_id id = {0};
    int expected = fail == 1 || fail == 2 ? -ENOMEM :
        fail == 4 || (fail == 3 && !bluetooth) ? -EIO : fail == 5 ? -ENODEV : 0;
    int rc = fant_input_connect(&handler, &dev, &id);
    assert(rc == expected);
    if (!rc) {
        assert(opened && opened->dev == &dev && opened->handler == &handler);
        assert(((struct input_handle_private *)opened->private)->sys_stat == 0);
        assert(depth == (bluetooth ? 4 : 5));
        fant_input_disconnect(opened);
    }
    assert(depth == 0 && !owners[0] && !owners[1] && !notifier && !registered && !opened);
    assert(pm_calls == (!bluetooth && fail != 1 && fail != 2));
    printf("input_cleanup bus=%s failure=%d connect_rc=%d resources=0 reverse_order=PASS\n", bluetooth ? "bluetooth" : "usb", fail, rc);
    return 0;
}
'''
source = tmp / "input-lifecycle.c"; source.write_text(input_harness)
for label, content in [("before", original), ("after", input_source)]:
    (tmp / "input-functions.inc").write_text(lifecycle(content))
    exe = tmp / ("input-" + label)
    subprocess.run(["cc", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter", str(source), "-o", str(exe)], check=True)
    for bluetooth in (0, 1):
        for failure in range(6):
            p = subprocess.run([str(exe), str(bluetooth), str(failure)], capture_output=True, text=True)
            reproduces = label == "before" and (failure in (2, 4, 5) or (failure == 3 and not bluetooth))
            assert p.returncode == (-6 if reproduces else 0), (label, bluetooth, failure, p.returncode, p.stderr)
            print(f"input_{label} bus={bluetooth} failure={failure} rc={p.returncode} " + ("EXPECTED_DEFECT" if reproduces else p.stdout.strip()))
print("input_cleanup_cases=12 passed=12 baseline_defects=7")
print("derived_exact_tree_sort_and_fb_ownership_contract=PASS")
PY
then
    ok derived-source-and-sort-contract
else
    bad derived-source-and-sort-contract "production derivation/order/failure contract"
fi

echo "PASS=$PASS FAIL=$FAILN"
[ "$FAILN" -eq 0 ] || exit 1
exit 0
