#!/usr/bin/env bash
# tests/unit/run-r16-f-payload-integrity-tests.sh — tools/r16-f-payload-integrity.py 单元验证
#
# 职责（docs/planning/5.0.0-i1-validation-plan.md §三 C3-a 放行前置 ③）：
# 用**小型合成 deb + 合成解包树**验证 来源身份 / deb↔解包字节忠实 /
# md5sums 精确子集与重定位子集 / 血统相干 / 固件对账 / 安装期暂存 /
# 确定性 / verify 复验 / 篡改检出 / 写保护 / fail-closed 输出纪律。
#
# 所有用例秒级，**不触碰真实 debs/ 与 build/**（同 run-o-stage-materialize-tests.sh
# 的假树口径）；只读 tools/r16-f-payload-integrity.py，只写 $TMP。
# 退出码：0=全过；1=任一用例失败；2=环境/构造错误。
#
# md5sums 子集计数由**构造期**已知的重定位前路径推导，不依赖被测工具的
# apply_relocations；该函数由 t18 用手写期望独立直测，避免自证循环。
#
# 用例：
#   t01 — 正向：合成夹具 → rc=0，三件产物落地，六项检查全 pass，双射成立
#   t02 — 确定性：两次独立构造+gen → 三件产物逐字节一致
#   t03 — verify：对新生成产物复验 → rc=0
#   t04 — verify 篡改检出：manifest 追加字节 → rc=1
#   t05 — B：解包树单文件字节被改 → rc=1 报 B:，且不写任何产物
#   t06 — C：精确子集 MD5 值被改 → rc=1 报 exact-path MD5
#   t07 — C：md5sums 出现重定位模型无法解释的路径 → rc=1 报 not explained
#   t08 — C：解包树 DEBIAN/postinst 与 deb 控制归档字节不一致 → rc=1
#   t09 — C：删掉一条重定位行 → rc=1 报 not reached by the relocation
#   t10 — E：符号链接悬空 → rc=1 报 dangling symlink
#   t11 — E：混入 O 血统 loader 文件名 → rc=1 报 O-lineage loader
#   t12 — E：必需 F 路径缺失 → rc=1 报 required F-lineage path missing
#   t13 — F：固件内容与基线记录值不符 → rc=1 报 firmware SHA-256 mismatch
#   t14 — G：DDX ABI 变体缺失 → rc=1 报 DDX ABI variant missing
#   t15 — A：基线 deb SHA-256 被偏移 → rc=1 报 deb SHA-256 mismatch
#   t16 — B：基线载荷文件数被偏移 → rc=1 报 count mismatch
#   t17 — C：基线 md5sums 精确子集计数被偏移 → rc=1 报 subset counts
#   t18 — apply_relocations 直测：无操作 / opt / privlib / opt+privlib 复合 /
#         docdir / 三段幂等
#   t19 — 守卫：--out-dir 落保护区 / deb 缺失 / 解包树缺失 / out-dir 不存在 /
#         基线键集不完整 → 一律 rc=2
#   t20 — fail-closed 输出纪律：审计中止时既有产物逐字节保留
#   t21 — G：postinst 缺 PCI ID → rc=1 报 postinst PCI ID missing
#   t22 — G：postinst 出现未锁定 PCI ID → rc=1 报 postinst PCI ID unexpected
#   t23 — 事务：注入 backup.1 失败 → rc=2，三件产物逐字节保留旧代，零残留
#   t24 — 事务：注入 commit.2 失败（manifest 已替换）→ rc=2，旧代全恢复，零残留
#   t25 — 事务：注入 cleanup.1 失败 → rc=0 + WARNING，journal/bak 残留；
#         再跑 gen → 残留清零、产物字节不变
#   t26 — 事务：提交中断现场（committing journal + bak + 部分新代）启动恢复 →
#         旧代全恢复，审计失败时（rc=1）旧代保留
#   t27 — 劫持：预置 <name>.tmp symlink → 不被跟随、victim 零改动、gen rc=0
#   t28 — 劫持：预置 <name>.bak symlink → fail-closed rc=2（无 journal 的
#         .bak 一律拒绝）、victim 零改动、零产物
#   t29 — 劫持：预置 journal symlink → fail-closed rc=2、victim 零改动
#   t30 — 事务：空目录首代生成注入 commit.2 → rc=2、零产物、零残留
#         （无旧备份的已提交新产物必须删除，codex re-review P1-1）
#   t31 — 恢复：无 journal + 仅 manifest.bak → fail-closed rc=2 保留现场、
#         不恢复（codex re-review P1-2 不完整 backup）
#   t32 — 恢复：无 journal + 三件 .bak 齐全 → fail-closed rc=2 保留现场、
#         不恢复（codex re-review P1-2 无 journal backup）
#   t33 — 恢复：journal=committing + 备份内容不一致（sha.bak≠manifest.bak）
#         → fail-closed rc=2 保留现场（codex re-review P1-2 备份不一致）
#   t34 — 事务：注入 rollback.1（回滚失败）→ rc=2、报 INCOMPLETE 不谎称已恢复、
#         journal 残留 rolling_back；再跑 gen 恢复旧代并清零残留
#   t35 — 事务：注入 "commit.2,rollback.2" → 反序恢复中途失败残留
#         manifest/sha.bak（前缀），启动恢复按 rolling_back 前缀不变量
#         恢复旧代并清零（codex re-review P1-1）
#   t36 — 事务：注入 "commit.2,rollback.3" → 残留全量 .bak（前缀），
#         启动恢复完成（codex re-review P1-1）
#   t37 — 恢复：journal=committed + dst JSON 损坏（not-json）→ fail-closed
#         rc=2 保留现场、不删 .bak/journal（codex re-review P1-2）
#   t38 — 恢复：journal=committing + 备份 JSON 与 manifest 不绑定 →
#         fail-closed rc=2 保留现场（codex re-review P1-2）
#   t39 — 恢复：启动恢复再次中断（注入 rollback.2）→ journal 已切
#         rolling_back、残留前缀 .bak，二次启动完成恢复
#         （codex re-review#4 P1-1）
#   t40 — 回滚：journal.rolling_back 切换失败 → 不修改任何 .bak/dst、
#         journal 保持 committing（完整备份集），二次启动恢复
#         （codex re-review#4 P1-1）
#   t41 — 守卫：事务开始前旧产物部分存在（三件非全有全无）→ rc=2
#         「partially present」、现场零改动（codex re-review#4 P1-2）
#   t42 — 恢复：backing_up + pre_existing=[F,F,F] + manifest.bak →
#         fail-closed rc=2 保留现场（.bak 不得对应未预存在路径，
#         codex re-review#4 P1-2）
#   t43 — 恢复：journal pre_existing=[F,T,T]（部分）→ fail-closed rc=2
#         保留现场（journal 字段非法，codex re-review#4 P1-2）
#   t44 — 暂存：注入 stage.1（首个暂存文件写入失败）→ rc=2、
#         零产物、零残留（含隐藏 .f-payload.txn.*，codex re-review#5 P2）
#   t45 — 暂存：注入 stage.2（第二个暂存文件写入失败）→ rc=2、
#         零产物、零残留（codex re-review#5 P2）
#   t46 — 暂存清理失败：注入 "stage.2,stage-cleanup.1" → rc=2、
#         报 cleanup INCOMPLETE、无 traceback、残留由下次 gen 恢复清零
#         （codex re-review#7 P2-1）
#   t47 — journal 清理失败：注入
#         "commit.2,journal.rolling_back,journal-cleanup.1" → rc=2、
#         报 INCOMPLETE、无 traceback、原始异常不被清理异常覆盖、
#         journal 保持 committing；二次启动恢复（codex re-review#7 P2-2）
#   t48 — journal 删除失败（committed 清理）：注入 journal-unlink.committed
#         → rc=0 + WARNING、无 traceback、残留由同次 gen 覆盖清除
#         （codex re-review#9 P2）
#   t49 — journal 删除失败（rolling_back 恢复）：注入
#         journal-unlink.recovered → rc=0 + WARNING、无 traceback、
#         旧代恢复、残留由同次 gen 清除（codex re-review#9 P2）
#   t50 — journal 删除失败（回滚）：注入
#         "commit.2,journal-unlink.rollback" → rc=2 报 previous generation
#         restored（恢复确已完成，不谎称 INCOMPLETE）+ WARNING、
#         无 traceback；二次启动清零（codex re-review#9 P2）

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOL="$ROOT/tools/r16-f-payload-integrity.py"
cd "$ROOT"
LC_ALL=C
export LC_ALL

