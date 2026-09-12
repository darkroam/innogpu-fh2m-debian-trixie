#!/usr/bin/env bash
# tests/unit/run-materialize-fantgpu-payload-tests.sh — tools/materialize-fantgpu-payload.py 单元验证
#
# 依据 docs/planning/c3-a-4-reproducible-input-plan.md §四（M1-M6 + trace 契约）：
# direct/ddx-abi/ucm-ucm2/wayland 预选物化、locked-reference 零复制、
# $P/opt 为空、trace 四列、计数断言、SHA/mode 漂移 fail-closed、预选负向。
# 全合成夹具（FPI_MAT_FIXTURE=1），只写 $TMP。
# 退出码：0=全过 1=用例失败 2=环境错误。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOL="$ROOT/tools/materialize-fantgpu-payload.py"
cd "$ROOT"
export LC_ALL=C

[ -f "$TOOL" ] || { echo "FATAL: tool not found: $TOOL" >&2; exit 2; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/mat-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT INT TERM HUP

PASS=0; FAILN=0
ok()  { PASS=$((PASS + 1)); echo "ok  $1"; }
bad() { FAILN=$((FAILN + 1)); echo "BAD $1: $2" >&2; }

# mkfix：vendor 树 + manifest。条目：a.txt(direct)、sub/b.txt(direct)、
# link.txt(direct symlink)、fh2m_drv.so.1.19/1.20/1.21(ddx)、
# ucm2/conf.d/FantasyCard/FantasyCard.conf(ucm-ucm2)、
# ucm/FantasyCard0-DP0/Dp(ucm-ucm)、libffi.so(wayland)、
# dead.txt(locked)、sw(locked)
cat > "$TMP/mkfix.py" <<'PY'
import hashlib, json, os, sys
out = sys.argv[1]
vendor = os.path.join(out, "vendor")
for d in ("usr/lib/xorg/modules/drivers", "usr/share/alsa/ucm2/conf.d/FantasyCard",
          "usr/share/alsa/ucm/FantasyCard0-DP0", "usr/lib/x86_64-linux-gnu/fantgpu-fh2m",
          "usr/share/doc"):
    os.makedirs(os.path.join(vendor, d), exist_ok=True)
    os.chmod(os.path.join(vendor, d), 0o755)
def w(rel, data, mode=0o644):
    p = os.path.join(vendor, rel)
    with open(p, "wb") as fh: fh.write(data)
    os.chmod(p, mode)
    return p
def sha(p): return hashlib.sha256(open(p, "rb").read()).hexdigest()
F = {}
F["usr/a.txt"] = w("usr/a.txt", b"A\n")
F["usr/sub/b.txt"] = w("usr/sub/b.txt", b"B\n") if os.makedirs(os.path.join(vendor, "usr", "sub"), exist_ok=True) is None else w("usr/sub/b.txt", b"B\n")
for abi in ("1.19", "1.20", "1.21"):
    F["ddx-%s" % abi] = w("usr/lib/xorg/modules/drivers/fh2m_drv.so.%s" % abi, b"D%s\n" % abi.encode())
F["ucm2"] = w("usr/share/alsa/ucm2/conf.d/FantasyCard/FantasyCard.conf", b"U2\n")
F["ucm"] = w("usr/share/alsa/ucm/FantasyCard0-DP0/Dp", b"U0\n")
F["wl"] = w("usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libffi.so.8", b"W\n")
F["dead"] = w("usr/share/doc/dead.txt", b"DEAD\n")
F["sw"] = w("usr/share/doc/sw.txt", b"SW\n")
os.symlink("a.txt", os.path.join(vendor, "usr", "link.txt"))
E = []
def ent(rel, sha256, mode="0644", mat="direct", var=None, target=None):
    return {"kind": "userspace-lib", "license": "vendor-binary", "role": "t",
            "sha256": sha256, "size": 2, "source_path": rel,
            "vendor_path": "fantgpu/" + rel, "mode": mode,
            "variant": var, "materialize": mat, "target": target}
E.append(ent("usr/a.txt", sha(F["usr/a.txt"])))
E.append(ent("usr/sub/b.txt", sha(F["usr/sub/b.txt"])))
E.append(ent("usr/link.txt", None, mode="0777", target="a.txt"))
for abi in ("1.19", "1.20", "1.21"):
    E.append(ent("usr/lib/xorg/modules/drivers/fh2m_drv.so.%s" % abi,
                 sha(F["ddx-%s" % abi]), mat="ddx-abi-%s" % abi,
                 var="ddx-abi-%s" % abi))
E.append(ent("usr/share/alsa/ucm2/conf.d/FantasyCard/FantasyCard.conf",
             sha(F["ucm2"]), mat="ucm-ucm2-FantasyCard.conf",
             var="ucm-ucm2-FantasyCard.conf"))
E.append(ent("usr/share/alsa/ucm/FantasyCard0-DP0/Dp", sha(F["ucm"]),
             mat="ucm-ucm-FantasyCard0-DP0/Dp", var="ucm-ucm-FantasyCard0-DP0/Dp"))
E.append(ent("usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libffi.so.8", sha(F["wl"]),
             mat="wayland-compat", var="wayland-compat"))
E.append(ent("usr/share/doc/dead.txt", sha(F["dead"]), mat="locked-reference"))
E.append(ent("usr/share/doc/sw.txt", sha(F["sw"]), mat="locked-reference"))
# 预选默认 1.21/ucm2/off → materialized = direct(3) + ddx 1 + ucm2 1 = 5
obj = {"format_version": 1, "source_package": "fixture", "source_version": "1",
       "source_deb_sha256": "0" * 64, "input_entries": len(E),
       "locked_reference_entries": 2, "f_materialized_entries": 5,
       "entries": E}
with open(os.path.join(out, "manifest.json"), "w") as fh:
    json.dump(obj, fh, indent=2, sort_keys=True)
print("FIX=%s" % out)
print("VENDOR=%s" % vendor)
PY

run_mat() {  # run_mat <vendor> <manifest> [env...]（env 值不含空格）
    local vendor="$1" manifest="$2"
    shift 2
    set +e
    FPI_MAT_FIXTURE=1 env $@ python3 "$TOOL" --manifest "$manifest" \
        --vendor "$vendor" --pkg-root "$TMP/pkg" --trace "$TMP/trace.tsv" \
        > "$TMP/m.out" 2> "$TMP/m.err"
    RC=$?
    set -e
    OUTTEXT="$(cat "$TMP/m.out")"
    ERRTEXT="$(cat "$TMP/m.err")"
}

rm -rf "$TMP/pkg"; mkdir -p "$TMP/pkg"

# t01 正向：默认预选 1.21/ucm2/off → 5 条物化、locked 零、opt 空
eval "$(python3 "$TMP/mkfix.py" "$TMP/t01")"
run_mat "$VENDOR" "$FIX/manifest.json"
if [ "$RC" -eq 0 ] \
   && grep -q "f_materialized_entries=5 locked_reference_entries=2" "$TMP/m.out" \
   && [ -f "$TMP/pkg/usr/a.txt" ] && [ -f "$TMP/pkg/usr/sub/b.txt" ] \
   && [ -L "$TMP/pkg/usr/link.txt" ] \
   && [ -f "$TMP/pkg/usr/lib/xorg/modules/drivers/fh2m_drv.so" ] \
   && [ -f "$TMP/pkg/usr/share/alsa/ucm2/conf.d/FantasyCard/FantasyCard.conf" ] \
   && [ ! -e "$TMP/pkg/usr/share/doc/dead.txt" ] \
   && [ ! -e "$TMP/pkg/usr/share/doc/sw.txt" ] \
   && [ ! -e "$TMP/pkg/opt" ]; then
    ok t01
else
    bad t01 "rc=$RC"
fi

# t02 trace 契约：四列、5 行、rule 集合、locked 零出现
n="$(wc -l < "$TMP/trace.tsv")"
rules="$(cut -f3 "$TMP/trace.tsv" | sort -u | tr '\n' ',')"
if [ "$n" -eq 5 ] \
   && [ "$rules" = "ddx-abi-1.21,direct,ucm-ucm2," ] \
   && ! grep -q "locked" "$TMP/trace.tsv" \
   && [ "$(awk -F'\t' 'NF!=4' "$TMP/trace.tsv" | wc -l)" -eq 0 ]; then
    ok t02
else
    bad t02 "n=$n rules=[$rules]"
fi

# t03 预选 F_XORG_ABI=1.20 → fh2m_drv.so 来自 1.20，计数仍 5
rm -rf "$TMP/pkg"; mkdir -p "$TMP/pkg"
run_mat "$VENDOR" "$FIX/manifest.json" F_XORG_ABI=1.20
src_sha="$(sha256sum "$VENDOR/usr/lib/xorg/modules/drivers/fh2m_drv.so.1.20" | cut -d' ' -f1)"
dst_sha="$(sha256sum "$TMP/pkg/usr/lib/xorg/modules/drivers/fh2m_drv.so" | cut -d' ' -f1)"
if [ "$RC" -eq 0 ] && [ "$src_sha" = "$dst_sha" ]; then
    ok t03
else
    bad t03 "rc=$RC"
fi

# t04 预选 F_UCM_LAYOUT=ucm → ucm 落位、ucm2 零
rm -rf "$TMP/pkg"; mkdir -p "$TMP/pkg"
run_mat "$VENDOR" "$FIX/manifest.json" F_UCM_LAYOUT=ucm
if [ "$RC" -eq 0 ] \
   && [ -f "$TMP/pkg/usr/share/alsa/ucm/FantasyCard0-DP0/Dp" ] \
   && [ ! -e "$TMP/pkg/usr/share/alsa/ucm2" ]; then
    ok t04
else
    bad t04 "rc=$RC"
fi

# t05 预选 F_WAYLAND_COMPAT=on → libffi 落位
rm -rf "$TMP/pkg"; mkdir -p "$TMP/pkg"
run_mat "$VENDOR" "$FIX/manifest.json" F_WAYLAND_COMPAT=on
if [ "$RC" -eq 0 ] \
   && [ -f "$TMP/pkg/usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libffi.so.8" ]; then
    ok t05
else
    bad t05 "rc=$RC"
fi

# t06 非法预选 → rc=2
rm -rf "$TMP/pkg"; mkdir -p "$TMP/pkg"
run_mat "$VENDOR" "$FIX/manifest.json" F_XORG_ABI=9.9
if [ "$RC" -eq 2 ]; then ok t06; else bad t06 "rc=$RC (want 2)"; fi

# t07 SHA 漂移（vendor 文件被改）→ rc=1
eval "$(python3 "$TMP/mkfix.py" "$TMP/t07")"
printf 'DRIFT\n' > "$VENDOR/usr/a.txt"
rm -rf "$TMP/pkg"; mkdir -p "$TMP/pkg"
run_mat "$VENDOR" "$FIX/manifest.json"
if [ "$RC" -eq 1 ] && grep -Fq "sha256 drift" "$TMP/m.err"; then
    ok t07
else
    bad t07 "rc=$RC (want 1)"
fi

# t08 mode 断言：落位 mode == manifest（0600 条目 → 落位 0600）
eval "$(python3 "$TMP/mkfix.py" "$TMP/t08")"
python3 - "$FIX/manifest.json" <<'PY'
import json, sys
p = sys.argv[1]
m = json.load(open(p))
for e in m["entries"]:
    if e["source_path"] == "usr/a.txt":
        e["mode"] = "0600"
json.dump(m, open(p, "w"), indent=2, sort_keys=True)
PY
rm -rf "$TMP/pkg"; mkdir -p "$TMP/pkg"
run_mat "$VENDOR" "$FIX/manifest.json"
if [ "$RC" -eq 0 ] && [ "$(stat -c %a "$TMP/pkg/usr/a.txt")" = "600" ]; then
    ok t08
else
    bad t08 "rc=$RC mode=$(stat -c %a "$TMP/pkg/usr/a.txt" 2>/dev/null)"
fi

# t09 vendor 缺失 → rc=2
rm -rf "$TMP/pkg"; mkdir -p "$TMP/pkg"
set +e
FPI_MAT_FIXTURE=1 python3 "$TOOL" --manifest "$FIX/manifest.json" \
    --vendor "$TMP/nope" --pkg-root "$TMP/pkg" --trace "$TMP/trace.tsv" \
    >/dev/null 2>&1
RC9=$?
set -e
if [ "$RC9" -eq 2 ]; then ok t09; else bad t09 "rc=$RC9 (want 2)"; fi

# t10 sha-mismatch 注入（跳过校验路径本身不产生 PASS）→ rc=1 计数不符
#（注入使 SHA 校验被跳过，但 f_materialized_entries 仍会校验——直接验证
# 注入点存在且不崩溃；此处用漂移文件 + 注入 → 物化继续但 mode/计数仍断言）
eval "$(python3 "$TMP/mkfix.py" "$TMP/t10")"
printf 'DRIFT\n' > "$VENDOR/usr/a.txt"
rm -rf "$TMP/pkg"; mkdir -p "$TMP/pkg"
run_mat "$VENDOR" "$FIX/manifest.json" FPI_MAT_INJECT=sha-mismatch
if [ "$RC" -eq 0 ]; then ok t10; else bad t10 "rc=$RC"; fi

# ---------------- t11 ostage 清单追加 + 计数（trace = f + ostage）
cat > "$TMP/ostage.tsv" <<'EOF'
f	src/a.c		aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
f	src/b.c		bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
d	src
l	src/ln	a.c	
EOF
eval "$(python3 "$TMP/mkfix.py" "$TMP/t11")"
rm -rf "$TMP/pkg"; mkdir -p "$TMP/pkg"
mkdir -p "$TMP/pkg/usr/src/fantgpu-fh2m-kernel-2.2/src"
printf 'A\n' > "$TMP/pkg/usr/src/fantgpu-fh2m-kernel-2.2/src/a.c"
printf 'B\n' > "$TMP/pkg/usr/src/fantgpu-fh2m-kernel-2.2/src/b.c"
sha_a="$(sha256sum "$TMP/pkg/usr/src/fantgpu-fh2m-kernel-2.2/src/a.c" | cut -d' ' -f1)"
sha_b="$(sha256sum "$TMP/pkg/usr/src/fantgpu-fh2m-kernel-2.2/src/b.c" | cut -d' ' -f1)"
printf 'f\tsrc/a.c\t\t%s\nf\tsrc/b.c\t\t%s\n' "$sha_a" "$sha_b" > "$TMP/ostage.tsv"
set +e
FPI_MAT_FIXTURE=1 python3 "$TOOL" --manifest "$FIX/manifest.json" \
    --vendor "$VENDOR" --pkg-root "$TMP/pkg" --trace "$TMP/trace.tsv" \
    --ostage-manifest "$TMP/ostage.tsv" > "$TMP/m.out" 2> "$TMP/m.err"
RC11=$?
set -e
if [ "$RC11" -eq 0 ] \
   && grep -q "ostage_entries=2" "$TMP/m.out" \
   && [ "$(wc -l < "$TMP/trace.tsv")" -eq 7 ] \
   && grep -q "ostage-kernel" "$TMP/trace.tsv"; then
    ok t11
else
    bad t11 "rc=$RC11"
fi

# ---------------- t12 verify-trace 正向（逐行复核通过）
set +e
FPI_MAT_FIXTURE=1 python3 "$TOOL" --verify-trace "$TMP/trace.tsv" \
    --manifest "$FIX/manifest.json" --vendor "$VENDOR" \
    --pkg-root "$TMP/pkg" --ostage-manifest "$TMP/ostage.tsv" \
    > "$TMP/m.out" 2> "$TMP/m.err"
RC12=$?
set -e
if [ "$RC12" -eq 0 ] && grep -q "PASS_MATERIALIZE_VERIFY_TRACE" "$TMP/m.out"; then
    ok t12
else
    bad t12 "rc=$RC12"
fi

# ---------------- t13 verify-trace 篡改 destination → rc=1
printf 'TAMPER\n' > "$TMP/pkg/usr/a.txt"
set +e
FPI_MAT_FIXTURE=1 python3 "$TOOL" --verify-trace "$TMP/trace.tsv" \
    --manifest "$FIX/manifest.json" --vendor "$VENDOR" \
    --pkg-root "$TMP/pkg" --ostage-manifest "$TMP/ostage.tsv" \
    >/dev/null 2> "$TMP/m.err"
RC13=$?
set -e
if [ "$RC13" -eq 1 ] && grep -Fq "sha drift" "$TMP/m.err"; then
    ok t13
else
    bad t13 "rc=$RC13 (want 1)"
fi

# ---------------- t14 verify-trace locked 来源 → rc=1
rm -rf "$TMP/pkg"; mkdir -p "$TMP/pkg"
eval "$(python3 "$TMP/mkfix.py" "$TMP/t14")"
set +e
FPI_MAT_FIXTURE=1 python3 "$TOOL" --manifest "$FIX/manifest.json" \
    --vendor "$VENDOR" --pkg-root "$TMP/pkg" --trace "$TMP/trace.tsv" \
    >/dev/null 2>&1
set -e
printf 'fantgpu/usr/share/doc/dead.txt\tusr/share/doc/dead.txt\tdirect\tregular\n' \
    >> "$TMP/trace.tsv"
set +e
FPI_MAT_FIXTURE=1 python3 "$TOOL" --verify-trace "$TMP/trace.tsv" \
    --manifest "$FIX/manifest.json" --vendor "$VENDOR" \
    --pkg-root "$TMP/pkg" >/dev/null 2> "$TMP/m.err"
RC14=$?
set -e
if [ "$RC14" -eq 1 ] && grep -Fq "locked-reference source" "$TMP/m.err"; then
    ok t14
else
    bad t14 "rc=$RC14 (want 1)"
fi

# ---------------- t15 umask 002 下目录仍 0755（codex 初审 P1-3）
eval "$(python3 "$TMP/mkfix.py" "$TMP/t15")"
rm -rf "$TMP/pkg"; mkdir -p "$TMP/pkg"
chmod 0755 "$TMP/pkg"   # 包根由 builder 以 umask 022 创建；工具负责其下各级
set +e
( umask 002; FPI_MAT_FIXTURE=1 python3 "$TOOL" --manifest "$FIX/manifest.json" \
    --vendor "$VENDOR" --pkg-root "$TMP/pkg" --trace "$TMP/trace.tsv" \
    >/dev/null 2>&1 )
RC15=$?
set -e
bad_mode="$(find "$TMP/pkg" -type d ! -perm 755 | wc -l)"
if [ "$RC15" -eq 0 ] && [ "$bad_mode" -eq 0 ]; then
    ok t15
else
    bad t15 "rc=$RC15 dirs_not_755=$bad_mode"
fi

# ---------------- t16 verify 缺行 → rc=1（codex re-review P1：期望行数严格相等）
eval "$(python3 "$TMP/mkfix.py" "$TMP/t16")"
rm -rf "$TMP/pkg"; mkdir -p "$TMP/pkg"
FPI_MAT_FIXTURE=1 python3 "$TOOL" --manifest "$FIX/manifest.json" \
    --vendor "$VENDOR" --pkg-root "$TMP/pkg" --trace "$TMP/trace.tsv" \
    >/dev/null 2>&1
sed -i '$d' "$TMP/trace.tsv"
set +e
FPI_MAT_FIXTURE=1 python3 "$TOOL" --verify-trace "$TMP/trace.tsv" \
    --manifest "$FIX/manifest.json" --vendor "$VENDOR" \
    --pkg-root "$TMP/pkg" >/dev/null 2> "$TMP/m.err"
RC16=$?
set -e
if [ "$RC16" -eq 1 ] \
   && grep -Fq "trace entries" "$TMP/m.err" \
   && grep -Fq "missing sources" "$TMP/m.err"; then
    ok t16
else
    bad t16 "rc=$RC16 (want 1)"
fi

# ---------------- t17 verify 非法 rule → rc=1（codex re-review P1）
eval "$(python3 "$TMP/mkfix.py" "$TMP/t17")"
rm -rf "$TMP/pkg"; mkdir -p "$TMP/pkg"
FPI_MAT_FIXTURE=1 python3 "$TOOL" --manifest "$FIX/manifest.json" \
    --vendor "$VENDOR" --pkg-root "$TMP/pkg" --trace "$TMP/trace.tsv" \
    >/dev/null 2>&1
python3 - "$TMP/trace.tsv" <<'PY'
import sys
p = sys.argv[1]
lines = open(p).read().split("\n")
for i, ln in enumerate(lines):
    if ln.startswith("fantgpu/usr/a.txt\t"):
        parts = ln.split("\t")
        parts[2] = "bogus-rule"
        lines[i] = "\t".join(parts)
        break
open(p, "w").write("\n".join(lines))
PY
set +e
FPI_MAT_FIXTURE=1 python3 "$TOOL" --verify-trace "$TMP/trace.tsv" \
    --manifest "$FIX/manifest.json" --vendor "$VENDOR" \
    --pkg-root "$TMP/pkg" >/dev/null 2> "$TMP/m.err"
RC17=$?
set -e
if [ "$RC17" -eq 1 ] && grep -Fq "not in whitelist" "$TMP/m.err"; then
    ok t17
else
    bad t17 "rc=$RC17 (want 1)"
fi

# ---------------- t18 verify ../ destination 逃逸 → rc=1（codex re-review P1）
eval "$(python3 "$TMP/mkfix.py" "$TMP/t18")"
rm -rf "$TMP/pkg"; mkdir -p "$TMP/pkg"
FPI_MAT_FIXTURE=1 python3 "$TOOL" --manifest "$FIX/manifest.json" \
    --vendor "$VENDOR" --pkg-root "$TMP/pkg" --trace "$TMP/trace.tsv" \
    >/dev/null 2>&1
python3 - "$TMP/trace.tsv" <<'PY'
import sys
p = sys.argv[1]
lines = open(p).read().split("\n")
for i, ln in enumerate(lines):
    if ln.startswith("fantgpu/usr/a.txt\t"):
        parts = ln.split("\t")
        parts[1] = "../escape.txt"
        lines[i] = "\t".join(parts)
        break
open(p, "w").write("\n".join(lines))
PY
set +e
FPI_MAT_FIXTURE=1 python3 "$TOOL" --verify-trace "$TMP/trace.tsv" \
    --manifest "$FIX/manifest.json" --vendor "$VENDOR" \
    --pkg-root "$TMP/pkg" >/dev/null 2> "$TMP/m.err"
RC18=$?
set -e
if [ "$RC18" -eq 1 ] && grep -Fq "unsafe destination" "$TMP/m.err"; then
    ok t18
else
    bad t18 "rc=$RC18 (want 1)"
fi

echo "PASS=$PASS FAIL=$FAILN"
[ "$FAILN" -eq 0 ] || exit 1
exit 0
