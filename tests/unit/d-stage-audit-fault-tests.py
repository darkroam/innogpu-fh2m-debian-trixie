#!/usr/bin/env python3
# tests/unit/d-stage-audit-fault-tests.py — O-2 snapshot 故障注入测试 harness（29 场景）
#
# 覆盖 audit §五 v24 契约 + design §5.3 v24 故障注入表 + genesis fault_injection_tests
# （schema 13.0）的 29 场景（17 单元 + 12 power-loss）：
#   - 17 个单元场景：锁争用 / kill -9 崩溃时机等价态构造 / 状态机手造故障，
#     本机可直接运行（kill -9 场景以"崩溃时刻的持久状态等价构造"注入，
#     注入方式如实记录）；
#   - 12 个 power-loss 场景：注入方式 = 强制断电（VM poweroff / 拔盘后重启），
#     **不得用 kill -9 代替**（kill -9 不丢 page cache）。真实断电必然终止
#     本进程，任何单进程自报结果都无法独立验证（codex 八轮复审 P1）→
#     由 VM 驱动六模式执行，注入点 = 工具内接缝 + 阻塞握手（codex 十轮
#     复审 P1：INNOGPU_DSTAGE_INJECT_DIR/TAG 使 snapshot 在注入点写 at-<tag>
#     marker 并阻塞，驱动在阻塞处断电，窗口确定性成立），验证全部由
#     harness 重启后独立完成：
#       --vm-prepare <root>         每场景隔离不可变输入 + pristine 旧对 +
#                                   注入接缝目录 + 只读 spec（chmod 444）
#       --vm-mark <spec> [--sw X]   每场景/子窗口注入前记录当前 boot_id 进
#                                   runtime 文件（断电证据与场景绑定）
#       --vm-setup-injection <spec> 构造注入前置状态（rolling_back 等价态；
#                                   其余重置为 pristine 旧对），幂等
#       --vm-inject <spec> [--sw X] 以注入环境启动 snapshot，等待 at-<tag>
#                                   marker → 工具阻塞在注入点 → 驱动断电
#       --vm-verify <spec> [--sw X] 重启后：marker 证据 + boot_id + 输入
#                                   不可变 + 恢复前注入窗口验证 + harness
#                                   亲自重跑 snapshot + rc/终态分支判定
#       --vm-summarize <root>       全部窗口独立复核（只读 spec 信任根 +
#                                   runtime 交叉绑定 + marker + boot 链 +
#                                   rc + 重跑终态验证器）→ fail-closed
#     未执行六模式时 12 场景如实 UNVERIFIED（不冒充通过）。
#
# 每个场景注入后重启（重新运行 snapshot）并核对期望最终状态；结果回写
# <out-dir>/d-stage-audit.genesis.json 的 fault_injection_tests.snapshot_subcommand
# （actual_final_state / passed）。未经全部场景通过的 snapshot 禁止在阶段二
# release commit 中使用。
#
# 用法：python3 tests/unit/d-stage-audit-fault-tests.py [--out-dir <evidence-dir>] \
#         [--keep-tmp]
# 三模式 VM 驱动：--vm-prepare ROOT | --vm-verify SPEC | --vm-summarize ROOT
# 测试根目录必须位于本地崩溃一致性文件系统（ext2/ext3/ext4/xfs/btrfs，
# 与 snapshot 的 check_out_dir_fs 白名单一致）；harness 按
# INNOGPU_DSTAGE_TEST_ROOT → TMPDIR → /var/tmp → /tmp 顺序探测，全部不符 →
# exit 2（环境前置硬性检查，codex 八轮复审 P1）。
# 退出码：0 = 全部单元场景 PASS（power-loss 场景如实 UNVERIFIED，不冒充通过）；
#         1 = 任一单元场景 FAIL；2 = 参数/环境错误。

import argparse
import fcntl
import importlib.util
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import threading
import time

_REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
_TOOL_PATH = os.path.join(_REPO_ROOT, "tools", "d-stage-audit-gen.py")
_spec = importlib.util.spec_from_file_location("d_stage_audit_gen", _TOOL_PATH)
tool = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(tool)

TAR = tool.TAR_BASENAME
SHA = tool.SHA_BASENAME

POWER_LOSS_SCENARIOS = [
    "power_loss_after_staged_persist",
    "power_loss_before_staged_persist",
    "power_loss_after_backed_up_persist",
    "power_loss_after_tarball_mv",
    "power_loss_after_verified_before_cleanup",
    "power_loss_after_backup_mv_before_persist",
    "power_loss_backup_mv_fsync_window",
    "power_loss_after_restore_mv_before_cleanup",
    "power_loss_rollback_mid_delete_window",
    "power_loss_rollback_mid_restore_window",
    "power_loss_rollback_restore_fsync_window",
    "power_loss_verified_cleanup_after_journal_rm",
]


def make_fixture_tree(root, files):
    for rel, content in files.items():
        p = os.path.join(root, rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "wb") as f:
            f.write(content)


def run_snapshot(d_stage_root, out_dir, ref=None, expect_rc=None):
    argv = ["python3", _TOOL_PATH, "snapshot",
            "--d-stage-root", d_stage_root, "--out-dir", out_dir]
    if ref:
        argv += ["--reference-manifest", ref]
    p = subprocess.run(argv, capture_output=True, text=True,
                       env=dict(os.environ, LC_ALL="C"))
    if expect_rc is not None and p.returncode != expect_rc:
        raise AssertionError("snapshot rc=%d (expected %d) stderr=%s"
                             % (p.returncode, expect_rc, p.stderr[-400:]))
    return p


class Scenario:
    def __init__(self, name, injection, expected):
        self.name = name
        self.injection = injection
        self.expected = expected
        self.actual = ""
        self.passed = None


def mk_journal(txn, **kw):
    base = {"schema": "1", "staging_dir": os.path.join(txn, "staging"),
            "new_tar_sha256": "0" * 64, "new_tar_size": "0",
            "old_tar": "absent", "old_sha": "absent",
            "old_tar_sha256": "none", "old_sha_sha256": "none"}
    base.update(kw)
    return base


def stat_fstype(path):
    try:
        out = subprocess.run(["stat", "-f", "-c", "%T", path],
                             capture_output=True, text=True,
                             env=dict(os.environ, LC_ALL="C"), timeout=30)
    except (OSError, subprocess.TimeoutExpired):
        return ""
    if out.returncode != 0:
        return ""
    return out.stdout.strip()


def pick_test_root():
    # codex 八轮复审 P1：默认临时目录可能是 tmpfs，与 snapshot 的
    # check_out_dir_fs 白名单冲突 → 探测位于允许文件系统上的测试根目录，
    # 全部不符返回 (None, None)（调用方 exit 2 环境前置硬性检查）
    cands = []
    for env in ("INNOGPU_DSTAGE_TEST_ROOT", "TMPDIR"):
        v = os.environ.get(env)
        if v:
            cands.append(v)
    cands += ["/var/tmp", "/tmp"]
    for d in cands:
        if not os.path.isdir(d):
            continue
        fs = stat_fstype(d)
        if tool.classify_fs(fs):
            return d, fs
    return None, None


def read_boot_id():
    # codex 八轮复审 P1：断电重启证据——真实断电必然改变内核 boot_id
    try:
        with open("/proc/sys/kernel/random/boot_id") as f:
            return f.read().strip()
    except OSError:
        return None


# ---- power-loss 场景：注入协议 + harness 侧权威验证（codex 九轮复审）----
# 无任何自报结果参与判定。每场景绑定：
#   pre_check   恢复前状态验证＝注入点执行证据（断电只能落在该场景注入窗口）
#   trigger     --vm-wait-trigger 轮询的注入点触发条件（机器可用，驱动无需读 genesis）
#   allowed_rcs 恢复 snapshot 允许退出码集合（0 正常提交 / 1 回滚 / 9 fail-closed）
#   terminal    终态验证器（返回 (ok, branch, desc)；branch ∈ new_pair/old_pair/evidence）
# 期望新对字节由 harness 用 build_staging（确定性 tar.zst）独立计算，驱动无法伪造。

JOURNAL_STATES = ("staged", "backed_up", "tarball_committed", "sha_committed",
                  "verified", "rolling_back")


def parse_journal_lenient(path):
    # 与 tool.parse_journal 全量同构但失败返回 None（harness 进程内不得 exit）：
    # schema/state/必填字段/64hex/数字 + old_tar/old_sha ∈ {present,absent}
    # + present→64hex、absent→none 语义绑定（codex 十轮复审 P2）
    if os.path.islink(path) or not os.path.isfile(path):
        return None
    try:
        with open(path) as f:
            lines = f.read().split("\n")
    except OSError:
        return None
    fields = {}
    for line in lines:
        if not line:
            continue
        if "=" not in line:
            return None
        k, v = line.split("=", 1)
        fields[k] = v
    if fields.get("schema") != "1":
        return None
    if fields.get("state") not in JOURNAL_STATES:
        return None
    for k in ("staging_dir", "new_tar_sha256", "new_tar_size"):
        if not fields.get(k):
            return None
    if not re.fullmatch(r"[0-9a-f]{64}", fields["new_tar_sha256"]):
        return None
    if not re.fullmatch(r"[0-9]+", fields["new_tar_size"]):
        return None
    for k in ("old_tar", "old_sha"):
        if fields.get(k) not in ("present", "absent"):
            return None
    for plan_k, sha_k in (("old_tar", "old_tar_sha256"),
                          ("old_sha", "old_sha_sha256")):
        v = fields.get(sha_k)
        if v is None:
            return None
        if fields[plan_k] == "present":
            if not re.fullmatch(r"[0-9a-f]{64}", v):
                return None
        else:
            if v != "none":
                return None
    return fields


def staging_dir_ok(staging_dir, txn_dir):
    # 与 tool.validate_staging_dir 同构但失败返回 False：绝对路径 + 词法组件
    # 遍历（拒绝 ..、逐组件 symlink）+ realpath -m 内嵌检查 + 非 symlink 目录
    if not os.path.isabs(staging_dir):
        return False
    cur = "/"
    for part in staging_dir.split("/"):
        if part in ("", "."):
            continue
        if part == "..":
            return False
        cur = ("/" + part) if cur == "/" else cur + "/" + part
        if os.path.islink(cur):
            return False
    can = tool.realpath_m(staging_dir)
    root = tool.realpath_m(os.path.join(txn_dir, "staging"))
    if not tool.is_within(can, root):
        return False
    if os.path.islink(staging_dir):
        return False
    return os.path.isdir(staging_dir)


def staging_fingerprint_lenient(staging_dir):
    tar_path = os.path.join(staging_dir, TAR)
    sha_path = os.path.join(staging_dir, SHA)
    for p in (tar_path, sha_path):
        if os.path.islink(p) or not os.path.isfile(p):
            return None
    new_sha = tool.sha256_of(tar_path)
    # journal 的 new_tar_size 是字符串——必须统一为 str 再与 journal 比较
    # （codex 十轮复审 P1）
    new_size = str(os.path.getsize(tar_path))
    if not tool.sidecar_is_strict(sha_path, new_sha):
        return None
    return new_sha, new_size


def _out_pair(out_dir, spec):
    return tool.classify_out_pair(out_dir, spec["new_tar_sha256"],
                                  spec["old_tar_sha256"], spec["old_sha_sha256"])


def _txn_clean(out_dir):
    txn = os.path.join(out_dir, ".txn")
    return not os.path.lexists(txn) and not os.path.lexists(txn + ".tombstone")


def _txn_real_dir(out_dir):
    txn = os.path.join(out_dir, ".txn")
    return os.path.islink(txn) is False and os.path.isdir(txn)


def _regular_in_txn(txn_dir, name):
    p = os.path.join(txn_dir, name)
    return os.path.islink(p) is False and os.path.isfile(p)


def _no_old_in_txn(txn_dir):
    return not os.path.lexists(os.path.join(txn_dir, "old.tar.zst")) and \
        not os.path.lexists(os.path.join(txn_dir, "old.sha256"))


def _journal_check(out_dir, expected_state, staging_required):
    # 严格证据校验：.txn 必须真实目录（拒绝 symlink）；journal 必须 regular
    # file（拒绝 symlink/FIFO）；journal schema/state/字段全量解析；staged 态
    # 还需 staging 路径约束 + 指纹 == journal（evidence_preserved 强校验，
    # codex 九轮复审 P2）
    if not _txn_real_dir(out_dir):
        return False, "txn not a real dir"
    txn = os.path.join(out_dir, ".txn")
    j = parse_journal_lenient(os.path.join(txn, "journal"))
    if j is None:
        return False, "journal missing/invalid (non-regular file or bad schema)"
    if j["state"] != expected_state:
        return False, "journal state=%s != expected %s" % (j["state"],
                                                           expected_state)
    if staging_required:
        if not staging_dir_ok(j["staging_dir"], txn):
            return False, "staging_dir constraint failed"
        fp = staging_fingerprint_lenient(j["staging_dir"])
        if fp is None or fp != (j["new_tar_sha256"], j["new_tar_size"]):
            return False, "staging fingerprint mismatch vs journal"
    return True, "journal=%s staging_ok=%s" % (j["state"], staging_required)


# ---- 终态验证器（返回 (ok, branch, desc)；cfg = 场景/子窗口配置；rc =
# 恢复 snapshot 实际退出码，evidence 分支必须与 rc=9 事实核验绑定）----

def term_new_pair(out_dir, spec, cfg, rc):
    ts, ss = _out_pair(out_dir, spec)
    clean = _txn_clean(out_dir)
    ok = (ts, ss) == ("new", "new") and clean
    return ok, "new_pair", "pair=(%s,%s) txn_clean=%s" % (ts, ss, clean)


def term_old_pair(out_dir, spec, cfg, rc):
    ts, ss = _out_pair(out_dir, spec)
    clean = _txn_clean(out_dir)
    ok = (ts, ss) == ("old", "old") and clean
    return ok, "old_pair", "pair=(%s,%s) txn_clean=%s" % (ts, ss, clean)


def term_backup_fsync(out_dir, spec, cfg, rc):
    # backup_mv_fsync_window：rc=0 → 新对；rc=9 → journal=staged + staging
    # 指纹 + 旧 tar 真实双存在/均缺失（codex 十二轮复审 P1）
    if rc == 0:
        return term_new_pair(out_dir, spec, cfg, rc)
    ok, d = _journal_check(out_dir, "staged", True)
    if not ok:
        return False, "evidence", d
    facts_ok, fdesc = _evidence_facts_fail(out_dir, spec, cfg["evidence_file"])
    return facts_ok, "evidence", d + " | " + fdesc


