#!/usr/bin/env bash
# tests/unit/run-gen-fantgpu-manifest-tests.sh — tools/gen-fantgpu-manifest.py 单元验证
#
# 依据 docs/planning/c3-a-4-reproducible-input-plan.md §二：
# S_INPUT 与 ③ 清单严格双射（含 mode 逐值）、零多写/漏写、变体组完备
# （未分类 /opt 条目拒绝）、symlink 条目口径、schema 违规拒绝、sidecar 计数。
# 正向用例读真实 ③ 证据（只读，不触碰 debs/vendor/build）；负向用临时拷贝。
# 退出码：0=全过 1=用例失败 2=环境错误。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOL="$ROOT/tools/gen-fantgpu-manifest.py"
TSV="$ROOT/docs/planning/evidence/o-stage/f-payload.manifest.tsv"
cd "$ROOT"
export LC_ALL=C

[ -f "$TOOL" ] || { echo "FATAL: tool not found: $TOOL" >&2; exit 2; }
[ -f "$TSV" ] || { echo "FATAL: ③ tsv not found: $TSV" >&2; exit 2; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/fman-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT INT TERM HUP

PASS=0; FAILN=0
ok()  { PASS=$((PASS + 1)); echo "ok  $1"; }
bad() { FAILN=$((FAILN + 1)); echo "BAD $1: $2" >&2; }

# t01 正向：生成成功，计数 660/497/107/87，条目三键齐备
python3 "$TOOL" --out "$TMP/m1.json" > "$TMP/out1" 2>"$TMP/err1"; RC1=$?
if [ "$RC1" -ne 0 ]; then
    bad t01 "rc=$RC1"
else
    grep -q "input_entries=660 locked_reference_entries=497 f_materialized_entries=107 f_dir_entries=87" "$TMP/out1" \
        || { bad t01 "counts line: $(cat "$TMP/out1")"; RC1=99; }
    if [ "$RC1" -eq 0 ]; then
        summary="$(python3 - "$TMP/m1.json" <<'PY'
import json, sys
m = json.load(open(sys.argv[1]))
es = m["entries"]
n_reg = sum(1 for e in es if e.get("sha256"))
n_sym = sum(1 for e in es if e.get("target") is not None)
bad_k = [e["source_path"] for e in es
         if e.get("materialize") is None or e.get("kind") is None
         or e.get("mode") is None]
bad_vp = [e["source_path"] for e in es
          if e.get("vendor_path") != "fantgpu/" + e["source_path"]]
ddx = sorted(e["variant"] for e in es
             if (e.get("variant") or "").startswith("ddx-abi-"))
print(len(es), n_reg, n_sym, len(bad_k), len(bad_vp), ",".join(ddx))
PY
)"
        want="660 602 58 0 0 ddx-abi-1.19,ddx-abi-1.20,ddx-abi-1.21"
        if [ "$summary" = "$want" ]; then ok t01; else bad t01 "summary=[$summary]"; fi
    fi
fi

# t02 确定性：双跑字节一致
python3 "$TOOL" --out "$TMP/m2.json" >/dev/null 2>&1
if cmp -s "$TMP/m1.json" "$TMP/m2.json"; then ok t02; else bad t02 "differs"; fi

# t03 sidecar 漂移 → rc=1
cp "$TSV" "$TMP/t3.tsv"
printf 'deadbeef  f-payload.manifest.tsv\n' > "$TMP/t3.tsv.sha256"
set +e
python3 "$TOOL" --tsv "$TMP/t3.tsv" --out "$TMP/m3.json" >/dev/null 2>&1
RC3=$?
set -e
if [ "$RC3" -eq 1 ]; then ok t03; else bad t03 "rc=$RC3 (want 1)"; fi

# t04 SHA 漂移注入 → rc=1
set +e
FPI_FMAN_GEN_INJECT=sha-drift python3 "$TOOL" --out "$TMP/m4.json" \
    >/dev/null 2>"$TMP/err4"
RC4=$?
set -e
if [ "$RC4" -eq 1 ] && grep -Fq "invalid sha256" "$TMP/err4"; then
    ok t04
else
    bad t04 "rc=$RC4 (want 1)"
fi

# t05 未分类 /opt 条目 → rc=1（sw-fant-gl 行改名，双射保持）
python3 - "$TSV" "$TMP/t5.tsv" <<'PY'
import sys
src, dst = sys.argv[1], sys.argv[2]
with open(src) as fh:
    lines = fh.read().split("\n")
out = []
for ln in lines:
    if "\topt/fantgpu-fh2m/usr/sbin/sw-fant-gl\t" in ln:
        ln = ln.replace("usr/sbin/sw-fant-gl", "usr/sbin/unknown-helper")
    out.append(ln)
with open(dst, "w") as fh:
    fh.write("\n".join(out))
PY
sha="$(sha256sum "$TMP/t5.tsv" | cut -d' ' -f1)"
printf '%s  f-payload.manifest.tsv\n' "$sha" > "$TMP/t5.tsv.sha256"
set +e
python3 "$TOOL" --tsv "$TMP/t5.tsv" --out "$TMP/m5.json" >/dev/null 2>"$TMP/err5"
RC5=$?
set -e
if [ "$RC5" -eq 1 ] && grep -Fq "unclassified /opt entry" "$TMP/err5"; then
    ok t05
else
    bad t05 "rc=$RC5 (want 1)"
fi

