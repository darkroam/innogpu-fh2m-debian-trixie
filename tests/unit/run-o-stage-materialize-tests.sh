#!/usr/bin/env bash
# tests/unit/run-o-stage-materialize-tests.sh — materialize-o-stage.sh 单元验证
#
# 职责（docs/planning/o-stage-integration-plan.md §四）：用小型假输入树 +
# 假 patch 链验证 路径边界 / fail-closed / 链点校验 / 事务回滚与恢复 /
# 幂等重跑 / patch SHA 校验。所有用例秒级（假树），不触碰真实 F0/030 输入。
#
# 只读 scripts/materialize-o-stage.sh；写入 /tmp 测试目录；零保护区写入。
# 退出码：0=全过；1=任一用例失败。

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
LC_ALL=C
export LC_ALL

MATERIALIZE="$ROOT/scripts/materialize-o-stage.sh"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/ostage-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT

# 与实现同源的 tree hash（O-4 契约）
tree_hash() {
    python3 - "$1" <<'PY'
import importlib.util, hashlib, sys
spec = importlib.util.spec_from_file_location("o4", "tools/o4-f0-lock-gen.py")
o4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(o4)
rows = list(o4.walk_rows(sys.argv[1]))
print(hashlib.sha256(o4.manifest_text(rows).encode()).hexdigest())
PY
}

PASS=0; FAILN=0
ok() { PASS=$((PASS + 1)); echo "ok  $1"; }
bad() { FAILN=$((FAILN + 1)); echo "BAD $1: $2" >&2; }

# ---- 假输入构造：小树 + 1 条假 patch + 假 meta ----
FAKE="$TMP/fake-input"
FAKE_TREE_SRC="$TMP/fake-tree"
FAKE_PATCH_DIR="$TMP/fake-patches"
mkdir -p "$FAKE" "$FAKE_TREE_SRC/fantgpu" "$FAKE_TREE_SRC/fantsrvkm" "$FAKE_PATCH_DIR"
printf 'subdir-ccflags-y := -Wall\nobj-m += fantgpu.o\n' > "$FAKE_TREE_SRC/Kbuild"
printf 'int fake_value = 1;\n' > "$FAKE_TREE_SRC/fantgpu/fake_a.c"
printf 'int other_value = 2;\n' > "$FAKE_TREE_SRC/fantsrvkm/fake_b.c"

BEFORE_TREE="$(tree_hash "$FAKE_TREE_SRC")"
sed -i 's/fake_value = 1/fake_value = 42/' "$FAKE_TREE_SRC/fantgpu/fake_a.c"
AFTER_TREE="$(tree_hash "$FAKE_TREE_SRC")"
sed -i 's/fake_value = 42/fake_value = 1/' "$FAKE_TREE_SRC/fantgpu/fake_a.c"

# 假 030-fake.patch（a/ b/ 树根前缀，-p1）
{
    echo "diff -ruN a/fantgpu/fake_a.c b/fantgpu/fake_a.c"
    echo "--- a/fantgpu/fake_a.c"
    echo "+++ b/fantgpu/fake_a.c"
    echo "@@ -1 +1 @@"
    echo "-int fake_value = 1;"
    echo "+int fake_value = 42;"
} > "$FAKE_PATCH_DIR/030-fake.patch"

FAKE_PATCH_SHA="$(sha256sum "$FAKE_PATCH_DIR/030-fake.patch" | awk '{print $1}')"
python3 - "$FAKE_PATCH_DIR/030-fake.meta.json" "$FAKE_PATCH_SHA" "$BEFORE_TREE" "$AFTER_TREE" <<'PY'
import json, sys
m = {
    "schema_version": "1.0", "id": "030-fake",
    "deepin_patch": {"path": "patches/fake.patch", "sha256": "x", "apply_level": "-p1"},
    "f0_base": {"snapshot": {"path": "x", "sha256": "x"},
                "tree_hash": sys.argv[3],
                "chain_base": {"id": "none", "after_tree_hash": sys.argv[3]}},
    "review": {
        "semantics": {"status": "reviewed", "verdict": "adapted-port", "summary": "fake"},
        "license": {"status": "pending-audit", "evidence": "fake"},
        "dependencies": {"status": "reviewed", "verdict": "none", "summary": "fake"},
        "o_stage_adaptation": {"status": "reviewed", "verdict": "adapted-port", "summary": "fake"},
    },
    "apply": {"before_tree_hash": sys.argv[3], "after_tree_hash": sys.argv[4],
              "patch_sha256": sys.argv[2],
              "command": "patch -p1", "result": "fake"},
    "rollback": {"command": "fake", "target": sys.argv[3]},
    "status": "draft",
    "verified": {"static": {"status": "pass", "checks": ["fake"]},
                 "runtime": {"status": "pending", "required": "fake"}},
}
json.dump(m, open(sys.argv[1], "w", encoding="utf-8"), indent=2, ensure_ascii=False)
PY

