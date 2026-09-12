#!/usr/bin/env python3
# tools/gen-fantgpu-manifest.py — 生成 F 血统载荷清单 binary-manifest-fantgpu.json
#
# 依据 = docs/planning/c3-a-4-reproducible-input-plan.md §二（v12）：
#  - S_INPUT = 所有条目（隐含集合）：F 载荷全部 602 常规文件 + 58 符号链接
#    （目录不入条目，O 先例同；DEBIAN/ 控制成员不入条目——builder 重生成）
#  - 条目键 = O schema（kind/license/role/sha256/size/source_path/vendor_path）
#    + variant / materialize / mode 三键
#  - S_LOCKED = materialize:locked-reference 的条目集合（内核源 + vendor
#    死暂存 + sw-fant-gl/service）
#  - S_MATERIALIZE = 其余经预选后实际物化的集合（期望计数按定案默认
#    F_XORG_ABI=1.21 / F_UCM_LAYOUT=ucm2 / F_WAYLAND_COMPAT=off 预计算）
#  - M6 分类表全量实现；任何 /opt 条目未分类 → fail-closed
#
# 输入：docs/planning/evidence/o-stage/f-payload.manifest.tsv（③ 产物，
#   7 列 type/path/size/mode/sha256/md5/link_target，753 行）+ 其 .sha256
#   sidecar（先校验 sidecar 一致才使用）。
# 输出：binary-manifest-fantgpu.json（git 追踪；含逐文件 SHA-256/mode/
#   link target，无墙钟时间戳 → 双跑字节一致）。
# 退出码：0=成功 1=审计失败 2=用法/环境错误。
# 故障注入（仅测试）：FPI_FMAN_GEN_INJECT=sha-drift 模拟清单 SHA 漂移。

import argparse
import hashlib
import json
import os
import re
import sys

PROG = "tools/gen-fantgpu-manifest.py"
EX_FAIL = 1
EX_USAGE = 2

SCHEMA_VERSION = "1.0"
FORMAT_VERSION = 1
SOURCE_PACKAGE = "fantgpu-fh2m"
SOURCE_VERSION = "3.3.8.126-driver-linux-desktop-sp-generic"
SOURCE_DEB_SHA256 = ("6f0daaf79fb6b2a547138c17628bb990dff0d0c684ee1c13775b"
                     "ebc2d28fd11b")
ARCH = "amd64"
LINEAGE = "fantgpu"

DEFAULT_TSV = ("docs/planning/evidence/o-stage/f-payload.manifest.tsv")
DEFAULT_OUT = "binary-manifest-fantgpu.json"

# ③ 清单固定规模（audit ③ 锁定值；任何变化 → 拒绝生成）
EXPECTED_COUNTS = {"dir": 87, "file": 602, "symlink": 58}

# 预选默认值（dsh 2026-09-12 定案）
PRESET = {"F_XORG_ABI": "1.21", "F_UCM_LAYOUT": "ucm2",
          "F_WAYLAND_COMPAT": "off"}

DDX_RE = re.compile(
    r"^opt/fantgpu-fh2m/usr/lib/xorg/modules/drivers/"
    r"fh2m_drv\.so\.(1\.19|1\.20|1\.21)$")
UCM2_CONFD_RE = re.compile(
    r"^opt/fantgpu-fh2m/usr/share/alsa/ucm2/conf\.d/"
    r"FantasyCard/(.+)$")
UCM2_PLAIN_RE = re.compile(
    r"^opt/fantgpu-fh2m/usr/share/alsa/ucm2/FantasyCard/(.+)$")
UCM0_RE = re.compile(
    r"^opt/fantgpu-fh2m/usr/share/alsa/ucm/(FantasyCard0[^/]*/.+)$")
UCM_PLAIN_RE = re.compile(
    r"^opt/fantgpu-fh2m/usr/share/alsa/ucm/FantasyCard/(.+)$")
WAYLAND_RE = re.compile(
    r"^opt/fantgpu-fh2m/usr/lib/(?:x86_64|i386)-linux-gnu/fantgpu-fh2m/"
    r"(?:libwayland-client|libffi)(?:\.so.*)?$")
SWGL_BIN = "opt/fantgpu-fh2m/usr/sbin/sw-fant-gl"
SWGL_SVC = "opt/fantgpu-fh2m/lib/systemd/system/sw-fant-gl.service"
KERNEL_SRC_PREFIX = "usr/src/fantgpu-fh2m-kernel-2.2/"
OPT_PREFIX = "opt/fantgpu-fh2m/"

LOADER_LIB_RE = re.compile(
    r"^(?:usr/lib/[^/]+/|etc/OpenCL/vendors/|etc/vulkan/icd\.d/|"
    r"usr/share/glvnd/egl_vendor\.d/|usr/share/drirc\.d/|"
    r"usr/share/vulkan/icd\.d/)")


def eprint(msg):
    sys.stderr.write(msg + "\n")


def die(msg, code=EX_USAGE):
    eprint("ERROR: %s" % msg)
    sys.exit(code)


