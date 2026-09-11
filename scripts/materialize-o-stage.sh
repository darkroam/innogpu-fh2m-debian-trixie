#!/usr/bin/env bash
# scripts/materialize-o-stage.sh — 阶段三 O_stage 源树物化（F0 + 13 条 030-NNN → o-stage 快照）
#
# 契约：docs/planning/o-stage-integration-plan.md §一（dsh 终审通过）。
#   - 前置校验（tar 1.35 / zstd 1.5.7 / f0 快照 SHA / 13 条 patch SHA）
#   - 解包 F0 → 树 hash 校验（O-4 锁定 7219d817…）
#   - 13 条 030-NNN 按链序 -p1 --fuzz=0 应用，每链点校验 after_tree_hash
#   - 最终树 hash == 030-029.meta.json after_tree_hash（= 937e3710…）
#   - 五件同代事务产物（生成顺序：manifest → manifest.sha256 → tar →
#     tar.sha256 → meta.json（引用四文件 SHA）；提交顺序 = 产物名固定序；
#     每步 rename 后目录 fsync；journal + 恢复；禁止混代；fail-closed 统一 5）
#
# tree hash 契约 = O-4 口径（复用 tools/o4-f0-lock-gen.py 的 walk_rows /
# manifest_text，SHA-256 of canonical manifest bytes）。
#
# 只读：f0-snapshot.tar.zst / patches/030-*；只写：--out-dir（默认
# docs/planning/evidence/o-stage，阶段三证据/产物写入目录）与 /tmp 工作目录；
# 绝不写任何保护区路径（debs/vendor/build/third_party 及 drivers/baselines）。
#
# 用法：scripts/materialize-o-stage.sh（无参数；不接受任何参数）
# 测试注入（仅 tests 使用，生产零影响）：OSTAGE_INPUT_DIR / OSTAGE_PATCH_DIR /
# OSTAGE_OUT_DIR / OSTAGE_WORK_DIR / OSTAGE_EXPECT_F0_SHA / OSTAGE_EXPECT_F0_TREE /
# OSTAGE_EXPECT_FINAL_TREE / OSTAGE_FAIL_INJECT
#
# 退出码：0=成功；1=输入缺失/补丁失败/残留物；2=参数错误；
# 5=事务失败（旧产物保持）；6=锁获取失败；7=tar/zstd 版本不匹配；
# 78=输出目录越界/配置错误。

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"
LC_ALL=C
export LC_ALL