[ -f "$TOOL" ] || { echo "FATAL: tool not found: $TOOL" >&2; exit 2; }
for need in dpkg-deb python3; do
    command -v "$need" >/dev/null 2>&1 || { echo "FATAL: $need is required" >&2; exit 2; }
done

TMP="$(mktemp -d "${TMPDIR:-/tmp}/fpi-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT INT TERM HUP

PASS=0; FAILN=0
ok()  { PASS=$((PASS + 1)); echo "ok  $1"; }
bad() { FAILN=$((FAILN + 1)); echo "BAD $1: $2" >&2; }

# ---------------------------------------------------------------- 夹具构造器
cat > "$TMP/mkfixture.py" <<'PY'
#!/usr/bin/env python3
"""构造合成 F 血统 deb + 解包树 + 配套基线 JSON；输出 shell 赋值行。"""
import hashlib
import json
import os
import re
import subprocess
import sys

PRIV = "fantgpu-fh2m"
TRI_RE = re.compile(
    r"^(.*usr/lib/(?:x86_64|i386)-linux-gnu/)" + PRIV + r"/([^/]+)$")
MD5_ROW_RE = re.compile(r"^([0-9a-f]{32})[ \t]+(.+)$")

CONTROL = {
    "Package": "fantgpu-fh2m",
    "Version": "0.0.1-fixture",
    "Architecture": "amd64",
    "Maintainer": "fixture <fixture@example.invalid>",
    "Description": "Synthetic fixture payload",
}
CONTROL_FILES = ("control", "md5sums", "postinst", "postrm", "prerm")
DDX_DIR = "opt/%s/usr/lib/xorg/modules/drivers" % PRIV
DDX_ABIS = ("1.19", "1.20", "1.21")
FW_CONTENT = {
    "lib/firmware/fantgpu/fh2m/fh2m.fw": b"FIXTURE-FH2M-FIRMWARE\n",
    "lib/firmware/fantgpu/fh2m/fh2c.fw": b"FIXTURE-FH2C-FIRMWARE\n",
}
GBM_REAL = "usr/lib/x86_64-linux-gnu/%s/libfh2m_gbm.so.1.0.0" % PRIV
REQUIRED = (
    "etc/OpenCL/vendors/FANT_fh2m.icd",
    "etc/modprobe.d/blacklist-fh2m.conf",
    "lib/firmware/fantgpu/fh2m/fh2c.fw",
    "lib/firmware/fantgpu/fh2m/fh2m.fw",
    "usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so",
    "usr/lib/x86_64-linux-gnu/gbm/fh2m_gbm.so",
    "usr/share/doc/%s/changelog.Debian.gz" % PRIV,
    "usr/share/glvnd/egl_vendor.d/00_fh2m.json",
    GBM_REAL,
) + tuple("%s/fh2m_drv.so.%s" % (DDX_DIR, a) for a in DDX_ABIS)
FORBIDDEN = ("innogpu_dri.so", "innogpu_drv.so", "innogpu_drv_video.so",
             "innogpu_gbm.so", "inno_drv_video.so")
MUTATIONS = ("none", "o-loader", "drop-required", "firmware-drift", "drop-ddx")
POSTMUTATIONS = ("none", "unpack-corrupt", "control-drift", "dangling")
CTRLMUTATIONS = ("none", "md5-value", "md5-bogus", "md5-drop",
                 "pci-missing", "pci-wrong")
PCI_ID = "1ec8:9810"


def preloc(path):
    """载荷路径 → (重定位前路径, 类名元组)。构造期已知，与被测工具无关。"""
    applied = []
    q = path
    opt_pre = "opt/%s/" % PRIV
    if q.startswith(opt_pre):
        q = "opt/" + q[len(opt_pre):]
        applied.append("opt")
    m = TRI_RE.match(q)
    if m:
        q = m.group(1) + m.group(2)
        applied.append("privlib")
    doc_pre = "usr/share/doc/%s/" % PRIV
    if q.startswith(doc_pre):
        q = "usr/share/doc/" + q[len(doc_pre):]
        applied.append("docdir")
    return q, tuple(applied)


def payload(mutate):
    """返回 ({相对路径: bytes}, {相对路径: 符号链接目标})。"""
    files = {
        "etc/OpenCL/vendors/FANT_fh2m.icd": b"libFTOCL_fh2m.so\n",
        "etc/modprobe.d/blacklist-fh2m.conf": b"blacklist innogpu\n",
        "etc/vulkan/icd.d/fh2m_conf.json":
            b'{"ICD":{"library_path":"libVK_FANT_fh2m.so"}}\n',
        "usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so": b"FIXTURE-DRI\n",
        "usr/lib/x86_64-linux-gnu/dri/fh2m_drv_video.so": b"FIXTURE-VA-API\n",
        "usr/share/doc/%s/changelog.Debian.gz" % PRIV: b"FIXTURE-CHANGELOG\n",
        "usr/share/glvnd/egl_vendor.d/00_fh2m.json":
            b'{"ICD":{"library_path":"libEGL_fh2m.so.0"}}\n',
        GBM_REAL: b"FIXTURE-GBM\n",
        "opt/%s/usr/lib/x86_64-linux-gnu/%s/libffi.so.8.1.3" % (PRIV, PRIV):
            b"FIXTURE-FFI\n",
        "opt/%s/usr/sbin/sw-fant-gl" % PRIV: b"FIXTURE-SWGL\n",
    }
    files.update(FW_CONTENT)
    for abi in DDX_ABIS:
        files["%s/fh2m_drv.so.%s" % (DDX_DIR, abi)] = \
            b"FIXTURE-DDX-" + abi.encode("ascii") + b"\n"
    links = {
        "usr/lib/x86_64-linux-gnu/gbm/fh2m_gbm.so":
            "../%s/libfh2m_gbm.so.1.0.0" % PRIV,
        "opt/%s/usr/lib/x86_64-linux-gnu/%s/libffi.so.8" % (PRIV, PRIV):
            "libffi.so.8.1.3",
    }
    if mutate == "o-loader":
        files["usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so"] = b"O-LINEAGE\n"
    elif mutate == "drop-required":
        del files["usr/share/glvnd/egl_vendor.d/00_fh2m.json"]
    elif mutate == "firmware-drift":
        files["lib/firmware/fantgpu/fh2m/fh2m.fw"] = b"DRIFTED-FIRMWARE\n"
    elif mutate == "drop-ddx":
        del files["%s/fh2m_drv.so.1.20" % DDX_DIR]
    return files, links


def write_payload(root, files, links):
    for path in sorted(set(files) | set(links)):
        dest = os.path.join(root, path)
        parent = os.path.dirname(dest)
        if parent:
            os.makedirs(parent, exist_ok=True)
        if path in links:
            if os.path.lexists(dest):
                os.unlink(dest)
            os.symlink(links[path], dest)
        else:
            with open(dest, "wb") as fh:
                fh.write(files[path])
            os.chmod(dest, 0o644)


def inventory(root):
    """解包树 → (常规文件 {path: md5}, 符号链接集合, 载荷目录数)。"""
    reg, sym, dirs = {}, set(), 0
    for dp, dn, fn in os.walk(root):
        for n in dn:
            rel = os.path.relpath(os.path.join(dp, n), root)
            if rel != "DEBIAN":
                dirs += 1
        for n in fn:
            ap = os.path.join(dp, n)
            rel = os.path.relpath(ap, root).replace(os.sep, "/")
            if rel.startswith("DEBIAN/"):
                continue
            if os.path.islink(ap):
                sym.add(rel)
            else:
                with open(ap, "rb") as fh:
                    reg[rel] = hashlib.md5(fh.read()).hexdigest()
    return reg, sym, dirs