def classify(path, is_symlink):
    """M6 分类表：返回 (materialize, variant, role, kind)。未分类 /opt → None。"""
    if path.startswith(KERNEL_SRC_PREFIX):
        return ("locked-reference", None,
                "F kernel source shipped in deb (locked reference; "
                "O_stage tree is the DKMS source)", "kernel-source")
    m = DDX_RE.match(path)
    if m:
        abi = m.group(1)
        return ("ddx-abi-%s" % abi, "ddx-abi-%s" % abi,
                "F Xorg DDX ABI %s variant" % abi, "ddx")
    m = UCM2_CONFD_RE.match(path)
    if m:
        return ("ucm-ucm2-%s" % m.group(1), "ucm-ucm2-%s" % m.group(1),
                "F ALSA UCM2 conf.d variant", "userspace-config")
    m = UCM_PLAIN_RE.match(path)
    if m:
        return ("locked-reference", None,
                "vendor dead staging (never copied by vendor postinst)",
                "userspace-config")
    m = UCM2_PLAIN_RE.match(path)
    if m:
        return ("locked-reference", None,
                "vendor dead staging (never copied by vendor postinst)",
                "userspace-config")
    m = UCM0_RE.match(path)
    if m:
        return ("ucm-ucm-%s" % m.group(1), "ucm-ucm-%s" % m.group(1),
                "F ALSA UCM (legacy layout) variant", "userspace-config")
    if WAYLAND_RE.match(path):
        return ("wayland-compat", "wayland-compat",
                "F wayland compat lib (build-time F_WAYLAND_COMPAT)",
                "userspace-lib")
    if path == SWGL_BIN:
        return ("locked-reference", None,
                "sw-fant-gl helper (not installed; depends on removed "
                "/opt and /etc/.fantgpu.cfg; superseded by static "
                "0-fantgpu-hwgl.conf)", "userspace-config")
    if path == SWGL_SVC:
        return ("locked-reference", None,
                "sw-fant-gl systemd unit (not installed; service not "
                "enabled)", "userspace-config")
    if path.startswith(OPT_PREFIX):
        return None  # 未分类 /opt 条目 → 生成器 fail-closed
    if path.startswith("lib/firmware/fantgpu/fh2m/"):
        return ("direct", None, "F firmware", "firmware")
    if (path.startswith("usr/lib/")
            or LOADER_LIB_RE.match(path)) and not is_symlink:
        return ("direct", None, "F userspace library", "userspace-lib")
    if path.startswith("usr/lib/") and is_symlink:
        return ("direct", None, "F userspace library symlink",
                "userspace-lib")
    return ("direct", None, "F userspace config", "userspace-config")


def read_fpayload_tsv(path):
    """读 ③ 清单 → {payload_path: {type,size,mode,sha256,md5,target}}。"""
    rows = {}
    with open(path, "r", encoding="utf-8") as fh:
        for lineno, line in enumerate(fh, 1):
            if not line.strip():
                continue
            cols = line.rstrip("\n").split("\t")
            if len(cols) != 7:
                die("malformed f-payload.manifest.tsv line %d" % lineno)
            typ, p, size, mode, sha, md5, target = cols
            if p == "DEBIAN" or p.startswith("DEBIAN/"):
                continue
            if typ not in ("d", "f", "l"):
                die("unexpected type %r at line %d" % (typ, lineno))
            rows[p] = {"type": typ, "size": size, "mode": mode,
                       "sha256": sha, "md5": md5, "target": target}
    return rows


def build_manifest(rows):
    entries = []
    counts = {"d": 0, "f": 0, "l": 0}
    for p in sorted(rows):
        r = rows[p]
        counts[r["type"]] += 1
        if r["type"] == "d":
            continue
        if r["type"] == "f":
            sha = r["sha256"]
            if os.environ.get("FPI_FMAN_GEN_INJECT") == "sha-drift" \
                    and p.startswith("usr/lib/"):
                sha = "zz"  # 非法格式 → 生成器 fail-closed（仅测试）
            if not re.fullmatch(r"[0-9a-f]{64}", sha or ""):
                die("missing/invalid sha256 for %s" % p, EX_FAIL)
            if len(sha) == 64 and not re.fullmatch(r"[0-9a-f]{64}", sha):
                die("invalid sha256 for %s" % p, EX_FAIL)
            entries.append({
                "kind": None, "license": "vendor-binary", "role": None,
                "sha256": sha, "size": int(r["size"]),
                "source_path": p, "vendor_path": "fantgpu/" + p,
                "mode": r["mode"], "variant": None, "materialize": None,
            })
        else:  # symlink
            # 词法边界校验（codex re-review P1-2 生成器侧防御）：target 非
            # 绝对、解析后不逃出载荷根（".." 组件越界拒绝）
            target = r["target"]
            if os.path.isabs(target):
                die("symlink target is absolute: %s -> %s" % (p, target),
                    EX_FAIL)
            resolved = os.path.normpath(
                os.path.join(os.path.dirname(p), target))
            if resolved == ".." or resolved.startswith("../") \
                    or resolved == ".":
                die("symlink target escapes payload root: %s -> %s"
                    % (p, target), EX_FAIL)
            entries.append({
                "kind": None, "license": "vendor-binary", "role": None,
                "sha256": None, "size": None,
                "source_path": p, "vendor_path": "fantgpu/" + p,
                "mode": r["mode"], "target": target,
                "variant": None, "materialize": None,
            })
    got_counts = {"dir": counts["d"], "file": counts["f"],
                  "symlink": counts["l"]}
    if got_counts != EXPECTED_COUNTS:
        die("f-payload.manifest.tsv counts %s != expected %s"
            % (json.dumps(got_counts, sort_keys=True),
               json.dumps(EXPECTED_COUNTS, sort_keys=True)), EX_FAIL)
    return entries


