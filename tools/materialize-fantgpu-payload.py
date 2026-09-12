#!/usr/bin/env python3
# tools/materialize-fantgpu-payload.py — C3-a ④ builder F 载荷物化（M1-M6）
#
# 依据 = docs/planning/c3-a-4-reproducible-input-plan.md §四（v12）：
# 读 binary-manifest-fantgpu.json，按 materialize 策略与预选参数
# （F_XORG_ABI/F_UCM_LAYOUT/F_WAYLAND_COMPAT，默认 1.21/ucm2/off）把
# vendor/fantgpu/ 物化到包组装根，并产出/校验 machine-checkable 的
# materialize-trace.tsv（四列 source_entry/destination/rule/kind）。
#
# 物化规则：
#   M1 direct        vendor_path=fantgpu/<p> → $P/<p>
#                    （install -m <mode>；symlink ln -sfn）
#   M2 locked-reference 不复制（trace 不记）
#   M3 ddx-abi-<x>   选中 ABI → /usr/lib/xorg/modules/drivers/fh2m_drv.so
#   M4 ucm-<layout>  选中布局 → 去 opt/fantgpu-fh2m/ staging 前缀
#   M5 wayland-compat on → 去 staging 前缀
# 断言（fail-closed）：落位逐条 SHA/mode/target 与 manifest 一致；
# locked-reference 零复制；$P/opt 必须为空；物化计数 == 动态期望（按
# 当前预选逐条计算；生产模式且预选 == manifest 记录预选时另与
# f_materialized_entries 等值互锁）。
#
# trace 契约（codex 初审 P1-5 修复）：source_entry = manifest vendor_path
# （含 fantgpu/ 前缀）；--ostage-manifest 追加 O_stage 内核 regular 行
# （source = o-stage.manifest.tsv 相对路径）；--verify-trace 逐行复核：
#   destination 存在；kind 一致；fantgpu-* regular 行 SHA+mode == manifest；
#   ostage-kernel 行 SHA == o-stage.manifest.tsv（O_stage mode 由锁定快照
#   承载，设计 v12）；symlink 行 target == manifest；source 不得属于
#   locked-reference 集合；行数 == f + ostage。
#
# 用法：
#   F_XORG_ABI=1.21 F_UCM_LAYOUT=ucm2 F_WAYLAND_COMPAT=off \
#   python3 tools/materialize-fantgpu-payload.py \
#       --manifest binary-manifest-fantgpu.json --vendor vendor/fantgpu \
#       --pkg-root <包组装根> --trace <trace.tsv> \
#       [--ostage-manifest docs/planning/evidence/o-stage/o-stage.manifest.tsv]
#   python3 tools/materialize-fantgpu-payload.py --verify-trace <trace.tsv> \
#       --manifest ... --pkg-root <包组装根> \
#       [--ostage-manifest ...]
# 退出码：0=PASS 1=审计失败 2=用法/环境错误。
# 测试夹具开关 FPI_MAT_FIXTURE=1（放宽全局 660/deb SHA 等生产常量）。

import argparse
import hashlib
import json
import os
import sys
import tempfile

PROG = "tools/materialize-fantgpu-payload.py"
EX_FAIL = 1
EX_USAGE = 2

PRESET_DEFAULTS = {"F_XORG_ABI": "1.21", "F_UCM_LAYOUT": "ucm2",
                   "F_WAYLAND_COMPAT": "off"}
DDX_ABIS = ("1.19", "1.20", "1.21")
DDX_DEST = "usr/lib/xorg/modules/drivers/fh2m_drv.so"
OPT_PREFIX = "opt/fantgpu-fh2m/"
F_DEB_SHA256 = ("6f0daaf79fb6b2a547138c17628bb990dff0d0c684ee1c13775b"
                "ebc2d28fd11b")


def eprint(msg):
    sys.stderr.write(msg + "\n")


def die(msg, code=EX_USAGE):
    eprint("ERROR: %s" % msg)
    sys.exit(code)


def strip_opt(p):
    return p[len(OPT_PREFIX):] if p.startswith(OPT_PREFIX) else p


def read_manifest(path):
    try:
        with open(path, "r", encoding="utf-8") as fh:
            m = json.load(fh)
    except (OSError, ValueError) as exc:
        die("manifest unreadable: %s (%s)" % (path, exc))
    if not isinstance(m, dict) or not isinstance(m.get("entries"), list):
        die("manifest schema invalid: %s" % path)
    if os.environ.get("FPI_MAT_FIXTURE") != "1":
        if m.get("source_deb_sha256") != F_DEB_SHA256:
            die("manifest source_deb_sha256 mismatch", EX_FAIL)
        if m.get("input_entries") != 660:
            die("manifest input_entries != 660: %r"
                % m.get("input_entries"), EX_FAIL)
    return m