def term_restore_fsync(out_dir, spec, cfg, rc):
    # rollback_restore_fsync_window（tar/sha 双窗口）：rc=1 → 旧对收敛；
    # rc=9 → journal=rolling_back + 对应旧文件真实双存在/均缺失
    # （codex 十二轮复审 P1）
    if rc == 1:
        return term_old_pair(out_dir, spec, cfg, rc)
    ok, d = _journal_check(out_dir, "rolling_back", False)
    if not ok:
        return False, "evidence", d
    facts_ok, fdesc = _evidence_facts_fail(out_dir, spec, cfg["evidence_file"])
    return facts_ok, "evidence", d + " | " + fdesc


# ---- 恢复前状态验证（注入点执行证据；断电必须落在该场景注入窗口）----

def pre_staged(out_dir, spec):
    ok, d = _journal_check(out_dir, "staged", True)
    if not ok:
        return False, d
    ts, ss = _out_pair(out_dir, spec)
    if ts not in ("old", "absent") or ss not in ("old", "absent"):
        return False, "pair=(%s,%s) not a legal staged-phase split" % (ts, ss)
    return True, d + " pair=(%s,%s)" % (ts, ss)


def pre_before_staged(out_dir, spec):
    txn = os.path.join(out_dir, ".txn")
    if os.path.lexists(os.path.join(txn, "journal")):
        return False, "journal unexpectedly present"
    if not _txn_real_dir(out_dir):
        return False, "txn residue not a real dir"
    ts, ss = _out_pair(out_dir, spec)
    if (ts, ss) != ("old", "old"):
        return False, "old pair not untouched: pair=(%s,%s)" % (ts, ss)
    return True, "journal absent + txn residue + old pair intact"


def pre_backed_up(out_dir, spec):
    ok, d = _journal_check(out_dir, "backed_up", True)
    if not ok:
        return False, d
    txn = os.path.join(out_dir, ".txn")
    if not (_regular_in_txn(txn, "old.tar.zst")
            and _regular_in_txn(txn, "old.sha256")):
        return False, "old.* not both regular files in .txn"
    ts, ss = _out_pair(out_dir, spec)
    if (ts, ss) != ("absent", "absent"):
        return False, "pair=(%s,%s) != (absent,absent)" % (ts, ss)
    return True, d + " old.* in .txn + pair=(absent,absent)"


def pre_tarball_mv(out_dir, spec):
    ok, d = _journal_check(out_dir, "backed_up", False)
    if not ok:
        return False, d
    ts, ss = _out_pair(out_dir, spec)
    if ts != "new" or ss != "absent":
        return False, "pair=(%s,%s) != (new,absent)" % (ts, ss)
    txn = os.path.join(out_dir, ".txn")
    j = parse_journal_lenient(os.path.join(txn, "journal"))
    staging = j["staging_dir"]
    if not staging_dir_ok(staging, txn):
        return False, "staging_dir constraint failed"
    if os.path.lexists(os.path.join(staging, TAR)):
        return False, "staging tar not yet moved out"
    if not tool.sidecar_is_strict(os.path.join(staging, SHA),
                                  spec["new_tar_sha256"]):
        return False, "staging sidecar not strict vs new sha"
    return True, d + " OUT tar=new + staging sidecar strict"


def pre_verified(out_dir, spec):
    ok, d = _journal_check(out_dir, "verified", False)
    if not ok:
        return False, d
    ts, ss = _out_pair(out_dir, spec)
    if (ts, ss) != ("new", "new"):
        return False, "pair=(%s,%s) != (new,new)" % (ts, ss)
    return True, d + " pair=(new,new)"


def pre_backup_mv_before_persist(out_dir, spec):
    ok, d = _journal_check(out_dir, "staged", True)
    if not ok:
        return False, d
    txn = os.path.join(out_dir, ".txn")
    if not (_regular_in_txn(txn, "old.tar.zst")
            and _regular_in_txn(txn, "old.sha256")):
        return False, "old.* not both regular files in .txn"
    ts, ss = _out_pair(out_dir, spec)
    if (ts, ss) != ("absent", "absent"):
        return False, "pair=(%s,%s) != (absent,absent)" % (ts, ss)
    return True, d + " old.* in .txn + pair=(absent,absent)"


def pre_backup_mv_fsync_window(out_dir, spec):
    ok, d = _journal_check(out_dir, "staged", True)
    if not ok:
        return False, d
    ts, ss = _out_pair(out_dir, spec)
    txn = os.path.join(out_dir, ".txn")
    backup_started = not ((ts, ss) == ("old", "old")
                          and _no_old_in_txn(txn))
    if not backup_started:
        return False, "backup mv not started (cut too early)"
    return True, d + " pair=(%s,%s) backup_started" % (ts, ss)


def pre_restore_before_cleanup(out_dir, spec):
    ok, d = _journal_check(out_dir, "rolling_back", False)
    if not ok:
        return False, d
    txn = os.path.join(out_dir, ".txn")
    ts, ss = _out_pair(out_dir, spec)
    if (ts, ss) != ("old", "old") or not _no_old_in_txn(txn):
        return False, "pair=(%s,%s) != (old,old) or old.* still in .txn" % (ts, ss)
    return True, d + " pair=(old,old) + no old.* in .txn"


def pre_tombstone_rmtree(out_dir, spec):
    # tombstone 窗口 b：rename .txn→.txn.tombstone + fsync_dir(OUT_DIR) 已完成、
    # rmtree 尚未开始 → .txn 必缺席、tombstone 必为持久真实目录
    txn = os.path.join(out_dir, ".txn")
    tomb = txn + ".tombstone"
    if os.path.lexists(txn):
        return False, ".txn unexpectedly present"
    if os.path.islink(tomb) or not os.path.isdir(tomb):
        return False, ".txn.tombstone not a durable real dir"
    ts, ss = _out_pair(out_dir, spec)
    if (ts, ss) != ("old", "old"):
        return False, "pair=(%s,%s) != (old,old)" % (ts, ss)
    return True, "txn absent + tombstone real dir + pair=(old,old)"


def pre_tombstone_final_fsync(out_dir, spec):
    # tombstone 窗口 c：rmtree 完成、最终 fsync_dir(OUT_DIR) 前 → .txn 必缺席，
    # tombstone 删除是否持久不确定（rmtree 后未 fsync 目录）→ 存在或缺失均可
    txn = os.path.join(out_dir, ".txn")
    tomb = txn + ".tombstone"
    if os.path.lexists(txn):
        return False, ".txn unexpectedly present"
    if os.path.islink(tomb):
        return False, ".txn.tombstone is a symlink"
    ts, ss = _out_pair(out_dir, spec)
    if (ts, ss) != ("old", "old"):
        return False, "pair=(%s,%s) != (old,old)" % (ts, ss)
    return True, "txn absent + tombstone present-or-absent + pair=(old,old)"


def pre_mid_delete(out_dir, spec):
    ok, d = _journal_check(out_dir, "rolling_back", False)
    if not ok:
        return False, d
    ts, ss = _out_pair(out_dir, spec)
    if (ts, ss) != ("absent", "new"):
        return False, "pair=(%s,%s) != (absent,new)" % (ts, ss)
    return True, d + " pair=(absent,new)"


def pre_mid_restore(out_dir, spec):
    ok, d = _journal_check(out_dir, "rolling_back", False)
    if not ok:
        return False, d
    ts, ss = _out_pair(out_dir, spec)
    if (ts, ss) != ("old", "absent"):
        return False, "pair=(%s,%s) != (old,absent)" % (ts, ss)
    return True, d + " pair=(old,absent)"


def pre_restore_fsync_tar_window(out_dir, spec):
    # 旧 tar 还原 mv 后、其双目录 fsync 前的四态矩阵分支 1-2（codex 十一轮
    # 复审 P1 tar/sha 分窗）：target-only → (old,absent)；source-only →
    # (absent,absent) 且旧 tar 仍在 .txn
    ok, d = _journal_check(out_dir, "rolling_back", False)
    if not ok:
        return False, d
    ts, ss = _out_pair(out_dir, spec)
    if (ts, ss) not in (("old", "absent"), ("absent", "absent")):
        return False, "pair=(%s,%s) not in tar-window matrix" % (ts, ss)
    return True, d + " pair=(%s,%s) tar-window" % (ts, ss)


def pre_restore_fsync_sha_window(out_dir, spec):
    # 旧 sidecar 还原 mv 后、其双目录 fsync 前的四态矩阵分支 3-4（旧 tar
    # 已持久还原 → OUT tar=old）：target-only → (old,old)；source-only →
    # (old,absent) 且旧 sidecar 仍在 .txn
    ok, d = _journal_check(out_dir, "rolling_back", False)
    if not ok:
        return False, d
    ts, ss = _out_pair(out_dir, spec)
    if (ts, ss) not in (("old", "old"), ("old", "absent")):
        return False, "pair=(%s,%s) not in sha-window matrix" % (ts, ss)
    return True, d + " pair=(%s,%s) sha-window" % (ts, ss)


def pre_cleanup_after_journal_rm(out_dir, spec):
    txn = os.path.join(out_dir, ".txn")
    if os.path.lexists(os.path.join(txn, "journal")):
        return False, "journal unexpectedly present"
    if not _txn_real_dir(out_dir):
        return False, "txn residue not a real dir"
    if not _no_old_in_txn(txn):
        return False, "old.* still in .txn"
    ts, ss = _out_pair(out_dir, spec)
    if (ts, ss) != ("new", "new"):
        return False, "pair=(%s,%s) != (new,new)" % (ts, ss)
    return True, "journal absent + txn without old.* + pair=(new,new)"


# ---- 注入协议（codex 十轮复审 P1）：工具内接缝 + 阻塞握手 ----
# 每个注入点 = tools/d-stage-audit-gen.py 内的 inject_pause(tag) 接缝：
# 写入并 fsync at-<tag> marker 后阻塞（INNOGPU_DSTAGE_INJECT_DIR/TAG 匹配时
# 生效）。--vm-inject 启动注入 snapshot 并等待 marker → 驱动在工具阻塞于
# 接缝处断电（窗口确定性成立；无轮询竞态）。marker 断电后仍持久，是注入点
# 执行证据。rolling_back 场景由 --vm-setup-injection 构造等价前置状态。

VM_SCENARIOS = {
    "power_loss_after_staged_persist": {
        "inject_tag": "staged_persisted", "setup": None,
        "pre_check": pre_staged,
        "allowed_rcs": {0}, "terminal": term_new_pair,
        "evidence_journal_state": None, "staging_required": False},
    "power_loss_before_staged_persist": {
        "inject_tag": "staging_built", "setup": None,
        "pre_check": pre_before_staged,
        "allowed_rcs": {0}, "terminal": term_new_pair,
        "evidence_journal_state": None, "staging_required": False},
    "power_loss_after_backed_up_persist": {
        "inject_tag": "backed_up_persisted", "setup": None,
        "pre_check": pre_backed_up,
        "allowed_rcs": {0}, "terminal": term_new_pair,
        "evidence_journal_state": None, "staging_required": False},
    "power_loss_after_tarball_mv": {
        "inject_tag": "tarball_mv_committed", "setup": None,
        "pre_check": pre_tarball_mv,
        "allowed_rcs": {0}, "terminal": term_new_pair,
        "evidence_journal_state": None, "staging_required": False},
    "power_loss_after_verified_before_cleanup": {
        "inject_tag": "verified_persisted", "setup": None,
        "pre_check": pre_verified,
        "allowed_rcs": {0}, "terminal": term_new_pair,
        "evidence_journal_state": None, "staging_required": False},
    "power_loss_after_backup_mv_before_persist": {
        "inject_tag": "backup_mv_complete", "setup": None,
        "pre_check": pre_backup_mv_before_persist,
        "allowed_rcs": {0}, "terminal": term_new_pair,
        "evidence_journal_state": None, "staging_required": False},
    "power_loss_backup_mv_fsync_window": {
        "inject_tag": "backup_tar_fsync_window", "setup": None,
        "pre_check": pre_backup_mv_fsync_window,
        "allowed_rcs": {0, 9}, "terminal": term_backup_fsync,
        "evidence_file": "tar",
        "evidence_journal_state": "staged", "staging_required": True},
    # tombstone 三窗口（codex 十轮复审 P1：原单一场景拆为 a/b/c 子窗口，
    # 各自注入接缝 + 恢复前状态验证 + 退出码/终态契约）
    "power_loss_after_restore_mv_before_cleanup": {
        "setup": "rolling_back",
        "subwindows": {
            "a": {"inject_tag": "rollback_restore_complete",
                  "pre_check": pre_restore_before_cleanup,
                  "allowed_rcs": {1}, "terminal": term_old_pair},
            "b": {"inject_tag": "rollback_tombstone_rmtree",
                  "pre_check": pre_tombstone_rmtree,
                  "allowed_rcs": {0}, "terminal": term_new_pair},
            "c": {"inject_tag": "rollback_tombstone_final_fsync",
                  "pre_check": pre_tombstone_final_fsync,
                  "allowed_rcs": {0}, "terminal": term_new_pair},
        },
        "evidence_journal_state": None, "staging_required": False},
    "power_loss_rollback_mid_delete_window": {
        "inject_tag": "rollback_deleted_tar", "setup": "rolling_back",
        "pre_check": pre_mid_delete,
        "allowed_rcs": {1}, "terminal": term_old_pair,
        "evidence_journal_state": None, "staging_required": False},
    "power_loss_rollback_mid_restore_window": {
        "inject_tag": "rollback_restored_tar", "setup": "rolling_back",
        "pre_check": pre_mid_restore,
        "allowed_rcs": {1}, "terminal": term_old_pair,
        "evidence_journal_state": None, "staging_required": False},
    # tar/sha 双窗口（codex 十一轮复审 P1：四态矩阵分支 1-2 与 3-4 分别
    # 覆盖——tar 窗口 setup=rolling_back（(new,new) 起步）；sha 窗口
    # setup=rolling_back_sha_restored（旧 tar 已还原，(old,absent) 起步）
    "power_loss_rollback_restore_fsync_window": {
        "subwindows": {
            "tar": {"inject_tag": "rollback_restore_tar_mv",
                    "setup": "rolling_back",
                    "pre_check": pre_restore_fsync_tar_window,
                    "allowed_rcs": {1, 9},
                    "terminal": term_restore_fsync,
                    "evidence_file": "tar"},
            "sha": {"inject_tag": "rollback_restore_sha_mv",
                    "setup": "rolling_back_sha_restored",
                    "pre_check": pre_restore_fsync_sha_window,
                    "allowed_rcs": {1, 9},
                    "terminal": term_restore_fsync,
                    "evidence_file": "sha"},
        },
        "evidence_journal_state": "rolling_back", "staging_required": False},
    "power_loss_verified_cleanup_after_journal_rm": {
        "inject_tag": "cleanup_journal_rm", "setup": None,
        "pre_check": pre_cleanup_after_journal_rm,
        "allowed_rcs": {0}, "terminal": term_new_pair,
        "evidence_journal_state": None, "staging_required": False},
}

