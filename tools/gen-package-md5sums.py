#!/usr/bin/env python3
# tools/gen-package-md5sums.py — 构建期按实际安装载荷重生成 DEBIAN/md5sums
#
# 依据 = docs/planning/c3-a-4-reproducible-input-plan.md §四 强制项 1
# （C3-a ④ builder 改造配套工具；F 分支专用，O 分支行为不变）。
# 上游 F deb 的 DEBIAN/md5sums 是对重定位前暂存布局生成的（95 条路径列
# 错配，③ 审计已严格定界），builder 改造**必须按实际安装的载荷重生成**，
# 不得原样继承厂商文件。
#
# 确定性契约（双构建字节一致依赖本工具的字节确定性）：
#   - LC_ALL=C（调用方负责 export；本工具内部亦对排序显式 locale 无关——
#     排序用 Python 字节序，等价 C locale）
#   - 只收集常规文件（符号链接与目录排除，与 dpkg 惯例一致）
#   - 排除 DEBIAN/ 子树（控制成员不入 md5sums，Debian 规范）
#   - 相对路径：去 --root 前缀、以 "/" 分隔、无 "./" 前缀
#   - 行格式：<md5 十六进制小写 32 位><两个空格><相对路径><LF>
#   - 输出按相对路径字节序排序；尾行换行
#
# 用法：python3 tools/gen-package-md5sums.py --root <包组装根> [-o <输出>]
#   缺省输出到 --root/DEBIAN/md5sums；--stdout 打印到 stdout 供测试。
# 退出码：0=成功 1=用法/环境错误。
# 故障注入（仅测试）：FPI_MD5_INJECT=walk-order 使目录枚举乱序，
#   验证输出不依赖 os.walk 顺序。

import argparse
import hashlib
import os
import sys
import tempfile

PROG = "tools/gen-package-md5sums.py"
EX_USAGE = 2


def eprint(msg):
    sys.stderr.write(msg + "\n")


def die(msg):
    eprint("ERROR: %s" % msg)
    sys.exit(EX_USAGE)


def collect_rows(root):
    """返回 [(相对路径, md5hex)]，按相对路径字节序排序；常规文件 only。"""
    if not os.path.isdir(root):
        die("--root is not a directory: %s" % root)
    rows = []
    for dirpath, dirnames, filenames in os.walk(root, followlinks=False):
        rel_dir = os.path.relpath(dirpath, root)
        if rel_dir == "DEBIAN" or rel_dir.startswith("DEBIAN" + os.sep):
            dirnames[:] = []
            continue
        if os.environ.get("FPI_MD5_INJECT") == "walk-order":
            dirnames.reverse()
        for name in filenames:
            abs_path = os.path.join(dirpath, name)
            if os.path.islink(abs_path):
                continue  # 符号链接不入 md5sums（dpkg 惯例）
            if not os.path.isfile(abs_path):
                continue
            rel = os.path.relpath(abs_path, root).replace(os.sep, "/")
            if rel.startswith("../"):
                die("unexpected path escape: %s" % rel)
            h = hashlib.md5()
            with open(abs_path, "rb") as fh:
                for chunk in iter(lambda: fh.read(1024 * 1024), b""):
                    h.update(chunk)
            rows.append((rel, h.hexdigest()))
    rows.sort(key=lambda r: r[0].encode("utf-8"))
    return rows


def render(rows):
    return "".join("%s  %s\n" % (md5, rel) for rel, md5 in rows)


def main():
    ap = argparse.ArgumentParser(prog=PROG)
    ap.add_argument("--root", required=True,
                    help="package assembly root (contains DEBIAN/)")
    ap.add_argument("-o", "--out", default=None,
                    help="output file (default: <root>/DEBIAN/md5sums)")
    ap.add_argument("--stdout", action="store_true",
                    help="write to stdout instead of a file (tests)")
    args = ap.parse_args()

    root = os.path.realpath(args.root)
    rows = collect_rows(root)
    data = render(rows).encode("utf-8")

    if args.stdout:
        sys.stdout.buffer.write(data)
        return 0

    out = args.out if args.out else os.path.join(root, "DEBIAN", "md5sums")
    if not os.path.isdir(os.path.dirname(out)):
        die("output directory does not exist: %s" % os.path.dirname(out))
    fd, tmp = tempfile.mkstemp(prefix=".md5sums.", dir=os.path.dirname(out))
    try:
        with os.fdopen(fd, "wb") as fh:
            fh.write(data)
            fh.flush()
            os.fsync(fh.fileno())
        os.replace(tmp, out)
        os.chmod(out, 0o644)
    except OSError as exc:
        try:
            if os.path.lexists(tmp):
                os.unlink(tmp)
        except OSError:
            pass
        die("failed to write md5sums: %s" % exc)
    print("RESULT: PASS_GEN_PACKAGE_MD5SUMS rows=%d out=%s"
          % (len(rows), out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
