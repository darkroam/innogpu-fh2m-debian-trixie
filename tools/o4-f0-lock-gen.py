#!/usr/bin/env python3
# tools/o4-f0-lock-gen.py — O-4 F0 源树锁定生成器（阶段三前置；设计 §6.5 O-4，dsh 提供）
#
# 产物（写入 --out-dir，仓库约定 docs/planning/evidence/o-stage/）：
#   f0.manifest.tsv             4 字段 manifest：type / path / symlink_target / file_sha256
#   f0.manifest.tsv.sha256      上述文件 SHA-256
#   o4-f0.genesis.json          源树身份 + 计数 + tree_hash + 工具版本 + deb 出处 + 自检
#   f0-snapshot.tar.zst         可复现快照（tar 1.35 --sort=name --mtime=@1640995200
#                               --owner=0 --group=0 --numeric-owner --no-acls --no-xattrs
#                               --no-selinux --transform=s,^\.,f0, | zstd -q -19 1.5.7）
#   f0-snapshot.tar.zst.sha256  快照 SHA-256
#
# 子命令：manifest / snapshot / verify（退出码在子命令作用域内解释）
#   0 成功；2 源树缺失/类型异常；3 manifest 格式违规；5 快照/校验失败；
#   7 tar/zstd 版本不匹配；78 配置错误（LC_ALL / 输出路径越界）
#
# 只读源树与 debs/；只写 --out-dir；绝不写任何保护区。
# tree_hash 定义：f0.manifest.tsv 规范化排序序列化后的字节 SHA-256（写入 genesis）。

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

PROG = "tools/o4-f0-lock-gen.py"
EX_CFG = 78
TAR_REQUIRED = "1.35"
ZSTD_REQUIRED = "1.5.7"
EPOCH_MTIME = "1640995200"  # design §5.3 固定确定性常量（@1640995200）
MANIFEST_BASENAME = "f0.manifest.tsv"
GENESIS_BASENAME = "o4-f0.genesis.json"
SNAPSHOT_BASENAME = "f0-snapshot.tar.zst"
GENESIS_SCHEMA = "1.0"

PROTECTED_TOPS = ("debs", "vendor", "build", "third_party", "migration",
                  "drivers", "baselines", "patches")
ALLOWED_OUT_SUFFIXES = ("docs/planning/evidence/o-stage",)


def eprint(msg):
    sys.stderr.write(msg + "\n")


def exit_err(code, msg):
    eprint("ERROR: " + msg)
    sys.exit(code)


def check_env():
    lc = os.environ.get("LC_ALL", "C")
    if lc != "C":
        exit_err(EX_CFG, "LC_ALL must be C (got %r)" % lc)
    os.environ["LC_ALL"] = "C"


def check_out_dir(out_dir):
    ap = os.path.abspath(out_dir)
    if ap.startswith("/tmp"):
        return ap
    for suf in ALLOWED_OUT_SUFFIXES:
        if ap.endswith("/" + suf) or ap == suf:
            return ap
    exit_err(EX_CFG, "out-dir not allowed: %s (must be %s or under /tmp)"
             % (out_dir, " / ".join(ALLOWED_OUT_SUFFIXES)))
    return None


def tool_version(name):
    try:
        out = subprocess.run([name, "--version"], capture_output=True,
                             text=True, timeout=30).stdout
        m = re.search(r"(\d+\.\d+(?:\.\d+)?)", out)
        return m.group(1) if m else None
    except Exception:
        return None


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def check_no_ctrl(fields):
    for f in fields:
        if any(ch in f for ch in ("\t", "\r", "\n", "\0")):
            return False
    return True


def walk_rows(src_root):
    """Return sorted rows (type, path, target, sha). Byte-order sort = LC_ALL=C."""
    rows = []
    root_abs = os.path.abspath(src_root)
    if not os.path.isdir(root_abs):
        exit_err(2, "src-root missing or not a dir: %s" % src_root)
    rows.append(("d", "", "", ""))

    def rel(abspath):
        return os.path.relpath(abspath, root_abs).replace(os.sep, "/")

    for dirpath, dirnames, filenames in os.walk(root_abs):
        dirnames.sort()
        for d in dirnames:
            p = os.path.join(dirpath, d)
            if os.path.islink(p):
                row = ("l", rel(p), os.readlink(p), "")
                if not check_no_ctrl((row[1], row[2], row[3])):
                    exit_err(3, "control char in manifest row: %r" % (row,))
                rows.append(row)
            else:
                rows.append(("d", rel(p), "", ""))
        for f in sorted(filenames):
            p = os.path.join(dirpath, f)
            if os.path.islink(p):
                row = ("l", rel(p), os.readlink(p), "")
                if not check_no_ctrl((row[1], row[2], row[3])):
                    exit_err(3, "control char in manifest row: %r" % (row,))
                rows.append(row)
            elif os.path.isfile(p):
                row = ("f", rel(p), "", sha256_file(p))
                if not check_no_ctrl((row[1], row[2], row[3])):
                    exit_err(3, "control char in manifest row: %r" % (row,))
                rows.append(row)
            else:
                exit_err(2, "unsupported entry type: %s" % p)
    rows.sort(key=lambda r: (r[0].encode("utf-8"), r[1].encode("utf-8")))
    return rows


