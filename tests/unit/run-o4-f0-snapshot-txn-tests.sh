#!/bin/bash
# tests/unit/run-o4-f0-snapshot-txn-tests.sh — O-4 snapshot 事务提交故障注入测试
# （codex O-4 复审 P1：快照/sidecar/genesis 一致提交 + 任一步失败回滚旧对）
set -u
LC_ALL=C
export LC_ALL
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SCRIPT="$ROOT/tools/o4-f0-lock-gen.py"
runtime="$(mktemp -d "${TMPDIR:-/tmp}/o4-f0-snapshot-txn.XXXXXX")"
trap 'rm -rf "$runtime"' EXIT

passed=0; failed=0; t=0
pass() { t=$((t+1)); passed=$((passed+1)); printf 'o4_snap_t%02d=PASS # %s\n' "$t" "$1"; }
fail() { t=$((t+1)); failed=$((failed+1)); printf 'o4_snap_t%02d=FAIL reason=%s\n' "$t" "$2"; }

# fixture 源树
SRC="$runtime/src"
mkdir -p "$SRC/sub"
printf 'alpha\n' > "$SRC/a.txt"
printf '\x00\x01\x02' > "$SRC/sub/b.bin"

# 手写最小 genesis（update 路径读取）
mk_genesis() { printf '{"schema_version":"1.0"}\n' > "$1/o4-f0.genesis.json"; }

sha_of() { sha256sum "$1" | cut -d' ' -f1; }

# 1) 干净成功 + 同 out-dir 二次重跑 SHA 一致（重跑契约）
OUT="$runtime/out-clean"
mkdir -p "$OUT"
mk_genesis "$OUT"
python3 "$SCRIPT" snapshot --src-root "$SRC" --out-dir "$OUT" >/dev/null 2>&1; rc=$?
S1="$(sha_of "$OUT/f0-snapshot.tar.zst")"
python3 "$SCRIPT" snapshot --src-root "$SRC" --out-dir "$OUT" >/dev/null 2>&1; rc2=$?
S2="$(sha_of "$OUT/f0-snapshot.tar.zst")"
if [ "$rc" -eq 0 ] && [ "$rc2" -eq 0 ] && [ "$S1" == "$S2" ]; then
    pass clean_rerun_sha_stable
else
    fail clean_rerun_sha_stable "rc=$rc/$rc2 sha=$S1/$S2"
fi

# 2) commit_sha 注入失败：旧对必须完整保留
OUT2="$runtime/out-inject"
mkdir -p "$OUT2"
mk_genesis "$OUT2"
python3 "$SCRIPT" snapshot --src-root "$SRC" --out-dir "$OUT2" >/dev/null 2>&1
OLD_T="$(sha_of "$OUT2/f0-snapshot.tar.zst")"
OLD_S="$(sha_of "$OUT2/f0-snapshot.tar.zst.sha256")"
OLD_G="$(sha_of "$OUT2/o4-f0.genesis.json")"
O4_FAIL_INJECT=commit_sha python3 "$SCRIPT" snapshot --src-root "$SRC" --out-dir "$OUT2" >/dev/null 2>&1; rc=$?
if [ "$rc" -eq 5 ] && [ "$(sha_of "$OUT2/f0-snapshot.tar.zst")" == "$OLD_T" ] \
   && [ "$(sha_of "$OUT2/f0-snapshot.tar.zst.sha256")" == "$OLD_S" ]; then
    pass commit_sha_rollback_old_pair
else
    fail commit_sha_rollback_old_pair "rc=$rc"
fi

# 3) genesis_write 注入失败：对 + genesis 全保留
O4_FAIL_INJECT=genesis_write python3 "$SCRIPT" snapshot --src-root "$SRC" --out-dir "$OUT2" >/dev/null 2>&1; rc=$?
if [ "$rc" -eq 5 ] && [ "$(sha_of "$OUT2/f0-snapshot.tar.zst")" == "$OLD_T" ] \
   && [ "$(sha_of "$OUT2/f0-snapshot.tar.zst.sha256")" == "$OLD_S" ] \
   && [ "$(sha_of "$OUT2/o4-f0.genesis.json")" == "$OLD_G" ]; then
    pass genesis_write_rollback_all
else
    fail genesis_write_rollback_all "rc=$rc"
fi

