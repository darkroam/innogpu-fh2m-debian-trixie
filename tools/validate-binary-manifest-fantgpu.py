#!/usr/bin/env python3
# tools/validate-binary-manifest-fantgpu.py — F 载荷清单校验（预检路径）
#
# 依据 = docs/planning/c3-a-4-reproducible-input-plan.md §二（v12）：
# S_INPUT validator——每条 `vendor/fantgpu/<path>` 必须存在且 SHA（常规
# 文件）/mode/链接目标与 manifest 一致；O 血统 loader 文件名禁则；载荷
# 缺失 fail-closed（builder 输入预检同源调用）。
#
# 用法：
#   python3 tools/validate-binary-manifest-fantgpu.py \
#       [--manifest binary-manifest-fantgpu.json] [--vendor vendor/fantgpu]
# 退出码：0=PASS 1=校验失败 2=用法/环境错误。

import argparse
import hashlib
import json
import os
import re
import sys

PROG = "tools/validate-binary-manifest-fantgpu.py"
EX_FAIL = 1
EX_USAGE = 2

DEFAULT_MANIFEST = "binary-manifest-fantgpu.json"
DEFAULT_VENDOR = "vendor/fantgpu"

FORBIDDEN_O_LOADER_NAMES = (
    "innogpu_dri.so", "innogpu_drv.so", "innogpu_drv_video.so",
    "innogpu_gbm.so", "inno_drv_video.so",
)


def eprint(msg):
    sys.stderr.write(msg + "\n")


def die(msg, code=EX_USAGE):
    eprint("ERROR: %s" % msg)
    sys.exit(code)