def annotate(entries):
    """M6 分类；未分类 /opt → fail-closed；输出物化期望计数。"""
    n_input = len(entries)
    n_locked = 0
    n_mat = 0
    for e in entries:
        p = e["source_path"]
        got = classify(p, e.get("target") is not None)
        if got is None:
            die("unclassified /opt entry (M6 classification table is "
                "complete; refuse to generate): %s" % p, EX_FAIL)
        mat, variant, role, kind = got
        e["materialize"] = mat
        e["variant"] = variant
        e["role"] = role
        e["kind"] = kind
        if mat == "locked-reference":
            n_locked += 1
        elif mat == "direct":
            n_mat += 1
        elif mat.startswith("ddx-abi-") and \
                mat == "ddx-abi-%s" % PRESET["F_XORG_ABI"]:
            n_mat += 1
        elif mat.startswith("ucm-ucm2-") and PRESET["F_UCM_LAYOUT"] == "ucm2":
            n_mat += 1
        elif mat.startswith("ucm-ucm-") and PRESET["F_UCM_LAYOUT"] == "ucm":
            n_mat += 1
        elif mat == "wayland-compat" and PRESET["F_WAYLAND_COMPAT"] == "on":
            n_mat += 1
        # 未选变体不计数
    # 变体组完备性：每个 ABI 三个变体必须都存在
    for abi in ("1.19", "1.20", "1.21"):
        if sum(1 for e in entries
               if e.get("variant") == "ddx-abi-%s" % abi) != 1:
            die("DDX ABI variant group incomplete: %s" % abi, EX_FAIL)
    return n_input, n_locked, n_mat


def main():
    ap = argparse.ArgumentParser(prog=PROG)
    ap.add_argument("--tsv", default=DEFAULT_TSV,
                    help="input ③ f-payload.manifest.tsv")
    ap.add_argument("--out", default=DEFAULT_OUT,
                    help="output manifest JSON")
    args = ap.parse_args()

    sidecar = args.tsv + ".sha256"
    if not os.path.isfile(args.tsv):
        die("input tsv not found: %s" % args.tsv)
    if not os.path.isfile(sidecar):
        die("input tsv sidecar not found: %s" % sidecar)
    with open(sidecar, "r", encoding="ascii") as fh:
        want = fh.read().split()[0]
    got = hashlib.sha256(open(args.tsv, "rb").read()).hexdigest()
    if want != got:
        die("f-payload.manifest.tsv sidecar mismatch (want %s, got %s)"
            % (want, got), EX_FAIL)

    rows = read_fpayload_tsv(args.tsv)
    entries = build_manifest(rows)
    n_input, n_locked, n_mat = annotate(entries)

    obj = {
        "format_version": FORMAT_VERSION,
        "schema_version": SCHEMA_VERSION,
        "architecture": ARCH,
        "lineage": LINEAGE,
        "source_package": SOURCE_PACKAGE,
        "source_version": SOURCE_VERSION,
        "source_deb_sha256": SOURCE_DEB_SHA256,
        "preset": dict(PRESET),
        "input_entries": n_input,
        "locked_reference_entries": n_locked,
        "f_materialized_entries": n_mat,
        "f_dir_entries": EXPECTED_COUNTS["dir"],
        "generator": PROG,
        "entries": entries,
    }
    data = (json.dumps(obj, indent=2, sort_keys=True, ensure_ascii=False)
            + "\n").encode("utf-8")

    out_dir = os.path.dirname(os.path.abspath(args.out))
    if not os.path.isdir(out_dir):
        die("output directory does not exist: %s" % out_dir)
    import tempfile
    fd, tmp = tempfile.mkstemp(prefix=".fman.", dir=out_dir)
    try:
        with os.fdopen(fd, "wb") as fh:
            fh.write(data)
            fh.flush()
            os.fsync(fh.fileno())
        os.replace(tmp, args.out)
        os.chmod(args.out, 0o644)  # codex re-review P2：tracked 清单 0644
    except OSError as exc:
        try:
            if os.path.lexists(tmp):
                os.unlink(tmp)
        except OSError:
            pass
        die("failed to write manifest: %s" % exc)

    print("RESULT: PASS_GEN_FANTGPU_MANIFEST input_entries=%d "
          "locked_reference_entries=%d f_materialized_entries=%d "
          "f_dir_entries=%d" % (n_input, n_locked, n_mat,
                                EXPECTED_COUNTS["dir"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