BRANCH_FOR_RC = {0: "new_pair", 1: "old_pair", 9: "evidence"}


def vm_subwindows(name):
    cfg = VM_SCENARIOS[name]
    if cfg.get("subwindows"):
        return sorted(cfg["subwindows"])
    return [None]


def vm_subwindow_cfg(name, sw):
    # 子窗口配置 = 顶层 cfg + 子窗口覆盖（顶层 evidence_journal_state /
    # staging_required / setup 等由子窗口继承）
    cfg = VM_SCENARIOS[name]
    if cfg.get("subwindows"):
        merged = dict(cfg)
        merged.update(cfg["subwindows"][sw])
        return merged
    return cfg


def run(keep_tmp=False):
    base_root, base_fstype = pick_test_root()
    if base_root is None:
        print("ENV-ERROR: no local crash-consistent filesystem for test root "
              "(probed INNOGPU_DSTAGE_TEST_ROOT, TMPDIR, /var/tmp, /tmp; "
              "require ext2/ext3/ext4/xfs/btrfs) — set "
              "INNOGPU_DSTAGE_TEST_ROOT to a directory on an allowed filesystem")
        sys.exit(2)
    base = tempfile.mkdtemp(prefix="r16-fault-", dir=base_root)
    results = []
    try:
        d_stage = os.path.join(base, "dstage")
        make_fixture_tree(d_stage, {"a.txt": b"alpha\n", "sub/b.bin": b"\x00\x01\x02"})
        os.symlink("a.txt", os.path.join(d_stage, "link_ok"))
        ref_dir = os.path.join(base, "ref")
        os.makedirs(ref_dir, exist_ok=True)
        ref_path = os.path.join(ref_dir, "ref.manifest.tsv")
        with open(ref_path, "w") as f:
            f.write(tool.build_manifest_tsv(d_stage))

        def refresh_ref():
            # 每次修改 d_stage 后必须重建 reference manifest，
            # 否则 staging reconcile 会因 4 字段不一致误报 exit 2
            with open(ref_path, "w") as f:
                f.write(tool.build_manifest_tsv(d_stage))

        def fresh_out(tag):
            d = os.path.join(base, "out-" + tag)
            os.makedirs(d, exist_ok=True)
            return d

        def tx(tag):
            # codex 初审 P1：tx() 必须创建 .txn 目录（否则后续 persist_journal
            # 直接失败）
            d = os.path.join(fresh_out(tag), ".txn")
            os.makedirs(d, exist_ok=True)
            return d

        # 1. lock_contention
        s = Scenario("lock_contention", "进程 A 持锁运行中，启动进程 B",
                     "B exit 6 + 明确错误；B 在任何 .txn/staging 变更前退出；A 正常完成")
        try:
            out = fresh_out("lock")
            fd = os.open(os.path.join(out, ".snapshot.lock"), os.O_RDWR | os.O_CREAT, 0o644)
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            try:
                p = run_snapshot(d_stage, out, ref_path)
                s.passed = (p.returncode == 6
                            and not os.path.exists(os.path.join(out, ".txn")))
                s.actual = "rc=%d txn_absent=%s" % (
                    p.returncode, not os.path.exists(os.path.join(out, ".txn")))
            finally:
                os.close(fd)
        except Exception as e:
            s.actual, s.passed = "EXC %s" % e, False
        results.append(s)

        # 2. staged_crash_before_backup（kill -9 在 persist(staged) 后、备份 mv 前）
        s = Scenario("crash_after_staged_persist",
                     "kill -9 在 journal(staged) 持久化之后、备份旧 tarball mv 之前"
                     "（等价态构造：journal=staged + staging 完整 + 旧对在 OUT_DIR）",
                     "重入：staging 校验 PASS → 补做备份 → backed_up → 提交新对 → "
                     "verified → 清理；OUT_DIR = 新对")
        try:
            out = fresh_out("stagedcrash")
            run_snapshot(d_stage, out, ref_path, expect_rc=0)
            old_tar_bytes = open(os.path.join(out, TAR), "rb").read()
            with open(os.path.join(d_stage, "a.txt"), "wb") as f:
                f.write(b"beta\n")
            refresh_ref()
            txn = tx("stagedcrash")
            new_sha, new_size = tool.build_staging(d_stage,
                                                   os.path.join(txn, "staging"),
                                                   ref_path)
            tool.persist_journal(os.path.join(txn, "journal"), mk_journal(
                txn, state="staged", new_tar_sha256=new_sha,
                new_tar_size=str(new_size), old_tar="present", old_sha="present",
                old_tar_sha256=tool.sha256_of(os.path.join(out, TAR)),
                old_sha_sha256=tool.sha256_of(os.path.join(out, SHA))))
            p = run_snapshot(d_stage, out, ref_path, expect_rc=0)
            s.passed = (open(os.path.join(out, TAR), "rb").read() != old_tar_bytes
                        and not os.path.exists(txn))
            s.actual = "rc=%d txn_cleaned=%s" % (p.returncode, not os.path.exists(txn))
        except Exception as e:
            s.actual, s.passed = "EXC %s" % e, False
        results.append(s)

        # 3. backup_mid_mv_crash（旧 tar 已 mv 入 .txn、旧 sha 未 mv）
        s = Scenario("crash_between_backup_mvs",
                     "kill -9 在旧 tarball 已 mv 入 .txn、旧 sha256 mv 之前"
                     "（等价态构造：journal=staged + old tar 在 .txn + old sha 在 OUT_DIR）",
                     "恢复补做剩余备份 mv（守卫幂等）→ 提交新对；OUT_DIR = 新对")
        try:
            out = fresh_out("bakmid")
            run_snapshot(d_stage, out, ref_path, expect_rc=0)
            with open(os.path.join(d_stage, "a.txt"), "wb") as f:
                f.write(b"gamma\n")
            refresh_ref()
            txn = tx("bakmid")
            new_sha, new_size = tool.build_staging(d_stage,
                                                   os.path.join(txn, "staging"),
                                                   ref_path)
            os.rename(os.path.join(out, TAR), os.path.join(txn, "old.tar.zst"))
            old_tar_sha = tool.sha256_of(os.path.join(txn, "old.tar.zst"))
            old_sha_sha = tool.sha256_of(os.path.join(out, SHA))
            tool.persist_journal(os.path.join(txn, "journal"), mk_journal(
                txn, state="staged", new_tar_sha256=new_sha,
                new_tar_size=str(new_size), old_tar="present", old_sha="present",
                old_tar_sha256=old_tar_sha, old_sha_sha256=old_sha_sha))
            p = run_snapshot(d_stage, out, ref_path, expect_rc=0)
            s.passed = (tool.sha256_of(os.path.join(out, TAR)) == new_sha
                        and not os.path.exists(txn))
            s.actual = "rc=%d new_pair=%s" % (
                p.returncode, tool.sha256_of(os.path.join(out, TAR)) == new_sha)
        except Exception as e:
            s.actual, s.passed = "EXC %s" % e, False
        results.append(s)

        # 4. backed_up_crash（backed_up 后、提交新 tar 前）
        s = Scenario("crash_after_backed_up_persist",
                     "kill -9 在 journal(backed_up) 之后、新 tarball mv 之前"
                     "（等价态构造：journal=backed_up + old.* 在 .txn + staging 完整）",
                     "恢复提交新 tar + 新 sha → 自校验 → 清理；OUT_DIR = 新对")
        try:
            out = fresh_out("bakupcrash")
            run_snapshot(d_stage, out, ref_path, expect_rc=0)
            with open(os.path.join(d_stage, "a.txt"), "wb") as f:
                f.write(b"delta\n")
            refresh_ref()
            txn = tx("bakupcrash")
            new_sha, new_size = tool.build_staging(d_stage,
                                                   os.path.join(txn, "staging"),
                                                   ref_path)
            old_tar_sha = tool.sha256_of(os.path.join(out, TAR))
            old_sha_sha = tool.sha256_of(os.path.join(out, SHA))
            os.rename(os.path.join(out, TAR), os.path.join(txn, "old.tar.zst"))
            os.rename(os.path.join(out, SHA), os.path.join(txn, "old.sha256"))
            tool.persist_journal(os.path.join(txn, "journal"), mk_journal(
                txn, state="backed_up", new_tar_sha256=new_sha,
                new_tar_size=str(new_size), old_tar="present", old_sha="present",
                old_tar_sha256=old_tar_sha, old_sha_sha256=old_sha_sha))
            p = run_snapshot(d_stage, out, ref_path, expect_rc=0)
            s.passed = (tool.sha256_of(os.path.join(out, TAR)) == new_sha
                        and not os.path.exists(txn))
            s.actual = "rc=%d new_pair=%s" % (
                p.returncode, tool.sha256_of(os.path.join(out, TAR)) == new_sha)
        except Exception as e:
            s.actual, s.passed = "EXC %s" % e, False
        results.append(s)

        # 5. tarball_mv_journal_crash（tar 已提交、journal(tarball_committed) 前）
        s = Scenario("crash_between_tarball_mv_and_journal",
                     "kill -9 在新 tar mv 到 OUT_DIR 后、persist(tarball_committed) 前"
                     "（等价态构造：journal=backed_up + OUT_DIR tar=new + staging 有 sha）",
                     "恢复跳过 tar 提交直接提交 sha → 自校验 → 清理；OUT_DIR = 新对")
        try:
            out = fresh_out("tarmv")
            run_snapshot(d_stage, out, ref_path, expect_rc=0)
            with open(os.path.join(d_stage, "a.txt"), "wb") as f:
                f.write(b"epsilon\n")
            refresh_ref()
            txn = tx("tarmv")
            new_sha, new_size = tool.build_staging(d_stage,
                                                   os.path.join(txn, "staging"),
                                                   ref_path)
            old_tar_sha = tool.sha256_of(os.path.join(out, TAR))
            old_sha_sha = tool.sha256_of(os.path.join(out, SHA))
            os.rename(os.path.join(out, TAR), os.path.join(txn, "old.tar.zst"))
            os.rename(os.path.join(out, SHA), os.path.join(txn, "old.sha256"))
            shutil.copyfile(os.path.join(txn, "staging", TAR),
                            os.path.join(out, TAR))
            os.remove(os.path.join(txn, "staging", TAR))
            tool.persist_journal(os.path.join(txn, "journal"), mk_journal(
                txn, state="backed_up", new_tar_sha256=new_sha,
                new_tar_size=str(new_size), old_tar="present", old_sha="present",
                old_tar_sha256=old_tar_sha, old_sha_sha256=old_sha_sha))
            p = run_snapshot(d_stage, out, ref_path, expect_rc=0)
            s.passed = (tool.sha256_of(os.path.join(out, TAR)) == new_sha
                        and not os.path.exists(txn))
            s.actual = "rc=%d new_pair=%s" % (
                p.returncode, tool.sha256_of(os.path.join(out, TAR)) == new_sha)
        except Exception as e:
            s.actual, s.passed = "EXC %s" % e, False
        results.append(s)

        # 6. selfcheck_fail_rollback（最终自校验失败 → fail-closed）
        s = Scenario("selfcheck_fail",
                     "sha_committed 后 OUT_DIR sidecar 与 tar 不匹配（sha256sum -c 失败）",
                     "回滚段 classify sidecar unknown → exit 9，journal 与现场保留"
                     "（fail-closed，v18 per codex v17 P1 #2）")
        try:
            out = fresh_out("selfcheck")
            run_snapshot(d_stage, out, ref_path, expect_rc=0)
            with open(os.path.join(d_stage, "a.txt"), "wb") as f:
                f.write(b"zeta\n")
            refresh_ref()
            txn = tx("selfcheck")
            new_sha, new_size = tool.build_staging(d_stage,
                                                   os.path.join(txn, "staging"),
                                                   ref_path)
            old_tar_sha = tool.sha256_of(os.path.join(out, TAR))
            old_sha_sha = tool.sha256_of(os.path.join(out, SHA))
            os.rename(os.path.join(out, TAR), os.path.join(txn, "old.tar.zst"))
            os.rename(os.path.join(out, SHA), os.path.join(txn, "old.sha256"))
            shutil.copyfile(os.path.join(txn, "staging", TAR),
                            os.path.join(out, TAR))
            with open(os.path.join(out, SHA), "w") as f:
                f.write("%s  %s\n" % ("f" * 64, TAR))
            tool.persist_journal(os.path.join(txn, "journal"), mk_journal(
                txn, state="sha_committed", new_tar_sha256=new_sha,
                new_tar_size=str(new_size), old_tar="present", old_sha="present",
                old_tar_sha256=old_tar_sha, old_sha_sha256=old_sha_sha))
            p = run_snapshot(d_stage, out, ref_path)
            s.passed = (p.returncode == 9
                        and os.path.exists(os.path.join(txn, "journal")))
            s.actual = "rc=%d journal_preserved=%s" % (
                p.returncode, os.path.exists(os.path.join(txn, "journal")))
        except Exception as e:
            s.actual, s.passed = "EXC %s" % e, False
        results.append(s)

        # 7. out_dir_readonly
        s = Scenario("out_dir_readonly", "OUT_DIR 只读（chmod 555）",
                     "锁/事务创建失败 → exit 5（旧对未动，可恢复）")
        try:
            out = fresh_out("readonly")
            run_snapshot(d_stage, out, ref_path, expect_rc=0)
            before = tool.sha256_of(os.path.join(out, TAR))
            os.chmod(out, 0o555)
            try:
                p = run_snapshot(d_stage, out, ref_path)
            finally:
                os.chmod(out, 0o755)
            after = tool.sha256_of(os.path.join(out, TAR))
            s.passed = (p.returncode == 5 and before == after)
            s.actual = "rc=%d old_pair_untouched=%s" % (p.returncode, before == after)
        except Exception as e:
            s.actual, s.passed = "EXC %s" % e, False
        results.append(s)

        # 8. journal_write_fail_at_staged（persist(staged) 前事务写入失败）
        s = Scenario("journal_write_fail_at_staged",
                     "staging 已生成后 .txn 只读 → persist(staged) 真实写入失败",
                     "exit 5；旧对未动（journal 先于移动的排序保证）")
        try:
            out = fresh_out("jwritefail")
            run_snapshot(d_stage, out, ref_path, expect_rc=0)
            before = tool.sha256_of(os.path.join(out, TAR))
            with open(os.path.join(d_stage, "a.txt"), "wb") as f:
                f.write(b"eta\n")
            refresh_ref()
            txn = tx("jwritefail")
            inject_dir = os.path.join(base, "inject-jwritefail")
            os.makedirs(inject_dir, exist_ok=True)
            env = dict(os.environ, LC_ALL="C",
                       INNOGPU_DSTAGE_INJECT_DIR=inject_dir,
                       INNOGPU_DSTAGE_INJECT_TAG="staging_built")
            pproc = subprocess.Popen(
                ["python3", _TOOL_PATH, "snapshot",
                 "--d-stage-root", d_stage, "--out-dir", out,
                 "--reference-manifest", ref_path],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                text=True, env=env)
            try:
                marker = os.path.join(inject_dir, "at-staging_built")
                deadline = time.time() + 30
                while not os.path.exists(marker) and time.time() < deadline:
                    if pproc.poll() is not None:
                        break
                    time.sleep(0.01)
                if not os.path.isfile(marker):
                    raise AssertionError("staging_built injection marker missing")
                os.chmod(txn, 0o555)
                with open(os.path.join(inject_dir, "release-staging_built"), "w"):
                    pass
                pproc.communicate(timeout=30)
                p = pproc
            finally:
                if pproc.poll() is None:
                    pproc.kill()
                    pproc.wait()
                os.chmod(txn, 0o755)
            after = tool.sha256_of(os.path.join(out, TAR))
            s.passed = (p.returncode == 5 and before == after)
            s.actual = "rc=%d old_pair_untouched=%s" % (p.returncode, before == after)
        except Exception as e:
            s.actual, s.passed = "EXC %s" % e, False
        results.append(s)

        # 9. journal_corrupt
        s = Scenario("journal_corrupt", "手工写入损坏 journal（state 非法）",
                     "journal 不可解析 → exit 9（证据不清理）")
        try:
            out = fresh_out("jcorrupt")
            txn = tx("jcorrupt")
            with open(os.path.join(txn, "journal"), "w") as f:
                f.write("schema=1\nstate=bogus_state\n")
            p = run_snapshot(d_stage, out, ref_path)
            s.passed = (p.returncode == 9
                        and os.path.exists(os.path.join(txn, "journal")))
            s.actual = "rc=%d journal_preserved=%s" % (
                p.returncode, os.path.exists(os.path.join(txn, "journal")))
        except Exception as e:
            s.actual, s.passed = "EXC %s" % e, False
        results.append(s)

        # 10. staging_dir_traversal
        s = Scenario("staging_dir_path_traversal", "journal staging_dir 指向 .txn 外",
                     "staging_dir 路径约束失败 → exit 9")
        try:
            out = fresh_out("straversal")
            txn = tx("straversal")
            tool.persist_journal(os.path.join(txn, "journal"), mk_journal(
                txn, state="staged", staging_dir=os.path.join(base, "outside")))
            p = run_snapshot(d_stage, out, ref_path)
            s.passed = (p.returncode == 9)
            s.actual = "rc=%d" % p.returncode
        except Exception as e:
            s.actual, s.passed = "EXC %s" % e, False
        results.append(s)

        # 11. existing_half_pair
        s = Scenario("old_pair_incomplete", "OUT_DIR 只有 tar 无 .sha256",
                     "4a 半对 fail-closed → exit 9")
        try:
            out = fresh_out("halfpair")
            with open(os.path.join(out, TAR), "wb") as f:
                f.write(b"x")
            p = run_snapshot(d_stage, out, ref_path)
            s.passed = (p.returncode == 9)
            s.actual = "rc=%d" % p.returncode
        except Exception as e:
            s.actual, s.passed = "EXC %s" % e, False
        results.append(s)

        # 12. old_pair_inconsistent
        s = Scenario("old_pair_inconsistent", "旧对 tarball 与 .sha256 内容不匹配",
                     "4a 自洽校验失败 → exit 9（损坏证据不静默丢弃）")
        try:
            out = fresh_out("oldinconsistent")
            with open(os.path.join(out, TAR), "wb") as f:
                f.write(b"old-content")
            with open(os.path.join(out, SHA), "w") as f:
                f.write("%s  %s\n" % ("1" * 64, TAR))
            p = run_snapshot(d_stage, out, ref_path)
            s.passed = (p.returncode == 9)
            s.actual = "rc=%d" % p.returncode
        except Exception as e:
            s.actual, s.passed = "EXC %s" % e, False
        results.append(s)

        # 13. out_dir_unknown_content
        s = Scenario("unknown_content_in_out_dir", "OUT_DIR tar 内容损坏/替换",
                     "classify unknown → exit 9")
        try:
            out = fresh_out("unknown")
            run_snapshot(d_stage, out, ref_path, expect_rc=0)
            with open(os.path.join(out, TAR), "wb") as f:
                f.write(b"corrupted")
            p = run_snapshot(d_stage, out, ref_path)
            s.passed = (p.returncode == 9)
            s.actual = "rc=%d" % p.returncode
        except Exception as e:
            s.actual, s.passed = "EXC %s" % e, False
        results.append(s)

        # 14. old_backup_missing
        s = Scenario("old_pair_restore_incomplete",
                     "journal 计划 present 但旧 tar 既不在 .txn 也不在 OUT_DIR",
                     "三者事实一致性 → exit 9")
        try:
            out = fresh_out("backupmissing")
            txn = tx("backupmissing")
            tool.persist_journal(os.path.join(txn, "journal"), mk_journal(
                txn, state="staged", new_tar_sha256="a" * 64, new_tar_size="10",
                old_tar="present", old_sha="present",
                old_tar_sha256="b" * 64, old_sha_sha256="c" * 64))
            p = run_snapshot(d_stage, out, ref_path)
            s.passed = (p.returncode == 9)
            s.actual = "rc=%d" % p.returncode
        except Exception as e:
            s.actual, s.passed = "EXC %s" % e, False
        results.append(s)

        # 15. rollback_sidecar_unknown
        s = Scenario("immediate_rollback_sidecar_unknown",
                     "rolling_back 态 OUT_DIR sidecar 内容未知（非精确 new）",
                     "拒绝删除 → exit 9（journal 与现场保留）")
        try:
            out = fresh_out("rbunknown")
            run_snapshot(d_stage, out, ref_path, expect_rc=0)
            with open(os.path.join(d_stage, "a.txt"), "wb") as f:
                f.write(b"theta\n")
            refresh_ref()
            txn = tx("rbunknown")
            new_sha, new_size = tool.build_staging(d_stage,
                                                   os.path.join(txn, "staging"),
                                                   ref_path)
            shutil.copyfile(os.path.join(txn, "staging", TAR),
                            os.path.join(out, TAR))
            with open(os.path.join(out, SHA), "w") as f:
                f.write("deadbeef\n")
            tool.persist_journal(os.path.join(txn, "journal"), mk_journal(
                txn, state="rolling_back", new_tar_sha256=new_sha,
                new_tar_size=str(new_size)))
            p = run_snapshot(d_stage, out, ref_path)
            s.passed = (p.returncode == 9
                        and os.path.exists(os.path.join(txn, "journal")))
            s.actual = "rc=%d journal_preserved=%s" % (
                p.returncode, os.path.exists(os.path.join(txn, "journal")))
        except Exception as e:
            s.actual, s.passed = "EXC %s" % e, False
        results.append(s)

        # 16. verified_half_pair
        s = Scenario("verified_half_pair", "journal=verified 但 OUT_DIR 只有新 tar",
                     "状态专属不变量失败 → exit 9，证据不清理")
        try:
            out = fresh_out("vhalf")
            txn = tx("vhalf")
            with open(os.path.join(out, TAR), "wb") as f:
                f.write(b"x")
            new_sha = tool.sha256_of(os.path.join(out, TAR))
            tool.persist_journal(os.path.join(txn, "journal"), mk_journal(
                txn, state="verified", new_tar_sha256=new_sha, new_tar_size="1"))
            p = run_snapshot(d_stage, out, ref_path)
            s.passed = (p.returncode == 9
                        and os.path.exists(os.path.join(txn, "journal")))
            s.actual = "rc=%d journal_preserved=%s" % (
                p.returncode, os.path.exists(os.path.join(txn, "journal")))
        except Exception as e:
            s.actual, s.passed = "EXC %s" % e, False
        results.append(s)

        # 17. noop_duplicate_sidecar
        s = Scenario("noop_duplicate_sidecar",
                     "旧对 tar == 新快照但 sidecar 两行重复（裸 sha256sum -c 会通过）",
                     "no-op 复用严格 sidecar 校验 → exit 9")
        try:
            out = fresh_out("noopdup")
            run_snapshot(d_stage, out, ref_path, expect_rc=0)
            h = tool.sha256_of(os.path.join(out, TAR))
            with open(os.path.join(out, SHA), "w") as f:
                f.write("%s  %s\n%s  %s\n" % (h, TAR, h, TAR))
            p = run_snapshot(d_stage, out, ref_path)
            s.passed = (p.returncode == 9)
            s.actual = "rc=%d" % p.returncode
        except Exception as e:
            s.actual, s.passed = "EXC %s" % e, False
        results.append(s)

        # 18-29 power-loss 场景（强制断电语义；本进程内永远 UNVERIFIED）
        # codex 八轮复审 P1：真实断电必然终止 harness 进程，任何"单进程内
        # hook 自报结果"的协议都无法独立验证断电/重启与终态——改为六模式
        # VM 驱动协议（codex 十轮复审 P1：工具内注入接缝 + 阻塞握手 +
        # rolling_back 等价态构造 + tombstone a/b/c 子窗口），验证全部由
        # harness 重启后独立完成（无任何自报结果参与判定）：
        #   --vm-prepare ROOT     隔离不可变输入 + 只读 spec 信任根
        #   --vm-mark SPEC --sw X 每场景/子窗口 boot_id 进 runtime
        #   --vm-setup-injection SPEC  构造注入前置状态（幂等）
        #   --vm-inject SPEC --sw X    snapshot 阻塞于接缝 → 驱动断电 → 重启
        #   --vm-verify SPEC --sw X    marker + boot + 输入不可变 + 恢复前
        #                              注入窗口 + 恢复 snapshot + rc/终态
        #   --vm-summarize ROOT    全部窗口独立复核 → fail-closed + genesis
        for name in POWER_LOSS_SCENARIOS:
            s = Scenario(name, "强制断电（VM poweroff / 拔盘后重启）；"
                         "**不得用 kill -9 代替**（kill -9 不丢 page cache）",
                         "见 design §5.3 v24 故障注入表对应行（三态/确定性契约）")
            s.actual = ("UNVERIFIED-pending-VM（真实断电必然终止本进程；由 VM "
                        "驱动经 --vm-prepare / --vm-mark / "
                        "--vm-setup-injection / --vm-inject / --vm-verify / "
                        "--vm-summarize 六模式执行，注入点为工具内接缝 + "
                        "阻塞握手，验证由 harness 重启后独立完成）")
            s.passed = None
            results.append(s)

        total = len(results)
        assert total == 29, "scenario count %d != 29" % total

        # ---- 补充回归测试（codex 七轮复审 P2：29 场景外的针对性回归）----
        # codex 八轮复审 P2：每个回归必须隔离异常——run_snapshot 的 expect_rc
        # 断言失败不得中断 fail-closed 汇总，异常一律记录为该回归 FAIL
        regs = []

        def add_reg(name, ok, actual):
            r = Scenario(name, "supplementary regression", "")
            r.passed = ok
            r.actual = actual
            regs.append(r)

        def run_reg(name, fn):
            try:
                ok, actual = fn()
            except Exception as e:
                ok, actual = False, "EXC %s" % e
            add_reg(name, ok, actual)

        # R1 中间级 symlink 绕过（.txn/staging -> external，journal 指向其子目录）
        def reg_midsymlink():
            out = fresh_out("reg-midsym")
            txn = os.path.join(out, ".txn")
            os.makedirs(txn, exist_ok=True)
            os.makedirs(os.path.join(base, "external"), exist_ok=True)
            os.symlink(os.path.join(base, "external"), os.path.join(txn, "staging"))
            tool.persist_journal(os.path.join(txn, "journal"), mk_journal(
                txn, state="staged",
                staging_dir=os.path.join(txn, "staging", "child")))
            p = run_snapshot(d_stage, out, ref_path)
            return p.returncode == 9, "rc=%d" % p.returncode

        run_reg("reg_staging_midsymlink", reg_midsymlink)

        # R2 sidecar 合法行 + 附加空行 → no-op 严格校验 → exit 9
        def reg_sidecar_blank():
            out = fresh_out("reg-sidecarblank")
            run_snapshot(d_stage, out, ref_path, expect_rc=0)
            h = tool.sha256_of(os.path.join(out, TAR))
            with open(os.path.join(out, SHA), "w") as f:
                f.write("%s  %s\n\n" % (h, TAR))
            p = run_snapshot(d_stage, out, ref_path)
            return p.returncode == 9, "rc=%d" % p.returncode

        run_reg("reg_sidecar_extra_blank", reg_sidecar_blank)

        # R3 FIFO journal → exit 9（非 regular file，不阻塞读取）
        def reg_fifo_journal():
            out = fresh_out("reg-fifo")
            txn = os.path.join(out, ".txn")
            os.makedirs(txn, exist_ok=True)
            os.mkfifo(os.path.join(txn, "journal"))
            p = run_snapshot(d_stage, out, ref_path)
            return p.returncode == 9, "rc=%d" % p.returncode

        run_reg("reg_fifo_journal", reg_fifo_journal)

        # R4 悬空 old.* symlink + 无 journal → exit 9（证据不静默删除）
        def reg_dangling_old():
            out = fresh_out("reg-danglingold")
            txn = os.path.join(out, ".txn")
            os.makedirs(txn, exist_ok=True)
            os.symlink(os.path.join(base, "nonexistent"),
                       os.path.join(txn, "old.tar.zst"))
            p = run_snapshot(d_stage, out, ref_path)
            return p.returncode == 9, "rc=%d" % p.returncode

        run_reg("reg_dangling_old_symlink", reg_dangling_old)

        # R5 相对 --out-dir 崩溃恢复（CLI 边界 canonicalize 修复验证）
        def reg_relative_outdir():
            rel_out = os.path.join(base, "reg-relout")
            os.makedirs(rel_out, exist_ok=True)
            run_snapshot(d_stage, rel_out, ref_path, expect_rc=0)
            old_bytes = open(os.path.join(rel_out, TAR), "rb").read()
            with open(os.path.join(d_stage, "a.txt"), "wb") as f:
                f.write(b"iota\n")
            refresh_ref()
            txn = os.path.join(rel_out, ".txn")
            os.makedirs(txn, exist_ok=True)
            new_sha, new_size = tool.build_staging(d_stage,
                                                   os.path.join(txn, "staging"),
                                                   ref_path)
            tool.persist_journal(os.path.join(txn, "journal"), mk_journal(
                txn, state="staged", new_tar_sha256=new_sha,
                new_tar_size=str(new_size), old_tar="present", old_sha="present",
                old_tar_sha256=tool.sha256_of(os.path.join(rel_out, TAR)),
                old_sha_sha256=tool.sha256_of(os.path.join(rel_out, SHA))))
            cwd = os.getcwd()
            os.chdir(base)
            try:
                p = subprocess.run(["python3", _TOOL_PATH, "snapshot",
                                    "--d-stage-root", d_stage,
                                    "--out-dir", "reg-relout",
                                    "--reference-manifest", ref_path],
                                   capture_output=True, text=True,
                                   env=dict(os.environ, LC_ALL="C"))
            finally:
                os.chdir(cwd)
            ok = (p.returncode == 0
                  and open(os.path.join(rel_out, TAR), "rb").read() != old_bytes)
            return ok, "rc=%d" % p.returncode

        run_reg("reg_relative_outdir_recovery", reg_relative_outdir)

        # R6 缺失 OUT_DIR → exit 5（锁打开失败，非 78）
        def reg_missing_outdir():
            p = subprocess.run(["python3", _TOOL_PATH, "snapshot",
                                "--d-stage-root", d_stage,
                                "--out-dir", os.path.join(base, "reg-missing"),
                                "--reference-manifest", ref_path],
                               capture_output=True, text=True,
                               env=dict(os.environ, LC_ALL="C"))
            return p.returncode == 5, "rc=%d" % p.returncode

        run_reg("reg_missing_outdir", reg_missing_outdir)

        # R7 文件系统白名单分类（纯函数单测）
        def reg_fs_classify():
            ok = (tool.classify_fs("ext4")
                  and tool.classify_fs("ext2/ext3")
                  and tool.classify_fs("xfs")
                  and tool.classify_fs("btrfs")
                  and not tool.classify_fs("tmpfs") and not tool.classify_fs("nfs")
                  and not tool.classify_fs("nfs4") and not tool.classify_fs("9p")
                  and not tool.classify_fs("virtiofs")
                  and not tool.classify_fs("overlay"))
            return ok, "classify_fs whitelist unit check"

        run_reg("reg_fs_classify", reg_fs_classify)

        # R8 jq 前置口径 fixture（dsh 裁定 2026-09-08：dpkg 包版本 ≥ 1.7.1
        # 为准，--version 接受 1.7.1/1.7 的 Debian 版本串怪癖 + dpkg-query
        # 佐证；纯函数单测，不依赖本机 jq 实际版本）
        def reg_jq_acceptance():
            ok = (tool.jq_version_ok("1.7.1", "1.7.1-6+deb13u3")
                  and tool.jq_version_ok("1.7", "1.7.1-6+deb13u3")
                  and tool.jq_version_ok("1.7", "1.7.1-1")
                  and tool.jq_version_ok("1.7", "2.0-1")
                  and not tool.jq_version_ok("1.7", "1.6-5")
                  and not tool.jq_version_ok("1.7", "")
                  and not tool.jq_version_ok("1.6", "1.7.1-6+deb13u3")
                  and not tool.jq_version_ok("", "1.7.1-6+deb13u3")
                  and not tool.jq_version_ok("1.7.2", "1.7.1-6+deb13u3")
                  and not tool.jq_version_ok("1.7.1", "garbage"))
            return ok, "jq dpkg-version acceptance fixture (dsh ruling)"

        run_reg("reg_jq_acceptance", reg_jq_acceptance)

        return results, regs, base, base_root, base_fstype
    finally:
        if not keep_tmp:
            shutil.rmtree(base, ignore_errors=True)