# 假 f0 快照（transform 前缀 f0，同 O-4 参数）
tar --sort=name --mtime=@1640995200 --owner=0 --group=0 --numeric-owner \
    --no-acls --no-xattrs --no-selinux --transform='s,^\.,f0,' \
    -C "$FAKE_TREE_SRC" -cf - . | zstd -q -19 > "$FAKE/f0-snapshot.tar.zst"
FAKE_F0_SHA="$(sha256sum "$FAKE/f0-snapshot.tar.zst" | awk '{print $1}')"
printf '%s  %s\n' "$FAKE_F0_SHA" "f0-snapshot.tar.zst" > "$FAKE/f0-snapshot.tar.zst.sha256"

# 假 chain 文件在 materialize 内由真实 13 条链驱动，假输入用例仅覆盖
# 校验层（路径/版本/SHA）；链点与事务用例用真实输入跑（t06-t10，长用例）。

env_base=(
    OSTAGE_INPUT_DIR="$FAKE"
    OSTAGE_PATCH_DIR="$FAKE_PATCH_DIR"
    OSTAGE_OUT_DIR="$TMP/out"
    OSTAGE_WORK_DIR="$TMP/work"
    OSTAGE_EXPECT_F0_SHA="$FAKE_F0_SHA"
    OSTAGE_EXPECT_F0_TREE="$BEFORE_TREE"
    OSTAGE_EXPECT_FINAL_TREE="$AFTER_TREE"
)

run_materialize() { # run_materialize <label> <expect-rc> [extra env...]
    local label="$1" want="$2"; shift 2
    rm -rf "$TMP/out" "$TMP/work"
    mkdir -p "$TMP/out"
    set +e
    env "${env_base[@]}" "$@" "$MATERIALIZE" > "$TMP/run.log" 2>&1
    local rc=$?
    set -e
    if [[ "$rc" == "$want" ]]; then ok "$label (rc=$rc)"; else
        bad "$label" "rc=$rc want=$want"; sed -n '1,3p' "$TMP/run.log" >&2
    fi
}

# t01 输出目录越界（仓库外非 /tmp 路径）→ rc=78
run_materialize "t01 out-dir escape" 78 OSTAGE_OUT_DIR="$TMP/../../etc/ostage-x"
# t02 输出目录 symlink 拒绝 → rc=78
ln -s /tmp "$TMP/out-link"
run_materialize "t02 out-dir symlink" 78 OSTAGE_OUT_DIR="$TMP/out-link"
# t03 f0 快照 SHA 不符 → rc=1
run_materialize "t03 f0 sha mismatch" 1 OSTAGE_EXPECT_F0_SHA="0000000000000000000000000000000000000000000000000000000000000000"
# t04 patch 输入破坏（缺 meta/坏 patch 目录）→ rc=1（fail-closed）
mkdir -p "$TMP/bad-patches"
cp -r "$FAKE_PATCH_DIR/." "$TMP/bad-patches/"
printf 'corrupted\n' >> "$TMP/bad-patches/030-fake.patch"
run_materialize "t04 broken patch input fail-closed" 1 OSTAGE_PATCH_DIR="$TMP/bad-patches"
# t05 工具版本锁定 → rc=7（PATH 遮蔽 tar 版本）
mkdir -p "$TMP/fake-bin"
printf '#!/bin/sh\necho "tar (GNU tar) 1.34"\n' > "$TMP/fake-bin/tar"
printf '#!/bin/sh\necho "zstd command line interface 64-bits v1.5.7"\n' > "$TMP/fake-bin/zstd"
chmod +x "$TMP/fake-bin/tar" "$TMP/fake-bin/zstd"
rm -rf "$TMP/out"; mkdir -p "$TMP/out"
set +e
env "${env_base[@]}" PATH="$TMP/fake-bin:$PATH" "$MATERIALIZE" > "$TMP/run.log" 2>&1
rc=$?
set -e
[[ "$rc" == 7 ]] && ok "t05 tool version lock (rc=7)" || bad "t05" "rc=$rc want=7"

# ---- 真实输入用例（事务/链点/幂等；较慢，始终执行核心两条） ----
REAL_OUT="$TMP/real-out"
REAL_WORK="$TMP/real-work"
rm -rf "$REAL_OUT" "$REAL_WORK"; mkdir -p "$REAL_OUT"

# t06 事务故障注入：commit 中途失败 → rc=5 且不残留任何产物（首跑无旧代）
set +e
env OSTAGE_OUT_DIR="$REAL_OUT" OSTAGE_WORK_DIR="$REAL_WORK" \
    OSTAGE_FAIL_INJECT="commit_o-stage.manifest.tsv" \
    timeout 570 "$MATERIALIZE" > "$TMP/run.log" 2>&1
