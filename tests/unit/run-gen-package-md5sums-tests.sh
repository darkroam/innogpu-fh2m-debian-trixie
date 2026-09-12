#!/usr/bin/env bash
# tests/unit/run-gen-package-md5sums-tests.sh — tools/gen-package-md5sums.py 单元验证
#
# 依据 docs/planning/c3-a-4-reproducible-input-plan.md §四 强制项 1：
# LC_ALL=C 字节序排序 / 常规文件 only（symlink、目录排除）/ DEBIAN/ 排除 /
# 相对路径规范化 / 双空格格式 / 尾换行 / 双跑字节一致（含目录枚举乱序注入）。
# 全合成夹具，秒级，只写 $TMP。
# 退出码：0=全过 1=用例失败 2=环境错误。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOL="$ROOT/tools/gen-package-md5sums.py"
cd "$ROOT"
export LC_ALL=C

[ -f "$TOOL" ] || { echo "FATAL: tool not found: $TOOL" >&2; exit 2; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/md5s-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT INT TERM HUP

PASS=0; FAILN=0
ok()  { PASS=$((PASS + 1)); echo "ok  $1"; }
bad() { FAILN=$((FAILN + 1)); echo "BAD $1: $2" >&2; }

mkdir -p "$TMP/root/usr/bin" "$TMP/root/usr/share/doc" "$TMP/root/DEBIAN"
printf 'A\n' > "$TMP/root/usr/bin/a"
printf 'B\n' > "$TMP/root/usr/share/doc/b"
printf 'C\n' > "$TMP/root/zz-last"
ln -s usr/bin/a "$TMP/root/usr/bin/link-a"
printf 'control\n' > "$TMP/root/DEBIAN/control"
printf 'D\n' > "$TMP/root/DEBIAN/md5sums"

md5_of() { md5sum "$1" | cut -d' ' -f1; }

# t01 正向：常规文件 3 条、symlink/DEBIAN 排除、字节序排序、双空格格式
python3 "$TOOL" --root "$TMP/root" --stdout > "$TMP/out1" 2>"$TMP/err1"; RC1=$?
if [ "$RC1" -ne 0 ]; then
    bad t01 "rc=$RC1"
else
    n="$(wc -l < "$TMP/out1")"
    first="$(head -1 "$TMP/out1")"
    want_first="$(md5_of "$TMP/root/usr/bin/a")  usr/bin/a"
    if [ "$n" -eq 3 ] && [ "$first" = "$want_first" ] \
       && ! grep -q "DEBIAN" "$TMP/out1" && ! grep -q "link-a" "$TMP/out1"; then
        ok t01
    else
        bad t01 "n=$n first=[$first]"
    fi
fi

# t02 排序锁定：三行顺序 = usr/bin/a, usr/share/doc/b, zz-last（字节序）
order="$(cut -d' ' -f3- "$TMP/out1" | tr '\n' '|')"
if [ "$order" = "usr/bin/a|usr/share/doc/b|zz-last|" ]; then
    ok t02
else
    bad t02 "order=[$order]"
fi

# t03 双跑字节一致 + 目录枚举乱序注入一致
python3 "$TOOL" --root "$TMP/root" --stdout > "$TMP/out2" 2>/dev/null
FPI_MD5_INJECT=walk-order python3 "$TOOL" --root "$TMP/root" --stdout \
    > "$TMP/out3" 2>/dev/null
if cmp -s "$TMP/out1" "$TMP/out2" && cmp -s "$TMP/out1" "$TMP/out3"; then
    ok t03
else
    bad t03 "double-run or injected-order output differs"
fi

# t04 落盘：默认写到 <root>/DEBIAN/md5sums（覆盖预置内容）且字节一致
python3 "$TOOL" --root "$TMP/root" >/dev/null 2>"$TMP/err4"; RC4=$?
if [ "$RC4" -ne 0 ]; then
    bad t04 "rc=$RC4"
elif cmp -s "$TMP/root/DEBIAN/md5sums" "$TMP/out1"; then
    ok t04
else
    bad t04 "file output differs from stdout"
fi

# t05 显式 -o 输出
python3 "$TOOL" --root "$TMP/root" -o "$TMP/custom.md5" >/dev/null 2>&1
if cmp -s "$TMP/custom.md5" "$TMP/out1"; then
    ok t05
else
    bad t05 "-o output differs"
fi

# t06 守卫：root 缺失 → rc=2
set +e
python3 "$TOOL" --root "$TMP/nope" --stdout >/dev/null 2>&1
RC6=$?
set -e
if [ "$RC6" -eq 2 ]; then ok t06; else bad t06 "rc=$RC6 (want 2)"; fi

# t07 守卫：-o 目录不存在 → rc=2
set +e
python3 "$TOOL" --root "$TMP/root" -o "$TMP/nodir/x" >/dev/null 2>&1
RC7=$?
set -e
if [ "$RC7" -eq 2 ]; then ok t07; else bad t07 "rc=$RC7 (want 2)"; fi

echo "PASS=$PASS FAIL=$FAILN"
[ "$FAILN" -eq 0 ] || exit 1
exit 0