def merge_genesis(entries, out_dir):
    # entries: [{name, injection, expected_final_state, actual_final_state, passed}]
    if not out_dir:
        return
    genesis_path = os.path.join(out_dir, "d-stage-audit.genesis.json")
    if not os.path.isfile(genesis_path):
        return
    with open(genesis_path) as f:
        genesis = json.load(f)
    sub = genesis.setdefault("fault_injection_tests", {})
    sub.setdefault("schema_version", "13.0")
    sub.setdefault("snapshot_subcommand", {})
    for e in entries:
        entry = sub["snapshot_subcommand"].setdefault(e["name"], {})
        entry["injection"] = e["injection"]
        entry["expected_final_state"] = e["expected_final_state"]
        entry["actual_final_state"] = e["actual_final_state"]
        entry["passed"] = e["passed"]
    tmp = os.path.join(out_dir, ".genesis.tmp")
    with open(tmp, "w") as f:
        json.dump(genesis, f, indent=2, sort_keys=True)
        f.write("\n")
    os.replace(tmp, genesis_path)


def _require_allowed_fs(path, what):
    fs = stat_fstype(path)
    if not tool.classify_fs(fs):
        print("ENV-ERROR: %s filesystem %s is not a local crash-consistent "
              "filesystem (require ext2/ext3/ext4/xfs/btrfs)" % (what, fs or "?"))
        sys.exit(2)
    return fs