# 4) 首跑 commit_tar 失败：不得留下不一致的新快照/sidecar
OUT3="$runtime/out-firstfail"
mkdir -p "$OUT3"
mk_genesis "$OUT3"
O4_FAIL_INJECT=commit_tar python3 "$SCRIPT" snapshot --src-root "$SRC" --out-dir "$OUT3" >/dev/null 2>&1; rc=$?
if [ "$rc" -eq 5 ] && [ ! -e "$OUT3/f0-snapshot.tar.zst" ] \
   && [ ! -e "$OUT3/f0-snapshot.tar.zst.sha256" ]; then
    pass first_run_failure_no_residue
else
    fail first_run_failure_no_residue "rc=$rc"
fi

# 5) 注入失败后正常重跑必须成功（残留事务可清理）
python3 "$SCRIPT" snapshot --src-root "$SRC" --out-dir "$OUT2" >/dev/null 2>&1; rc=$?
if [ "$rc" -eq 0 ] && [ "$(sha_of "$OUT2/f0-snapshot.tar.zst")" == "$S1" ]; then
    pass post_injection_rerun_ok
else
    fail post_injection_rerun_ok "rc=$rc"
fi

# 6) genesis 提交成功后备份清理失败：不得触发回滚，完整新对保持生效
#    （codex O-4 复审 P1 二轮：旧对+新 genesis 不一致窗口）
OUT4="$runtime/out-cleanupfail"
mkdir -p "$OUT4"
mk_genesis "$OUT4"
python3 "$SCRIPT" snapshot --src-root "$SRC" --out-dir "$OUT4" >/dev/null 2>&1
OLD4="$(sha_of "$OUT4/f0-snapshot.tar.zst")"
printf 'gamma\n' > "$SRC/c.txt"   # 变更源树 → 新快照 != 旧快照
O4_FAIL_INJECT=cleanup_bak python3 "$SCRIPT" snapshot --src-root "$SRC" --out-dir "$OUT4" >/dev/null 2>&1; rc=$?
NEW4="$(sha_of "$OUT4/f0-snapshot.tar.zst")"
GEN_SHA="$(python3 -c "import json;print(json.load(open('$OUT4/o4-f0.genesis.json'))['snapshot']['sha256'])")"
if [ "$rc" -eq 0 ] && [ "$NEW4" != "$OLD4" ] && [ "$GEN_SHA" == "$NEW4" ] \
   && grep -q "$NEW4" "$OUT4/f0-snapshot.tar.zst.sha256"; then
    pass cleanup_bak_failure_keeps_new_pair
else
    fail cleanup_bak_failure_keeps_new_pair "rc=$rc new=$NEW4 old=$OLD4 gen=$GEN_SHA"
fi
python3 "$SCRIPT" snapshot --src-root "$SRC" --out-dir "$OUT4" >/dev/null 2>&1; rc=$?
if [ "$rc" -eq 0 ] \
   && [ ! -e "$OUT4/f0-snapshot.tar.zst.bak.tmp" ] \
   && [ ! -e "$OUT4/f0-snapshot.tar.zst.sha256.bak.tmp" ]; then
    pass cleanup_bak_retry_ok
else
    fail cleanup_bak_retry_ok "rc=$rc"
fi

# 7) backup-only 中断状态 → 下次启动先恢复旧对（codex O-4 复审 P1 三轮：
#    真实进程中断于备份后/提交前，备份是唯一旧证据，禁止删除）
OUT5="$runtime/out-recover"
mkdir -p "$OUT5"
mk_genesis "$OUT5"
python3 "$SCRIPT" snapshot --src-root "$SRC" --out-dir "$OUT5" >/dev/null 2>&1
R_OLD="$(sha_of "$OUT5/f0-snapshot.tar.zst")"
# 模拟真实中断：旧对已移入 backup、正式对缺失
mv "$OUT5/f0-snapshot.tar.zst" "$OUT5/f0-snapshot.tar.zst.bak.tmp"
mv "$OUT5/f0-snapshot.tar.zst.sha256" "$OUT5/f0-snapshot.tar.zst.sha256.bak.tmp"
# 注入 post_recovery：恢复旧对后立即中止 → rc=5 + 旧对已还原 + 无备份残留
O4_FAIL_INJECT=post_recovery python3 "$SCRIPT" snapshot --src-root "$SRC" --out-dir "$OUT5" >/dev/null 2>&1; rc=$?
if [ "$rc" -eq 5 ] && [ "$(sha_of "$OUT5/f0-snapshot.tar.zst")" == "$R_OLD" ] \
   && [ ! -e "$OUT5/f0-snapshot.tar.zst.bak.tmp" ] \
   && [ ! -e "$OUT5/f0-snapshot.tar.zst.sha256.bak.tmp" ]; then
    pass backup_only_interruption_restores_old_pair