[[ $# -eq 0 ]] || { echo "Usage: $0" >&2; exit 2; }

# ---- 锁定常量（可被测试注入覆盖） ----
F0_SNAPSHOT_SHA_DEFAULT="65fa17ec56925241a33b2daf6eacdd93699f8d3764aaeecf47d9888c40a6da9e"
F0_TREE_HASH_DEFAULT="7219d817c412fcf87a5341f1604e03bb24b7b6670cf51d0f2c8814e5fe72c0cb"
FINAL_TREE_HASH_DEFAULT="937e37107f692712e6fba9b5eb93a48dd6bb034f138e5e51a2bb9c2beaa0d652"
EXPECT_F0_SHA="${OSTAGE_EXPECT_F0_SHA:-$F0_SNAPSHOT_SHA_DEFAULT}"
EXPECT_F0_TREE="${OSTAGE_EXPECT_F0_TREE:-$F0_TREE_HASH_DEFAULT}"
EXPECT_FINAL_TREE="${OSTAGE_EXPECT_FINAL_TREE:-$FINAL_TREE_HASH_DEFAULT}"

# ---- 快照确定性参数（O-4 先例，仅 transform 前缀不同） ----
TAR_REQUIRED="1.35"
ZSTD_REQUIRED="1.5.7"
EPOCH_MTIME="1640995200"
# ---- 5.0.0-i1 provenance（dsh 批准） ----
VERSION="5.0.0-i1"
TAG="fantgpu-5.0.0-i1"
SOURCE_DATE_EPOCH="1788796800"
VALIDATION_RESULTS="docs/planning/evidence/o-stage/5.0.0-i1-validation-results.json"
VALIDATION_SCHEMA="validation-results-1.0"

# ---- 路径 ----
IN_DIR="${OSTAGE_INPUT_DIR:-$ROOT/docs/planning/evidence/o-stage}"
PATCH_DIR="${OSTAGE_PATCH_DIR:-$ROOT/patches}"
OUT_DIR="${OSTAGE_OUT_DIR:-$ROOT/docs/planning/evidence/o-stage}"
WORK="${OSTAGE_WORK_DIR:-${TMPDIR:-/tmp}/r16-o-stage-materialize}"

SNAP_IN="$IN_DIR/f0-snapshot.tar.zst"
SNAP_SIDE_IN="$IN_DIR/f0-snapshot.tar.zst.sha256"

# ---- 产物名（五件同代） ----
P_TAR="o-stage-snapshot.tar.zst"
P_TAR_SHA="o-stage-snapshot.tar.zst.sha256"
P_MANIFEST="o-stage.manifest.tsv"
P_MANIFEST_SHA="o-stage.manifest.tsv.sha256"
P_META="5.0.0-i1.meta.json"
LOCK_FILE="$OUT_DIR/.materialize.lock"
JOURNAL="$OUT_DIR/.materialize.journal"

# ---- 13 条 030-NNN 链（显式清单，禁止 glob） ----
CHAIN=(
  "030-001|patches/030-001.patch"
  "030-002|patches/030-002.patch"
  "030-006|patches/030-006.patch"
  "030-009|patches/030-009.patch"
  "030-007|patches/030-007.patch"
  "030-023|patches/030-023.patch"
  "030-025|patches/030-025.patch"
  "030-024|patches/030-024.patch"
  "030-026|patches/030-026.patch"
  "030-027|patches/030-027.patch"
  "030-026-lifecycle|patches/030-026-lifecycle.patch"
  "030-028|patches/030-028.patch"
  "030-029|patches/030-029.patch"
)

die() { echo "materialize=FAIL: $*" >&2; exit 1; }

sha256_of() { sha256sum "$1" | awk '{print $1}'; }

tool_version() {
    local out
    out="$("$1" --version 2>/dev/null | head -1)"
    if [[ "$out" =~ ([0-9]+\.[0-9]+(\.[0-9]+)?) ]]; then
        printf '%s' "${BASH_REMATCH[1]}"
    else
        printf 'unknown'
    fi
}

require_tools() {
    local tv zv
    tv="$(tool_version tar)"
    zv="$(tool_version zstd)"
    if [[ "$tv" != "$TAR_REQUIRED" || "$zv" != "$ZSTD_REQUIRED" ]]; then
        echo "materialize=FAIL: exact tool lock failed: tar=$tv (need $TAR_REQUIRED), zstd=$zv (need $ZSTD_REQUIRED)" >&2
        exit 7
    fi
}

# tree_hash：O-4 契约（复用 o4 模块 walk_rows + manifest_text 单一来源）
tree_hash() {
    python3 - "$1" <<'PY'
import importlib.util, hashlib, sys
spec = importlib.util.spec_from_file_location("o4", "tools/o4-f0-lock-gen.py")
o4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(o4)
rows = list(o4.walk_rows(sys.argv[1]))
text = o4.manifest_text(rows)
print(hashlib.sha256(text.encode()).hexdigest())
PY
}

# meta 字段读取（python json）
meta_field() { # meta_field <meta.json> <dotted-key>
    python3 - "$1" "$2" <<'PY'
import json, sys
m = json.load(open(sys.argv[1], encoding="utf-8"))
v = m
for k in sys.argv[2].split("."):
    v = v[k]
print(v)
PY
}

check_out_dir() {
    # O-4 check_out_dir 口径：realpath 边界 + symlink 拒绝
    local ap rp tmp_root allowed
    ap="$(cd "$(dirname "$OUT_DIR")" 2>/dev/null && echo "$(pwd)/$(basename "$OUT_DIR")")"
    if [[ -L "$OUT_DIR" ]]; then
        echo "materialize=FAIL: out-dir is a symlink: $OUT_DIR (refuse to write through it)" >&2
        exit 78
    fi
    rp="$(realpath -m "$OUT_DIR")"
    tmp_root="$(realpath /tmp)"
    allowed="$(realpath -m "$ROOT/docs/planning/evidence/o-stage")"
    case "$rp" in
        "$tmp_root"|"$tmp_root"/*|"$allowed"|"$allowed"/*) ;;
        *)
            echo "materialize=FAIL: out-dir not allowed: $OUT_DIR (realpath=$rp)" >&2
            exit 78 ;;
    esac
}

# 故障注入（仅测试；OSTAGE_FAIL_INJECT 匹配时失败）
inject_fail() {
    if [[ "${OSTAGE_FAIL_INJECT:-}" == "$1" ]]; then
        die "injected failure at $1"
    fi
}

# ---- 五件同代事务产物（内联 python；O-4 cmd_snapshot 语义扩展为五件） ----
commit_artifacts() { # commit_artifacts <tree_root> <chain_json_file>
    OSTAGE_FAIL_INJECT="${OSTAGE_FAIL_INJECT:-}" \
    python3 - "$1" "$2" "$OUT_DIR" <<'PY'
import hashlib, importlib.util, json, os, subprocess, sys

tree_root, chain_file, out_dir = sys.argv[1], sys.argv[2], sys.argv[3]
fail = os.environ.get("OSTAGE_FAIL_INJECT") or ""
fails = set(p for p in fail.split(",") if p)

def inject(point):
    if point in fails:
        raise OSError("injected failure at %s" % point)

def die(code, msg):
    print("materialize=FAIL: %s" % msg, file=sys.stderr)
    sys.exit(code)

def fsync_fd(f):
    os.fsync(f.fileno())

def fsync_path(p):
    fd = os.open(p, os.O_RDONLY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for blk in iter(lambda: fh.read(1 << 20), b""):
            h.update(blk)
    return h.hexdigest()

spec = importlib.util.spec_from_file_location("o4", "tools/o4-f0-lock-gen.py")
o4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(o4)

EPOCH_MTIME = "1640995200"
TAR_REQUIRED, ZSTD_REQUIRED = "1.35", "1.5.7"
VERSION, TAG, SDE = "5.0.0-i1", "fantgpu-5.0.0-i1", "1788796800"
VAL_RES = "docs/planning/evidence/o-stage/5.0.0-i1-validation-results.json"
VAL_SCHEMA = "validation-results-1.0"

NAMES = ["o-stage-snapshot.tar.zst", "o-stage-snapshot.tar.zst.sha256",
         "o-stage.manifest.tsv", "o-stage.manifest.tsv.sha256",
         "5.0.0-i1.meta.json"]
dest = {n: os.path.join(out_dir, n) for n in NAMES}
txn = {n: dest[n] + ".txn.tmp" for n in NAMES}
bak = {n: dest[n] + ".bak.tmp" for n in NAMES}
journal = os.path.join(out_dir, ".materialize.journal")

def write_journal(state):
    with open(journal + ".tmp", "w", encoding="ascii") as fh:
        fh.write(state + "\n")
        fsync_fd(fh)
    os.replace(journal + ".tmp", journal)
    fsync_path(os.path.dirname(journal))

def journal_state():
    try:
        with open(journal, "r", encoding="ascii") as fh:
            return fh.read().strip()
    except UnicodeDecodeError:
        return "corrupt"
    except OSError:
        return None

def consistent_set():
    # 五件要么全在、要么全无（同代）；部分存在 = 混代/中断
    present = [os.path.lexists(dest[n]) for n in NAMES]
    return all(present), any(present)

def clean_txn():
    for n in NAMES:
        if os.path.lexists(txn[n]):
            os.unlink(txn[n])
            fsync_path(out_dir)

def clean_bak():
    for n in NAMES:
        if os.path.lexists(bak[n]):
            os.unlink(bak[n])
            fsync_path(out_dir)

def artifacts_self_consistent():
    # 以 dest meta.json 的四文件 SHA 交叉验证五件自洽（提交循环完成但
    # journal 未更新的状态判定依据）
    try:
        with open(dest["5.0.0-i1.meta.json"], "r", encoding="utf-8") as fh:
            m = json.load(fh)
        art = m["snapshot_artifacts"]
        return (sha256_file(dest["o-stage-snapshot.tar.zst"])
                == art["o-stage-snapshot.tar.zst"]["sha256"]
                and sha256_file(dest["o-stage.manifest.tsv"])
                == art["o-stage.manifest.tsv"]["sha256"]
                and sha256_file(dest["o-stage-snapshot.tar.zst.sha256"])
                == art["o-stage-snapshot.tar.zst.sha256"]["sha256"]
                and sha256_file(dest["o-stage.manifest.tsv.sha256"])
                == art["o-stage.manifest.tsv.sha256"]["sha256"])
    except (OSError, KeyError, ValueError):
        return False

# ---- 恢复入口（O-4 recover_interrupted 口径，扩展为五件集合） ----
st = journal_state()
full, anyp = consistent_set()
bak_present = any(os.path.lexists(bak[n]) for n in NAMES)
if st == "rolling_back":
    # 回滚中断：无法判定恢复方向，保留全部文件人工裁决（O-4 口径）
    die(5, "journal state rolling_back; all files preserved; "
           "manual resolution required before re-run")
elif st == "staged":
    try:
        inject("recover_clean")
        if not anyp:
            clean_txn(); clean_bak()
        elif full and not bak_present:
            # 提交循环已完成、仅 journal 未更新：视为提交点已过
            clean_txn()
        elif full and bak_present:
            # 提交循环完成但 committed 写入失败（bak 未清理）：
            # 以 meta 四文件 SHA 自洽判定——一致则完成提交收尾并续跑，
            # 不一致则人工裁决（codex re-review P1）
            if artifacts_self_consistent():
                clean_bak(); clean_txn()
                write_journal("committed")
            else:
                die(5, "staged with full set and backups; artifacts not "
                       "self-consistent; all files preserved; manual resolution "
                       "required before re-run")
        else:
            # 提交点前中断：还原备份（旧代完整），每步 fsync
            for n in NAMES:
                if os.path.lexists(bak[n]):
                    os.replace(bak[n], dest[n])
                    fsync_path(out_dir)
                elif os.path.lexists(dest[n]):
                    os.unlink(dest[n])
                    fsync_path(out_dir)
            clean_txn()
    except SystemExit:
        raise
    except Exception as e:
        die(5, "staged recovery failed; all files preserved; manual resolution "
               "required before re-run: %s" % e)
elif st == "committed":
    # 提交点已过：清理残留备份/事务文件——失败 = fail-closed die(5)
    # （残留保留、下次启动重试；不做静默吞）
    try:
        clean_bak(); clean_txn()
    except OSError as e:
        die(5, "committed cleanup failed; artifacts remain valid; residue "
               "preserved; re-run to retry: %s" % e)
elif st is not None:
    # 未知/损坏 journal 状态：不得当作"无 journal"继续（codex re-review P1）
    die(5, "unknown or corrupt journal state %r; all files preserved; "
           "manual resolution required before re-run" % st)
elif full:
    # 无 journal 但五件齐全：视为上代完整，正常进入事务
    pass
elif anyp:
    die(5, "mixed artifact set without journal; all files preserved; "
           "manual resolution required before re-run")

# ---- 生成五件（事务目录内；异常 → 清理 txn + die(5)，不触碰正式产物） ----
try:
    write_journal("staged")
except Exception as e:
    die(5, "initial journal write failed (formal artifacts untouched): %s" % e)
try:
    rows = list(o4.walk_rows(tree_root))
    manifest_text = o4.manifest_text(rows)
    tree_hash = hashlib.sha256(manifest_text.encode()).hexdigest()

    with open(txn["o-stage.manifest.tsv"], "w", encoding="utf-8", newline="\n") as fh:
        fh.write(manifest_text)
        fsync_fd(fh)
    man_sha = sha256_file(txn["o-stage.manifest.tsv"])
    with open(txn["o-stage.manifest.tsv.sha256"], "w", encoding="utf-8") as fh:
        fh.write("%s  %s\n" % (man_sha, "o-stage.manifest.tsv"))
        fsync_fd(fh)

    cmd = ["tar", "--sort=name", "--mtime=@%s" % EPOCH_MTIME,
           "--owner=0", "--group=0", "--numeric-owner",
           "--no-acls", "--no-xattrs", "--no-selinux",
           "--transform=s,^\\.,o-stage,", "-C", tree_root, "-cf", "-", "."]
    p1 = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    p2 = subprocess.Popen(["zstd", "-q", "-19", "-o", txn["o-stage-snapshot.tar.zst"]],
                          stdin=p1.stdout, stderr=subprocess.PIPE)
    p1.stdout.close()
    _, e2 = p2.communicate()
    rc1 = p1.wait()
    if rc1 != 0 or p2.returncode != 0:
        raise OSError("snapshot pipeline failed: tar rc=%s zstd rc=%s %s"
                      % (rc1, p2.returncode, e2.decode(errors="replace")[:300]))
    tar_sha = sha256_file(txn["o-stage-snapshot.tar.zst"])
    with open(txn["o-stage-snapshot.tar.zst.sha256"], "w", encoding="utf-8") as fh:
        fh.write("%s  %s\n" % (tar_sha, "o-stage-snapshot.tar.zst"))
        fsync_fd(fh)
    tar_side_sha = sha256_file(txn["o-stage-snapshot.tar.zst.sha256"])
    man_side_sha = sha256_file(txn["o-stage.manifest.tsv.sha256"])

    chain = json.load(open(chain_file, encoding="utf-8"))
    meta = {
        "schema_version": "1.0",
        "id": "5.0.0-i1",
        "lineage": "fantgpu",
        "version": VERSION,
        "tag": TAG,
        "source_date_epoch": int(SDE),
        "o_stage_tree_hash": tree_hash,
        "tree_hash_definition":
            "SHA-256 of o-stage.manifest.tsv canonical sorted bytes (O-4 contract)",
        "input": {
            "f0_snapshot_sha256": os.environ.get("OSTAGE_EXPECT_F0_SHA", ""),
            "f0_tree_hash": os.environ.get("OSTAGE_EXPECT_F0_TREE", ""),
        },
        "patch_chain": chain,
        "unverified": [
            {"id": "025-display",
             "note": "excluded (i4-only); runtime observation registered UNVERIFIED"},
            {"id": "patch-000",
             "note": "no-transform operational verdict; G0M GPU PLL double-init "
                     "semantic risk remains UNVERIFIED"},
        ],
        # 快照四文件各自 SHA-256（内容文件 + 两个 sidecar 自身）
        "snapshot_artifacts": {
            "o-stage-snapshot.tar.zst":
                {"sha256": tar_sha,
                 "byte_count": os.path.getsize(txn["o-stage-snapshot.tar.zst"])},
            "o-stage-snapshot.tar.zst.sha256": {"sha256": tar_side_sha},
            "o-stage.manifest.tsv": {"sha256": man_sha},
            "o-stage.manifest.tsv.sha256": {"sha256": man_side_sha},
        },
        "validation_results": {"path": VAL_RES, "schema": VAL_SCHEMA},
        "epoch_mtime": "@" + EPOCH_MTIME,
        "snapshot_tool_versions":
            {"tar_required": TAR_REQUIRED, "zstd_required": ZSTD_REQUIRED},
    }
    with open(txn["5.0.0-i1.meta.json"], "w", encoding="utf-8") as fh:
        json.dump(meta, fh, indent=2, ensure_ascii=False)
        fh.write("\n")
        fsync_fd(fh)

    inject("staged_done")
except SystemExit:
    raise
except Exception as e:
    try:
        clean_txn()
        write_journal("staged")
    except OSError as e2:
        die(5, "staging failed and txn cleanup incomplete: %s (cleanup: %s)" % (e, e2))
    die(5, "staging failed (txn cleaned; formal artifacts untouched): %s" % e)

# ---- 提交前 reconcile（fail-closed 保证，codex 初审 P1） ----
# 解包新 tar → 重新 walk_rows 逐行比对 manifest；重读两个 sidecar 校验。
try:
    import tempfile, shutil
    rtmp = tempfile.mkdtemp(prefix="ostage-reconcile-")
    try:
        r = subprocess.run(["tar", "--use-compress-program=zstd", "-xf",
                            txn["o-stage-snapshot.tar.zst"], "-C", rtmp],
                           capture_output=True, text=True)
        if r.returncode != 0:
            raise OSError("reconcile extract failed: %s" % r.stderr[:300])
        inner = os.path.join(rtmp, "o-stage")
        if not os.path.isdir(inner):
            raise OSError("reconcile: extracted tree missing o-stage/ prefix")
        new_rows = list(o4.walk_rows(inner))
        new_text = o4.manifest_text(new_rows)
        if new_text != manifest_text:
            raise OSError("reconcile: extracted tree manifest differs from "
                          "staged manifest")
        want_ts = "%s  %s\n" % (tar_sha, "o-stage-snapshot.tar.zst")
        want_ms = "%s  %s\n" % (man_sha, "o-stage.manifest.tsv")
        with open(txn["o-stage-snapshot.tar.zst.sha256"], "r", encoding="ascii") as fh:
            if fh.read() != want_ts:
                raise OSError("reconcile: tar sidecar content mismatch")
        with open(txn["o-stage.manifest.tsv.sha256"], "r", encoding="ascii") as fh:
            if fh.read() != want_ms:
                raise OSError("reconcile: manifest sidecar content mismatch")
    finally:
        shutil.rmtree(rtmp, ignore_errors=True)
    inject("reconcile_done")
except SystemExit:
    raise
except Exception as e:
    try:
        clean_txn()
        write_journal("staged")
    except OSError as e2:
        die(5, "reconcile failed and txn cleanup incomplete: %s (cleanup: %s)" % (e, e2))
    die(5, "reconcile failed (txn cleaned; formal artifacts untouched): %s" % e)

# ---- 提交点：备份旧五件 → 提交新五件（按生成顺序）→ 清理备份 ----
# 每步 rename/unlink 后目录 fsync（方案 §一 断电一致性契约）
had = {n: os.path.lexists(dest[n]) for n in NAMES}
moved = []
try:
    for n in NAMES:
        if had[n]:
            os.replace(dest[n], bak[n])
            fsync_path(out_dir)
            moved.append(n)
    inject("backup_done")
    for n in NAMES:
        os.replace(txn[n], dest[n])
        fsync_path(out_dir)
        inject("commit_" + n)
except SystemExit:
    raise
except Exception as e:
    # 回滚（O-4 口径：回滚失败不吞——保留现场并保持 rolling_back 供人工裁决）
    try:
        write_journal("rolling_back")
    except OSError as je:
        die(5, "transaction failed and journal write failed; all files preserved "
               "(original error: %s; journal: %s)" % (e, je))
    rollback_ok = True
    try:
        inject("rollback_fail")
        for n in NAMES:
            if os.path.lexists(bak[n]):
                os.replace(bak[n], dest[n])
                fsync_path(out_dir)
            elif not had[n] and os.path.lexists(dest[n]):
                os.unlink(dest[n])
                fsync_path(out_dir)
        for n in NAMES:
            if os.path.lexists(txn[n]):
                os.unlink(txn[n])
                fsync_path(out_dir)
    except OSError:
        rollback_ok = False
    if not rollback_ok:
        die(5, "transaction failed and rollback incomplete; journal=rolling_back; "
               "all files preserved; manual resolution required before re-run "
               "(original error: %s)" % e)
    try:
        write_journal("staged")
    except OSError as je:
        die(5, "rollback done but journal update failed; all files preserved "
               "(original error: %s; journal: %s)" % (e, je))
    die(5, "artifact transaction failed (rolled back; old set preserved): %s" % e)

try:
    write_journal("committed")
    fsync_path(out_dir)
except Exception as e:
    # 五件已提交（提交点已过）；journal/fsync 失败仅影响恢复状态，
    # 下次启动按 staged+full 恢复；统一事务错误码 5
    die(5, "post-commit journal/fsync failed (artifacts committed; "
           "recovery will reconcile on next run): %s" % e)
# 提交点已过：清理失败仅留残留，不得回滚
for n in moved:
    try:
        inject("cleanup_bak")
        if os.path.lexists(bak[n]):
            os.unlink(bak[n])
            fsync_path(out_dir)
    except OSError:
        pass
print("artifacts: 5-file set committed; tree_hash=%s; tar sha256=%s" % (tree_hash, tar_sha))
PY
}

# ================= main =================
check_out_dir
require_tools

# 输入校验：f0 快照 + sidecar + SHA
[[ -f "$SNAP_IN" ]] || die "missing F0 snapshot: $SNAP_IN"
[[ -f "$SNAP_SIDE_IN" ]] || die "missing F0 snapshot sidecar: $SNAP_SIDE_IN"
f0_sha="$(sha256_of "$SNAP_IN")"
side_sha="$(awk '{print $1}' "$SNAP_SIDE_IN")"
if [[ "$f0_sha" != "$side_sha" || "$f0_sha" != "$EXPECT_F0_SHA" ]]; then
    die "F0 snapshot SHA mismatch: actual=$f0_sha sidecar=$side_sha expect=$EXPECT_F0_SHA"
fi

# 13 条 patch + SHA 校验（与 meta.json apply.patch_sha256 一致）
for entry in "${CHAIN[@]}"; do
    id="${entry%%|*}"
    pf="${entry#*|}"
    meta="$PATCH_DIR/${id}.meta.json"
    patch="$PATCH_DIR/$(basename "$pf")"
    [[ -f "$meta" ]] || die "missing meta: $meta"
    [[ -f "$patch" ]] || die "missing patch: $patch"
    want="$(meta_field "$meta" "apply.patch_sha256")"
    got="$(sha256_of "$patch")"
    if [[ "$got" != "$want" ]]; then
        die "patch SHA mismatch for $id: actual=$got expect=$want"
    fi
done

# 排他锁（任何输出变更前取得；out_dir 必须已存在——materialize 不创建它，
# 锁文件 .materialize.lock 永不删除，同 d-stage-audit 先例）
[[ -d "$OUT_DIR" ]] || die "out-dir missing (create it before running): $OUT_DIR"
exec 9>"$LOCK_FILE"
if ! flock -n 9; then
    echo "materialize=FAIL: another materialize is running (lock held)" >&2
    exit 6
fi

# 解包 F0 → 工作树（全新重建）
rm -rf -- "$WORK"
mkdir -p -- "$WORK"
tar --use-compress-program=zstd -xf "$SNAP_IN" -C "$WORK"
TREE="$WORK/f0"
[[ -d "$TREE" ]] || die "extracted tree missing f0/ prefix"

got_tree="$(tree_hash "$TREE")"
if [[ "$got_tree" != "$EXPECT_F0_TREE" ]]; then
    die "F0 tree hash mismatch: actual=$got_tree expect=$EXPECT_F0_TREE"
fi

# 13 条链序应用 + 每链点 after_tree_hash 校验
chain_json="$(mktemp "${TMPDIR:-/tmp}/ostage-chain.XXXXXX.json")"
python3 - "$chain_json" <<'PY'
import json, sys
json.dump([], open(sys.argv[1], "w", encoding="utf-8"))
PY
for entry in "${CHAIN[@]}"; do
    id="${entry%%|*}"
    pf="${entry#*|}"
    meta="$PATCH_DIR/${id}.meta.json"
    patch="$PATCH_DIR/$(basename "$pf")"
    want_before="$(meta_field "$meta" "apply.before_tree_hash")"
    got_before="$(tree_hash "$TREE")"
    if [[ "$got_before" != "$want_before" ]]; then
        die "chain base mismatch before $id: actual=$got_before expect=$want_before"
    fi
    inject_fail "pre_apply_$id"
    if ! patch --batch --forward --fuzz=0 --no-backup-if-mismatch -p1 -s -d "$TREE" < "$patch"; then
        die "patch apply failed: $id"
    fi
    inject_fail "post_apply_$id"
    artifact="$(find "$TREE" -type f \( -name '*.orig' -o -name '*.rej' \) -print -quit)"
    [[ -z "$artifact" ]] || die "patch artifact in tree after $id: ${artifact#"$TREE"/}"
    want_after="$(meta_field "$meta" "apply.after_tree_hash")"
    got_after="$(tree_hash "$TREE")"
    if [[ "$got_after" != "$want_after" ]]; then
        die "after_tree_hash mismatch for $id: actual=$got_after expect=$want_after"
    fi
    python3 - "$chain_json" "$id" "$pf" "$(sha256_of "$patch")" "$got_before" "$got_after" <<'PY'
import json, sys
path, pid, pfile, psha, before, after = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5], sys.argv[6]
lst = json.load(open(path, encoding="utf-8"))
lst.append({"id": pid, "patch": pfile, "patch_sha256": psha,
            "before_tree_hash": before, "after_tree_hash": after})
json.dump(lst, open(path, "w", encoding="utf-8"), indent=2, ensure_ascii=False)
PY
done

# 最终树 hash（= 链尾 meta after + 文档锁定值）
got_final="$(tree_hash "$TREE")"
last_after="$(meta_field "$PATCH_DIR/030-029.meta.json" "apply.after_tree_hash")"
if [[ "$got_final" != "$last_after" || "$got_final" != "$EXPECT_FINAL_TREE" ]]; then
    die "final tree hash mismatch: actual=$got_final last_after=$last_after expect=$EXPECT_FINAL_TREE"
fi

# 五件同代事务提交
OSTAGE_EXPECT_F0_SHA="$EXPECT_F0_SHA" \
OSTAGE_EXPECT_F0_TREE="$EXPECT_F0_TREE" \
commit_artifacts "$TREE" "$chain_json"

rm -f -- "$chain_json"
flock -u 9 2>/dev/null || true

echo "materialize=OK"
echo "o_stage_tree_hash=$got_final"
echo "artifacts=$(for n in "$P_TAR" "$P_TAR_SHA" "$P_MANIFEST" "$P_MANIFEST_SHA" "$P_META"; do printf '%s,' "$OUT_DIR/$n"; done)"
echo "version=$VERSION"
echo "tag=$TAG"
echo "source_date_epoch=$SOURCE_DATE_EPOCH"
echo "validation_results=$VALIDATION_RESULTS (filled by validation stage; not generated here)"
exit 0