def _spec_path_for(root, name):
    return os.path.join(root, "vm-specs", "%s.json" % name)


def _sw_suffix(sw):
    return "" if sw is None else ".%s" % sw


def _runtime_path_for(spec_path, sw):
    return spec_path + _sw_suffix(sw) + ".runtime.json"


def _result_path_for(spec_path, sw):
    return spec_path + _sw_suffix(sw) + ".result.json"


def _load_spec(spec_path):
    # 拒绝 symlink / 非 regular file（codex 九轮复审 P2）
    if os.path.islink(spec_path) or not os.path.isfile(spec_path):
        print("ERROR: spec not a regular file: %s" % spec_path)
        return None
    try:
        with open(spec_path) as f:
            spec = json.load(f)
    except (OSError, ValueError) as e:
        print("ERROR: cannot read spec %s: %s" % (spec_path, e))
        return None
    name = spec.get("scenario", "")
    if name not in VM_SCENARIOS:
        print("ERROR: unknown scenario %r" % name)
        return None
    for k in ("out_dir", "d_stage_root", "reference_manifest",
              "new_tar_sha256", "old_tar_sha256", "old_sha_sha256",
              "d_stage_manifest_sha256", "inject_dir",
              "oldpair_tar", "oldpair_sha"):
        if not spec.get(k):
            print("ERROR: spec missing field %s" % k)
            return None
    seq = spec.get("sequence")
    if not isinstance(seq, int) or not (0 <= seq < len(POWER_LOSS_SCENARIOS)):
        print("ERROR: spec sequence invalid: %r" % seq)
        return None
    if POWER_LOSS_SCENARIOS[seq] != name:
        print("ERROR: spec sequence %s != %s" % (seq, name))
        return None
    sw_keys = spec.get("subwindows", [])
    for sw in sw_keys:
        # key=None 为无子窗口场景（codex 十四轮复审 P1：不得要求 None ∈ 场景
        # subwindows 字典）；非 None 必须存在于场景 subwindows
        if sw["key"] is not None and \
                sw["key"] not in VM_SCENARIOS[name].get("subwindows", {}):
            print("ERROR: spec subwindow unknown: %r" % sw["key"])
            return None
        if not sw.get("out_dir"):
            print("ERROR: spec subwindow missing out_dir: %r" % sw["key"])
            return None
        if not sw.get("inject_dir"):
            print("ERROR: spec subwindow missing inject_dir: %r" % sw["key"])
            return None
    return spec


def _spec_sha256(spec_path):
    return tool.sha256_of(spec_path)


def _fsync_file(path):
    fd = os.open(path, os.O_RDONLY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def _fsync_dir(path):
    dfd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(dfd)
    finally:
        os.close(dfd)


def _fsync_tree(root):
    # codex 十轮复审 P2：断电持久化屏障——所有文件 + 目录（后序）逐一 fsync
    for dirpath, dirnames, filenames in os.walk(root):
        for name in filenames:
            p = os.path.join(dirpath, name)
            if not os.path.islink(p) and os.path.isfile(p):
                _fsync_file(p)
    for dirpath, dirnames, filenames in os.walk(root, topdown=False):
        for name in dirnames:
            d = os.path.join(dirpath, name)
            if not os.path.islink(d):
                _fsync_dir(d)
        if not os.path.islink(dirpath):
            _fsync_dir(dirpath)


def _write_json_atomic(path, obj):
    # tmp 写入 + fsync 文件 + os.replace + fsync 父目录（codex 十轮复审 P2：
    # 断电不得丢失 mark/spec/result 更新，否则产生不可恢复的假失败）
    d = os.path.dirname(path) or "."
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(obj, f, sort_keys=True, indent=2)
        f.write("\n")
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, path)
    _fsync_dir(d)


def _write_text_atomic(path, text):
    d = os.path.dirname(path) or "."
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        f.write(text)
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, path)
    _fsync_dir(d)


def cmd_vm_prepare(root):
    # VM 驱动协议 · 阶段 A（codex 九/十轮复审 P1）：每场景隔离且不可变的
    # d_stage/ref.manifest/out + pristine 旧对副本 + 注入接缝目录；spec 写入
    # 后 chmod 444（不可变信任根，codex 十轮复审 P1）并全树 fsync 持久化
    # （codex 十轮复审 P2）；mark/verify 运行时状态写入独立 runtime 文件。
    root = tool.realpath_m(root)
    os.makedirs(root, exist_ok=True)
    _require_allowed_fs(root, "VM root")
    spec_dir = os.path.join(root, "vm-specs")
    if os.path.isdir(spec_dir) and os.listdir(spec_dir):
        print("ERROR: %s already contains specs; use a fresh root (prepare "
              "refuses to clobber prior runs)" % spec_dir)
        return 2
    os.makedirs(spec_dir, exist_ok=True)
    for idx, name in enumerate(POWER_LOSS_SCENARIOS):
        sc_dir = os.path.join(root, "vm-%s" % name)
        d_stage = os.path.join(sc_dir, "dstage")
        os.makedirs(sc_dir, exist_ok=True)
        make_fixture_tree(d_stage, {"a.txt": b"alpha\n",
                                    "sub/b.bin": b"\x00\x01\x02"})
        if not os.path.lexists(os.path.join(d_stage, "link_ok")):
            os.symlink("a.txt", os.path.join(d_stage, "link_ok"))
        ref_path = os.path.join(sc_dir, "ref.manifest.tsv")
        with open(ref_path, "w") as f:
            f.write(tool.build_manifest_tsv(d_stage))
        # 每子窗口独立 out_dir（codex 十三轮复审 P1：多子窗口场景共享
        # out_dir 会令后续窗口重置摧毁早期窗口终态证据）
        subwindows = [{"key": None,
                       "inject_tag": VM_SCENARIOS[name]["inject_tag"]}] \
            if not VM_SCENARIOS[name].get("subwindows") else \
            [{"key": sw, "inject_tag": VM_SCENARIOS[name]["subwindows"][sw]
              ["inject_tag"]} for sw in vm_subwindows(name)]
        for sw_entry in subwindows:
            sw = sw_entry["key"]
            out = os.path.join(sc_dir, "out" if sw is None else "out-%s" % sw)
            os.makedirs(out, exist_ok=True)
            # 基线旧对（旧对内容 = 无 marker 的 fixture，与随后 marker 相异）
            run_snapshot(d_stage, out, ref_path, expect_rc=0)
            sw_entry["out_dir"] = out
            # 每子窗口独立 inject_dir（codex 十四轮复审 P1：共享 inject_dir
            # 时后续子窗口 setup 清理会摧毁早期窗口 marker 证据）
            inj = os.path.join(sc_dir,
                               "inject" if sw is None else "inject-%s" % sw)
            os.makedirs(inj, exist_ok=True)
            sw_entry["inject_dir"] = inj
        first_out = subwindows[0]["out_dir"]
        old_tar_sha = tool.sha256_of(os.path.join(first_out, TAR))
        old_sha_sha = tool.sha256_of(os.path.join(first_out, SHA))
        # pristine 旧对副本（setup-injection 幂等重置用，不回跑 snapshot）
        oldpair_dir = os.path.join(sc_dir, "oldpair")
        os.makedirs(oldpair_dir, exist_ok=True)
        oldpair_tar = os.path.join(oldpair_dir, TAR)
        oldpair_sha = os.path.join(oldpair_dir, SHA)
        shutil.copyfile(os.path.join(first_out, TAR), oldpair_tar)
        shutil.copyfile(os.path.join(first_out, SHA), oldpair_sha)
        # 本场景唯一 marker → 期望新对 != 旧对（验证具备区分度）
        marker = os.path.join(d_stage, "vm_marker.txt")
        with open(marker, "w") as f:
            f.write("vm power-loss scenario %s\n" % name)
        with open(ref_path, "w") as f:
            f.write(tool.build_manifest_tsv(d_stage))
        # harness 独立计算期望新对（确定性 tar.zst；驱动无法伪造字节）
        scratch = tempfile.mkdtemp(prefix="vm-expect-", dir=sc_dir)
        try:
            new_sha, new_size = tool.build_staging(d_stage, scratch, ref_path)
        finally:
            shutil.rmtree(scratch, ignore_errors=True)
        spec = {
            "scenario": name,
            "sequence": idx,
            "out_dir": first_out,
            "d_stage_root": d_stage,
            "reference_manifest": ref_path,
            "new_tar_sha256": new_sha,
            "new_tar_size": new_size,
            "old_tar_sha256": old_tar_sha,
            "old_sha_sha256": old_sha_sha,
            "d_stage_manifest_sha256": tool.sha256_of(ref_path),
            "inject_dir": subwindows[0]["inject_dir"],
            "oldpair_tar": oldpair_tar,
            "oldpair_sha": oldpair_sha,
            "trigger_timeout_s": 120,
            "subwindows": subwindows,
            "injection": ("见 genesis fault_injection_tests."
                          "snapshot_subcommand.%s.injection" % name),
        }
        spec_path = _spec_path_for(root, name)
        _write_json_atomic(spec_path, spec)
        os.chmod(spec_path, 0o444)  # 不可变信任根（codex 十轮复审 P1）
        print("vm_prepare: %s -> %s (subwindows: %s)"
              % (name, spec_path, [s["key"] for s in subwindows]))
    _fsync_tree(root)
    print("vm_prepare: OK (12 isolated immutable specs ready). Per scenario "
          "per subwindow the VM driver: --vm-mark <spec> [--subwindow X] → "
          "--vm-setup-injection <spec> → --vm-inject <spec> [--subwindow X] → "
          "power off → reboot → --vm-verify <spec> [--subwindow X]")
    return 0