def read_ostage_manifest(path):
    """o-stage.manifest.tsv → {rel: sha256}（regular 行）。"""
    rows = {}
    with open(path, "r", encoding="utf-8") as fh:
        for line in fh:
            if not line.strip():
                continue
            parts = line.rstrip("\n").split("\t")
            if len(parts) != 4:
                die("malformed o-stage manifest line: %r" % line[:60],
                    EX_FAIL)
            typ, p, target, sha = parts
            if typ != "f":
                continue
            rows[p] = sha
    return rows


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def ensure_parents_0755(dst, pkg_root):
    """逐级 chmod 目标父链（自 pkg 根起）为 0755——不受调用者 umask 影响
    （codex 初审 P1-3）。"""
    pkg_root = os.path.abspath(pkg_root)
    parent = os.path.abspath(os.path.dirname(dst))
    if not (parent == pkg_root or parent.startswith(pkg_root + os.sep)):
        die("destination escapes pkg root: %s" % dst, EX_FAIL)
    rel = os.path.relpath(parent, pkg_root)
    cur = pkg_root
    for part in ([p for p in rel.split(os.sep) if p]):
        cur = os.path.join(cur, part)
        if os.path.lexists(cur) and not os.path.islink(cur) \
                and os.path.isdir(cur):
            os.chmod(cur, 0o755)


def install_file(src, dst, want_sha, want_mode, pkg_root):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    ensure_parents_0755(dst, pkg_root)
    got = sha256_file(src)
    if os.environ.get("FPI_MAT_INJECT") != "sha-mismatch":
        if got != want_sha:
            die("materialize sha256 drift: %s" % src, EX_FAIL)
    with open(src, "rb") as fh:
        data = fh.read()
    parent = os.path.dirname(dst)
    fd, tmp = tempfile.mkstemp(prefix=".mat.", dir=parent)
    try:
        with os.fdopen(fd, "wb") as fh:
            fh.write(data)
        os.chmod(tmp, int(want_mode, 8))
        os.replace(tmp, dst)
    except BaseException:
        try:
            if os.path.lexists(tmp):
                os.unlink(tmp)
        except OSError:
            pass
        raise


def install_link(target, dst, pkg_root):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    ensure_parents_0755(dst, pkg_root)
    if os.path.lexists(dst):
        os.unlink(dst)
    os.symlink(target, dst)


def expected_materialized(m, preset):
    """按当前预选逐条计算动态期望（codex 初审 P2 修复：非默认预选可用）。"""
    n = 0
    for e in m["entries"]:
        mat = e["materialize"]
        if mat == "locked-reference":
            continue
        if mat == "direct":
            n += 1
        elif mat.startswith("ddx-abi-"):
            if mat == "ddx-abi-%s" % preset["F_XORG_ABI"]:
                n += 1
        elif mat.startswith("ucm-ucm2-"):
            if preset["F_UCM_LAYOUT"] == "ucm2":
                n += 1
        elif mat.startswith("ucm-ucm-"):
            if preset["F_UCM_LAYOUT"] == "ucm":
                n += 1
        elif mat == "wayland-compat":
            if preset["F_WAYLAND_COMPAT"] == "on":
                n += 1
        else:
            die("unclassified materialize policy: %s (%s)"
                % (mat, e["source_path"]), EX_FAIL)
    return n