def build(out, mutate, postmutate, ctrl_mutate, skew):
    for name, val, allowed in (("mutate", mutate, MUTATIONS),
                               ("postmutate", postmutate, POSTMUTATIONS),
                               ("ctrl_mutate", ctrl_mutate, CTRLMUTATIONS)):
        if val not in allowed:
            raise SystemExit("unknown %s: %r" % (name, val))
    debroot = os.path.join(out, "debroot")
    deb = os.path.join(out, "fixture.deb")
    unpack = os.path.join(out, "unpack")
    baseline = os.path.join(out, "baseline.json")
    artifacts = os.path.join(out, "artifacts")
    for d in (debroot, unpack, artifacts):
        os.makedirs(d, exist_ok=True)

    files, links = payload(mutate)
    if postmutate == "dangling":
        links["usr/lib/x86_64-linux-gnu/gbm/fh2m_gbm.so"] = "../nowhere.so"
    write_payload(debroot, files, links)

    # md5sums 按**重定位前**路径生成，复刻上游打包缺陷；类归属构造期记录
    rows, row_class = {}, {}
    by_class = {}
    for path in sorted(files):
        pre, classes = preloc(path)
        rows[pre] = hashlib.md5(files[path]).hexdigest()
        row_class[pre] = classes
        if classes:
            key = "+".join(classes)
            by_class[key] = by_class.get(key, 0) + 1
    exact_rows = sorted(p for p in rows if not row_class[p])
    reloc_rows = sorted(p for p in rows if row_class[p])
    if ctrl_mutate == "md5-value":
        rows[exact_rows[0]] = "0" * 32
    elif ctrl_mutate == "md5-bogus":
        rows["usr/share/unexplainable/fh2m.bin"] = "1" * 32
    elif ctrl_mutate == "md5-drop":
        victim = reloc_rows[0]
        del rows[victim]
        key = "+".join(row_class[victim])
        by_class[key] -= 1
        if by_class[key] == 0:
            del by_class[key]

    debian = os.path.join(debroot, "DEBIAN")
    os.makedirs(debian, exist_ok=True)
    with open(os.path.join(debian, "control"), "w", encoding="utf-8") as fh:
        for k in ("Package", "Version", "Architecture", "Maintainer",
                  "Description"):
            fh.write("%s: %s\n" % (k, CONTROL[k]))
    with open(os.path.join(debian, "md5sums"), "w", encoding="utf-8") as fh:
        for p in sorted(rows):
            fh.write("%s  %s\n" % (rows[p], p))
    for name in ("postinst", "postrm", "prerm"):
        p = os.path.join(debian, name)
        with open(p, "w", encoding="utf-8") as fh:
            fh.write("#!/bin/bash\nset -e\n")
            if name == "postinst":
                if ctrl_mutate == "pci-missing":
                    fh.write("# fixture: no device gate\n")
                elif ctrl_mutate == "pci-wrong":
                    fh.write("# fixture device gate: 1ec8:9999\n")
                else:
                    fh.write("# fixture device gate: %s\n" % PCI_ID)
            fh.write("exit 0\n")
        os.chmod(p, 0o755)

    subprocess.run(["dpkg-deb", "--root-owner-group", "-b", debroot, deb],
                   check=True, capture_output=True)
    subprocess.run(["dpkg-deb", "-R", deb, unpack], check=True,
                   capture_output=True)

    if postmutate == "unpack-corrupt":
        with open(os.path.join(
                unpack, "usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so"),
                "ab") as fh:
            fh.write(b"CORRUPT")
    elif postmutate == "control-drift":
        with open(os.path.join(unpack, "DEBIAN", "postinst"), "a",
                  encoding="utf-8") as fh:
            fh.write("# drifted\n")
    elif postmutate == "dangling":
        victim = os.path.join(unpack,
                              "usr/lib/x86_64-linux-gnu/gbm/fh2m_gbm.so")
        os.unlink(victim)
        os.symlink("../nowhere.so", victim)

    # ---- 基线：结构性计数从**实际夹具**推导；语义期望取构造期常量
    reg, sym, dirs = inventory(unpack)
    md5rows = {}
    with open(os.path.join(unpack, "DEBIAN", "md5sums"),
              encoding="utf-8") as fh:
        for line in fh:
            if not line.strip():
                continue
            m = MD5_ROW_RE.match(line.rstrip("\n"))
            if not m:
                raise SystemExit("fixture md5sums malformed: %r" % line)
            md5rows[m.group(2)] = m.group(1)
    exact = sum(1 for p in md5rows if p in reg)
    with open(deb, "rb") as fh:
        deb_sha = hashlib.sha256(fh.read()).hexdigest()
    bl = {
        "deb_sha256": deb_sha,
        "deb_size": os.path.getsize(deb),
        "control": dict(CONTROL),
        "control_files": list(CONTROL_FILES),
        "counts": {"dir": dirs, "file": len(reg), "symlink": len(sym),
                   "md5sums_rows": len(md5rows)},
        "md5sums_exact": exact,
        "md5sums_relocated_total": len(md5rows) - exact,
        "md5sums_relocated_by_class": by_class,
        "required_f_paths": list(REQUIRED),
        "forbidden_o_loader_names": list(FORBIDDEN),
        "firmware_sha256": {
            p: [hashlib.sha256(v).hexdigest(),
                hashlib.sha256(v).hexdigest()[:8], "fixture"]
            for p, v in sorted(FW_CONTENT.items())},
        "ddx_abi_variants": list(DDX_ABIS),
        "pci_ids": [PCI_ID],
    }
    for key, val in (skew or {}).items():
        if key == "counts.file":
            bl["counts"]["file"] = val
        elif key not in bl:
            raise SystemExit("skew key not in baseline: %s" % key)
        else:
            bl[key] = val
    with open(baseline, "w", encoding="utf-8") as fh:
        json.dump(bl, fh, indent=2, sort_keys=True)
        fh.write("\n")
    print("DEB=%s" % deb)
    print("UNPACK=%s" % unpack)
    print("BASELINE=%s" % baseline)
    print("ARTIFACTS=%s" % artifacts)


if __name__ == "__main__":
    out = sys.argv[1]
    spec = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
    os.makedirs(out, exist_ok=True)
    build(out, spec.get("mutate", "none"), spec.get("postmutate", "none"),
          spec.get("ctrl_mutate", "none"), spec.get("skew"))
PY

# mkfixture <name> [json-spec] → eval 后得到 DEB/UNPACK/BASELINE/ARTIFACTS
mkfixture() {
    local name="$1" spec="${2-}"
    [ -n "$spec" ] || spec='{}'
    rm -rf "$TMP/$name"
    local out
    out="$(python3 "$TMP/mkfixture.py" "$TMP/$name" "$spec")" || return 2
    printf '%s\n' "$out"
}

RC=0; OUTTEXT=""; ERRTXT=""; OUT_OVERRIDE=""
run_tool() {  # run_tool <gen|verify> [extra args...]；使用 DEB/UNPACK/...
    local cmd="$1"; shift
    local out="${OUT_OVERRIDE:-$ARTIFACTS}"
    set +e
    OUTTEXT="$(python3 "$TOOL" "$cmd" --root "$TMP" --deb "$DEB" \
        --unpack "$UNPACK" --out-dir "$out" --baseline "$BASELINE" \
        "$@" 2>"$TMP/stderr")"
    RC=$?
    set -e
    ERRTXT="$(cat "$TMP/stderr")"
}

artifact_count() {
    local n=0 f
    for f in f-payload.manifest.tsv f-payload.manifest.tsv.sha256 \
             f-payload-integrity.json; do
        [ -f "$ARTIFACTS/$f" ] && n=$((n + 1))
    done
    printf '%d' "$n"
}

# ---------------------------------------------------------------- t01 正向
eval "$(mkfixture t01)" || { echo "FATAL: t01 fixture build failed" >&2; exit 2; }
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t01 "expected rc=0, got rc=$RC"
    printf '%s\n' "$ERRTXT" | head -10 >&2
else
    summary="$(ARTIFACTS="$ARTIFACTS" python3 - <<'PY'
import json, os
d = json.load(open(os.path.join(os.environ["ARTIFACTS"],
                                "f-payload-integrity.json")))
c = d["checks"]
print(d["overall_pass"],
      all(v["pass"] for v in c.values()),
      len(c),
      d["baseline_source"]["source"],
      c["C_control_and_md5sums"]["relocation_bijection"],
      c["E_lineage_coherence"]["dangling_symlinks"] == [])
PY
)"
    n="$(artifact_count)"
    if [ "$n" -eq 3 ] && [ "$summary" = "True True 6 override True True" ]; then
        ok t01
    else
        bad t01 "artifacts=$n summary=[$summary]"
    fi
fi