def _receipt_file_ok(path, expected_content):
    # receipt 不可变信任根校验（codex 十八轮复审 P1：拒绝 symlink；十九轮
    # 残余风险注记：改用 O_NOFOLLOW + fd 锚定——open 即失败于 symlink，
    # 消除 islink-then-open 的 TOCTOU 竞态；随后 fstat regular file /
    # 无写位 / 逐字节内容 / chattr i 标志）
    try:
        fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    except OSError:
        return False
    try:
        st = os.fstat(fd)
        if not stat.S_ISREG(st.st_mode):
            return False
        with os.fdopen(fd) as f:
            content = f.read()
    except OSError:
        return False
    return (not (stat.S_IMODE(st.st_mode) & 0o222)
            and content == expected_content
            and _has_immutable_flag(path))


def _marker_content(tag, pid, token, secret):
    # marker 固定 schema：四行、固定顺序、无额外内容（codex 十五轮复审 P2）
    return "tag=%s\npid=%d\ntoken=%s\nsecret=%s\n" % (tag, pid, token, secret)


def _receipt_line(tag, pid, token, secret):
    return "INJECT_MARKER tag=%s pid=%d token=%s secret=%s" % (
        tag, pid, token, secret)


def _marker_matches(path, tag, token, pid=None, secret=None):
    # 注入 marker 校验：**逐字节全内容相等**（固定 schema，拒绝重复字段/
    # 额外字段/额外内容/行序变化，codex 十五轮复审 P2）；O_NOFOLLOW + fd
    # 锚定读取（open 即失败于 symlink，消除 islink-then-open 的 TOCTOU
    # 竞态，codex 十九轮残余风险注记）——内容与管道收据为同一四元组的
    # 两份渲染
    try:
        fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
        with os.fdopen(fd) as f:
            content = f.read()
    except OSError:
        return False
    return content == _marker_content(tag, pid, token, secret)


def _stop_child(proc):
    # 注入失败路径的进程回收（codex 十五轮复审 P1：terminate → wait →
    # kill → wait；十六轮复审 P2：返回 True = 无法回收存活，调用方必须
    # fail-closed 记录 PID，不得吞掉异常静默继续）
    for action in (proc.terminate, proc.kill):
        try:
            action()
        except Exception:
            pass
        try:
            proc.wait(timeout=10)
            break
        except Exception:
            continue
    return proc.poll() is None


def _chattr(path, flag):
    # chattr +i/-i（ext4/xfs/btrfs 白名单文件系统均支持；失败抛异常由
    # 调用方 fail-closed）；成功后必须 fsync 文件 inode 与父目录——i 标志
    # 变更与 chmod 同为元数据，断电未持久化会导致真实注入被校验误判失败
    # （codex 十七轮复审 P1）
    subprocess.run(["chattr", flag, path], check=True,
                   capture_output=True, text=True,
                   env=dict(os.environ, LC_ALL="C"), timeout=30)
    _fsync_file(path)
    _fsync_dir(os.path.dirname(path) or ".")


def _has_immutable_flag(path):
    # lsattr 解析 i 标志（codex 十六轮复审 P1：目录所有者可 unlink/替换
    # 444 文件，真正的不可变信任根需要 chattr +i；校验失败一律 False）
    try:
        out = subprocess.run(["lsattr", path], capture_output=True,
                             text=True,
                             env=dict(os.environ, LC_ALL="C"), timeout=30)
    except (OSError, subprocess.TimeoutExpired):
        return False
    if out.returncode != 0:
        return False
    lines = out.stdout.splitlines()
    if not lines:
        return False
    attrs = lines[0].split()
    if not attrs:
        return False
    return "i" in attrs[0]


def _chmod_and_fsync(path, mode):
    # chmod + 文件元数据 fsync + 目录 fsync（codex 十六轮复审 P2：断电不得
    # 丢失 mode 变更，否则真实注入会被 mode 校验误判失败）
    os.chmod(path, mode)
    _fsync_file(path)
    _fsync_dir(os.path.dirname(path) or ".")


def _remove_residue(p):
    # 残留清理：先解除不可变标志（成功注入的 marker/receipt 带 chattr +i），
    # 再 symlink/regular file 删除、目录 rmtree（codex 十二轮复审 P2 +
    # 十六轮复审 P1）
    try:
        _chattr(p, "-i")
    except Exception:
        pass
    if os.path.islink(p) or os.path.isfile(p):
        os.remove(p)
    elif os.path.isdir(p):
        shutil.rmtree(p)


def sw_out_dir(spec, sw):
    # 每个子窗口独立 out_dir（codex 十三轮复审 P1：多子窗口场景不得共享
    # out_dir，否则后续窗口重置会摧毁早期窗口终态证据）
    for s in spec.get("subwindows", []):
        if s["key"] == sw:
            return s.get("out_dir") or spec["out_dir"]
    return spec["out_dir"]


def sw_inject_dir(spec, sw):
    # 每个子窗口独立 inject_dir（codex 十四轮复审 P1：共享 inject_dir 时
    # 后续子窗口 setup 会清理并摧毁早期窗口的 marker 证据）
    for s in spec.get("subwindows", []):
        if s["key"] == sw:
            return s.get("inject_dir") or spec["inject_dir"]
    return spec["inject_dir"]


def sw_recovery_log(spec, sw):
    # 每个子窗口独立 recovery.log（codex 十三轮复审 P1）
    return os.path.join(sw_inject_dir(spec, sw),
                        "recovery%s.log" % _sw_suffix(sw))


def _evidence_facts_fail(out_dir, spec, which):
    # rc=9 evidence 的事实核验（codex 十二轮复审 P1）：旧文件必须真实处于
    # 双存在（.txn 与 OUT_DIR 都有）或均缺失（都没有）——合法四态 + 无关
    # journal 不得冒充 fail-closed 证据
    txn = os.path.join(out_dir, ".txn")
    ts, ss = _out_pair(out_dir, spec)
    name = "old.tar.zst" if which == "tar" else "old.sha256"
    in_txn = _regular_in_txn(txn, name)
    in_out = (ts == "old") if which == "tar" else (ss == "old")
    inconsistent = (in_txn and in_out) or (not in_txn and not in_out)
    return inconsistent, "%s in_txn=%s in_out=%s double-or-missing=%s" % (
        name, in_txn, in_out, inconsistent)


def _load_runtime(spec, spec_path, sw):
    # runtime 文件：mark 写入的可变状态（spec 只读后 boot_id/token 放这里）。
    # codex 十一轮复审 P2：全字段语义校验——scenario/subwindow/sequence/
    # protocol 必须与 spec 一致，boot_id_before 与 run_token 必须为非空
    # 字符串；任一不符 → None（拒绝跨场景/跨子窗口复用）
    p = _runtime_path_for(spec_path, sw)
    if os.path.islink(p) or not os.path.isfile(p):
        return None
    try:
        with open(p) as f:
            rt = json.load(f)
    except (OSError, ValueError):
        return None
    if rt.get("scenario") != spec["scenario"]:
        return None
    if rt.get("subwindow") != sw:
        return None
    if rt.get("sequence") != spec["sequence"]:
        return None
    if rt.get("protocol") != 1:
        return None
    if not isinstance(rt.get("boot_id_before"), str) or \
            not rt.get("boot_id_before"):
        return None
    if not isinstance(rt.get("run_token"), str) or not rt.get("run_token"):
        return None
    return rt


def cmd_vm_mark(spec_path, subwindow):
    # 每场景/子窗口注入前记录当前 boot_id + 每轮唯一 run_token 进 runtime
    # 文件（spec 只读不可写；codex 九轮复审 P1 每场景绑定 + 十轮复审 P2
    # fsync 持久化 + 十一轮复审 P1 token 拒绝残留 marker / P2 语义绑定）
    spec = _load_spec(spec_path)
    if spec is None:
        return 2
    sw_keys = [s["key"] for s in spec.get("subwindows", [])]
    if subwindow not in sw_keys:
        print("vm_mark: ERROR: subwindow %r not in spec %r"
              % (subwindow, sw_keys))
        return 2
    boot = read_boot_id()
    if boot is None:
        print("vm_mark: ERROR: cannot read boot_id")
        return 2
    rt = {"scenario": spec["scenario"], "subwindow": subwindow,
          "sequence": spec["sequence"], "protocol": 1,
          "boot_id_before": boot,
          "run_token": os.urandom(16).hex(),
          "written_by": "harness --vm-mark"}
    _write_json_atomic(_runtime_path_for(spec_path, subwindow), rt)
    print("vm_mark: %s sw=%r boot_id_before=%s token=%s"
          % (spec["scenario"], subwindow, boot, rt["run_token"]))
    return 0


def cmd_vm_setup_injection(spec_path, subwindow):
    # 注入前置状态构造（codex 十轮复审 P1：rolling_back 场景的等价态构造；
    # 十一轮复审 P1：清理 inject_dir 上一轮残留 at-*/release-* 并 fsync，
    # 防止旧 marker 使 --vm-inject 误判本轮已到达接缝）。setup 取值：
    #   None                      → 重置为 pristine 旧对（全新路径注入）
    #   rolling_back              → journal=rolling_back + OUT=(new,new) +
    #                               old.* 在 .txn（回滚删除/还原窗口注入）
    #   rolling_back_sha_restored → journal=rolling_back + OUT=(old,absent) +
    #                               old.sha 在 .txn（旧 sidecar 还原 fsync
    #                               窗口注入；旧 tar 已还原，codex 十一轮
    #                               复审 P1 tar/sha 双窗口覆盖）
    # 幂等（重试安全），全部 fsync 持久化（十轮复审 P2）。
    spec = _load_spec(spec_path)
    if spec is None:
        return 2
    sw_keys = [s["key"] for s in spec.get("subwindows", [])]
    if subwindow not in sw_keys:
        print("vm_setup_injection: ERROR: subwindow %r not in spec %r"
              % (subwindow, sw_keys))
        return 2
    sw_cfg = vm_subwindow_cfg(spec["scenario"], subwindow)
    setup = sw_cfg.get("setup")
    out = sw_out_dir(spec, subwindow)
    if not os.path.isdir(out):
        print("vm_setup_injection: ERROR: out_dir missing: %s" % out)
        return 2
    _require_allowed_fs(out, "OUT_DIR")
    # 清理上一轮注入残留（marker/release/log，含同名目录；十一轮复审 P1 +
    # 十二轮复审 P2 + 十三/十四轮复审 P1：仅清理本子窗口 inject_dir）
    inject_dir = sw_inject_dir(spec, subwindow)
    os.makedirs(inject_dir, exist_ok=True)
    for name in os.listdir(inject_dir):
        if name.startswith(("at-", "release-", "inject", "recovery",
                            "receipt")):
            _remove_residue(os.path.join(inject_dir, name))
    _fsync_dir(inject_dir)
    # 重置 OUT_DIR（保留 .snapshot.lock）
    for name in (TAR, SHA, ".txn", ".txn.tombstone"):
        p = os.path.join(out, name)
        if os.path.islink(p) or os.path.isfile(p):
            os.remove(p)
        elif os.path.isdir(p):
            shutil.rmtree(p)
    shutil.copyfile(spec["oldpair_tar"], os.path.join(out, TAR))
    shutil.copyfile(spec["oldpair_sha"], os.path.join(out, SHA))
    if setup == "rolling_back":
        txn = os.path.join(out, ".txn")
        os.makedirs(txn, exist_ok=True)
        staging = os.path.join(txn, "staging")
        new_sha, new_size = tool.build_staging(
            spec["d_stage_root"], staging, spec["reference_manifest"])
        # 旧对移入 .txn（old.*），新对进入 OUT_DIR（(new,new)）
        os.rename(os.path.join(out, TAR), os.path.join(txn, "old.tar.zst"))
        os.rename(os.path.join(out, SHA), os.path.join(txn, "old.sha256"))
        shutil.copyfile(os.path.join(staging, TAR), os.path.join(out, TAR))
        shutil.copyfile(os.path.join(staging, SHA), os.path.join(out, SHA))
        journal = ("schema=1\nstate=rolling_back\nstaging_dir=%s\n"
                   "new_tar_sha256=%s\nnew_tar_size=%s\n"
                   "old_tar=present\nold_sha=present\n"
                   "old_tar_sha256=%s\nold_sha_sha256=%s\n"
                   % (staging, new_sha, str(new_size),
                      spec["old_tar_sha256"], spec["old_sha_sha256"]))
        _write_text_atomic(os.path.join(txn, "journal"), journal)
    elif setup == "rolling_back_sha_restored":
        # 旧 tar 已还原在 OUT_DIR，旧 sidecar 待还原（facts_check 计划
        # present + XOR 互斥：tar 仅 OUT / sha 仅 .txn）
        txn = os.path.join(out, ".txn")
        os.makedirs(txn, exist_ok=True)
        staging = os.path.join(txn, "staging")
        new_sha, new_size = tool.build_staging(
            spec["d_stage_root"], staging, spec["reference_manifest"])
        os.rename(os.path.join(out, SHA), os.path.join(txn, "old.sha256"))
        journal = ("schema=1\nstate=rolling_back\nstaging_dir=%s\n"
                   "new_tar_sha256=%s\nnew_tar_size=%s\n"
                   "old_tar=present\nold_sha=present\n"
                   "old_tar_sha256=%s\nold_sha_sha256=%s\n"
                   % (staging, new_sha, str(new_size),
                      spec["old_tar_sha256"], spec["old_sha_sha256"]))
        _write_text_atomic(os.path.join(txn, "journal"), journal)
    _fsync_tree(out)
    print("vm_setup_injection: %s sw=%r ready (setup=%s)"
          % (spec["scenario"], subwindow, setup or "baseline"))
    return 0


def _log_rc_category(stdout_text, stderr_text):
    # 由工具自身输出完成行独立判定恢复 run 的退出码类别（0/1/9），不信任
    # result 自报 rc（codex 十二轮复审 P1）
    if "OK: rolled back" in stdout_text or "was rolling_back" in stdout_text:
        return 1
    if "OK:" in stdout_text or "recovery complete (was verified)" in stdout_text:
        return 0
    if "ERROR:" in stderr_text:
        return 9
    return None