def materialize(args, m, preset, ostage_rows):
    pkg = os.path.abspath(args.pkg_root)
    vendor = os.path.abspath(args.vendor)
    if not os.path.isdir(pkg):
        die("--pkg-root is not a directory: %s" % pkg)
    if not os.path.isdir(vendor) or os.path.islink(vendor):
        die("--vendor is not a real directory: %s" % vendor)

    trace_rows = []
    n_mat = 0
    n_locked = 0
    for e in sorted(m["entries"], key=lambda x: x["source_path"]):
        p = e["source_path"]
        mat = e["materialize"]
        src = os.path.join(vendor, p)
        vp = e["vendor_path"]  # trace source_entry（含 fantgpu/ 前缀）
        if mat == "locked-reference":
            n_locked += 1
            continue
        if mat == "direct":
            dst = os.path.join(pkg, p)
            rule = "direct"
            kind = "symlink" if e.get("target") is not None else "regular"
        elif mat.startswith("ddx-abi-"):
            abi = mat[len("ddx-abi-"):]
            if abi != preset["F_XORG_ABI"]:
                continue
            dst = os.path.join(pkg, DDX_DEST)
            rule = "ddx-abi-%s" % abi
            kind = "regular"
        elif mat.startswith("ucm-ucm2-"):
            if preset["F_UCM_LAYOUT"] != "ucm2":
                continue
            base = strip_opt(p)
            rel = base[len("usr/share/alsa/ucm2/conf.d/"):]
            dst = os.path.join(pkg, "usr/share/alsa/ucm2/conf.d", rel)
            rule = "ucm-ucm2"
            kind = "regular"
        elif mat.startswith("ucm-ucm-"):
            if preset["F_UCM_LAYOUT"] != "ucm":
                continue
            base = strip_opt(p)
            rel = base[len("usr/share/alsa/ucm/"):]
            dst = os.path.join(pkg, "usr/share/alsa/ucm", rel)
            rule = "ucm-ucm"
            kind = "regular"
        elif mat == "wayland-compat":
            if preset["F_WAYLAND_COMPAT"] != "on":
                continue
            rel = strip_opt(p)
            dst = os.path.join(pkg, rel)
            rule = "wayland-compat"
            kind = "symlink" if e.get("target") is not None else "regular"
        else:
            die("unclassified materialize policy: %s (%s)" % (mat, p),
                EX_FAIL)
        if not os.path.lexists(src):
            die("materialize source missing: %s" % src, EX_FAIL)
        if kind == "regular":
            install_file(src, dst, e["sha256"], e["mode"], pkg)
        else:
            install_link(e["target"], dst, pkg)
        trace_rows.append((vp, os.path.relpath(dst, pkg), rule, kind))
        n_mat += 1

    if n_locked != m.get("locked_reference_entries"):
        die("locked-reference count %d != manifest %d"
            % (n_locked, m.get("locked_reference_entries")), EX_FAIL)
    expect = expected_materialized(m, preset)
    if n_mat != expect:
        die("materialized %d != dynamic expectation %d (preset %s)"
            % (n_mat, expect, json.dumps(preset, sort_keys=True)), EX_FAIL)
    if os.environ.get("FPI_MAT_FIXTURE") != "1" \
            and preset == m.get("preset", PRESET_DEFAULTS):
        if n_mat != m.get("f_materialized_entries"):
            die("materialized %d != manifest f_materialized_entries %d"
                % (n_mat, m.get("f_materialized_entries")), EX_FAIL)
    opt_dir = os.path.join(pkg, "opt")
    if os.path.lexists(opt_dir):
        die("package root /opt must be empty (staging prefix stripped)",
            EX_FAIL)

    rows = sorted(trace_rows)
    if ostage_rows:
        for rel in sorted(ostage_rows):
            rows.append((rel, "usr/src/fantgpu-fh2m-kernel-2.2/" + rel,
                         "ostage-kernel", "regular"))
    with open(args.trace, "w", encoding="utf-8") as fh:
        for r in rows:
            fh.write("%s\t%s\t%s\t%s\n" % r)
    print("RESULT: PASS_MATERIALIZE_FANTGPU_PAYLOAD "
          "f_materialized_entries=%d locked_reference_entries=%d "
          "ostage_entries=%d trace_entries=%d preset=%s"
          % (n_mat, n_locked, len(ostage_rows), len(rows),
             json.dumps(preset, sort_keys=True)))
    return 0


RULE_WHITELIST = ("direct", "ddx-abi-1.19", "ddx-abi-1.20", "ddx-abi-1.21",
                  "ucm-ucm2", "ucm-ucm", "wayland-compat", "ostage-kernel")
OSTAGE_DST_PREFIX = "usr/src/fantgpu-fh2m-kernel-2.2/"


def safe_relative_dst(dst):
    """dst 必须为安全相对路径：非绝对、无 .. 组件、无反斜杠、无 ./ 前缀。"""
    if not dst or dst.startswith("/") or "\\" in dst \
            or dst.startswith("./") or any(c == ".." for c in dst.split("/")):
        return False
    return True