# ------------------------------------------------------------ t02 确定性
# 同一夹具、两个输出目录、两次 gen：检验**工具自身**的字节幂等，而不是两个
# 独立构造的 deb 相同（dpkg-deb 嵌入 mtime，两个 deb 本就不逐字节相等）。
eval "$(mkfixture t02)" || exit 2
mkdir -p "$TMP/t02-run2"
run_tool gen; RA=$RC
cp "$ARTIFACTS/f-payload.manifest.tsv" "$TMP/a.tsv"
cp "$ARTIFACTS/f-payload.manifest.tsv.sha256" "$TMP/a.sha"
cp "$ARTIFACTS/f-payload-integrity.json" "$TMP/a.json"
OUT_OVERRIDE="$TMP/t02-run2"
run_tool gen; RB=$RC
OUT_OVERRIDE=""
if [ "$RA" -ne 0 ] || [ "$RB" -ne 0 ]; then
    bad t02 "gen rc=$RA/$RB"
elif cmp -s "$TMP/a.tsv" "$TMP/t02-run2/f-payload.manifest.tsv" \
     && cmp -s "$TMP/a.sha" "$TMP/t02-run2/f-payload.manifest.tsv.sha256" \
     && cmp -s "$TMP/a.json" "$TMP/t02-run2/f-payload-integrity.json"; then
    ok t02
else
    bad t02 "artifacts differ between two runs over the same fixture"
fi

# ---------------------------------------------------------- t03/t04 verify
eval "$(mkfixture t03)" || exit 2
run_tool gen
run_tool verify
if [ "$RC" -eq 0 ]; then ok t03; else bad t03 "verify rc=$RC (want 0)"; fi
printf 'x\n' >> "$ARTIFACTS/f-payload.manifest.tsv"
run_tool verify
if [ "$RC" -eq 1 ]; then ok t04; else bad t04 "tampered verify rc=$RC (want 1)"; fi

# ------------------------------------------------------- t05-t17 负向
neg() {  # neg <label> <spec-json> <stderr-needle>
    local label="$1" spec="$2" needle="$3"
    eval "$(mkfixture "$label" "$spec")" || { bad "$label" "fixture build"; return; }
    run_tool gen
    if [ "$RC" -ne 1 ]; then
        bad "$label" "expected rc=1, got rc=$RC"
        return
    fi
    if ! printf '%s' "$ERRTXT" | grep -Fq -- "$needle"; then
        bad "$label" "stderr lacks needle: $needle"
        printf '%s\n' "$ERRTXT" | head -6 >&2
        return
    fi
    local n; n="$(artifact_count)"
    if [ "$n" -ne 0 ]; then
        bad "$label" "wrote $n artifact(s) despite audit failure"
        return
    fi
    ok "$label"
}

neg t05 '{"postmutate":"unpack-corrupt"}' "B:"
neg t06 '{"ctrl_mutate":"md5-value"}'     "exact-path MD5 value mismatches"
neg t07 '{"ctrl_mutate":"md5-bogus"}'     "not explained by the relocation"
neg t08 '{"postmutate":"control-drift"}'  "DEBIAN/ bytes differ"
neg t09 '{"ctrl_mutate":"md5-drop"}'      "not reached by the relocation"
neg t10 '{"postmutate":"dangling"}'       "dangling symlink"
neg t11 '{"mutate":"o-loader"}'           "O-lineage loader filename present"
neg t12 '{"mutate":"drop-required"}'      "required F-lineage path missing"
neg t13 '{"mutate":"firmware-drift"}'     "firmware SHA-256 mismatch"
neg t14 '{"mutate":"drop-ddx"}'           "DDX ABI variant missing"
neg t15 '{"skew":{"deb_sha256":"0000"}}'  "deb SHA-256 mismatch"
neg t16 '{"skew":{"counts.file":1}}'      "count mismatch"
neg t17 '{"skew":{"md5sums_exact":1}}'    "md5sums subset counts differ"
neg t21 '{"ctrl_mutate":"pci-missing"}'   "postinst PCI ID missing"
neg t22 '{"ctrl_mutate":"pci-wrong"}'     "postinst PCI ID unexpected"

# ------------------------------------------- t18 apply_relocations 直测
if python3 - "$TOOL" <<'PY'
import importlib.util, sys
spec = importlib.util.spec_from_file_location("fpi", sys.argv[1])
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
cases = [
    # (输入, 期望类, 期望输出)
    # 回归锁定：三元组目录下的**子目录**路径不得触发 privlib 重写
    # （原 (.+) 正则会把 dri/fh2m_dri.so 误改写成 fantgpu-fh2m/dri/…）
    ("usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so", (),
     "usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so"),
    ("usr/lib/x86_64-linux-gnu/gbm/fh2m_gbm.so", (),
     "usr/lib/x86_64-linux-gnu/gbm/fh2m_gbm.so"),
    ("usr/lib/x86_64-linux-gnu/va/drivers/fh2m_drv_video.so", (),
     "usr/lib/x86_64-linux-gnu/va/drivers/fh2m_drv_video.so"),
    ("etc/vulkan/icd.d/fh2m_conf.json", (),
     "etc/vulkan/icd.d/fh2m_conf.json"),
    ("opt/lib/systemd/system/sw.service", ("opt",),
     "opt/fantgpu-fh2m/lib/systemd/system/sw.service"),
    ("usr/lib/i386-linux-gnu/libEGL_fh2m.so.1", ("privlib",),
     "usr/lib/i386-linux-gnu/fantgpu-fh2m/libEGL_fh2m.so.1"),
    ("opt/usr/lib/x86_64-linux-gnu/libffi.so.8.1.3", ("opt", "privlib"),
     "opt/fantgpu-fh2m/usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libffi.so.8.1.3"),
    ("usr/share/doc/changelog.Debian.gz", ("docdir",),
     "usr/share/doc/fantgpu-fh2m/changelog.Debian.gz"),
    # 幂等：已含该组件则不重复插入
    ("opt/fantgpu-fh2m/usr/sbin/sw-fant-gl", (),
     "opt/fantgpu-fh2m/usr/sbin/sw-fant-gl"),
    ("usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libgbm.so.1", (),
     "usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libgbm.so.1"),
    ("usr/share/doc/fantgpu-fh2m/changelog.Debian.gz", (),
     "usr/share/doc/fantgpu-fh2m/changelog.Debian.gz"),
    ("opt/fantgpu-fh2m/usr/lib/i386-linux-gnu/fantgpu-fh2m/libffi.so.8", (),
     "opt/fantgpu-fh2m/usr/lib/i386-linux-gnu/fantgpu-fh2m/libffi.so.8"),
]
bad = 0
for src, want_cls, want_out in cases:
    got_cls, got_out = m.apply_relocations(src)
    if got_cls != want_cls or got_out != want_out:
        bad += 1
        print("MISMATCH %s -> %s %s (want %s %s)"
              % (src, got_cls, got_out, want_cls, want_out))
for src, _, want_out in cases:
    _, once = m.apply_relocations(src)
    _, twice = m.apply_relocations(once)
    if once != want_out or twice != once:
        bad += 1
        print("NOT IDEMPOTENT %s -> %s -> %s" % (src, once, twice))
sys.exit(1 if bad else 0)
PY
then ok t18; else bad t18 "apply_relocations cases failed"; fi

# ------------------------------------------------------------- t19 守卫
eval "$(mkfixture t19)" || exit 2
mkdir -p "$TMP/fakeroot/build"
guard() {  # guard <label> <args...> → 期望 rc=2
    local label="$1"; shift
    set +e
    python3 "$TOOL" "$@" >/dev/null 2>"$TMP/guard.err"
    local rc=$?
    set -e
    if [ "$rc" -eq 2 ]; then
        ok "$label"
    else
        bad "$label" "expected rc=2, got rc=$rc ($(head -1 "$TMP/guard.err"))"
    fi
}
guard t19/out-dir-protected gen --root "$TMP/fakeroot" --deb "$DEB" \
    --unpack "$UNPACK" --out-dir "$TMP/fakeroot/build/out" \
    --baseline "$BASELINE"
guard t19/deb-missing gen --root "$TMP" --deb "$TMP/nope.deb" \
    --unpack "$UNPACK" --out-dir "$ARTIFACTS" --baseline "$BASELINE"
guard t19/unpack-missing gen --root "$TMP" --deb "$DEB" \
    --unpack "$TMP/nope-tree" --out-dir "$ARTIFACTS" --baseline "$BASELINE"