def validate(args):
    if not os.path.isfile(args.manifest):
        die("manifest not found: %s" % args.manifest)
    with open(args.manifest, "r", encoding="utf-8") as fh:
        try:
            m = json.load(fh)
        except ValueError as exc:
            die("manifest not valid JSON: %s (%s)" % (args.manifest, exc))
    if not isinstance(m, dict) or not isinstance(m.get("entries"), list):
        die("manifest schema invalid (entries missing)")
    fixture = os.environ.get("FPI_VAL_FIXTURE") == "1"
    if not fixture:
        if m.get("source_deb_sha256") != (
                "6f0daaf79fb6b2a547138c17628bb990dff0d0c684ee1c13775b"
                "ebc2d28fd11b"):
            die("manifest source_deb_sha256 does not match locked deb",
                EX_FAIL)
        if m.get("input_entries") != 660:
            die("manifest input_entries != 660: %r" % m.get("input_entries"),
                EX_FAIL)
    expect_n = m.get("input_entries", 660)
    expect_dir = m.get("dir_entries", m.get("f_dir_entries", 87))
    if not os.path.isdir(args.vendor):
        die("vendor payload dir not found: %s" % args.vendor)

    failures = []
    entries = {}
    for e in m["entries"]:
        p = e.get("source_path")
        if not isinstance(p, str) or e.get("vendor_path") != "fantgpu/" + p:
            failures.append("entry vendor_path/source_path mismatch: %r"
                            % p)
            continue
        # 路径安全（codex 初审 P1-2）：必须为安全相对路径——非绝对、
        # 无 ".." 组件、无反斜杠、不以 "./" 起首
        if p.startswith("/") or "\\" in p or p.startswith("./") \
                or any(c == ".." for c in p.split("/")):
            failures.append("unsafe source_path: %r" % p)
            continue
        entries[p] = e

    # 全量 lstat 清点（严格双射 + 目录 mode；codex 初审 P1-2）
    files = {}
    syms = {}
    dirs = 0
    for dirpath, dirnames, filenames in os.walk(args.vendor,
                                                followlinks=False):
        dirnames.sort()
        filenames.sort()
        for n in dirnames:
            ap = os.path.join(dirpath, n)
            if os.path.islink(ap):
                failures.append("symlink directory in vendor: %s" % ap)
                continue
            mode = "%04o" % (os.stat(ap).st_mode & 0o7777)
            if mode != "0755":
                failures.append("directory mode drift: %s (%s)"
                                % (ap, mode))
            dirs += 1
        for n in filenames:
            ap = os.path.join(dirpath, n)
            rel = os.path.relpath(ap, args.vendor).replace(os.sep, "/")
            if os.path.islink(ap):
                syms[rel] = os.readlink(ap)
            else:
                files[rel] = ap
    if dirs != expect_dir:
        failures.append("directory count %d != expected %d"
                        % (dirs, expect_dir))
    disk = set(files) | set(syms)
    want = set(entries)
    if disk != want:
        extra = sorted(disk - want)
        missing = sorted(want - disk)
        if extra:
            failures.append("extra entries in vendor: %s" % extra[:3])
        if missing:
            failures.append("missing payload entry: %s" % missing[:3])

    n_regular = n_symlink = 0
    for p in sorted(entries):
        if p in syms:
            n_symlink += 1
            e = entries[p]
            if e.get("sha256") is not None:
                failures.append("entry declares regular but disk is "
                                "symlink: %s" % p)
            elif syms[p] != e.get("target"):
                failures.append("symlink target drift: %s (want %r, got "
                                "%r)" % (p, e.get("target"), syms[p]))
            elif e.get("mode") != "0777":
                failures.append("symlink mode drift: %s (%s)"
                                % (p, e.get("mode")))
        elif p in files:
            n_regular += 1
            e = entries[p]
            if e.get("sha256") is None:
                failures.append("entry declares symlink but disk is "
                                "regular: %s" % p)
                continue
            h = hashlib.sha256()
            with open(files[p], "rb") as fh:
                for chunk in iter(lambda: fh.read(1024 * 1024), b""):
                    h.update(chunk)
            if h.hexdigest() != e["sha256"]:
                failures.append("sha256 drift: %s" % p)
            got_mode = "%04o" % (os.stat(files[p]).st_mode & 0o7777)
            if got_mode != e.get("mode"):
                failures.append("mode drift: %s (want %s, got %s)"
                                % (p, e.get("mode"), got_mode))

    # symlink target 载荷根边界校验（codex re-review P1-2）
    vendor_real = os.path.realpath(args.vendor)
    for p, got_target in syms.items():
        if os.path.isabs(got_target):
            failures.append("symlink target is absolute: %s -> %s"
                            % (p, got_target))
            continue
        link_dir = os.path.realpath(os.path.dirname(
            os.path.join(args.vendor, p)))
        final = os.path.realpath(os.path.join(link_dir, got_target))
        if not (final == vendor_real
                or final.startswith(vendor_real + os.sep)):
            failures.append("symlink escapes vendor root: %s -> %s"
                            % (p, got_target))
            continue
        if not os.path.isfile(final):
            failures.append("symlink does not resolve to a regular file "
                            "in vendor root: %s" % p)

    if n_regular + n_symlink != expect_n:
        failures.append("disk entries %d != manifest %d"
                        % (n_regular + n_symlink, expect_n))

    forbidden = set()
    # 全树 basename 禁则（与 ③ E 检查同口径：按文件名判定）
    for dirpath, dirnames, filenames in os.walk(args.vendor,
                                                followlinks=False):
        for name in filenames:
            if name in FORBIDDEN_O_LOADER_NAMES:
                forbidden.add(name)
    if forbidden:
        failures.append("O-lineage loader filename present: %s"
                        % ", ".join(sorted(forbidden)))

    if failures:
        for f in failures:
            eprint("FAIL: %s" % f)
        eprint("RESULT: FAIL_VALIDATE_FANTGPU_MANIFEST failures=%d"
               % len(failures))
        return EX_FAIL
    print("RESULT: PASS_VALIDATE_FANTGPU_MANIFEST entries=%d "
          "regular=%d symlink=%d" % (len(m["entries"]), n_regular,
                                     n_symlink))
    return 0


def main():
    ap = argparse.ArgumentParser(prog=PROG)
    ap.add_argument("--manifest", default=DEFAULT_MANIFEST)
    ap.add_argument("--vendor", default=DEFAULT_VENDOR)
    args = ap.parse_args()
    return validate(args)


if __name__ == "__main__":
    sys.exit(main())