def expected_destination(e, preset):
    """由条目 + 预选反推物化目标（与 materialize 完全同构）。"""
    p = e["source_path"]
    mat = e["materialize"]
    if mat == "direct":
        return p
    if mat.startswith("ddx-abi-"):
        return DDX_DEST
    if mat.startswith("ucm-ucm2-"):
        base = strip_opt(p)
        return "usr/share/alsa/ucm2/conf.d/" + \
            base[len("usr/share/alsa/ucm2/conf.d/"):]
    if mat.startswith("ucm-ucm-"):
        base = strip_opt(p)
        return "usr/share/alsa/ucm/" + base[len("usr/share/alsa/ucm/"):]
    if mat == "wayland-compat":
        return strip_opt(p)
    return None


def verify_trace(args, m, ostage_rows, preset):
    """逐行复核 trace（codex 初审 P1-5 / re-review P1 修复：可承担审计证据）。

    校验：期望行数严格相等（动态期望 + ostage）、rule 白名单、rule 与来源
    条目 materialize 策略/预选参数/目标路径的映射一致、dst 为安全相对路径
    且 realpath 位于包根内、来源覆盖与清单严格双射（零多写/零漏写）。
    """
    pkg = os.path.abspath(args.pkg_root)
    pkg_real = os.path.realpath(pkg)
    locked_sources = {e["vendor_path"] for e in m["entries"]
                      if e["materialize"] == "locked-reference"}
    entries = {e["vendor_path"]: e for e in m["entries"]}
    expected_f = expected_materialized(m, preset)
    if os.environ.get("FPI_MAT_FIXTURE") != "1" \
            and preset == m.get("preset", PRESET_DEFAULTS):
        expected_f = m.get("f_materialized_entries", expected_f)
    expected_total = expected_f + len(ostage_rows)
    expected_sources = {e["vendor_path"] for e in m["entries"]
                        if e["materialize"] != "locked-reference"
                        and rule_selected(e, preset)} | set(ostage_rows)

    with open(args.trace, "r", encoding="utf-8") as fh:
        lines = [l.rstrip("\n") for l in fh if l.strip()]
    failures = []
    n = 0
    seen = set()
    for lineno, line in enumerate(lines, 1):
        parts = line.split("\t")
        if len(parts) != 4:
            failures.append("trace line %d not 4 columns" % lineno)
            continue
        source, dst, rule, kind = parts
        if rule not in RULE_WHITELIST:
            failures.append("trace line %d rule not in whitelist: %s"
                            % (lineno, rule))
            continue
        if source in locked_sources:
            failures.append("trace line %d references locked-reference "
                            "source: %s" % (lineno, source))
            continue
        if source in seen:
            failures.append("trace line %d duplicate source: %s"
                            % (lineno, source))
            continue
        seen.add(source)
        if not safe_relative_dst(dst):
            failures.append("trace line %d unsafe destination: %s"
                            % (lineno, dst))
            continue
        dpath = os.path.join(pkg, dst)
        dpath_real = os.path.realpath(dpath)
        if not (dpath_real == pkg_real
                or dpath_real.startswith(pkg_real + os.sep)):
            failures.append("trace line %d destination escapes pkg root: "
                            "%s" % (lineno, dst))
            continue
        if not os.path.lexists(dpath):
            failures.append("trace line %d destination missing: %s"
                            % (lineno, dst))
            continue
        n += 1
        # rule↔来源条目映射（ostage 行：source 必须来自 O_stage 清单）
        if rule == "ostage-kernel":
            if source not in ostage_rows or dst != OSTAGE_DST_PREFIX + source:
                failures.append("trace line %d ostage rule/source/dest "
                                "mismatch: %s" % (lineno, source))
            if kind != "regular":
                failures.append("trace line %d ostage kind must be "
                                "regular" % lineno)
            got = sha256_file(dpath)
            if ostage_rows.get(source) != got:
                failures.append("trace line %d ostage sha drift: %s"
                                % (lineno, dst))
            continue
        e = entries.get(source)
        if e is None:
            failures.append("trace line %d unknown source: %s"
                            % (lineno, source))
            continue
        mat = e["materialize"]
        rule_ok = {
            "direct": mat == "direct",
            "ucm-ucm2": mat.startswith("ucm-ucm2-")
                        and preset["F_UCM_LAYOUT"] == "ucm2",
            "ucm-ucm": mat.startswith("ucm-ucm-")
                       and preset["F_UCM_LAYOUT"] == "ucm",
            "wayland-compat": mat == "wayland-compat"
                              and preset["F_WAYLAND_COMPAT"] == "on",
        }
        if rule.startswith("ddx-abi-"):
            rule_ok[rule] = mat == rule and \
                preset["F_XORG_ABI"] == rule[len("ddx-abi-"):]
        if not rule_ok.get(rule, False):
            failures.append("trace line %d rule does not match source "
                            "materialize/preset: %s (%s)"
                            % (lineno, source, rule))
            continue
        want_dst = expected_destination(e, preset)
        if want_dst is None or dst != want_dst:
            failures.append("trace line %d destination does not match "
                            "expected mapping: %s (want %s)"
                            % (lineno, dst, want_dst))
            continue
        if kind == "symlink":
            if not os.path.islink(dpath):
                failures.append("trace line %d kind mismatch (want "
                                "symlink): %s" % (lineno, dst))
                continue
            if os.readlink(dpath) != e.get("target"):
                failures.append("trace line %d symlink target drift: %s"
                                % (lineno, dst))
        elif kind == "regular":
            if os.path.islink(dpath) or not os.path.isfile(dpath):
                failures.append("trace line %d kind mismatch (want "
                                "regular): %s" % (lineno, dst))
                continue
            if sha256_file(dpath) != e.get("sha256"):
                failures.append("trace line %d sha drift: %s"
                                % (lineno, dst))
            mode = "%04o" % (os.stat(dpath).st_mode & 0o7777)
            if mode != e.get("mode"):
                failures.append("trace line %d mode drift: %s"
                                % (lineno, dst))
        else:
            failures.append("trace line %d unknown kind: %s"
                            % (lineno, kind))

    # 期望行数严格相等 + 来源覆盖严格双射（codex re-review P1：缺行/多行拒绝）
    if n != expected_total:
        failures.append("trace entries %d != expected %d (f=%d ostage=%d)"
                        % (n, expected_total, expected_f, len(ostage_rows)))
    if seen != expected_sources:
        missing = sorted(expected_sources - seen)
        extra = sorted(seen - expected_sources)
        if missing:
            failures.append("trace missing sources: %s" % missing[:3])
        if extra:
            failures.append("trace extra sources: %s" % extra[:3])

    if failures:
        for f in failures:
            eprint("FAIL: %s" % f)
        eprint("RESULT: FAIL_MATERIALIZE_VERIFY_TRACE failures=%d"
               % len(failures))
        return EX_FAIL
    print("RESULT: PASS_MATERIALIZE_VERIFY_TRACE trace_entries=%d "
          "expected=%d" % (n, expected_total))
    return 0