guard t19/out-dir-missing gen --root "$TMP" --deb "$DEB" --unpack "$UNPACK" \
    --out-dir "$TMP/no-such-dir" --baseline "$BASELINE"
python3 -c "
import json, sys
d = json.load(open(sys.argv[1]))
d.pop('deb_size', None)
json.dump(d, open(sys.argv[2], 'w'))
" "$BASELINE" "$TMP/bad-baseline.json"
guard t19/baseline-incomplete gen --root "$TMP" --deb "$DEB" \
    --unpack "$UNPACK" --out-dir "$ARTIFACTS" \
    --baseline "$TMP/bad-baseline.json"

# --------------------------------------------- t20 fail-closed 输出纪律
eval "$(mkfixture t20)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t20 "baseline gen rc=$RC"
else
    before=""
    after=""
    for f in f-payload.manifest.tsv f-payload.manifest.tsv.sha256 \
             f-payload-integrity.json; do
        before="$before$(sha256sum "$ARTIFACTS/$f" | cut -d' ' -f1) "
    done
    printf 'CORRUPT\n' >> "$UNPACK/usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so"
    run_tool gen
    for f in f-payload.manifest.tsv f-payload.manifest.tsv.sha256 \
             f-payload-integrity.json; do
        after="$after$(sha256sum "$ARTIFACTS/$f" | cut -d' ' -f1) "
    done
    if [ "$RC" -eq 1 ] && [ "$before" = "$after" ]; then
        ok t20
    else
        bad t20 "rc=$RC (want 1) or artifacts mutated on abort"
    fi
fi