def cmd_vm_inject(spec_path, subwindow):
    # 注入执行（codex 十轮复审 P1：工具内接缝 + 阻塞握手替代轮询；十一轮
    # 复审 P1：每轮唯一 token；十二轮复审 P1：注入前清理本 tag 残留 marker
    # （含目录）、marker 内容必须含本轮 token 且 pid 与**当前 spawn 的
    # snapshot 子进程 pid 一致**——外部伪造 marker 无法通过，同一 runtime
    # 重试时旧 marker 先被清除不会假触发）——以 INNOGPU_DSTAGE_INJECT_
    # DIR/TAG/TOKEN 启动 snapshot，轮询 at-<tag> marker；marker 出现且
    # pid 匹配即工具已阻塞在注入点（窗口确定性成立），exit 0 并把子进程
    # pid 记入 runtime 后**留下阻塞中的子进程**，由驱动断电。子进程提前
    # 退出（未到接缝）→ exit 1；超时 → exit 2。
    spec = _load_spec(spec_path)
    if spec is None:
        return 2
    sw_keys = [s["key"] for s in spec.get("subwindows", [])]
    if subwindow not in sw_keys:
        print("vm_inject: ERROR: subwindow %r not in spec %r"
              % (subwindow, sw_keys))
        return 2
    rt = _load_runtime(spec, spec_path, subwindow)
    if rt is None:
        print("vm_inject: ERROR: no valid runtime file (run --vm-mark first)")
        return 2
    tag = vm_subwindow_cfg(spec["scenario"], subwindow)["inject_tag"]
    out = sw_out_dir(spec, subwindow)
    inject_dir = sw_inject_dir(spec, subwindow)
    os.makedirs(inject_dir, exist_ok=True)
    _fsync_dir(inject_dir)
    # 本轮残留清理（同 tag marker/release，含目录；十二轮复审 P1/P2）
    for name in ("at-" + tag, "release-" + tag):
        _remove_residue(os.path.join(inject_dir, name))
    _fsync_dir(inject_dir)
    marker = os.path.join(inject_dir, "at-" + tag)
    # 每轮一次性 secret：仅经 env 传给工具子进程、触发后才落盘 runtime——
    # 外部驱动无法预先伪造 marker（codex 十三轮复审 P2；工具侧 O_EXCL
    # 保证 marker 只能由工具进程创建）
    secret = os.urandom(16).hex()
    env = dict(os.environ, LC_ALL="C",
               INNOGPU_DSTAGE_INJECT_DIR=inject_dir,
               INNOGPU_DSTAGE_INJECT_TAG=tag,
               INNOGPU_DSTAGE_INJECT_TOKEN=rt["run_token"],
               INNOGPU_DSTAGE_INJECT_SECRET=secret)
    log_path = os.path.join(inject_dir, "inject.log")
    logf = open(log_path, "a")
    # codex 十四轮复审 P2：stdout 走 harness 私有管道（驱动无法写入）——
    # 工具在接缝经管道输出 INJECT_MARKER 收据行，管道来源即工具侧认证；
    # marker 文件（驱动可写目录）仅作断电后持久证据，其内容必须与管道
    # 收据一致
    proc = subprocess.Popen(
        ["python3", _TOOL_PATH, "snapshot",
         "--d-stage-root", spec["d_stage_root"],
         "--out-dir", out,
         "--reference-manifest", spec["reference_manifest"]],
        stdout=subprocess.PIPE, stderr=logf, env=env, text=True)
    expected_receipt = _receipt_line(tag, proc.pid, rt["run_token"], secret)
    lines = []

    def _reader():
        for line in proc.stdout:
            lines.append(line.rstrip("\n"))
        proc.stdout.close()

    threading.Thread(target=_reader, daemon=True).start()
    deadline = time.time() + spec.get("trigger_timeout_s", 120)
    while time.time() < deadline:
        if proc.poll() is not None:
            logf.close()
            print("vm_inject: ERROR: snapshot exited before reaching seam "
                  "(rc=%s); see %s" % (proc.returncode, log_path))
            return 1
        for ln in lines:
            if not ln.startswith("INJECT_MARKER "):
                continue
            if ln != expected_receipt or \
                    not _marker_matches(marker, tag, rt["run_token"],
                                        proc.pid, secret):
                # 收据不匹配：终止并回收子进程，确认其不再写事务目录
                # （codex 十五轮复审 P1；十六轮复审 P2 无法回收显式升级）
                logf.close()
                alive = _stop_child(proc)
                print("vm_inject: ERROR: receipt mismatch via private pipe "
                      "(%r); child stopped=%s pid=%d"
                      % (ln, not alive, proc.pid))
                return 1
            rt["inject_pid"] = proc.pid
            rt["inject_secret"] = secret
            rt["inject_receipt"] = ln
            _write_json_atomic(_runtime_path_for(spec_path, subwindow), rt)
            # 不可变收据文件（codex 十五轮复审 P1 + 十六轮复审 P1/P2：
            # 管道收据的只读持久载体；chmod 后文件元数据 fsync + 目录
            # fsync；再 chattr +i —— 目录所有者 unlink/替换 444 文件不可
            # 防止，chattr +i 才是不可重写信任根，verify/summarize 校验
            # lsattr i 标志）
            receipt_path = os.path.join(inject_dir, "receipt")
            try:
                _write_text_atomic(receipt_path, ln + "\n")
                _chmod_and_fsync(receipt_path, 0o444)
                _chattr(receipt_path, "+i")
                _chattr(marker, "+i")
            except Exception as e:
                logf.close()
                alive = _stop_child(proc)
                print("vm_inject: ERROR: receipt/marker immutability persist "
                      "failed (%s); child stopped=%s pid=%d"
                      % (e, not alive, proc.pid))
                return 1
            logf.close()
            print("vm_inject: %s sw=%r TRIGGERED at seam %s (pid=%d, receipt "
                  "via private pipe; cut power now)"
                  % (spec["scenario"], subwindow, tag, proc.pid))
            return 0
        time.sleep(0.05)
    # 超时：终止并回收仍可能在接缝阻塞或继续运行的子进程（codex 十五轮
    # 复审 P1：确认其不再修改 OUT_DIR；十六轮复审 P2 无法回收显式升级）
    logf.close()
    alive = _stop_child(proc)
    print("vm_inject: ERROR: %s sw=%r marker timeout after %ss (child "
          "stopped=%s pid=%d)"
          % (spec["scenario"], subwindow,
             spec.get("trigger_timeout_s", 120), not alive, proc.pid))
    return 2


def cmd_vm_verify(spec_path, subwindow):
    # VM 驱动协议 · 阶段 B（codex 九/十轮复审 P1）：重启后由 harness 独立
    # 验证，无任何自报结果参与判定：
    #   ① 注入 marker 证据（工具在接缝写入并 fsync 的 at-<tag>，证明注入点
    #      执行真实发生）
    #   ② boot_id 变化（runtime 文件每场景/子窗口绑定断电证据）
    #   ③ 输入不可变（spec 只读 + ref.manifest sha 重算一致）
    #   ④ 恢复前状态验证（断电必须落在该场景/子窗口注入窗口）
    #   ⑤ harness 亲自重跑 snapshot（无注入环境；命令由 spec 字段重建）
    #   ⑥ 恢复退出码 ∈ allowed_rcs + 终态分支与退出码一致
    spec = _load_spec(spec_path)
    if spec is None:
        return 2
    sw_keys = [s["key"] for s in spec.get("subwindows", [])]
    if subwindow not in sw_keys:
        print("vm_verify: ERROR: subwindow %r not in spec %r"
              % (subwindow, sw_keys))
        return 2
    sw_cfg = vm_subwindow_cfg(spec["scenario"], subwindow)
    out = sw_out_dir(spec, subwindow)
    if not os.path.isdir(out):
        print("vm_verify: ERROR: out_dir missing: %s" % out)
        return 2
    _require_allowed_fs(out, "OUT_DIR")
    # ① marker 证据 + 不可变收据（codex 十五轮复审 P1/P2）：管道收据的只读
    #    持久载体为 inject_dir/receipt（无写位 + 逐字节 = 收据行 + \n）；
    #    runtime.inject_receipt 必须与之一致；marker 无写位 + 逐字节全内容
    #    = 同一四元组的固定 schema 渲染——拒绝重复/额外字段或内容
    tag = sw_cfg["inject_tag"]
    rt = _load_runtime(spec, spec_path, subwindow)
    inject_dir = sw_inject_dir(spec, subwindow)
    marker = os.path.join(inject_dir, "at-" + tag)
    receipt_path = os.path.join(inject_dir, "receipt")
    inject_pid = rt.get("inject_pid") if rt else None
    inject_secret = rt.get("inject_secret") if rt else None
    expected_receipt = _receipt_line(tag, inject_pid,
                                     rt["run_token"] if rt else "",
                                     inject_secret)
    receipt_file_ok = (rt is not None and isinstance(inject_pid, int)
                       and isinstance(inject_secret, str)
                       and bool(inject_secret)
                       and _receipt_file_ok(receipt_path,
                                            expected_receipt + "\n"))
    runtime_receipt_ok = (rt is not None
                          and rt.get("inject_receipt") == expected_receipt)
    marker_ok = (runtime_receipt_ok and receipt_file_ok
                 and _marker_matches(marker, tag, rt["run_token"],
                                     inject_pid, inject_secret)
                 and (os.stat(marker).st_mode & 0o7777 & 0o222) == 0
                 and _has_immutable_flag(marker))
    marker_desc = "marker_ok"
    if not marker_ok:
        marker_desc = "marker missing or token/tag/pid mismatch"
        print("vm_verify: ERROR: injection marker evidence failed: %s"
              % marker)
        _write_result(spec_path, subwindow, _result_fail(
            spec, subwindow, None, None, False,
            "injection marker evidence failed: %s" % marker, None, None))
        return 1
    # ② boot 证据（runtime 文件；codex 十轮复审 P1 交叉绑定 + 十一轮复审
    #    P2 语义校验）
    runtime_path = _runtime_path_for(spec_path, subwindow)
    boot_before = rt.get("boot_id_before") if rt else None
    boot_after = read_boot_id()
    reboot_ok = (boot_before is not None and boot_after is not None
                 and boot_before != boot_after)
    if not reboot_ok:
        print("vm_verify: ERROR: boot_id unchanged (%r vs runtime %r) — no "
              "per-subwindow power-cycle evidence; refusing to verify"
              % (boot_after, boot_before))
        _write_result(spec_path, subwindow, _result_fail(
            spec, subwindow, None, boot_after, False,
            "boot_id unchanged (no power-cycle evidence)", None, None))
        return 1
    # ③ 输入不可变
    if tool.sha256_of(spec["reference_manifest"]) != \
            spec["d_stage_manifest_sha256"]:
        print("vm_verify: ERROR: reference manifest mutated (sha mismatch)")
        _write_result(spec_path, subwindow, _result_fail(
            spec, subwindow, None, boot_after, True,
            "manifest mutated", "input immutability failed", None))
        return 1
    # ④ 恢复前状态验证（注入点窗口证据）
    pre_ok, pre_desc = sw_cfg["pre_check"](out, spec)
    if not pre_ok:
        print("vm_verify: ERROR: pre-recovery state does not evidence the "
              "scenario injection window: %s" % pre_desc)
        _write_result(spec_path, subwindow, _result_fail(
            spec, subwindow, None, boot_after, True,
            "pre-recovery injection evidence failed: %s" % pre_desc,
            pre_desc, None))
        return 1
    # ⑤ 恢复 snapshot（无注入环境；stdout/stderr 写入 recovery.log 并
    #    fsync——独立 rc 溯源证据，codex 十二轮复审 P1）
    p = subprocess.run(["python3", _TOOL_PATH, "snapshot",
                        "--d-stage-root", spec["d_stage_root"],
                        "--out-dir", out,
                        "--reference-manifest", spec["reference_manifest"]],
                       capture_output=True, text=True,
                       env=dict(os.environ, LC_ALL="C"))
    rc = p.returncode
    recovery_log = sw_recovery_log(spec, subwindow)
    _write_text_atomic(recovery_log, p.stdout + p.stderr)
    log_cat = _log_rc_category(p.stdout, p.stderr)
    log_ok = (log_cat == rc)
    if not log_ok:
        print("vm_verify: ERROR: recovery rc=%d not corroborated by tool "
              "output (log category=%s)" % (rc, log_cat))
    # ⑥ rc + 终态分支（evidence 分支与 rc=9 事实核验绑定）
    rc_ok = rc in sw_cfg["allowed_rcs"]
    term_ok, branch, term_desc = sw_cfg["terminal"](out, spec, sw_cfg, rc)
    branch_ok = (not rc_ok) or branch == BRANCH_FOR_RC[rc]
    passed = marker_ok and reboot_ok and pre_ok and log_ok and rc_ok \
        and term_ok and branch_ok
    result = {
        "scenario": spec["scenario"],
        "subwindow": subwindow,
        "injection": spec["injection"],
        "expected_final_state": "见 design §5.3 v24 故障注入表对应行",
        "actual_final_state": (
            "marker=%s pre_recovery=%s snapshot_rc=%d log_ok=%s allowed=%s "
            "terminal=%s branch_ok=%s"
            % (marker_desc, pre_desc, rc, log_ok, rc_ok, term_desc,
               branch_ok)),
        "marker_ok": marker_ok,
        "pre_recovery_state": pre_desc,
        "terminal_state": term_desc,
        "snapshot_rc": rc,
        "log_rc_ok": log_ok,
        "rc_allowed": rc_ok,
        "terminal_branch": branch,
        "terminal_branch_ok": branch_ok,
        "reboot_boot_id_changed": reboot_ok,
        "boot_id_before": boot_before,
        "boot_id_after": boot_after,
        "runtime_sha256": tool.sha256_of(runtime_path),
        "recovery_log_sha256": tool.sha256_of(recovery_log),
        "passed": passed,
    }
    _write_result(spec_path, subwindow, result)
    # 只读信任根：mode 变更必须文件元数据 fsync + 目录 fsync（codex 十七轮
    # 复审 P2：断电未持久化无写位会导致合法 result 被 summarize 判为无效）
    _chmod_and_fsync(_result_path_for(spec_path, subwindow), 0o444)
    print("vm_verify: %s sw=%r -> %s" % (
        spec["scenario"], subwindow, "PASS" if passed else "FAIL"))
    print("  %s" % result["actual_final_state"])
    return 0 if passed else 1