def rule_selected(e, preset):
    mat = e["materialize"]
    if mat == "direct":
        return True
    if mat.startswith("ddx-abi-"):
        return mat == "ddx-abi-%s" % preset["F_XORG_ABI"]
    if mat.startswith("ucm-ucm2-"):
        return preset["F_UCM_LAYOUT"] == "ucm2"
    if mat.startswith("ucm-ucm-"):
        return preset["F_UCM_LAYOUT"] == "ucm"
    if mat == "wayland-compat":
        return preset["F_WAYLAND_COMPAT"] == "on"
    return False


def main():
    ap = argparse.ArgumentParser(prog=PROG)
    ap.add_argument("--manifest", required=True)
    ap.add_argument("--vendor", default=None,
                    help="required for materialize; unused for --verify-trace")
    ap.add_argument("--pkg-root", required=True)
    ap.add_argument("--trace", default=None)
    ap.add_argument("trace_path", nargs="?", default=None,
                    help="trace file (positional; --verify-trace mode)")
    ap.add_argument("--ostage-manifest", default=None)
    ap.add_argument("--verify-trace", action="store_true",
                    help="只读复核既有 trace（不再物化）")
    args = ap.parse_args()
    args.trace = args.trace or args.trace_path
    if not args.trace:
        die("trace file required (--trace or positional)")

    preset = {k: os.environ.get(k, v) for k, v in PRESET_DEFAULTS.items()}
    if preset["F_XORG_ABI"] not in DDX_ABIS:
        die("F_XORG_ABI must be one of %s" % ", ".join(DDX_ABIS))
    if preset["F_UCM_LAYOUT"] not in ("ucm", "ucm2"):
        die("F_UCM_LAYOUT must be ucm|ucm2")
    if preset["F_WAYLAND_COMPAT"] not in ("on", "off"):
        die("F_WAYLAND_COMPAT must be on|off")

    m = read_manifest(args.manifest)
    ostage_rows = {}
    if args.ostage_manifest:
        ostage_rows = read_ostage_manifest(args.ostage_manifest)
    if args.verify_trace:
        return verify_trace(args, m, ostage_rows, preset)
    if not args.vendor:
        die("--vendor required for materialize mode")
    return materialize(args, m, preset, ostage_rows)


if __name__ == "__main__":
    sys.exit(main())