# ------------------------------------------- 事务与劫持辅助
residue_count() {  # journal/.bak/.f-payload.txn.* 残留计数（含隐藏文件）
    local n=0 f base
    for f in "$ARTIFACTS"/* "$ARTIFACTS"/.[!.]*; do
        [ -e "$f" ] || [ -L "$f" ] || continue
        base="$(basename "$f")"
        case "$base" in
            .f-payload.txn.*|*.bak|f-payload.commit.journal) n=$((n + 1)) ;;
        esac
    done
    printf '%d' "$n"
}

trio_sha() {  # 三件产物 sha256 拼接（同 t20 口径）
    local out="" f
    for f in f-payload.manifest.tsv f-payload.manifest.tsv.sha256 \
             f-payload-integrity.json; do
        out="$out$(sha256sum "$ARTIFACTS/$f" | cut -d' ' -f1) "
    done
    printf '%s' "$out"
}

# ------------------------------ t23 事务：备份阶段注入失败（旧代零改动）
eval "$(mkfixture t23)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t23 "baseline gen rc=$RC"
else
    before="$(trio_sha)"
    export FPI_FAIL_INJECT=backup.1
    run_tool gen
    unset FPI_FAIL_INJECT
    after="$(trio_sha)"
    resid="$(residue_count)"
    if [ "$RC" -eq 2 ] && [ "$before" = "$after" ] && [ "$resid" -eq 0 ]; then
        ok t23
    else
        bad t23 "rc=$RC (want 2) unchanged=$([ "$before" = "$after" ] \
            && echo y || echo n) residue=$resid"
    fi
fi

# ------------------- t24 事务：提交阶段注入失败（manifest 已替换须回滚）
eval "$(mkfixture t24)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t24 "baseline gen rc=$RC"
else
    before="$(trio_sha)"
    export FPI_FAIL_INJECT=commit.2
    run_tool gen
    unset FPI_FAIL_INJECT
    after="$(trio_sha)"
    resid="$(residue_count)"
    if [ "$RC" -eq 2 ] && [ "$before" = "$after" ] && [ "$resid" -eq 0 ]; then
        ok t24
    else
        bad t24 "rc=$RC (want 2) unchanged=$([ "$before" = "$after" ] \
            && echo y || echo n) residue=$resid"
    fi
fi

# ---------------- t25 事务：清理阶段注入失败 → 残留由下次 gen 启动恢复
eval "$(mkfixture t25)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t25 "baseline gen rc=$RC"
else
    before="$(trio_sha)"
    export FPI_FAIL_INJECT=cleanup.1
    run_tool gen
    RC1=$RC
    unset FPI_FAIL_INJECT
    resid1="$(residue_count)"
    run_tool gen
    resid2="$(residue_count)"
    after="$(trio_sha)"
    if [ "$RC1" -eq 0 ] && [ "$RC" -eq 0 ] && [ "$resid1" -gt 0 ] \
       && [ "$resid2" -eq 0 ] && [ "$before" = "$after" ]; then
        ok t25
    else
        bad t25 "rc=$RC1/$RC resid1=$resid1 resid2=$resid2 unchanged=\
$([ "$before" = "$after" ] && echo y || echo n)"
    fi
fi

# --------------------- t26 事务：提交中断现场启动恢复（旧代恢复且审计失败保留）
eval "$(mkfixture t26)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t26 "baseline gen rc=$RC"
else
    old_trio="$(trio_sha)"
    cp "$ARTIFACTS/f-payload.manifest.tsv" "$ARTIFACTS/f-payload.manifest.tsv.bak"
    cp "$ARTIFACTS/f-payload.manifest.tsv.sha256" \
       "$ARTIFACTS/f-payload.manifest.tsv.sha256.bak"
    cp "$ARTIFACTS/f-payload-integrity.json" "$ARTIFACTS/f-payload-integrity.json.bak"
    printf '{"state": "committing", "pre_existing": [true, true, true]}\n' \
        > "$ARTIFACTS/f-payload.commit.journal"
    printf 'FOREIGN-NEW-GEN\n' > "$ARTIFACTS/f-payload.manifest.tsv"
    rm -f "$ARTIFACTS/f-payload.manifest.tsv.sha256" \
          "$ARTIFACTS/f-payload-integrity.json"
    printf 'CORRUPT\n' >> "$UNPACK/usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so"
    run_tool gen
    after="$(trio_sha)"
    resid="$(residue_count)"
    if [ "$RC" -eq 1 ] && [ "$old_trio" = "$after" ] && [ "$resid" -eq 0 ]; then
        ok t26
    else
        bad t26 "rc=$RC (want 1) restored=$([ "$old_trio" = "$after" ] \
            && echo y || echo n) residue=$resid"
    fi
fi

# --------------------------- t27 劫持：固定 .tmp 名 symlink 不被跟随/写穿
eval "$(mkfixture t27)" || exit 2
victim="$TMP/t27-victim"
printf 'VICTIM-CONTENT\n' > "$victim"
for f in f-payload.manifest.tsv f-payload.manifest.tsv.sha256 \
         f-payload-integrity.json; do
    ln -s "$victim" "$ARTIFACTS/$f.tmp"
done
run_tool gen
if [ "$RC" -eq 0 ] && [ "$(cat "$victim")" = "VICTIM-CONTENT" ] \
   && [ "$(artifact_count)" -eq 3 ]; then
    ok t27
else
    bad t27 "rc=$RC victim=[$(cat "$victim")] artifacts=$(artifact_count)"
fi

# ----------------- t28 劫持：.bak symlink → fail-closed rc=2、不写穿
eval "$(mkfixture t28)" || exit 2
victim="$TMP/t28-victim"
printf 'VICTIM2-CONTENT\n' > "$victim"
for f in f-payload.manifest.tsv f-payload.manifest.tsv.sha256 \
         f-payload-integrity.json; do
    ln -s "$victim" "$ARTIFACTS/$f.bak"
done
run_tool gen
if [ "$RC" -eq 2 ] && [ "$(cat "$victim")" = "VICTIM2-CONTENT" ] \
   && [ "$(artifact_count)" -eq 0 ] \
   && printf '%s' "$ERRTXT" | grep -Fq -- "bak without journal"; then
    ok t28
else
    bad t28 "rc=$RC (want 2) victim=[$(cat "$victim")] \
artifacts=$(artifact_count)"
fi

# --------------------------- t29 劫持：journal symlink → fail-closed rc=2
eval "$(mkfixture t29)" || exit 2
victim="$TMP/t29-victim"
printf 'VICTIM3-CONTENT\n' > "$victim"
ln -s "$victim" "$ARTIFACTS/f-payload.commit.journal"
run_tool gen
if [ "$RC" -eq 2 ] && [ "$(cat "$victim")" = "VICTIM3-CONTENT" ] \
   && [ "$(artifact_count)" -eq 0 ] \
   && printf '%s' "$ERRTXT" | grep -Fq -- "symlink"; then
    ok t29
else
    bad t29 "rc=$RC (want 2) victim=[$(cat "$victim")] \
artifacts=$(artifact_count)"
fi

# --------- t30 事务：空目录首代生成注入 commit.2 → 零产物、零残留（P1-1）
eval "$(mkfixture t30)" || exit 2
export FPI_FAIL_INJECT=commit.2
run_tool gen
unset FPI_FAIL_INJECT
if [ "$RC" -eq 2 ] && [ "$(artifact_count)" -eq 0 ] \
   && [ "$(residue_count)" -eq 0 ]; then
    ok t30
else
    bad t30 "rc=$RC (want 2) artifacts=$(artifact_count) \
residue=$(residue_count)"
fi

# ----- t31 恢复：无 journal + 仅 manifest.bak → fail-closed 保留现场（P1-2）
eval "$(mkfixture t31)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t31 "baseline gen rc=$RC"
else
    before="$(trio_sha)"
    cp "$ARTIFACTS/f-payload.manifest.tsv" "$ARTIFACTS/f-payload.manifest.tsv.bak"
    run_tool gen
    if [ "$RC" -eq 2 ] && [ "$before" = "$(trio_sha)" ] \
       && [ -f "$ARTIFACTS/f-payload.manifest.tsv.bak" ] \
       && printf '%s' "$ERRTXT" | grep -Fq -- "bak without journal"; then
        ok t31
    else
        bad t31 "rc=$RC (want 2) trio_unchanged=$([ "$before" = "$(trio_sha)" ] \
            && echo y || echo n) bak_preserved=$([ -f \
            "$ARTIFACTS/f-payload.manifest.tsv.bak" ] && echo y || echo n)"
    fi
fi

# ----- t32 恢复：无 journal + 三件 .bak 齐全 → fail-closed 保留现场（P1-2）
eval "$(mkfixture t32)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t32 "baseline gen rc=$RC"
else
    before="$(trio_sha)"
    for f in f-payload.manifest.tsv f-payload.manifest.tsv.sha256 \
             f-payload-integrity.json; do
        cp "$ARTIFACTS/$f" "$ARTIFACTS/$f.bak"
    done
    run_tool gen
    if [ "$RC" -eq 2 ] && [ "$before" = "$(trio_sha)" ] \
       && [ "$(residue_count)" -eq 3 ]; then
        ok t32
    else
        bad t32 "rc=$RC (want 2) trio_unchanged=$([ "$before" = "$(trio_sha)" ] \
            && echo y || echo n) residue=$(residue_count)"
    fi
fi

# ----- t33 恢复：journal=committing + 备份内容不一致 → fail-closed（P1-2）
eval "$(mkfixture t33)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t33 "baseline gen rc=$RC"
else
    before="$(trio_sha)"
    cp "$ARTIFACTS/f-payload.manifest.tsv" "$ARTIFACTS/f-payload.manifest.tsv.bak"
    printf 'WRONG-SIDECAR\n' > "$ARTIFACTS/f-payload.manifest.tsv.sha256.bak"
    cp "$ARTIFACTS/f-payload-integrity.json" "$ARTIFACTS/f-payload-integrity.json.bak"
    printf '{"state": "committing", "pre_existing": [true, true, true]}\n' \
        > "$ARTIFACTS/f-payload.commit.journal"
    run_tool gen
    if [ "$RC" -eq 2 ] && [ "$before" = "$(trio_sha)" ] \
       && [ -f "$ARTIFACTS/f-payload.commit.journal" ] \
       && printf '%s' "$ERRTXT" | grep -Fq -- "not bound to manifest"; then
        ok t33
    else
        bad t33 "rc=$RC (want 2) trio_unchanged=$([ "$before" = "$(trio_sha)" ] \
            && echo y || echo n) journal_preserved=$([ -f \
            "$ARTIFACTS/f-payload.commit.journal" ] && echo y || echo n)"
    fi
fi

# ----- t34 事务：回滚失败如实报告（P2：不谎称已恢复，journal 残留）
eval "$(mkfixture t34)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t34 "baseline gen rc=$RC"
else
    before="$(trio_sha)"
    export FPI_FAIL_INJECT="commit.2,rollback.1"
    run_tool gen
    RC1=$RC
    err1="$ERRTXT"
    unset FPI_FAIL_INJECT
    jstate="$(python3 -c "
import json
print(json.load(open('$ARTIFACTS/f-payload.commit.journal'))['state'])" 2>/dev/null \
        || echo missing)"
    run_tool gen
    if [ "$RC1" -eq 2 ] \
       && printf '%s' "$err1" | grep -Fq -- "INCOMPLETE" \
       && [ "$jstate" = "rolling_back" ] \
       && [ "$RC" -eq 0 ] && [ "$before" = "$(trio_sha)" ] \
       && [ "$(residue_count)" -eq 0 ]; then
        ok t34
    else
        bad t34 "rc=$RC1/$RC jstate=$jstate unchanged=$([ "$before" = \
            "$(trio_sha)" ] && echo y || echo n) residue=$(residue_count)"
    fi
fi

# ------- t35 事务：回滚中途失败残留后缀 bak {sha,json} → 启动恢复（P1-1）
eval "$(mkfixture t35)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t35 "baseline gen rc=$RC"
else
    before="$(trio_sha)"
    export FPI_FAIL_INJECT="commit.2,rollback.2"
    run_tool gen
    RC1=$RC
    err1="$ERRTXT"
    unset FPI_FAIL_INJECT
    jstate="$(python3 -c "
import json
print(json.load(open('$ARTIFACTS/f-payload.commit.journal'))['state'])" 2>/dev/null \
        || echo missing)"
    bak_man=$([ -f "$ARTIFACTS/f-payload.manifest.tsv.bak" ] && echo y || echo n)
    bak_sha=$([ -f "$ARTIFACTS/f-payload.manifest.tsv.sha256.bak" ] && echo y || echo n)
    bak_json=$([ -f "$ARTIFACTS/f-payload-integrity.json.bak" ] && echo y || echo n)
    run_tool gen
    if [ "$RC1" -eq 2 ] \
       && printf '%s' "$err1" | grep -Fq -- "INCOMPLETE" \
       && [ "$jstate" = "rolling_back" ] \
       && [ "$bak_man$bak_sha$bak_json" = "yyn" ] \
       && [ "$RC" -eq 0 ] && [ "$before" = "$(trio_sha)" ] \
       && [ "$(residue_count)" -eq 0 ]; then
        ok t35
    else
        bad t35 "rc=$RC1/$RC jstate=$jstate baks=$bak_man$bak_sha$bak_json \
unchanged=$([ "$before" = "$(trio_sha)" ] && echo y || echo n) \
residue=$(residue_count)"
    fi
fi

# ------- t36 事务：回滚中途失败残留后缀 bak {json} → 启动恢复（P1-1）
eval "$(mkfixture t36)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t36 "baseline gen rc=$RC"
else
    before="$(trio_sha)"
    export FPI_FAIL_INJECT="commit.2,rollback.3"
    run_tool gen
    RC1=$RC
    unset FPI_FAIL_INJECT
    jstate="$(python3 -c "
import json
print(json.load(open('$ARTIFACTS/f-payload.commit.journal'))['state'])" 2>/dev/null \
        || echo missing)"
    bak_man=$([ -f "$ARTIFACTS/f-payload.manifest.tsv.bak" ] && echo y || echo n)
    bak_sha=$([ -f "$ARTIFACTS/f-payload.manifest.tsv.sha256.bak" ] && echo y || echo n)
    bak_json=$([ -f "$ARTIFACTS/f-payload-integrity.json.bak" ] && echo y || echo n)
    run_tool gen
    if [ "$RC1" -eq 2 ] \
       && [ "$jstate" = "rolling_back" ] \
       && [ "$bak_man$bak_sha$bak_json" = "yyy" ] \
       && [ "$RC" -eq 0 ] && [ "$before" = "$(trio_sha)" ] \
       && [ "$(residue_count)" -eq 0 ]; then
        ok t36
    else
        bad t36 "rc=$RC1/$RC jstate=$jstate baks=$bak_man$bak_sha$bak_json \
unchanged=$([ "$before" = "$(trio_sha)" ] && echo y || echo n) \
residue=$(residue_count)"
    fi
fi

# ----- t37 恢复：committed + dst JSON 损坏 → fail-closed 保留现场（P1-2）
eval "$(mkfixture t37)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t37 "baseline gen rc=$RC"
else
    for f in f-payload.manifest.tsv f-payload.manifest.tsv.sha256 \
             f-payload-integrity.json; do
        cp "$ARTIFACTS/$f" "$ARTIFACTS/$f.bak"
    done
    printf '{"state": "committed", "pre_existing": [true, true, true]}\n' \
        > "$ARTIFACTS/f-payload.commit.journal"
    printf 'NOT-JSON\n' > "$ARTIFACTS/f-payload-integrity.json"
    run_tool gen
    if [ "$RC" -eq 2 ] \
       && [ "$(cat "$ARTIFACTS/f-payload-integrity.json")" = "NOT-JSON" ] \
       && [ -f "$ARTIFACTS/f-payload.commit.journal" ] \
       && [ "$(residue_count)" -eq 4 ] \
       && printf '%s' "$ERRTXT" | grep -Fq -- "JSON unreadable"; then
        ok t37
    else
        bad t37 "rc=$RC (want 2) json=[$(cat \
            "$ARTIFACTS/f-payload-integrity.json")] residue=$(residue_count)"
    fi
fi

# ----- t38 恢复：committing + 备份 JSON 与 manifest 不绑定 → fail-closed（P1-2）
eval "$(mkfixture t38)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t38 "baseline gen rc=$RC"
else
    before="$(trio_sha)"
    cp "$ARTIFACTS/f-payload.manifest.tsv" "$ARTIFACTS/f-payload.manifest.tsv.bak"
    cp "$ARTIFACTS/f-payload.manifest.tsv.sha256" \
       "$ARTIFACTS/f-payload.manifest.tsv.sha256.bak"
    python3 -c "
import json
d = json.load(open('$ARTIFACTS/f-payload-integrity.json'))
d['manifest']['sha256'] = '0' * 64
json.dump(d, open('$ARTIFACTS/f-payload-integrity.json.bak', 'w'))
"
    printf '{"state": "committing", "pre_existing": [true, true, true]}\n' \
        > "$ARTIFACTS/f-payload.commit.journal"
    run_tool gen
    if [ "$RC" -eq 2 ] && [ "$before" = "$(trio_sha)" ] \
       && [ -f "$ARTIFACTS/f-payload.commit.journal" ] \
       && printf '%s' "$ERRTXT" | grep -Fq -- "JSON not bound to manifest"; then
        ok t38
    else
        bad t38 "rc=$RC (want 2) trio_unchanged=$([ "$before" = "$(trio_sha)" ] \
            && echo y || echo n) journal_preserved=$([ -f \
            "$ARTIFACTS/f-payload.commit.journal" ] && echo y || echo n)"
    fi
fi

# ----- t39 恢复：启动恢复再次中断 → journal 切 rolling_back、二次启动续恢复
eval "$(mkfixture t39)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t39 "baseline gen rc=$RC"
else
    before="$(trio_sha)"
    for f in f-payload.manifest.tsv f-payload.manifest.tsv.sha256 \
             f-payload-integrity.json; do
        cp "$ARTIFACTS/$f" "$ARTIFACTS/$f.bak"
    done
    printf '{"state": "committing", "pre_existing": [true, true, true]}\n' \
        > "$ARTIFACTS/f-payload.commit.journal"
    printf 'FOREIGN-NEW-GEN\n' > "$ARTIFACTS/f-payload.manifest.tsv"
    rm -f "$ARTIFACTS/f-payload.manifest.tsv.sha256" \
          "$ARTIFACTS/f-payload-integrity.json"
    export FPI_FAIL_INJECT=rollback.2
    run_tool gen
    RC1=$RC
    err1="$ERRTXT"
    unset FPI_FAIL_INJECT
    jstate="$(python3 -c "
import json
print(json.load(open('$ARTIFACTS/f-payload.commit.journal'))['state'])" 2>/dev/null \
        || echo missing)"
    run_tool gen
    if [ "$RC1" -eq 2 ] \
       && printf '%s' "$err1" | grep -Fq -- "recovery incomplete" \
       && [ "$jstate" = "rolling_back" ] \
       && [ "$RC" -eq 0 ] && [ "$before" = "$(trio_sha)" ] \
       && [ "$(residue_count)" -eq 0 ]; then
        ok t39
    else
        bad t39 "rc=$RC1/$RC jstate=$jstate unchanged=$([ "$before" = \
            "$(trio_sha)" ] && echo y || echo n) residue=$(residue_count)"
    fi
fi

# ----- t40 回滚：journal.rolling_back 切换失败 → 不动现场、二次启动恢复
eval "$(mkfixture t40)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t40 "baseline gen rc=$RC"
else
    before="$(trio_sha)"
    export FPI_FAIL_INJECT="commit.2,journal.rolling_back"
    run_tool gen
    RC1=$RC
    err1="$ERRTXT"
    unset FPI_FAIL_INJECT
    jstate="$(python3 -c "
import json
print(json.load(open('$ARTIFACTS/f-payload.commit.journal'))['state'])" 2>/dev/null \
        || echo missing)"
    run_tool gen
    if [ "$RC1" -eq 2 ] \
       && printf '%s' "$err1" | grep -Fq -- "INCOMPLETE" \
       && [ "$jstate" = "committing" ] \
       && [ "$RC" -eq 0 ] && [ "$before" = "$(trio_sha)" ] \
       && [ "$(residue_count)" -eq 0 ]; then
        ok t40
    else
        bad t40 "rc=$RC1/$RC jstate=$jstate unchanged=$([ "$before" = \
            "$(trio_sha)" ] && echo y || echo n) residue=$(residue_count)"
    fi
fi

# ----- t41 守卫：事务开始前旧产物部分存在（all-or-none）→ rc=2 零改动
eval "$(mkfixture t41)" || exit 2
printf 'PARTIAL\n' > "$ARTIFACTS/f-payload.manifest.tsv"
run_tool gen
if [ "$RC" -eq 2 ] \
   && [ "$(cat "$ARTIFACTS/f-payload.manifest.tsv")" = "PARTIAL" ] \
   && [ "$(artifact_count)" -eq 1 ] && [ "$(residue_count)" -eq 0 ] \
   && [ -z "$(find "$ARTIFACTS" -maxdepth 1 -name '.f-payload.txn.*' \
        -print -quit)" ] \
   && printf '%s' "$ERRTXT" | grep -Fq -- "partially present"; then
    ok t41
else
    bad t41 "rc=$RC (want 2) artifacts=$(artifact_count) \
residue=$(residue_count)"
fi

# ----- t42 恢复：pre_existing=[F,F,F] + manifest.bak → fail-closed
eval "$(mkfixture t42)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t42 "baseline gen rc=$RC"
else
    before="$(trio_sha)"
    cp "$ARTIFACTS/f-payload.manifest.tsv" "$ARTIFACTS/f-payload.manifest.tsv.bak"
    printf '{"state": "backing_up", "pre_existing": [false, false, false]}\n' \
        > "$ARTIFACTS/f-payload.commit.journal"
    run_tool gen
    if [ "$RC" -eq 2 ] && [ "$before" = "$(trio_sha)" ] \
       && [ -f "$ARTIFACTS/f-payload.manifest.tsv.bak" ] \
       && [ -f "$ARTIFACTS/f-payload.commit.journal" ] \
       && printf '%s' "$ERRTXT" | grep -Fq -- "did not pre-exist"; then
        ok t42
    else
        bad t42 "rc=$RC (want 2) trio_unchanged=$([ "$before" = "$(trio_sha)" ] \
            && echo y || echo n) preserved=$([ -f \
            "$ARTIFACTS/f-payload.manifest.tsv.bak" ] && echo y || echo n)"
    fi
fi

# ----- t43 恢复：journal pre_existing 部分（[F,T,T]）→ fail-closed
eval "$(mkfixture t43)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t43 "baseline gen rc=$RC"
else
    before="$(trio_sha)"
    printf '{"state": "backing_up", "pre_existing": [false, true, true]}\n' \
        > "$ARTIFACTS/f-payload.commit.journal"
    run_tool gen
    if [ "$RC" -eq 2 ] && [ "$before" = "$(trio_sha)" ] \
       && [ -f "$ARTIFACTS/f-payload.commit.journal" ] \
       && printf '%s' "$ERRTXT" | grep -Fq -- "neither all-true nor all-false"; then
        ok t43
    else
        bad t43 "rc=$RC (want 2) trio_unchanged=$([ "$before" = "$(trio_sha)" ] \
            && echo y || echo n) journal_preserved=$([ -f \
            "$ARTIFACTS/f-payload.commit.journal" ] && echo y || echo n)"
    fi
fi

# ----- t44 暂存：首个暂存文件写入失败 → rc=2、零产物、零隐藏残留（P2）
eval "$(mkfixture t44)" || exit 2
export FPI_FAIL_INJECT=stage.1
run_tool gen
unset FPI_FAIL_INJECT
if [ "$RC" -eq 2 ] && [ "$(artifact_count)" -eq 0 ] \
   && [ "$(residue_count)" -eq 0 ] \
   && [ -z "$(find "$ARTIFACTS" -maxdepth 1 -name '.f-payload.txn.*' \
        -print -quit)" ] \
   && printf '%s' "$ERRTXT" | grep -Fq -- "failed to stage artifacts"; then
    ok t44
else
    bad t44 "rc=$RC (want 2) artifacts=$(artifact_count) \
residue=$(residue_count)"
fi

# ----- t45 暂存：第二个暂存文件写入失败 → rc=2、零产物、零隐藏残留（P2）
eval "$(mkfixture t45)" || exit 2
export FPI_FAIL_INJECT=stage.2
run_tool gen
unset FPI_FAIL_INJECT
if [ "$RC" -eq 2 ] && [ "$(artifact_count)" -eq 0 ] \
   && [ "$(residue_count)" -eq 0 ] \
   && [ -z "$(find "$ARTIFACTS" -maxdepth 1 -name '.f-payload.txn.*' \
        -print -quit)" ]; then
    ok t45
else
    bad t45 "rc=$RC (want 2) artifacts=$(artifact_count) \
residue=$(residue_count)"
fi

# ----- t46 暂存清理失败：统一 rc=2、无 traceback、残留可被下次 gen 恢复
eval "$(mkfixture t46)" || exit 2
export FPI_FAIL_INJECT="stage.2,stage-cleanup.1"
run_tool gen
RC1=$RC
err1="$ERRTXT"
unset FPI_FAIL_INJECT
resid1="$(residue_count)"
run_tool gen
if [ "$RC1" -eq 2 ] \
   && printf '%s' "$err1" | grep -Fq -- "cleanup INCOMPLETE" \
   && ! printf '%s' "$err1" | grep -Fq -- "Traceback" \
   && [ "$(artifact_count)" -eq 3 ] \
   && [ "$RC" -eq 0 ] && [ "$(residue_count)" -eq 0 ]; then
    ok t46
else
    bad t46 "rc=$RC1/$RC resid1=$resid1 artifacts=$(artifact_count)"
fi

# ----- t47 journal 清理失败：原始异常不被覆盖、无 traceback、二次启动恢复
eval "$(mkfixture t47)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t47 "baseline gen rc=$RC"
else
    before="$(trio_sha)"
    export FPI_FAIL_INJECT="commit.2,journal.rolling_back,journal-cleanup.1"
    run_tool gen
    RC1=$RC
    err1="$ERRTXT"
    unset FPI_FAIL_INJECT
    jstate="$(python3 -c "
import json
print(json.load(open('$ARTIFACTS/f-payload.commit.journal'))['state'])" 2>/dev/null \
        || echo missing)"
    run_tool gen
    if [ "$RC1" -eq 2 ] \
       && printf '%s' "$err1" | grep -Fq -- "INCOMPLETE" \
       && ! printf '%s' "$err1" | grep -Fq -- "Traceback" \
       && [ "$jstate" = "committing" ] \
       && [ "$RC" -eq 0 ] && [ "$before" = "$(trio_sha)" ] \
       && [ "$(residue_count)" -eq 0 ]; then
        ok t47
    else
        bad t47 "rc=$RC1/$RC jstate=$jstate unchanged=$([ "$before" = \
            "$(trio_sha)" ] && echo y || echo n) residue=$(residue_count)"
    fi
fi

# ----- t48 journal 删除失败（committed 清理）→ WARNING、无 traceback
eval "$(mkfixture t48)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t48 "baseline gen rc=$RC"
else
    for f in f-payload.manifest.tsv f-payload.manifest.tsv.sha256 \
             f-payload-integrity.json; do
        cp "$ARTIFACTS/$f" "$ARTIFACTS/$f.bak"
    done
    printf '{"state": "committed", "pre_existing": [true, true, true]}\n' \
        > "$ARTIFACTS/f-payload.commit.journal"
    export FPI_FAIL_INJECT=journal-unlink.committed
    run_tool gen
    unset FPI_FAIL_INJECT
    if [ "$RC" -eq 0 ] \
       && printf '%s' "$ERRTXT" | grep -Fq -- "journal cleanup failed" \
       && ! printf '%s' "$ERRTXT" | grep -Fq -- "Traceback" \
       && [ "$(artifact_count)" -eq 3 ] && [ "$(residue_count)" -eq 0 ]; then
        ok t48
    else
        bad t48 "rc=$RC (want 0) artifacts=$(artifact_count) \
residue=$(residue_count)"
    fi
fi

# ----- t49 journal 删除失败（rolling_back 恢复）→ WARNING、无 traceback
eval "$(mkfixture t49)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t49 "baseline gen rc=$RC"
else
    before="$(trio_sha)"
    cp "$ARTIFACTS/f-payload.manifest.tsv" "$ARTIFACTS/f-payload.manifest.tsv.bak"
    printf '{"state": "rolling_back", "pre_existing": [true, true, true]}\n' \
        > "$ARTIFACTS/f-payload.commit.journal"
    printf 'FOREIGN-NEW-GEN\n' > "$ARTIFACTS/f-payload.manifest.tsv"
    export FPI_FAIL_INJECT=journal-unlink.recovered
    run_tool gen
    unset FPI_FAIL_INJECT
    if [ "$RC" -eq 0 ] && [ "$before" = "$(trio_sha)" ] \
       && printf '%s' "$ERRTXT" | grep -Fq -- "journal cleanup failed" \
       && ! printf '%s' "$ERRTXT" | grep -Fq -- "Traceback" \
       && [ "$(residue_count)" -eq 0 ]; then
        ok t49
    else
        bad t49 "rc=$RC (want 0) unchanged=$([ "$before" = "$(trio_sha)" ] \
            && echo y || echo n) residue=$(residue_count)"
    fi
fi

# ----- t50 journal 删除失败（回滚）→ 报 restored 不谎称 INCOMPLETE
eval "$(mkfixture t50)" || exit 2
run_tool gen
if [ "$RC" -ne 0 ]; then
    bad t50 "baseline gen rc=$RC"
else
    before="$(trio_sha)"
    export FPI_FAIL_INJECT="commit.2,journal-unlink.rollback"
    run_tool gen
    RC1=$RC
    err1="$ERRTXT"
    unset FPI_FAIL_INJECT
    jstate="$(python3 -c "
import json
print(json.load(open('$ARTIFACTS/f-payload.commit.journal'))['state'])" 2>/dev/null \
        || echo missing)"
    run_tool gen
    if [ "$RC1" -eq 2 ] \
       && printf '%s' "$err1" | grep -Fq -- "previous generation restored" \
       && ! printf '%s' "$err1" | grep -Fq -- "INCOMPLETE" \
       && printf '%s' "$err1" | grep -Fq -- "journal cleanup failed" \
       && ! printf '%s' "$err1" | grep -Fq -- "Traceback" \
       && [ "$jstate" = "rolling_back" ] \
       && [ "$RC" -eq 0 ] && [ "$before" = "$(trio_sha)" ] \
       && [ "$(residue_count)" -eq 0 ]; then
        ok t50
    else
        bad t50 "rc=$RC1/$RC jstate=$jstate unchanged=$([ "$before" = \
            "$(trio_sha)" ] && echo y || echo n) residue=$(residue_count)"
    fi
fi

# ------------------------------------------------------------------- 汇总
echo "PASS=$PASS FAIL=$FAILN"
[ "$FAILN" -eq 0 ] || exit 1
exit 0