else
    fail backup_only_interruption_restores_old_pair "rc=$rc"
fi

# 8) 恢复后正常重跑成功
python3 "$SCRIPT" snapshot --src-root "$SRC" --out-dir "$OUT5" >/dev/null 2>&1; rc=$?
if [ "$rc" -eq 0 ]; then pass post_recovery_rerun_ok; else fail post_recovery_rerun_ok "rc=$rc"; fi

# 9) 恢复第二步失败 → 撤销第一步，保留完整 backup-only 状态可重试
#    （codex O-4 复审 P1 四轮：不得留下"正式 tar + bak sidecar"partial）
OUT6="$runtime/out-recover2"
mkdir -p "$OUT6"
mk_genesis "$OUT6"
python3 "$SCRIPT" snapshot --src-root "$SRC" --out-dir "$OUT6" >/dev/null 2>&1
R2_OLD="$(sha_of "$OUT6/f0-snapshot.tar.zst")"
mv "$OUT6/f0-snapshot.tar.zst" "$OUT6/f0-snapshot.tar.zst.bak.tmp"
mv "$OUT6/f0-snapshot.tar.zst.sha256" "$OUT6/f0-snapshot.tar.zst.sha256.bak.tmp"
O4_FAIL_INJECT=restore_sha python3 "$SCRIPT" snapshot --src-root "$SRC" --out-dir "$OUT6" >/dev/null 2>&1; rc=$?
if [ "$rc" -eq 5 ] \
   && [ "$(sha_of "$OUT6/f0-snapshot.tar.zst.bak.tmp")" == "$R2_OLD" ] \
   && [ -e "$OUT6/f0-snapshot.tar.zst.sha256.bak.tmp" ] \
   && [ ! -e "$OUT6/f0-snapshot.tar.zst" ]; then
    pass restore_step2_failure_undo_keeps_backup_only
else
    fail restore_step2_failure_undo_keeps_backup_only "rc=$rc"
fi

# 10) 完整 backup-only 状态可于下次启动正常恢复并重跑
python3 "$SCRIPT" snapshot --src-root "$SRC" --out-dir "$OUT6" >/dev/null 2>&1; rc=$?
if [ "$rc" -eq 0 ]; then pass backup_only_retry_ok; else fail backup_only_retry_ok "rc=$rc"; fi

# 12) 正式 tar/genesis 一致但 sidecar 不一致 → 保留现场并拒绝重跑
OUT7="$runtime/out-sidecar-mismatch"
mkdir -p "$OUT7"
mk_genesis "$OUT7"
python3 "$SCRIPT" snapshot --src-root "$SRC" --out-dir "$OUT7" >/dev/null 2>&1
MISMATCH_T="$(sha_of "$OUT7/f0-snapshot.tar.zst")"
MISMATCH_G="$(sha_of "$OUT7/o4-f0.genesis.json")"
printf '0000000000000000000000000000000000000000000000000000000000000000  f0-snapshot.tar.zst\n' > \
    "$OUT7/f0-snapshot.tar.zst.sha256"
python3 "$SCRIPT" snapshot --src-root "$SRC" --out-dir "$OUT7" >/dev/null 2>&1; rc=$?
if [ "$rc" -eq 5 ] \
   && [ "$(sha_of "$OUT7/f0-snapshot.tar.zst")" == "$MISMATCH_T" ] \
   && [ "$(sha_of "$OUT7/o4-f0.genesis.json")" == "$MISMATCH_G" ] \
   && grep -q '^0000000000000000000000000000000000000000000000000000000000000000  f0-snapshot.tar.zst$' \
       "$OUT7/f0-snapshot.tar.zst.sha256"; then
    pass sidecar_mismatch_fail_closed
else
    fail sidecar_mismatch_fail_closed "rc=$rc"
fi

printf 'tests_total=%d tests_passed=%d tests_failed=%d tests_skipped=0\n' "$t" "$passed" "$failed"
[[ "$failed" -eq 0 ]]