rc=$?
set -e
n_left=0
for f in o-stage-snapshot.tar.zst o-stage-snapshot.tar.zst.sha256 \
         o-stage.manifest.tsv o-stage.manifest.tsv.sha256 5.0.0-i1.meta.json; do
    [[ -e "$REAL_OUT/$f" ]] && n_left=$((n_left + 1))
done
n_txn="$(find "$REAL_OUT" -name '*.txn.tmp' | wc -l)"
if [[ "$rc" == 5 && "$n_left" == 0 && "$n_txn" == 0 ]]; then
    ok "t06 txn fail-closed rc=5 no artifacts no txn-residue"
else
    bad "t06" "rc=$rc leftover=$n_left txn=$n_txn (want rc=5 leftover=0 txn=0)"
fi

# t07 恢复 + 幂等：无注入重跑两次 → rc=0 且字节一致
set +e
env OSTAGE_OUT_DIR="$REAL_OUT" OSTAGE_WORK_DIR="$REAL_WORK" \
    timeout 570 "$MATERIALIZE" > "$TMP/run1.log" 2>&1
rc1=$?
set -e
[[ "$rc1" == 0 ]] && ok "t07 recovery rerun rc=0" || { bad "t07" "rc=$rc1"; sed -n '1,3p' "$TMP/run1.log" >&2; }
sha256sum "$REAL_OUT/o-stage-snapshot.tar.zst" "$REAL_OUT/5.0.0-i1.meta.json" > "$TMP/run1.sha"
set +e
env OSTAGE_OUT_DIR="$REAL_OUT" OSTAGE_WORK_DIR="$REAL_WORK" \
    timeout 570 "$MATERIALIZE" > "$TMP/run2.log" 2>&1
rc2=$?
set -e
[[ "$rc2" == 0 ]] && ok "t08 idempotent rerun rc=0" || bad "t08" "rc=$rc2"
sha256sum "$REAL_OUT/o-stage-snapshot.tar.zst" "$REAL_OUT/5.0.0-i1.meta.json" > "$TMP/run2.sha"
diff -q "$TMP/run1.sha" "$TMP/run2.sha" >/dev/null && ok "t09 idempotent byte-identical" \
    || bad "t09" "artifact bytes differ across runs"

# t10 链点校验：最终树 hash 注入错误值 → rc=1 且旧五件保留
before_artifacts="$(sha256sum "$REAL_OUT/o-stage-snapshot.tar.zst" | awk '{print $1}')"
set +e
env OSTAGE_OUT_DIR="$REAL_OUT" OSTAGE_WORK_DIR="$REAL_WORK" \
    OSTAGE_EXPECT_FINAL_TREE="0000000000000000000000000000000000000000000000000000000000000000" \
    timeout 570 "$MATERIALIZE" > "$TMP/run.log" 2>&1
rc=$?
set -e
after_artifacts="$(sha256sum "$REAL_OUT/o-stage-snapshot.tar.zst" | awk '{print $1}')"
if [[ "$rc" == 1 && "$before_artifacts" == "$after_artifacts" ]]; then
    ok "t10 chain-point fail-closed preserves artifacts"
else
    bad "t10" "rc=$rc preserved=$([[ "$before_artifacts" == "$after_artifacts" ]] && echo yes || echo no)"
fi

# t11 生成段故障注入（staged_done）：rc=5 + txn 零残留 + 旧五件保留
set +e
env OSTAGE_OUT_DIR="$REAL_OUT" OSTAGE_WORK_DIR="$REAL_WORK" \
    OSTAGE_FAIL_INJECT="staged_done" \
    timeout 570 "$MATERIALIZE" > "$TMP/run.log" 2>&1
rc=$?
set -e
n_txn11="$(find "$REAL_OUT" -name '*.txn.tmp' | wc -l)"
after11="$(sha256sum "$REAL_OUT/o-stage-snapshot.tar.zst" | awk '{print $1}')"
if [[ "$rc" == 5 && "$n_txn11" == 0 && "$after11" == "$before_artifacts" ]]; then
    ok "t11 staging fail rc=5 no txn residue old set preserved"
else
    bad "t11" "rc=$rc txn=$n_txn11 preserved=$([[ "$after11" == "$before_artifacts" ]] && echo yes || echo no)"
fi

# t12 提交失败 + 回滚失败组合：rc=5 + journal=rolling_back + 保留现场
set +e
env OSTAGE_OUT_DIR="$REAL_OUT" OSTAGE_WORK_DIR="$REAL_WORK" \
    OSTAGE_FAIL_INJECT="commit_o-stage.manifest.tsv,rollback_fail" \
    timeout 570 "$MATERIALIZE" > "$TMP/run.log" 2>&1