# t06 计数漂移（删一行载荷文件）→ rc=1
python3 - "$TSV" "$TMP/t6.tsv" <<'PY'
import sys
src, dst = sys.argv[1], sys.argv[2]
with open(src) as fh:
    lines = fh.read().split("\n")
out = []
skipped = False
for ln in lines:
    if not skipped and ln.startswith("f\tusr/lib/"):
        skipped = True
        continue
    out.append(ln)
with open(dst, "w") as fh:
    fh.write("\n".join(out))
PY
sha="$(sha256sum "$TMP/t6.tsv" | cut -d' ' -f1)"
printf '%s  f-payload.manifest.tsv\n' "$sha" > "$TMP/t6.tsv.sha256"
set +e
python3 "$TOOL" --tsv "$TMP/t6.tsv" --out "$TMP/m6.json" >/dev/null 2>&1
RC6=$?
set -e
if [ "$RC6" -eq 1 ]; then ok t06; else bad t06 "rc=$RC6 (want 1)"; fi

# t07 畸形行（7 列变 6 列）→ rc=2（输入格式 = 用法/环境错误口径）
python3 - "$TSV" "$TMP/t7.tsv" <<'PY'
import sys
src, dst = sys.argv[1], sys.argv[2]
with open(src) as fh:
    lines = fh.read().split("\n")
out = []
for ln in lines:
    if ln.startswith("f\tusr/lib/"):
        ln = ln.replace("\t0644\t", "\t0644;", 1)
        # 合并两列使总列数为 6
        parts = ln.split("\t")
        if len(parts) == 7:
            parts[3] = parts[3] + parts[4]
            del parts[4]
            ln = "\t".join(parts)
    out.append(ln)
with open(dst, "w") as fh:
    fh.write("\n".join(out))
PY
sha="$(sha256sum "$TMP/t7.tsv" | cut -d' ' -f1)"
printf '%s  f-payload.manifest.tsv\n' "$sha" > "$TMP/t7.tsv.sha256"
set +e
python3 "$TOOL" --tsv "$TMP/t7.tsv" --out "$TMP/m7.json" >/dev/null 2>&1
RC7=$?
set -e
if [ "$RC7" -eq 2 ]; then ok t07; else bad t07 "rc=$RC7 (want 2)"; fi

# t08 守卫：tsv 缺失 → rc=2
set +e
python3 "$TOOL" --tsv "$TMP/nope.tsv" --out "$TMP/m8.json" >/dev/null 2>&1
RC8=$?
set -e
if [ "$RC8" -eq 2 ]; then ok t08; else bad t08 "rc=$RC8 (want 2)"; fi

# t09 symlink target 绝对路径 → rc=1（词法边界，codex re-review P1-2）
python3 - "$TSV" "$TMP/t9.tsv" <<'PY'
import sys
src, dst = sys.argv[1], sys.argv[2]
with open(src) as fh:
    lines = fh.read().split("\n")
out = []
for ln in lines:
    if ln.startswith("l\t") and "\t/tmp/x" not in ln:
        parts = ln.split("\t")
        if len(parts) == 7:
            parts[6] = "/tmp/x"
            ln = "\t".join(parts)
    out.append(ln)
with open(dst, "w") as fh:
    fh.write("\n".join(out))
PY
sha="$(sha256sum "$TMP/t9.tsv" | cut -d' ' -f1)"
printf '%s  f-payload.manifest.tsv\n' "$sha" > "$TMP/t9.tsv.sha256"
set +e
python3 "$TOOL" --tsv "$TMP/t9.tsv" --out "$TMP/m9.json" >/dev/null 2>"$TMP/err9"
RC9=$?
set -e
if [ "$RC9" -eq 1 ] && grep -Fq "symlink target is absolute" "$TMP/err9"; then
    ok t09
else
    bad t09 "rc=$RC9 (want 1)"
fi

# t10 symlink target ../ 逃逸 → rc=1（词法边界）
python3 - "$TSV" "$TMP/t10.tsv" <<'PY'
import sys
src, dst = sys.argv[1], sys.argv[2]
with open(src) as fh:
    lines = fh.read().split("\n")
out = []
for ln in lines:
    if ln.startswith("l\t"):
        parts = ln.split("\t")
        if len(parts) == 7:
            parts[6] = "../" * 10 + "escape.txt"
            ln = "\t".join(parts)
    out.append(ln)
with open(dst, "w") as fh:
    fh.write("\n".join(out))
PY
sha="$(sha256sum "$TMP/t10.tsv" | cut -d' ' -f1)"
printf '%s  f-payload.manifest.tsv\n' "$sha" > "$TMP/t10.tsv.sha256"
set +e
python3 "$TOOL" --tsv "$TMP/t10.tsv" --out "$TMP/m10.json" >/dev/null 2>"$TMP/err10"
RC10=$?
set -e
if [ "$RC10" -eq 1 ] && grep -Fq "escapes payload root" "$TMP/err10"; then
    ok t10
else
    bad t10 "rc=$RC10 (want 1)"
fi

# t11 产物 mode 0644（codex re-review P2：tracked 清单可读性）
python3 "$TOOL" --out "$TMP/m11.json" >/dev/null 2>&1
if [ "$(stat -c %a "$TMP/m11.json")" = "644" ]; then
    ok t11
else
    bad t11 "mode=$(stat -c %a "$TMP/m11.json")"
fi

echo "PASS=$PASS FAIL=$FAILN"
[ "$FAILN" -eq 0 ] || exit 1
exit 0
