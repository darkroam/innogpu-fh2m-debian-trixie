#!/usr/bin/env bash
# tests/unit/run-validate-fantgpu-manifest-tests.sh — tools/validate-binary-manifest-fantgpu.py 单元验证
#
# 依据 docs/planning/c3-a-4-reproducible-input-plan.md §二：SHA/mode/链接
# 目标漂移检出、O 血统 loader 文件名禁则、载荷缺失检出（fail-closed 预检）。
# 全合成夹具（FPI_VAL_FIXTURE=1 放宽全局 660/deb SHA 检查——与 ③ 工具
# --baseline 同口径，生产运行必须缺省）。
# 退出码：0=全过 1=用例失败 2=环境错误。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOL="$ROOT/tools/validate-binary-manifest-fantgpu.py"
cd "$ROOT"
export LC_ALL=C

[ -f "$TOOL" ] || { echo "FATAL: tool not found: $TOOL" >&2; exit 2; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/fval-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT INT TERM HUP

PASS=0; FAILN=0
ok()  { PASS=$((PASS + 1)); echo "ok  $1"; }
bad() { FAILN=$((FAILN + 1)); echo "BAD $1: $2" >&2; }

# mkfix <name> [link_target]：构造 vendor 树 + 匹配 manifest（2 常规 + 1 symlink + 2 目录）
mkfix() {
    local name="$1" link_target="${2:-a.txt}"
    local base="$TMP/$name"
    mkdir -p "$base/vendor/usr/sub"
    chmod 0755 "$base/vendor" "$base/vendor/usr" "$base/vendor/usr/sub"
    printf 'A-CONTENT\n' > "$base/vendor/usr/a.txt"
    printf 'B-CONTENT\n' > "$base/vendor/usr/sub/b.txt"
    ln -s "$link_target" "$base/vendor/usr/link.txt"
    chmod 0644 "$base/vendor/usr/a.txt" "$base/vendor/usr/sub/b.txt"
    LINK_TARGET="$link_target" python3 - "$base" <<'PY'
import hashlib, json, os, sys
base = sys.argv[1]
link_target = os.environ["LINK_TARGET"]
def sha(p):
    return hashlib.sha256(open(p, "rb").read()).hexdigest()
entries = [
    {"kind": "userspace-lib", "license": "vendor-binary", "role": "t",
     "sha256": sha(os.path.join(base, "vendor/usr/a.txt")), "size": 9,
     "source_path": "usr/a.txt", "vendor_path": "fantgpu/usr/a.txt",
     "mode": "0644", "variant": None, "materialize": "direct"},
    {"kind": "userspace-lib", "license": "vendor-binary", "role": "t",
     "sha256": sha(os.path.join(base, "vendor/usr/sub/b.txt")), "size": 9,
     "source_path": "usr/sub/b.txt",
     "vendor_path": "fantgpu/usr/sub/b.txt",
     "mode": "0644", "variant": None, "materialize": "direct"},
    {"kind": "userspace-lib", "license": "vendor-binary", "role": "t",
     "sha256": None, "size": None, "source_path": "usr/link.txt",
     "vendor_path": "fantgpu/usr/link.txt", "mode": "0777",
     "target": link_target, "variant": None, "materialize": "direct"},
]
obj = {"format_version": 1, "architecture": "amd64",
       "source_package": "fixture", "source_version": "1",
       "source_deb_sha256": "0" * 64, "input_entries": 3,
       "dir_entries": 2, "entries": entries}
with open(os.path.join(base, "manifest.json"), "w") as fh:
    json.dump(obj, fh, indent=2, sort_keys=True)
PY
    echo "FIX=$base"
}

run_val() {  # run_val <fix-base> [env-assign]
    local base="$1"; shift
    set +e
    FPI_VAL_FIXTURE=1 "$@" python3 "$TOOL" --manifest "$base/manifest.json" \
        --vendor "$base/vendor" > "$TMP/v.out" 2> "$TMP/v.err"
    RC=$?
    set -e
    ERRTXT="$(cat "$TMP/v.err")"
}

# t01 正向
eval "$(mkfix t01)"
run_val "$FIX"
if [ "$RC" -eq 0 ] && grep -q "entries=3 regular=2 symlink=1" "$TMP/v.out"; then
    ok t01
else
    bad t01 "rc=$RC"
fi

# t02 SHA 漂移
eval "$(mkfix t02)"
printf 'DRIFT\n' > "$FIX/vendor/usr/a.txt"
run_val "$FIX"
if [ "$RC" -eq 1 ] && grep -Fq "sha256 drift" "$TMP/v.err"; then
    ok t02
else
    bad t02 "rc=$RC"
fi

# t03 mode 漂移（chmod 不改 SHA 但必须 fail）
eval "$(mkfix t03)"
chmod 0600 "$FIX/vendor/usr/a.txt"
run_val "$FIX"
if [ "$RC" -eq 1 ] && grep -Fq "mode drift" "$TMP/v.err"; then
    ok t03
else
    bad t03 "rc=$RC"
fi

# t04 链接目标漂移
eval "$(mkfix t04)"
rm "$FIX/vendor/usr/link.txt"
ln -s b.txt "$FIX/vendor/usr/link.txt"
run_val "$FIX"
if [ "$RC" -eq 1 ] && grep -Fq "symlink target drift" "$TMP/v.err"; then
    ok t04
else
    bad t04 "rc=$RC"
fi

# t05 载荷缺失
eval "$(mkfix t05)"
rm "$FIX/vendor/usr/sub/b.txt"
run_val "$FIX"
if [ "$RC" -eq 1 ] && grep -Fq "missing payload entry" "$TMP/v.err"; then
    ok t05
else
    bad t05 "rc=$RC"
fi

# t06 O 血统 loader 文件名禁则
eval "$(mkfix t06)"
printf 'O\n' > "$FIX/vendor/usr/innogpu_dri.so"
run_val "$FIX"
if [ "$RC" -eq 1 ] && grep -Fq "O-lineage loader filename" "$TMP/v.err"; then
    ok t06
else
    bad t06 "rc=$RC"
fi

# t07 生产口径：非 fixture（全局 deb SHA 检查）→ rc=1
eval "$(mkfix t07)"
set +e
python3 "$TOOL" --manifest "$FIX/manifest.json" --vendor "$FIX/vendor" \
    >/dev/null 2>&1
RC7=$?
set -e
if [ "$RC7" -eq 1 ]; then ok t07; else bad t07 "rc=$RC7 (want 1)"; fi

# t08 manifest 非 JSON → rc=2
eval "$(mkfix t08)"
printf 'not-json\n' > "$FIX/manifest.json"
run_val "$FIX"
if [ "$RC" -eq 2 ]; then ok t08; else bad t08 "rc=$RC (want 2)"; fi

# t09 vendor 缺失 → rc=2
eval "$(mkfix t09)"
run_val "$FIX"
rm -rf "$FIX/vendor"
set +e
FPI_VAL_FIXTURE=1 python3 "$TOOL" --manifest "$FIX/manifest.json" \
    --vendor "$FIX/vendor" >/dev/null 2>&1
RC9=$?
set -e
if [ "$RC9" -eq 2 ]; then ok t09; else bad t09 "rc=$RC9 (want 2)"; fi

# t10 vendor 额外普通文件 → 严格双射失败 rc=1
eval "$(mkfix t10)"
printf 'EXTRA\n' > "$FIX/vendor/usr/extra.txt"
run_val "$FIX"
if [ "$RC" -eq 1 ] && grep -Fq "extra entries in vendor" "$TMP/v.err"; then
    ok t10
else
    bad t10 "rc=$RC"
fi

# t11 source_path 路径逃逸（../）→ rc=1
eval "$(mkfix t11)"
python3 - "$FIX/manifest.json" <<'PY'
import json, sys
p = sys.argv[1]
m = json.load(open(p))
m["entries"][0]["source_path"] = "../escape.txt"
m["entries"][0]["vendor_path"] = "fantgpu/../escape.txt"
json.dump(m, open(p, "w"), indent=2, sort_keys=True)
PY
run_val "$FIX"
if [ "$RC" -eq 1 ] && grep -Fq "unsafe source_path" "$TMP/v.err"; then
    ok t11
else
    bad t11 "rc=$RC"
fi

# t12 目录 mode 漂移 → rc=1
eval "$(mkfix t12)"
chmod 0700 "$FIX/vendor/usr/sub"
run_val "$FIX"
if [ "$RC" -eq 1 ] && grep -Fq "directory mode drift" "$TMP/v.err"; then
    ok t12
else
    bad t12 "rc=$RC"
fi

# t13 vendor 额外目录 → 目录计数失败 rc=1
eval "$(mkfix t13)"
mkdir -p "$FIX/vendor/usr/extra-dir"
chmod 0755 "$FIX/vendor/usr/extra-dir"
run_val "$FIX"
if [ "$RC" -eq 1 ] && grep -Fq "directory count" "$TMP/v.err"; then
    ok t13
else
    bad t13 "rc=$RC"
fi

# t14 symlink target 绝对路径 → rc=1（载荷根边界）
eval "$(mkfix t14 /tmp/victim)"
run_val "$FIX"
if [ "$RC" -eq 1 ] && grep -Fq "symlink target is absolute" "$TMP/v.err"; then
    ok t14
else
    bad t14 "rc=$RC"
fi

# t15 symlink target ../ 逃逸（解析后出根）→ rc=1（载荷根边界）
eval "$(mkfix t15 ../../escape.txt)"
run_val "$FIX"
if [ "$RC" -eq 1 ] && grep -Fq "symlink escapes vendor root" "$TMP/v.err"; then
    ok t15
else
    bad t15 "rc=$RC"
fi

echo "PASS=$PASS FAIL=$FAILN"
[ "$FAILN" -eq 0 ] || exit 1
exit 0
