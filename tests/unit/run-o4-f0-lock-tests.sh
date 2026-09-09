#!/bin/bash
# tests/unit/run-o4-f0-lock-tests.sh — O-4 输出路径边界校验负向测试
# （codex O-4 初审 P1：/tmpfoo 前缀误匹配 / 仓库外同名路径 /
# 输出目录 symlink 指向保护区，均必须 exit 78；合法路径通过边界检查
# 后因 fixture 缺失 exit 2，证明路径校验本身放行）
set -u
LC_ALL=C
export LC_ALL
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SCRIPT="$ROOT/tools/o4-f0-lock-gen.py"
runtime="$(mktemp -d "${TMPDIR:-/tmp}/o4-f0-lock-tests.XXXXXX")"
repo_runtime="$ROOT/.o4-f0-lock-tests.$$"
trap 'rm -rf "$runtime" "$repo_runtime"' EXIT

passed=0; failed=0; t=0
pass() { t=$((t+1)); passed=$((passed+1)); printf 'o4_f0_lock_t%02d=PASS # %s\n' "$t" "$1"; }
fail() { t=$((t+1)); failed=$((failed+1)); printf 'o4_f0_lock_t%02d=FAIL reason=%s\n' "$t" "$2"; }

# 边界检查先于 src-root/deb 校验（main: check_env -> check_out_dir -> fn），
# 故负向用例的 src-root/deb 可为占位串；正向用例用缺失 fixture 的 exit 2
# 证明路径校验放行。
expect_rc() { # <label> <want> <out-dir>
    local label="$1" want="$2" outdir="$3" rc
    python3 "$SCRIPT" manifest --src-root /nonexistent-src --deb-path /nonexistent.deb \
        --out-dir "$outdir" >/dev/null 2>&1
    rc=$?
    if [ "$rc" -eq "$want" ]; then pass "$label"; else fail "$label" "rc=$rc want=$want out=$outdir"; fi
}

# 1) /tmpfoo 前缀误匹配（旧实现 startswith('/tmp') 会放行）→ 78
expect_rc tmpfoo_rejected 78 "/tmpfoo-o4-$t"

# 2) /tmp 下合法（fresh 子目录）→ 通过边界（fixture 缺失 exit 2，非 78）
expect_rc tmp_subdir_ok 2 "$runtime/out-ok"

# 3) 本仓库真实 o-stage → 通过边界（fixture 缺失 exit 2，非 78；无写入发生）
expect_rc repo_ostage_ok 2 "$ROOT/docs/planning/evidence/o-stage"

# 4) 仓库外且不在 /tmp 的同名 docs/planning/evidence/o-stage → 78
expect_rc external_same_name_rejected 78 \
    "$ROOT/../o4-f0-lock-tests-$$/docs/planning/evidence/o-stage"

# 5) /tmp 下输出目录为 symlink 指向保护区（vendor/）→ 78
ln -s "$ROOT/vendor" "$runtime/out-sym-protected"
expect_rc tmp_symlink_into_protected 78 "$runtime/out-sym-protected"

# 6) 仓库内但非允许根的 o-stage symlink 指向保护区 → 78
mkdir -p "$repo_runtime/docs/planning/evidence"
ln -s "$ROOT/vendor" "$repo_runtime/docs/planning/evidence/o-stage"
expect_rc ostage_symlink_into_protected 78 "$repo_runtime/docs/planning/evidence/o-stage"

printf 'tests_total=%d tests_passed=%d tests_failed=%d tests_skipped=0\n' "$t" "$passed" "$failed"
[[ "$failed" -eq 0 ]]