def manifest_text(rows):
    return "".join("%s\t%s\t%s\t%s\n" % r for r in rows)


def deb_identity(deb_path):
    if not os.path.isfile(deb_path):
        exit_err(2, "deb missing: %s" % deb_path)
    fields = {}
    try:
        out = subprocess.run(
            ["dpkg-deb", "-f", deb_path, "Package", "Version", "Architecture"],
            capture_output=True, text=True, timeout=60, check=True).stdout
    except Exception as e:
        exit_err(2, "dpkg-deb -f failed: %s" % e)
    for line in out.splitlines():
        if ":" in line:
            k, _, v = line.partition(":")
            fields[k.strip()] = v.strip()
    return {
        "path": deb_path,
        "sha256": sha256_file(deb_path),
        "package": fields.get("Package", ""),
        "version": fields.get("Version", ""),
        "architecture": fields.get("Architecture", ""),
    }


def cmd_manifest(args):
    rows = walk_rows(args.src_root)
    text = manifest_text(rows)
    counts = {"dirs": sum(1 for r in rows if r[0] == "d"),
              "files": sum(1 for r in rows if r[0] == "f"),
              "symlinks": sum(1 for r in rows if r[0] == "l")}
    os.makedirs(args.out_dir, exist_ok=True)
    mpath = os.path.join(args.out_dir, MANIFEST_BASENAME)
    with open(mpath, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)
    msha = sha256_file(mpath)
    with open(mpath + ".sha256", "w", encoding="utf-8") as fh:
        fh.write("%s  %s\n" % (msha, MANIFEST_BASENAME))
    genesis = {
        "schema_version": GENESIS_SCHEMA,
        "created_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "label": "F0",
        "src_root": os.path.abspath(args.src_root),
        "deb": deb_identity(args.deb_path),
        "counts": counts,
        "byte_counts": {MANIFEST_BASENAME: len(text.encode("utf-8"))},
        "manifest_sha256": msha,
        "tree_hash": msha,
        "tree_hash_definition": "SHA-256 of f0.manifest.tsv canonical sorted bytes",
        "epoch_mtime": "@" + EPOCH_MTIME,
        "snapshot_tool_versions": {
            "tar_required": TAR_REQUIRED,
            "zstd_required": ZSTD_REQUIRED,
        },
        "snapshot": {},
        "determinism_self_check": None,
        "verify": None,
        "tool": {
            "path": PROG,
            "sha256": sha256_file(os.path.join(os.getcwd(), PROG)),
        },
    }
    gpath = os.path.join(args.out_dir, GENESIS_BASENAME)
    with open(gpath, "w", encoding="utf-8") as fh:
        json.dump(genesis, fh, indent=2, ensure_ascii=False)
        fh.write("\n")
    print("manifest: %d rows (%d dirs / %d files / %d symlinks); tree_hash=%s"
          % (len(rows), counts["dirs"], counts["files"], counts["symlinks"], msha))
    return 0


def require_tools():
    tv, zv = tool_version("tar"), tool_version("zstd")
    if tv != TAR_REQUIRED or zv != ZSTD_REQUIRED:
        exit_err(7, "exact tool lock failed: tar=%s (need %s), zstd=%s (need %s)"
                 % (tv, TAR_REQUIRED, zv, ZSTD_REQUIRED))
    return tv, zv