def _result_fail(spec, subwindow, rc, boot_after, reboot_ok, reason,
                 pre_desc, term_desc):
    return {
        "scenario": spec["scenario"],
        "subwindow": subwindow,
        "injection": spec["injection"],
        "expected_final_state": "见 design §5.3 v24 故障注入表对应行",
        "actual_final_state": "FAIL: %s" % reason,
        "marker_ok": False,
        "pre_recovery_state": pre_desc,
        "terminal_state": term_desc,
        "snapshot_rc": rc,
        "rc_allowed": False,
        "terminal_branch": None,
        "terminal_branch_ok": False,
        "reboot_boot_id_changed": bool(reboot_ok),
        "boot_id_before": None,
        "boot_id_after": boot_after,
        "runtime_sha256": None,
        "passed": False,
    }


def _write_result(spec_path, subwindow, result):
    result["spec_sha256"] = _spec_sha256(spec_path)
    _write_json_atomic(_result_path_for(spec_path, subwindow), result)


def cmd_vm_summarize(root, out_dir):
    # VM 驱动协议 · 汇总（codex 九轮复审 P2 + 十轮复审 P1）：不信任任何
    # 自报 JSON——对每个场景×子窗口独立复核：
    #   - spec 必须为 prepare 写入的不可变只读文件（mode 无写位）且 sha 匹配
    #   - runtime 文件存在、非 symlink、sha == result.runtime_sha256、
    #     runtime.boot_id_before == result.boot_id_before（交叉绑定）
    #   - 注入 marker（工具写入的 at-<tag>）存在且为 regular file
    #   - boot 证据 + boot 链（前一窗口 result.boot_id_after == 本窗口
    #     runtime.boot_id_before，mark 必在上一 verify 同一 boot）
    #   - rc ∈ allowed_rcs + 重跑终态验证器直接检查当前文件系统 + 分支一致
    # 全部窗口通过才 exit 0，否则 fail-closed exit 1。
    root = tool.realpath_m(root)
    flat = []
    for name in POWER_LOSS_SCENARIOS:
        for sw in vm_subwindows(name):
            flat.append((name, sw))
    entries = []
    missing = []
    prev_boot_after = None
    for name, sw in flat:
        spec_path = _spec_path_for(root, name)
        runtime_path = _runtime_path_for(spec_path, sw)
        result_path = _result_path_for(spec_path, sw)
        label = name + ("" if sw is None else "(%s)" % sw)
        if os.path.islink(spec_path) or not os.path.isfile(spec_path):
            print("  [FAIL] %s :: spec missing or not a regular file" % label)
            missing.append(label)
            continue
        mode = os.stat(spec_path).st_mode & 0o7777
        if mode & 0o222:
            print("  [FAIL] %s :: spec writable (mode %o) — immutable "
                  "trust root violated" % (label, mode))
            missing.append(label)
            continue
        spec = _load_spec(spec_path)
        if spec is None:
            missing.append(label)
            continue
        if os.path.islink(result_path) or not os.path.isfile(result_path):
            print("  [FAIL] %s :: result missing or not a regular file" % label)
            missing.append(label)
            continue
        result_mode = os.stat(result_path).st_mode & 0o7777
        if result_mode & 0o222:
            print("  [FAIL] %s :: result writable (mode %o) — verify 写入的 "
                  "只读信任根被破坏" % (label, result_mode))
            missing.append(label)
            continue
        try:
            with open(result_path) as f:
                res = json.load(f)
        except (OSError, ValueError) as e:
            print("  [FAIL] %s :: bad result JSON: %s" % (label, e))
            missing.append(label)
            continue
        # runtime 交叉绑定（codex 十轮复审 P1 + 十一轮复审 P2 语义校验：
        # scenario/subwindow/sequence/protocol/token 必须与 spec 一致）
        rt = _load_runtime(spec, spec_path, sw)
        rt_ok = rt is not None
        sha_ok = res.get("spec_sha256") == _spec_sha256(spec_path)
        rt_sha_ok = (rt_ok
                     and res.get("runtime_sha256")
                     == tool.sha256_of(runtime_path))
        bind_ok = (rt_ok and res.get("boot_id_before") == rt["boot_id_before"]
                   and bool(res.get("boot_id_before")))
        boot_ok = (res.get("reboot_boot_id_changed") is True
                   and bool(res.get("boot_id_after"))
                   and res.get("boot_id_before") != res.get("boot_id_after"))
        chain_ok = (prev_boot_after is None
                    or (rt_ok and rt["boot_id_before"] == prev_boot_after))
        # marker 证据 + 不可变收据（十一/十二/十三/十四/十五轮复审 P1/P2：
        # 残留/伪造 marker 拒绝；receipt 文件无写位 + 逐字节一致；
        # runtime.inject_receipt 交叉绑定；marker 无写位 + 逐字节全内容）
        tag = vm_subwindow_cfg(name, sw)["inject_tag"]
        inj = sw_inject_dir(spec, sw)
        marker = os.path.join(inj, "at-" + tag)
        receipt_path = os.path.join(inj, "receipt")
        inject_pid = rt.get("inject_pid") if rt else None
        inject_secret = rt.get("inject_secret") if rt else None
        expected_receipt = _receipt_line(tag, inject_pid,
                                         rt["run_token"] if rt else "",
                                         inject_secret)
        receipt_file_ok = (rt_ok and isinstance(inject_pid, int)
                           and isinstance(inject_secret, str)
                           and bool(inject_secret)
                           and _receipt_file_ok(receipt_path,
                                                expected_receipt + "\n"))
        runtime_receipt_ok = (rt_ok
                              and rt.get("inject_receipt") == expected_receipt)
        marker_ok = False
        if runtime_receipt_ok and receipt_file_ok and \
                _marker_matches(marker, tag, rt["run_token"], inject_pid,
                                inject_secret):
            try:
                marker_ok = ((os.stat(marker).st_mode & 0o7777 & 0o222) == 0
                             and _has_immutable_flag(marker))
            except OSError:
                marker_ok = False
        rc = res.get("snapshot_rc")
        cfg = vm_subwindow_cfg(name, sw)
        rc_ok = isinstance(rc, int) and rc in cfg["allowed_rcs"]
        # recovery.log 独立 rc 溯源（codex 十二轮复审 P1：由工具输出完成行
        # 重新判定 rc 类别，不信任 result 自报；log 内容 = stdout+stderr 合并；
        # 十三轮复审 P1：按子窗口分文件）
        log_ok = False
        if res.get("recovery_log_sha256"):
            log_path = sw_recovery_log(spec, sw)
            try:
                log_ok = (tool.sha256_of(log_path)
                          == res["recovery_log_sha256"])
                if log_ok:
                    with open(log_path) as f:
                        log_text = f.read()
                    log_cat = _log_rc_category(log_text, log_text)
                    log_ok = log_cat == rc
            except OSError:
                log_ok = False
        term_ok, branch, term_desc = cfg["terminal"](sw_out_dir(spec, sw),
                                                     spec, cfg, rc)
        branch_ok = (not rc_ok) or branch == BRANCH_FOR_RC[rc]
        ok = (res.get("scenario") == name
              and res.get("subwindow") == sw
              and res.get("passed") is True
              and sha_ok and rt_ok and rt_sha_ok and bind_ok and boot_ok
              and chain_ok and marker_ok and log_ok and rc_ok and term_ok
              and branch_ok)
        entries.append({"name": name, "sw": sw,
                        "injection": spec["injection"],
                        "expected_final_state":
                            res.get("expected_final_state", ""),
                        "actual_final_state":
                            res.get("actual_final_state", "")
                            + " [recheck: sha=%s rt=%s rt_sha=%s bind=%s "
                              "boot=%s chain=%s marker=%s log=%s rc=%r "
                              "rc_ok=%s term=%s branch_ok=%s]"
                            % (sha_ok, rt_ok, rt_sha_ok, bind_ok, boot_ok,
                               chain_ok, marker_ok, log_ok, rc, rc_ok,
                               term_desc, branch_ok),
                        "passed": ok,
                        "_boot_after": res.get("boot_id_after"),
                        "_rt_boot_before": rt.get("boot_id_before")
                        if rt else None})
        if ok and res.get("boot_id_after"):
            prev_boot_after = res["boot_id_after"]
    by_key = {(e["name"], e["sw"]): e for e in entries}
    # 末窗口 boot 现场复核（codex 十二轮复审 P1：summarize 自身读取当前
    # boot_id，必须等于末窗口 verify 的 boot_id_after——协议要求 summarize
    # 在末窗口 verify 的同一 boot 内、任何进一步重启前运行）
    current_boot = read_boot_id()
    last_name, last_sw = flat[-1]
    last_e = by_key.get((last_name, last_sw))
    last_boot_ok = (last_e is not None and last_e["passed"]
                    and bool(current_boot)
                    and last_e["_boot_after"] == current_boot
                    and bool(last_e["_rt_boot_before"])
                    and last_e["_rt_boot_before"] != current_boot)
    if not last_boot_ok:
        print("  [FAIL] last window live boot check failed (current boot=%s; "
              "summarize must run in the same boot as the final verify)"
              % current_boot)
    for name, sw in flat:
        label = name + ("" if sw is None else "(%s)" % sw)
        if label in missing:
            print("  [FAIL] %s :: missing/invalid spec/runtime/result" % label)
        else:
            e = by_key[(name, sw)]
            print("  [%s] %s :: %s" % ("PASS" if e["passed"] else "FAIL",
                                       label, e["actual_final_state"]))
    # genesis 回写：子窗口聚合成单场景条目（genesis schema 每场景一条）
    genesis_entries = []
    for name in POWER_LOSS_SCENARIOS:
        wins = [e for e in entries if e["name"] == name]
        if not wins:
            continue
        agg_actual = " | ".join(
            "%s: %s" % ("" if w["sw"] is None else w["sw"],
                        w["actual_final_state"]) for w in wins)
        genesis_entries.append({
            "name": name,
            "injection": wins[0]["injection"],
            "expected_final_state": wins[0]["expected_final_state"],
            "actual_final_state": agg_actual,
            "passed": all(w["passed"] for w in wins)})
    merge_genesis(genesis_entries, out_dir)
    total_windows = len(flat)
    if missing or any(not e["passed"] for e in entries) or not last_boot_ok:
        print("FAIL-closed: %d window(s) missing/invalid + %d failed + "
              "last-boot-live-check=%s — all %d scenario windows must PASS "
              "before snapshot use in stage-2 release commit" % (
                  len(missing), sum(1 for e in entries if not e["passed"]),
                  last_boot_ok, total_windows))
        return 1
    print("OK: all %d scenario windows PASS (independently re-verified from "
          "immutable spec + tool-written marker + filesystem evidence)"
          % total_windows)
    return 0


def main():
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument("--out-dir")
    ap.add_argument("--keep-tmp", action="store_true")
    ap.add_argument("--subwindow", metavar="X", default=None)
    ap.add_argument("--vm-prepare", metavar="ROOT")
    ap.add_argument("--vm-mark", metavar="SPEC_JSON")
    ap.add_argument("--vm-setup-injection", metavar="SPEC_JSON")
    ap.add_argument("--vm-inject", metavar="SPEC_JSON")
    ap.add_argument("--vm-verify", metavar="SPEC_JSON")
    ap.add_argument("--vm-summarize", metavar="ROOT")
    try:
        args, _ = ap.parse_known_args(sys.argv[1:])
    except SystemExit:
        print("Usage: python3 tests/unit/d-stage-audit-fault-tests.py "
              "[--out-dir DIR] [--keep-tmp]\n"
              "       python3 tests/unit/d-stage-audit-fault-tests.py "
              "--vm-prepare ROOT | --vm-mark SPEC [--subwindow X] | "
              "--vm-setup-injection SPEC [--subwindow X] | "
              "--vm-inject SPEC [--subwindow X] | "
              "--vm-verify SPEC [--subwindow X] | --vm-summarize ROOT")
        sys.exit(2)
    if os.environ.get("LC_ALL", "C") != "C":
        os.environ["LC_ALL"] = "C"
    if args.vm_prepare:
        sys.exit(cmd_vm_prepare(args.vm_prepare))
    if args.vm_mark:
        sys.exit(cmd_vm_mark(args.vm_mark, args.subwindow))
    if args.vm_setup_injection:
        sys.exit(cmd_vm_setup_injection(args.vm_setup_injection,
                                        args.subwindow))
    if args.vm_inject:
        sys.exit(cmd_vm_inject(args.vm_inject, args.subwindow))
    if args.vm_verify:
        sys.exit(cmd_vm_verify(args.vm_verify, args.subwindow))
    if args.vm_summarize:
        sys.exit(cmd_vm_summarize(args.vm_summarize, args.out_dir))
    results, regs, _base, test_root, test_fs = run(keep_tmp=args.keep_tmp)
    total = len(results)
    passed = sum(1 for r in results if r.passed is True)
    failed = [r for r in results if r.passed is False]
    unverified = [r for r in results if r.passed is None]
    reg_failed = [r for r in regs if r.passed is False]
    print("test_root: dir=%s fstype=%s" % (test_root, test_fs))
    print("fault_injection: total=%d passed=%d failed=%d unverified=%d"
          % (total, passed, len(failed), len(unverified)))
    for r in results:
        status = "PASS" if r.passed else ("FAIL" if r.passed is False else "UNVERIFIED")
        print("  [%s] %s :: %s" % (status, r.name, r.actual))
    print("regressions: total=%d failed=%d" % (len(regs), len(reg_failed)))
    for r in regs:
        print("  [%s] %s :: %s" % ("PASS" if r.passed else "FAIL", r.name, r.actual))
    merge_genesis([{"name": r.name, "injection": r.injection,
                    "expected_final_state": r.expected,
                    "actual_final_state": r.actual,
                    "passed": r.passed} for r in results], args.out_dir)
    if failed or unverified or reg_failed:
        # codex 初审 P1：power-loss 场景未验证时不得返回 0——
        # fail-closed：全部 29 场景 + 全部回归通过前，harness 必须非零退出
        print("FAIL-closed: %d scenario(s) failed + %d unverified (pending VM "
              "power-off) + %d regression(s) failed — all 29 scenarios and "
              "regressions must PASS before snapshot use in stage-2 release "
              "commit" % (len(failed), len(unverified), len(reg_failed)))
        sys.exit(1)
    print("OK: all %d scenarios + %d regressions PASS" % (passed, len(regs)))
    sys.exit(0)


if __name__ == "__main__":
    main()
