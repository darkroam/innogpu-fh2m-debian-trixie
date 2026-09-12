#!/usr/bin/env bash
# tests/unit/run-check-release-package-tests.sh — scripts/check-release-package.sh 单测
#
# C1-① 口径：包名白名单（innogpu-fh2m-trixie|fantgpu-fh2m-trixie）、版本正则
# （patched-N>20 | 4.0.x-iN | 5.0.0-iN）、helper 路径按 $package 参数化。
# required/forbidden 载荷断言 = C1-②（批 3）——本套件只用合成 deb 验证
# 白名单与版本门（合成 deb 缺 required 文件，通过白名单后失败于
# 「required release file」，据此区分白名单拒绝与内容拒绝）。
# 退出码：0=全过 1=用例失败 2=环境错误。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
GATE="$ROOT/scripts/check-release-package.sh"
cd "$ROOT"
export LC_ALL=C

[ -f "$GATE" ] || { echo "FATAL: gate not found" >&2; exit 2; }
command -v dpkg-deb >/dev/null 2>&1 || { echo "FATAL: dpkg-deb required" >&2; exit 2; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/crp-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT INT TERM HUP

PASS=0; FAILN=0
ok()  { PASS=$((PASS + 1)); echo "ok  $1"; }
bad() { FAILN=$((FAILN + 1)); echo "BAD $1: $2" >&2; }

mkdeb() {  # mkdeb <name> <package> <version>
    local name="$1" package="$2" version="$3"
    local root="$TMP/$name"
    mkdir -p "$root/DEBIAN" "$root/usr/share/$package"
    printf 'Package: %s\nVersion: %s\nArchitecture: amd64\n' \
        "$package" "$version" > "$root/DEBIAN/control"
    printf 'Maintainer: fixture\nDescription: fixture\nInstalled-Size: 1\n' \
        >> "$root/DEBIAN/control"
    touch "$root/usr/share/$package/placeholder"
    dpkg-deb --root-owner-group -b "$root" "$TMP/$name.deb" >/dev/null
    echo "DEB=$TMP/$name.deb"
}

run_gate() {  # run_gate <deb>
    set +e
    bash "$GATE" "$1" > "$TMP/g.out" 2> "$TMP/g.err"
    RC=$?
    set -e
    ERRTEXT="$(cat "$TMP/g.err")"
}

# t01 O 白名单 + 版本正则：4.0.2-i3 通过白名单后失败于 required 内容
eval "$(mkdeb t01 innogpu-fh2m-trixie 4.0.2-i3)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "required release file is missing" "$TMP/g.err"; then
    ok t01
else
    bad t01 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t02 F 白名单 + 5.0.0-iN 正则：fantgpu-fh2m-trixie 5.0.0-i2 通过白名单
eval "$(mkdeb t02 fantgpu-fh2m-trixie 5.0.0-i2)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "required release file is missing" "$TMP/g.err"; then
    ok t02
else
    bad t02 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t03 未知包名 → unexpected package
eval "$(mkdeb t03 other-fh2m-trixie 4.0.2-i3)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "unexpected package" "$TMP/g.err"; then
    ok t03
else
    bad t03 "rc=$RC"
fi

# t04 F 包 + 未审核版本 → 血统配对拒绝
eval "$(mkdeb t04 fantgpu-fh2m-trixie 9.9.9)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "only accepts 5.0.0-iN" "$TMP/g.err"; then
    ok t04
else
    bad t04 "rc=$RC"
fi

# t05 patched-N 旧口径回归：patched-28 通过白名单后失败于 required
eval "$(mkdeb t05 innogpu-fh2m-trixie 3.3.3.42-patched-28)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "required release file is missing" "$TMP/g.err"; then
    ok t05
else
    bad t05 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t06 无参数 → rc=2
set +e
bash "$GATE" >/dev/null 2>&1
RC6=$?
set -e
if [ "$RC6" -eq 2 ]; then ok t06; else bad t06 "rc=$RC6 (want 2)"; fi

# t07 deb 缺失 → rc=1
set +e
bash "$GATE" "$TMP/nope.deb" >/dev/null 2>&1
RC7=$?
set -e
if [ "$RC7" -eq 1 ]; then ok t07; else bad t07 "rc=$RC7 (want 1)"; fi

# t08 交叉负向：innogpu 包 + 5.0.0-i2 → 拒绝（codex 初审 P1-4）
eval "$(mkdeb t08 innogpu-fh2m-trixie 5.0.0-i2)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "only accepts patched-N" "$TMP/g.err"; then
    ok t08
else
    bad t08 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t09 交叉负向：fantgpu 包 + 4.0.2-i3 → 拒绝（codex 初审 P1-4）
eval "$(mkdeb t09 fantgpu-fh2m-trixie 4.0.2-i3)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "only accepts 5.0.0-iN" "$TMP/g.err"; then
    ok t09
else
    bad t09 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

echo "PASS=$PASS FAIL=$FAILN"
[ "$FAILN" -eq 0 ] || exit 1
exit 0