def build_snapshot(src_root, dest):
    cmd = ["tar", "--sort=name", "--mtime=@%s" % EPOCH_MTIME,
           "--owner=0", "--group=0", "--numeric-owner",
           "--no-acls", "--no-xattrs", "--no-selinux",
           r"--transform=s,^\.,f0,", "-C", src_root, "-cf", "-", "."]
    p1 = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    p2 = subprocess.Popen(["zstd", "-q", "-19", "-o", dest],
                          stdin=p1.stdout, stderr=subprocess.PIPE)
    p1.stdout.close()
    _, e2 = p2.communicate()
    rc1 = p1.wait()
    if rc1 != 0 or p2.returncode != 0:
        eprint("tar rc=%s; zstd rc=%s; zstd stderr=%s"
               % (rc1, p2.returncode, e2.decode(errors="replace")[:500]))
        exit_err(5, "snapshot pipeline failed")
    return sha256_file(dest)


def update_genesis(out_dir, mutator):
    gpath = os.path.join(out_dir, GENESIS_BASENAME)
    with open(gpath, "r", encoding="utf-8") as fh:
        g = json.load(fh)
    mutator(g)
    with open(gpath, "w", encoding="utf-8") as fh:
        json.dump(g, fh, indent=2, ensure_ascii=False)
        fh.write("\n")


def cmd_snapshot(args):
    tv, zv = require_tools()
    dest = os.path.join(args.out_dir, SNAPSHOT_BASENAME)
    sha_a = build_snapshot(args.src_root, dest)
    tmp2 = dest + ".det2.tmp"
    sha_b = build_snapshot(args.src_root, tmp2)
    deterministic = (sha_a == sha_b)
    os.unlink(tmp2)
    if not deterministic:
        exit_err(5, "snapshot not deterministic (two runs differ)")

    with open(dest + ".sha256", "w", encoding="utf-8") as fh:
        fh.write("%s  %s\n" % (sha_a, SNAPSHOT_BASENAME))

    def mut(g):
        g["snapshot"] = {"basename": SNAPSHOT_BASENAME, "sha256": sha_a,
                         "byte_count": os.path.getsize(dest)}
        g["snapshot_tool_versions"] = {"tar": tv, "zstd": zv,
                                       "tar_required": TAR_REQUIRED,
                                       "zstd_required": ZSTD_REQUIRED}
        g["determinism_self_check"] = "pass" if deterministic else "fail"
    update_genesis(args.out_dir, mut)
    print("snapshot: %s sha256=%s determinism=%s"
          % (SNAPSHOT_BASENAME, sha_a, "pass" if deterministic else "fail"))
    return 0


def cmd_verify(args):
    dest = os.path.join(args.out_dir, SNAPSHOT_BASENAME)
    if not os.path.isfile(dest):
        exit_err(5, "snapshot missing: %s" % dest)
    tmpd = tempfile.mkdtemp(prefix="o4-f0-verify-")
    try:
        r = subprocess.run(["tar", "-xf", dest, "-C", tmpd],
                           capture_output=True, text=True)
        if r.returncode != 0:
            exit_err(5, "tar -xf failed: %s" % r.stderr[:400])
        inner = os.path.join(tmpd, "f0")
        if not os.path.isdir(inner):
            exit_err(5, "extracted tree missing f0/ prefix")
        rows = walk_rows(inner)
        new_text = manifest_text(rows)
        mpath = os.path.join(args.out_dir, MANIFEST_BASENAME)
        with open(mpath, "r", encoding="utf-8") as fh:
            old_text = fh.read()
        ok = (new_text == old_text)

        def mut(g):
            g["verify"] = {
                "status": "pass" if ok else "fail",
                "rebuilt_tree_hash": hashlib.sha256(new_text.encode("utf-8")).hexdigest(),
            }
        update_genesis(args.out_dir, mut)
        print("verify: %s (4-field manifest rebuild vs tarball)"
              % ("PASS" if ok else "FAIL"))
        return 0 if ok else 5
    finally:
        shutil.rmtree(tmpd, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(prog=PROG)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("manifest")
    p.add_argument("--src-root", required=True)
    p.add_argument("--deb-path", required=True)
    p.add_argument("--out-dir", required=True)
    p.set_defaults(fn=cmd_manifest)
    p = sub.add_parser("snapshot")
    p.add_argument("--src-root", required=True)
    p.add_argument("--out-dir", required=True)
    p.set_defaults(fn=cmd_snapshot)
    p = sub.add_parser("verify")
    p.add_argument("--out-dir", required=True)
    p.set_defaults(fn=cmd_verify)
    args = ap.parse_args()
    check_env()
    args.out_dir = check_out_dir(args.out_dir)
    sys.exit(args.fn(args))


if __name__ == "__main__":
    main()