rc=$?
set -e
jstate="$(cat "$REAL_OUT/.materialize.journal" 2>/dev/null || true)"
if [[ "$rc" == 5 && "$jstate" == "rolling_back" ]]; then
    ok "t12 rollback-fail preserves rolling_back journal"
else
    bad "t12" "rc=$rc journal=$jstate (want rc=5 journal=rolling_back)"
fi
# 人工裁决后恢复：清事务残留并重置 journal，重跑应 rc=0
rm -f "$REAL_OUT"/*.txn.tmp "$REAL_OUT"/*.bak.tmp
printf 'staged\n' > "$REAL_OUT/.materialize.journal"
set +e
env OSTAGE_OUT_DIR="$REAL_OUT" OSTAGE_WORK_DIR="$REAL_WORK" \
    timeout 570 "$MATERIALIZE" > "$TMP/run.log" 2>&1
rc=$?
set -e
[[ "$rc" == 0 ]] && ok "t13 post-rollback recovery rc=0" || bad "t13" "rc=$rc"

# t14 提交循环完成但 committed 写入失败（五件新代 + bak 保留 + journal=staged）
# → 自洽判定应自动完成提交收尾并续跑 rc=0
for f in o-stage-snapshot.tar.zst o-stage-snapshot.tar.zst.sha256 \
         o-stage.manifest.tsv o-stage.manifest.tsv.sha256 5.0.0-i1.meta.json; do
    cp -p "$REAL_OUT/$f" "$REAL_OUT/$f.bak.tmp"
done
printf 'staged\n' > "$REAL_OUT/.materialize.journal"
set +e
env OSTAGE_OUT_DIR="$REAL_OUT" OSTAGE_WORK_DIR="$REAL_WORK" \
    timeout 570 "$MATERIALIZE" > "$TMP/run.log" 2>&1
rc=$?
set -e
n_bak14="$(find "$REAL_OUT" -name '*.bak.tmp' | wc -l)"
j14="$(cat "$REAL_OUT/.materialize.journal" 2>/dev/null || true)"
if [[ "$rc" == 0 && "$n_bak14" == 0 ]]; then
    ok "t14 self-consistent recovery after committed-write failure (rc=0 bak cleaned)"
else
    bad "t14" "rc=$rc bak=$n_bak14 journal=$j14"
fi

# t15 未知 journal 状态（ASCII garbage + 五件齐全）→ rc=5 保留现场
printf 'garbage\n' > "$REAL_OUT/.materialize.journal"
set +e
env OSTAGE_OUT_DIR="$REAL_OUT" OSTAGE_WORK_DIR="$REAL_WORK" \
    timeout 570 "$MATERIALIZE" > "$TMP/run.log" 2>&1
rc=$?
set -e
[[ "$rc" == 5 ]] && ok "t15 unknown journal state fail-closed rc=5" \
    || bad "t15" "rc=$rc want=5"

# t16 损坏 journal（非 ASCII 字节）→ rc=5
printf '\xff\xfe\x00garbage\n' > "$REAL_OUT/.materialize.journal"
set +e
env OSTAGE_OUT_DIR="$REAL_OUT" OSTAGE_WORK_DIR="$REAL_WORK" \
    timeout 570 "$MATERIALIZE" > "$TMP/run.log" 2>&1
rc=$?
set -e
[[ "$rc" == 5 ]] && ok "t16 corrupt journal fail-closed rc=5" \
    || bad "t16" "rc=$rc want=5"

# t17 恢复入口清理失败注入（recover_clean）→ rc=5 保留现场
printf 'staged\n' > "$REAL_OUT/.materialize.journal"
set +e
env OSTAGE_OUT_DIR="$REAL_OUT" OSTAGE_WORK_DIR="$REAL_WORK" \
    OSTAGE_FAIL_INJECT="recover_clean" \
    timeout 570 "$MATERIALIZE" > "$TMP/run.log" 2>&1
rc=$?
set -e
[[ "$rc" == 5 ]] && ok "t17 recovery cleanup failure fail-closed rc=5" \
    || bad "t17" "rc=$rc want=5"
# 复位后正常重跑
printf 'staged\n' > "$REAL_OUT/.materialize.journal"
set +e
env OSTAGE_OUT_DIR="$REAL_OUT" OSTAGE_WORK_DIR="$REAL_WORK" \
    timeout 570 "$MATERIALIZE" > "$TMP/run.log" 2>&1
rc=$?
set -e
[[ "$rc" == 0 ]] && ok "t18 final reset rerun rc=0" || bad "t18" "rc=$rc"

echo
echo "PASS=$PASS FAIL=$FAILN"
[[ "$FAILN" == 0 ]] || exit 1
exit 0
