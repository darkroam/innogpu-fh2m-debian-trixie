#!/usr/bin/env python3
# tools/d-stage-audit-gen.py — O-2 D → D_stage 完整性审计生成器
#
# 实现唯一依据 = docs/planning/030-d-stage-audit.md §五 v24 Python 契约
# （design §5.3 bash 草案仅为算法伪代码；契约与草案冲突时以本契约为准）。
#
# 子命令：
#   gen-manifest  → 生成 9 文件 O-2 输出（主表 + 专表 + genesis + 2 manifest + 4 .sha256）
#   snapshot      → 生成 d-stage-snapshot.tar.zst + .sha256（排他锁 + 持久化事务目录
#                   + 结构化 journal 状态机 + fsync 状态依赖数据屏障 + 回滚协议）
#   reconcile     → 独立 reconcile 入口（对已存在 tarball + manifest 做 4 字段校验）
#
# 退出码在子命令作用域内解释（per codex v11 P2 #4）。本脚本只读 D / D_stage 源树，
# 只写 --out-dir（非保护区）与其中 .txn；绝不写任何保护区路径。

import argparse
import datetime
import fcntl
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time

PROG = "tools/d-stage-audit-gen.py"
EX_CONFIG = 78
TAR_REQUIRED = "1.35"
ZSTD_REQUIRED = "1.5.7"
EPOCH = "@1640995200"  # 2022-01-01 00:00:00 UTC 固定时间戳（design §5.3）
TAR_BASENAME = "d-stage-snapshot.tar.zst"
SHA_BASENAME = "d-stage-snapshot.tar.zst.sha256"
GENESIS_SCHEMA = "2.4"
FAULT_SCHEMA = "13.0"
INJECT_RELEASE_TIMEOUT = 300  # 注入接缝释放等待上限（秒）

PROTECTED_TOP = ("debs", "vendor", "build", "third_party", "migration",
                 "drivers", "baselines")
F0_NS_PATTERNS = (
    re.compile(r"(^|/)fantgpu(/|$)"),
    re.compile(r"(^|/)F0(/|$)"),
    re.compile(r"(^|/)fant_"),
    re.compile(r"(^|/)ftx?_"),
    re.compile(r"(^|/)ftsrv(/|$)"),
)

SYMLINK_CLASSES = [
    "symlink-same-target",
    "symlink-target-changed",
    "symlink-single-side-d",
    "symlink-single-side-dstage",
    "symlink-d2dstage",
    "symlink-dstage2d",
    "symlink-dangling",
    "symlink-broken",
    "symlink-loop",
]


def eprint(msg):
    sys.stderr.write(msg + "\n")


def exit_err(code, msg):
    eprint("ERROR: " + msg)
    sys.exit(code)


# ---------------------------------------------------------------- 通用检查

def check_env():
    if os.environ.get("LC_ALL", "C") != "C":
        exit_err(EX_CONFIG, "LC_ALL must be C (got %r)" % os.environ.get("LC_ALL"))
    os.environ["LC_ALL"] = "C"


def check_tool_versions():
    def _ver(cmd):
        try:
            out = subprocess.run(cmd, capture_output=True, text=True,
                                 env=dict(os.environ, LC_ALL="C"), timeout=30)
        except (OSError, subprocess.TimeoutExpired):
            exit_err(EX_CONFIG, "tool unavailable: %s" % cmd[0])
        # codex 四轮复审 P2：版本命令自身非零退出即失败（任一工具 exit 非零
        # 立即失败契约），不得只看输出内容
        if out.returncode != 0:
            exit_err(EX_CONFIG, "tool %s exit non-zero (%d): %s"
                     % (cmd[0], out.returncode, (out.stdout + out.stderr).strip()[-200:]))
        # GNU tar 输出 "1.35"（两段）；zstd 输出 "1.5.7"（三段）——
        # 允许两段或三段版本号（codex 初审 P1：三段正则漏配 1.35）
        m = re.search(r"[0-9]+\.[0-9]+(?:\.[0-9]+)?", out.stdout + out.stderr)
        return m.group(0) if m else ""
    if _ver(["tar", "--version"]) != TAR_REQUIRED:
        exit_err(EX_CONFIG, "tar version must be exactly %s (cross-machine SHA "
                 "reproducibility requires exact tool version)" % TAR_REQUIRED)
    if _ver(["zstd", "--version"]) != ZSTD_REQUIRED:
        exit_err(EX_CONFIG, "zstd version must be exactly %s" % ZSTD_REQUIRED)
    # jq 前置（dsh 裁定 2026-09-08）：以 dpkg 包版本 ≥ 1.7.1 为准；
    # --version 打印 "jq-1.7" 是 Debian 1.7.1 构建的版本串打印怪癖
    # （dpkg 包版本 1.7.1-6+deb13u3、dpkg -V 完整性通过），接受 1.7.1/1.7
    # 且必须由 dpkg-query 佐证，不得只看 --version 输出
    jq_out = subprocess.run(["jq", "--version"], capture_output=True,
                            text=True, env=dict(os.environ, LC_ALL="C"),
                            timeout=30)
    if jq_out.returncode != 0:
        exit_err(EX_CONFIG, "jq exit non-zero (%d)" % jq_out.returncode)
    m = re.search(r"jq-([0-9]+\.[0-9]+(?:\.[0-9]+)?)",
                  jq_out.stdout + jq_out.stderr)
    jq_v = m.group(1) if m else ""
    dpkg_out = subprocess.run(["dpkg-query", "-W", "-f=${Version}", "jq"],
                              capture_output=True, text=True,
                              env=dict(os.environ, LC_ALL="C"), timeout=30)
    dpkg_v = dpkg_out.stdout.strip() if dpkg_out.returncode == 0 else ""
    if not jq_version_ok(jq_v, dpkg_v):
        exit_err(EX_CONFIG, "jq >= 1.7.1 required (dpkg package version is "
                 "authoritative; got --version %r, dpkg %r)"
                 % (jq_v or "unavailable", dpkg_v or "unavailable"))


JQ_OK_VERSION_STRS = ("1.7.1", "1.7")


def jq_version_ok(version_output, dpkg_version):
    # dsh 裁定（2026-09-08）：jq 前置口径——dpkg 包版本 ≥ 1.7.1 为准，
    # --version 接受 1.7.1/1.7（Debian 1.7.1 构建的版本串怪癖）且必须由
    # dpkg-query 佐证；任一缺失/不符/佐证失败 → False（fail-closed）。
    # 纯函数（fixture 单测直接调用）。
    if not version_output or not dpkg_version:
        return False
    if version_output not in JQ_OK_VERSION_STRS:
        return False
    # dpkg --compare-versions accepts some malformed strings with only a
    # warning (for example, "garbage"). Reject those before trusting its rc.
    if not re.fullmatch(r"[0-9]+(?::[0-9]+)?[0-9A-Za-z.+~-]*", dpkg_version):
        return False
    try:
        chk = subprocess.run(["dpkg", "--compare-versions", dpkg_version,
                              "ge", "1.7.1"], capture_output=True, text=True,
                             env=dict(os.environ, LC_ALL="C"), timeout=30)
    except (OSError, subprocess.TimeoutExpired):
        return False
    return chk.returncode == 0


def realpath_m(path):
    # GNU realpath -m 等价：解析存在的 symlink，其余部分词法规范化；不要求存在
    return os.path.normpath(os.path.realpath(os.path.abspath(path)))


def is_within(path_abs, root_abs):
    return path_abs == root_abs or path_abs.startswith(root_abs + "/")


def public_evidence_path(path):
    # Evidence is tracked and must not capture a developer's home directory.
    # Keep an absolute, stable logical path for repository inputs instead.
    absolute = realpath_m(path)
    repo_root = realpath_m(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if is_within(absolute, repo_root):
        relative = os.path.relpath(absolute, repo_root)
        return "/repo/innogpu-fh2m-debian-trixie" + (
            "" if relative == "." else "/" + relative)
    return absolute


def check_out_dir(out_dir):
    # codex 初审 P1：保护区锚定仓库根（脚本位于 tools/ 时为其上级目录），
    # 不再依赖调用方 cwd；同时保留 cwd 锚定检查（仓库外运行时兜底）
    out = realpath_m(out_dir)
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    anchors = [realpath_m(os.path.join(repo_root, top)) for top in PROTECTED_TOP]
    anchors += [realpath_m(top) for top in PROTECTED_TOP]
    for anchor in anchors:
        if is_within(out, anchor):
            exit_err(3, "protected path violation: out-dir inside %s" % anchor)
    if is_within(out, realpath_m(os.path.join(repo_root, "patches"))):
        exit_err(3, "protected path violation: out-dir inside patches/")


def check_no_f0_ref(root):
    for dirpath, _dirnames, filenames in os.walk(root):
        for name in filenames + [os.path.basename(dirpath)]:
            rel = os.path.relpath(os.path.join(dirpath, name), root)
            for pat in F0_NS_PATTERNS:
                if pat.search(rel) or pat.search(name):
                    exit_err(4, "F0 reference violation: %s" % rel)


# ---------------------------------------------------------------- fsync helpers

def fsync_file(path):
    try:
        fd = os.open(path, os.O_RDONLY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
    except OSError as e:
        exit_err(5, "fsync_file(%s) failed: %s" % (path, e))


def fsync_dir(path):
    try:
        dfd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(dfd)
        finally:
            os.close(dfd)
    except OSError as e:
        exit_err(5, "fsync_dir(%s) failed: %s" % (path, e))


def inject_pause(tag):
    # codex 十轮复审 P1：测试注入接缝——仅当 INNOGPU_DSTAGE_INJECT_DIR 与
    # INNOGPU_DSTAGE_INJECT_TAG 同时设置且 tag 匹配时生效：写 at-<tag>
    # marker（O_CREAT|O_EXCL——marker 只能由本工具进程创建，注入前被清理
    # 过的残留或外部预置 marker 会使本进程 exit 5 fail-closed；内容含
    # tag/pid/每轮唯一 token/每轮一次性 secret + 文件/目录 fsync，断电后仍
    # 可作注入点执行证据；token+secret 用于拒绝上一轮残留与外部伪造 marker，
    # codex 十一/十三轮复审 P1），随后阻塞等待 release-<tag> 出现（超时放行，
    # 注入未命中由 harness 恢复前状态验证 fail-closed 检出）。未设置环境
    # 变量时零开销直通，生产与正常测试行为不变。
    inject_dir = os.environ.get("INNOGPU_DSTAGE_INJECT_DIR")
    inject_tag = os.environ.get("INNOGPU_DSTAGE_INJECT_TAG")
    if not inject_dir or not inject_tag or tag != inject_tag:
        return
    token = os.environ.get("INNOGPU_DSTAGE_INJECT_TOKEN", "")
    secret = os.environ.get("INNOGPU_DSTAGE_INJECT_SECRET", "")
    marker = os.path.join(inject_dir, "at-" + tag)
    try:
        fd = os.open(marker, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644)
        with os.fdopen(fd, "w") as f:
            f.write("tag=%s\npid=%d\ntoken=%s\nsecret=%s\n"
                    % (tag, os.getpid(), token, secret))
            f.flush()
            os.fsync(f.fileno())
        # marker 写后置只读（codex 十五轮复审 P1：不可变收据载体，verify/
        # summarize 校验无写位 + 内容与管道收据逐字节一致）；mode 变更必须
        # fsync 文件元数据 + 目录（codex 十六轮复审 P2：断电不得丢失无写位，
        # 否则真实注入被判失败）
        os.chmod(marker, 0o444)
        mfd = os.open(marker, os.O_RDONLY)
        try:
            os.fsync(mfd)
        finally:
            os.close(mfd)
        fsync_dir(inject_dir)
    except FileExistsError:
        exit_err(5, "inject marker already exists (stale residue or external "
                    "pre-creation race): %s" % marker)
    except OSError as e:
        exit_err(5, "inject marker write failed (%s): %s" % (marker, e))
    # codex 十四轮复审 P2：受保护 IPC 收据——marker 文件位于驱动可写目录，
    # 创建后可被 unlink/替换；stdout 是 harness 私有管道（驱动无法写入），
    # 收据行经管道到达 harness 即为工具侧来源认证。在 marker 持久化之后、
    # 阻塞之前输出。
    sys.stdout.write("INJECT_MARKER tag=%s pid=%d token=%s secret=%s\n"
                     % (tag, os.getpid(), token, secret))
    sys.stdout.flush()
    release = os.path.join(inject_dir, "release-" + tag)
    deadline = time.time() + INJECT_RELEASE_TIMEOUT
    while time.time() < deadline:
        if os.path.exists(release):
            return
        time.sleep(0.01)


def atomic_write(path, data, mode=0o644):
    d = os.path.dirname(path) or "."
    tmp = os.path.join(d, ".tmp.%d" % os.getpid())
    try:
        with open(tmp, "w", newline="\n") as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        os.chmod(tmp, mode)
        os.replace(tmp, path)
        fsync_dir(d)
    except OSError as e:
        exit_err(5, "atomic_write(%s) failed: %s" % (path, e))


def persist_journal(journal, fields):
    # tmp 写入 + fsync_file(tmp) + os.rename + fsync_dir(parent)；严禁全局 sync
    lines = ["schema=1"]
    lines += ["%s=%s" % (k, v) for k, v in fields.items()]
    data = "\n".join(lines) + "\n"
    d = os.path.dirname(journal)
    tmp = os.path.join(d, ".journal.tmp")
    try:
        with open(tmp, "w") as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, journal)
        fsync_dir(d)
    except OSError as e:
        exit_err(5, "persist_journal(%s) failed: %s（journal 保持前一持久状态）"
                 % (journal, e))


# ---------------------------------------------------------------- sidecar 校验

SIDECAR_RE = re.compile(r"^[0-9a-f]{64}  d-stage-snapshot\.tar\.zst$")


def sidecar_is_strict(path, expected_sha):
    # 恰好一行 + 精确格式 + hash 字段 == expected_sha（4a 严格校验与幂等 no-op 共用）
    # codex 三轮复审 P1：symlink 不得跟随解析
    # codex 六轮复审 P1：严格"恰好一行"——wc -l == 1 语义（恰一个尾部换行），
    # 附加空行/缺失换行/多行一律拒绝
    if os.path.islink(path):
        return False
    try:
        with open(path, "rb") as f:
            raw = f.read()
    except OSError:
        return False
    if not raw.endswith(b"\n"):
        return False
    body = raw[:-1]
    if b"\n" in body:
        return False
    try:
        line = body.decode("ascii")
    except UnicodeDecodeError:
        return False
    m = SIDECAR_RE.match(line)
    if not m:
        return False
    return line[:64] == expected_sha


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# ---------------------------------------------------------------- classify_out_pair

def classify_out_pair(out_dir, new_tar_sha256, old_tar_sha256, old_sha_sha256):
    # 共享分类函数（v16 per codex v15 P1 #1 + codex v16 P1 #1）：即时回滚与
    # 启动恢复共用；sidecar 单行 + 精确格式 + hash 字段，任一不满足 → unknown。
    # codex 三轮复审 P1：制品必须是 regular file——symlink（含悬空）一律
    # unknown，绝不跟随解析、绝不误判 absent。
    tar_path = os.path.join(out_dir, TAR_BASENAME)
    sha_path = os.path.join(out_dir, SHA_BASENAME)
    if not os.path.lexists(tar_path):
        tar_state = "absent"
    elif os.path.islink(tar_path) or not os.path.isfile(tar_path):
        tar_state = "unknown"
    else:
        got = sha256_of(tar_path)
        if got == new_tar_sha256:
            tar_state = "new"
        elif got == old_tar_sha256:
            tar_state = "old"
        else:
            tar_state = "unknown"
    if not os.path.lexists(sha_path):
        sha_state = "absent"
    elif os.path.islink(sha_path) or not os.path.isfile(sha_path):
        sha_state = "unknown"
    else:
        if sidecar_is_strict(sha_path, new_tar_sha256):
            sha_state = "new"
        elif sidecar_is_strict(sha_path, old_tar_sha256):
            sha_state = "old"
        else:
            sha_state = "unknown"
    return tar_state, sha_state


def rolling_back_mid_state_ok(tar_state, sha_state):
    # rolling_back 合法中途态白名单（tuple/set 成员判断；禁止照抄 shell case 未转义 | 语法）
    return (tar_state, sha_state) in {
        ("new", "new"),
        ("absent", "new"),
        ("absent", "absent"),
        ("old", "absent"),
        ("old", "old"),
    }


# ---------------------------------------------------------------- journal

def parse_journal(journal_path):
    fields = {}
    try:
        with open(journal_path, "r") as f:
            for line in f:
                line = line.rstrip("\n")
                if not line:
                    continue
                if "=" not in line:
                    exit_err(9, "journal line malformed (no '='): %r" % line)
                k, v = line.split("=", 1)
                fields[k] = v
    except OSError as e:
        exit_err(9, "journal unreadable: %s" % e)
    if fields.get("schema") != "1":
        exit_err(9, "journal schema != 1")
    state = fields.get("state")
    if state not in ("staged", "backed_up", "tarball_committed",
                     "sha_committed", "verified", "rolling_back"):
        exit_err(9, "journal state unknown: %r" % state)
    for k in ("staging_dir", "new_tar_sha256", "new_tar_size"):
        if not fields.get(k):
            exit_err(9, "journal missing field: %s" % k)
    if not re.fullmatch(r"[0-9a-f]{64}", fields["new_tar_sha256"]):
        exit_err(9, "journal new_tar_sha256 not 64 hex")
    if not re.fullmatch(r"[0-9]+", fields["new_tar_size"]):
        exit_err(9, "journal new_tar_size not numeric")
    # codex 初审 P1：old_tar/old_sha 与旧对指纹必须为必填字段，缺失即 exit 9
    # （否则 facts_check 索引时 KeyError/exit 1，违反 fail-closed 契约）
    for k in ("old_tar", "old_sha"):
        if fields.get(k) not in ("present", "absent"):
            exit_err(9, "journal missing/invalid field: %s" % k)
    # codex 复审 P1：present/absent 与指纹语义绑定——
    # present → 64 hex；absent → none（禁止 present+none 绕过内容校验）
    for plan_k, sha_k in (("old_tar", "old_tar_sha256"),
                          ("old_sha", "old_sha_sha256")):
        # codex 四轮复审 P1：字段缺失必须 exit 9，不得 KeyError/exit 1
        v = fields.get(sha_k)
        if v is None:
            exit_err(9, "journal missing field: %s" % sha_k)
        if fields[plan_k] == "present":
            if not re.fullmatch(r"[0-9a-f]{64}", v):
                exit_err(9, "journal %s must be 64 hex when %s=present"
                         % (sha_k, plan_k))
        else:
            if v != "none":
                exit_err(9, "journal %s must be none when %s=absent"
                         % (sha_k, plan_k))
    return fields


def validate_staging_dir(staging_dir, txn_dir):
    # 绝对路径 → 词法组件 symlink 检查（不 resolve）→ realpath -m canonicalize
    # → is_within(.txn/staging) → 非 symlink → 目录
    if not os.path.isabs(staging_dir):
        exit_err(9, "staging_dir not absolute: %r" % staging_dir)
    # codex 五轮复审 P1：必须从文件系统根 "/" 开始逐组件检查 staging_dir
    # **真实词法路径**（从 txn_dir 拼接会把绝对路径组件接成虚构路径，检查落空）；
    # 含 .. 组件的原始路径直接拒绝。
    cur = "/"
    for part in staging_dir.split("/"):
        if part in ("", "."):
            continue
        if part == "..":
            exit_err(9, "staging_dir contains '..' component: %r" % staging_dir)
        # codex 六轮复审 P1：首个组件必须保留前导 "/"（cur="/tmp"），
        # 否则退化为 cwd 相对路径、检查落空
        cur = ("/" + part) if cur == "/" else cur + "/" + part
        if os.path.islink(cur):
            exit_err(9, "staging path component is symlink: %s" % cur)
    can = realpath_m(staging_dir)
    staging_root = realpath_m(os.path.join(txn_dir, "staging"))
    if not is_within(can, staging_root):
        exit_err(9, "staging_dir escapes .txn/staging: %r" % can)
    if os.path.islink(staging_dir):
        exit_err(9, "staging_dir is symlink: %r" % staging_dir)
    if not os.path.isdir(staging_dir):
        exit_err(9, "staging_dir not a directory: %r" % staging_dir)
    return can


def read_journal(txn_dir):
    journal = os.path.join(txn_dir, "journal")
    # codex 四轮复审 P1：journal 为 symlink（含悬空）一律按损坏证据 exit 9，
    # 不得跟随读取、不得当作缺失进入残留清理路径
    if os.path.islink(journal):
        exit_err(9, "journal is a symlink (treat as corrupt evidence)")
    # codex 六轮复审 P2：journal 必须为 regular file（FIFO 等会阻塞读取）
    if os.path.lexists(journal) and not os.path.isfile(journal):
        exit_err(9, "journal is not a regular file (treat as corrupt evidence)")
    if not os.path.exists(journal):
        # codex 六轮复审 P1：lexists 检测悬空 old.* symlink（损坏证据不得
        # 随 .txn 被静默删除）
        if os.path.lexists(os.path.join(txn_dir, "old.tar.zst")) or \
           os.path.lexists(os.path.join(txn_dir, "old.sha256")):
            exit_err(9, ".txn invariant violation: old.* exists but journal missing")
        # v21 per codex v20 P1 #2：无 journal 恢复一律继续全新路径
        return None
    return parse_journal(journal)


# ---------------------------------------------------------------- lock

def acquire_snapshot_lock(out_dir):
    # fcntl.flock LOCK_EX|LOCK_NB 于 ${out-dir}/.snapshot.lock（锁文件永不删除）；
    # 锁在参数解析之后、任何文件系统变更之前获取；争用失败 exit 6，
    # 打开/加锁系统错误 exit 5。
    lock_path = os.path.join(out_dir, ".snapshot.lock")
    try:
        fd = os.open(lock_path, os.O_RDWR | os.O_CREAT, 0o644)
    except OSError as e:
        exit_err(5, "cannot open lock file %s: %s" % (lock_path, e))
    try:
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        os.close(fd)
        exit_err(6, "OUT_DIR exclusive lock held by another snapshot process "
                    "(.snapshot.lock)")
    return fd


# ---------------------------------------------------------------- gen-manifest

def walk_tree(root):
    # (file_type, rel, symlink_target, sha256) 全集；NUL 无关（python 字符串安全）
    # codex 初审 P1：记录源树根目录条目（rel=""）；symlink 目录记 l 而非 d。
    entries = [("d", "", "", "")]
    # codex 复审 P1：os.walk 必须带 onerror——不可读子目录不得被静默遗漏
    def _onerror(e):
        exit_err(5, "walk error: %s" % e)

    for dirpath, dirnames, filenames in os.walk(root, followlinks=False,
                                                onerror=_onerror):
        dirnames.sort()
        filenames.sort()
        for name in dirnames:
            p = os.path.join(dirpath, name)
            rel = os.path.relpath(p, root).replace(os.sep, "/")
            if os.path.islink(p):
                entries.append(("l", rel, os.readlink(p), ""))
            else:
                entries.append(("d", rel, "", ""))
        for name in filenames:
            p = os.path.join(dirpath, name)
            rel = os.path.relpath(p, root).replace(os.sep, "/")
            if os.path.islink(p):
                tgt = os.readlink(p)
                entries.append(("l", rel, tgt, ""))
            else:
                entries.append(("f", rel, "", sha256_of(p)))
    entries.sort(key=lambda e: (e[0], e[1]))
    return entries


def build_manifest_tsv(root):
    lines = []
    for ft, rel, tgt, sha in walk_tree(root):
        for field in (rel, tgt):
            for ch in ("\t", "\r", "\n", "\x00"):
                if ch in field:
                    exit_err(1, "schema violation: control char in path field %r" % rel)
        lines.append("%s\t%s\t%s\t%s\n" % (ft, rel, tgt, sha))
    return "".join(lines)


def resolve_link(link_path):
    # 解析 symlink 链；返回 (final_abs, is_loop, is_dangling, is_broken)
    seen = set()
    cur = link_path
    for _ in range(64):
        cur_abs = os.path.abspath(cur)
        if cur_abs in seen:
            return cur_abs, True, False, False
        seen.add(cur_abs)
        if not os.path.islink(cur):
            return realpath_m(cur), False, False, False
        try:
            nxt = os.readlink(cur)
        except OSError:
            return realpath_m(cur), False, False, True
        cur = os.path.join(os.path.dirname(cur), nxt)
    return realpath_m(cur), True, False, False


def classify_symlink_pair(d_root, dstage_root, d_entry, dstage_entry):
    # d_entry/dstage_entry: None 或 (ft, rel, tgt, sha)
    if d_entry is not None and dstage_entry is None:
        return "symlink-single-side-d"
    if d_entry is None and dstage_entry is not None:
        return "symlink-single-side-dstage"
    d_rel, dstage_rel = d_entry[1], dstage_entry[1]
    d_link = os.path.join(d_root, d_rel)
    ds_link = os.path.join(dstage_root, dstage_rel)
    tgt_d = d_entry[2]
    tgt_ds = dstage_entry[2]
    resolved_d, loop_d, _dangling, _broken = resolve_link(d_link)
    resolved_ds, loop_ds, _dangling, _broken = resolve_link(ds_link)
    if loop_d or loop_ds:
        return "symlink-loop"
    # dangling：解析后 target 路径不存在（realpath -m 不要求存在；用最终链尾判断）
    if not os.path.lexists(resolved_d):
        return "symlink-dangling"
    if not os.path.lexists(resolved_ds):
        return "symlink-dangling"
    # broken：target 存在但非 file/dir/symlink
    for r in (resolved_d, resolved_ds):
        if os.path.lexists(r) and not (os.path.isfile(r) or os.path.isdir(r) or os.path.islink(r)):
            return "symlink-broken"
    target_d_abs = canonicalize_target(tgt_d, d_link)
    target_dstage_abs = canonicalize_target(tgt_ds, ds_link)
    if is_within(target_d_abs, realpath_m(dstage_root)):
        return "symlink-d2dstage"
    if is_within(target_dstage_abs, realpath_m(d_root)):
        return "symlink-dstage2d"
    if tgt_d == tgt_ds:
        return "symlink-same-target"
    return "symlink-target-changed"


def canonicalize_target(target, symlink_path):
    # v11 per codex v10 P1 #4：绝对/相对分支 + realpath -m canonicalize
    symlink_dir = realpath_m(os.path.dirname(symlink_path))
    joined = target if target.startswith("/") else "%s/%s" % (symlink_dir, target)
    return realpath_m(joined)


def build_main_table(d_root, dstage_root):
    d_entries = {e[1]: e for e in walk_tree(d_root)}
    ds_entries = {e[1]: e for e in walk_tree(dstage_root)}
    rows = []
    for rel in sorted(set(d_entries) | set(ds_entries)):
        de = d_entries.get(rel)
        dse = ds_entries.get(rel)
        # codex 复审 P1：任一端为 symlink 且另一端为 regular file/目录
        # 的类型变化必须 differs，不得进入 symlink 9 类分类
        if (de is not None and de[0] == "l") != (dse is not None and dse[0] == "l") \
                and de is not None and dse is not None:
            rows.append(["differs", de[1], dse[1], "UNASSIGNED", "OK"])
            continue
        if de is not None and de[0] == "l" or dse is not None and dse[0] == "l":
            cls = classify_symlink_pair(d_root, dstage_root, de, dse)
            bc = "SYM-N-A"
            d_rel = de[1] if de else ""
            ds_rel = dse[1] if dse else ""
            rows.append([cls, d_rel, ds_rel, bc, "OK"])
            continue
        if de is not None and dse is not None:
            # codex 初审 P1：文件类型变化（file↔dir 等）必须 differs，
            # 不得标 identical
            if de[0] != dse[0]:
                cls = "differs"
            elif de[0] == "f" and de[3] != dse[3]:
                cls = "differs"
            else:
                cls = "identical"
        elif de is not None:
            cls = "D-only"
        else:
            cls = "D_stage-only"
        rows.append([cls, de[1] if de else "", dse[1] if dse else "", "UNASSIGNED", "OK"])
    order = {c: i for i, c in enumerate(
        ["identical", "differs", "D-only", "D_stage-only"] + SYMLINK_CLASSES)}
    rows.sort(key=lambda r: (order.get(r[0], 99), r[1], r[2]))
    lines = []
    for i, r in enumerate(rows, 1):
        lines.append("%d\t%s\t%s\t%s\t%s\t%s\n" % (i, r[1], r[2], r[0], r[3], r[4]))
    return "".join(lines), rows


def build_symlink_table(d_root, dstage_root):
    d_entries = {e[1]: e for e in walk_tree(d_root)}
    ds_entries = {e[1]: e for e in walk_tree(dstage_root)}
    rows = []
    for rel in sorted(set(d_entries) | set(ds_entries)):
        de = d_entries.get(rel)
        dse = ds_entries.get(rel)
        if not ((de and de[0] == "l") or (dse and dse[0] == "l")):
            continue
        # codex 复审 P1：symlink↔regular 类型变化不进入专表（主表 differs）
        if (de and de[0] == "l") != (dse and dse[0] == "l") \
                and de is not None and dse is not None:
            continue
        cls = classify_symlink_pair(d_root, dstage_root, de, dse)
        rows.append([cls,
                     de[1] if de else "",
                     dse[1] if dse else "",
                     de[2] if de else "",
                     dse[2] if dse else "",
                     "SYM-N-A", "OK"])
    order = {c: i for i, c in enumerate(SYMLINK_CLASSES)}
    rows.sort(key=lambda r: (order.get(r[0], 99), r[1], r[2]))
    lines = []
    for r in rows:
        lines.append("%s\t%s\t%s\t%s\t%s\t%s\t%s\n" % tuple(r))
    return "".join(lines), rows


def check_nf(content, expected, what):
    bad = sum(1 for ln in content.split("\n") if ln != "" and ln.count("\t") + 1 != expected)
    if bad:
        exit_err(1, "schema violation: %d row(s) with NF != %d in %s" % (bad, expected, what))


def gen_full_audit(argv):
    # gen-manifest：--label D → 仅生成 D manifest + .sha256；
    # --label D_stage → D_stage manifest + .sha256 + 主表 + 专表 + genesis
    # （v7 per codex v6 P1 #3：两次独立调用，label 标识当前调用生成哪个 manifest）
    p = argparse.ArgumentParser(prog=PROG + " gen-manifest", add_help=False)
    p.add_argument("--d-root", required=True)
    p.add_argument("--d-stage-root", required=True)
    p.add_argument("--out-dir", required=True)
    p.add_argument("--label", required=True, choices=["D", "D_stage"])
    try:
        args = p.parse_args(argv)
    except SystemExit:
        exit_err(EX_CONFIG, "gen-manifest: unknown/malformed arguments")
    check_env()
    check_tool_versions()
    check_out_dir(args.out_dir)
    os.makedirs(args.out_dir, exist_ok=True)
    if not os.path.isdir(args.d_root):
        exit_err(5, "input root missing: %s" % args.d_root)
    check_no_f0_ref(args.d_root)
    if args.label == "D":
        name = "D"
        root = args.d_root
    else:
        if not os.path.isdir(args.d_stage_root):
            exit_err(5, "input root missing: %s" % args.d_stage_root)
        check_no_f0_ref(args.d_stage_root)
        name = "D_stage"
        root = args.d_stage_root
    # 对应 label 的 manifest + sha256（确定性双跑）
    a = build_manifest_tsv(root)
    b = build_manifest_tsv(root)
    if a != b:
        exit_err(2, "determinism violation: two runs differ")
    out_path = os.path.join(args.out_dir, "d-stage-audit.%s.manifest.tsv" % name)
    atomic_write(out_path, a)
    atomic_write(out_path + ".sha256",
                 "%s  %s\n" % (hashlib.sha256(a.encode()).hexdigest(),
                               os.path.basename(out_path)))
    if args.label != "D_stage":
        return 0
    # D_stage 调用额外生成主表 + 专表 + genesis（9 文件输出在两次调用后齐备）
    main_a, main_rows = build_main_table(args.d_root, args.d_stage_root)
    main_b, _ = build_main_table(args.d_root, args.d_stage_root)
    if main_a != main_b:
        exit_err(2, "determinism violation: main table two runs differ")
    sym_a, sym_rows = build_symlink_table(args.d_root, args.d_stage_root)
    sym_b, _ = build_symlink_table(args.d_root, args.d_stage_root)
    if sym_a != sym_b:
        exit_err(2, "determinism violation: symlink table two runs differ")
    check_nf(main_a, 6, "d-stage-audit.tsv")
    check_nf(sym_a, 7, "d-stage-audit.symlink.tsv")
    # symlink 闭合：主表 symlink 分类行数 == 专表行数（exit 7）
    main_sym_cnt = sum(1 for r in main_rows if r[0] in SYMLINK_CLASSES)
    if main_sym_cnt != len(sym_rows):
        exit_err(7, "symlink closure failed: main=%d vs symlink table=%d"
                 % (main_sym_cnt, len(sym_rows)))
    # SYM-N-A reconcile：symlink 行 bc ∉ {SYM-N-A, UNASSIGNED} → exit 8
    for r in main_rows:
        if r[0] in SYMLINK_CLASSES and r[3] not in ("SYM-N-A", "UNASSIGNED"):
            exit_err(8, "SYM-N-A reconcile failed: symlink row with bc=%r" % r[3])
    main_path = os.path.join(args.out_dir, "d-stage-audit.tsv")
    sym_path = os.path.join(args.out_dir, "d-stage-audit.symlink.tsv")
    atomic_write(main_path, main_a)
    atomic_write(main_path + ".sha256",
                 "%s  %s\n" % (hashlib.sha256(main_a.encode()).hexdigest(),
                               os.path.basename(main_path)))
    atomic_write(sym_path, sym_a)
    atomic_write(sym_path + ".sha256",
                 "%s  %s\n" % (hashlib.sha256(sym_a.encode()).hexdigest(),
                               os.path.basename(sym_path)))
    # genesis.json（运行时元数据，不计 SHA 一致判定；§11.3 v24 完整 schema，
    # codex 初审 P1 修订）
    started_at = datetime.datetime.now(datetime.timezone.utc).isoformat()

    def tool_ver(cmd):
        # codex 五轮复审 P2：genesis 工具版本探测同样执行"任一工具 exit 非零
        # 立即失败"契约（与 check_tool_versions 一致）
        try:
            out = subprocess.run(cmd, capture_output=True, text=True,
                                 env=dict(os.environ, LC_ALL="C"), timeout=30)
        except (OSError, subprocess.TimeoutExpired):
            exit_err(EX_CONFIG, "tool unavailable: %s" % cmd[0])
        if out.returncode != 0:
            exit_err(EX_CONFIG, "tool %s exit non-zero (%d)" % (cmd[0], out.returncode))
        m = re.search(r"[0-9]+\.[0-9]+(?:\.[0-9]+)?", out.stdout + out.stderr)
        if not m:
            exit_err(EX_CONFIG, "tool %s version unparsable" % cmd[0])
        return m.group(0)

    def which(cmd):
        p = shutil.which(cmd)
        return p if p else "unavailable"

    tool_versions = {
        "diffutils": tool_ver(["diff", "--version"]),
        "git": tool_ver(["git", "--version"]),
        "python3": sys.version.split()[0],
        "coreutils": tool_ver(["sha256sum", "--version"]),
        "findutils": tool_ver(["find", "--version"]),
        "tar": TAR_REQUIRED,
        "zstd": ZSTD_REQUIRED,
        "jq": tool_ver(["jq", "--version"]),
    }
    jq_dpkg = subprocess.run(["dpkg-query", "-W", "-f=${Version}", "jq"],
                              capture_output=True, text=True,
                              env=dict(os.environ, LC_ALL="C"), timeout=30)
    if jq_dpkg.returncode != 0 or not jq_dpkg.stdout.strip():
        exit_err(EX_CONFIG, "dpkg-query could not report jq package version")
    tool_versions["jq_dpkg_version"] = jq_dpkg.stdout.strip()
    snapshot_tool_versions = {
        "tar": TAR_REQUIRED,
        "zstd": ZSTD_REQUIRED,
        "tar_path": which("tar"),
        "zstd_path": which("zstd"),
        "build_environment": "host=REDACTED system=%s release=%s "
                             "version=%s machine=%s LC_ALL=C" % (
                                 platform.system(), platform.release(),
                                 platform.version(), platform.machine()),
        "notes": "精确锁定 tar 1.35 + zstd 1.5.7（本机实测）；如需在非本机"
                 "环境复现 SHA，必须使用完全相同的 tar + zstd + jq 版本 + "
                 "相同构建环境（locale / libc / 容器）。",
    }
    fault_tests = {
        "schema_version": FAULT_SCHEMA,
        "snapshot_subcommand": {},
        "notes": "共 29 场景（17 单元 + 12 power-loss）。power-loss 场景注入"
                 "方式 = 强制断电（VM poweroff / 拔盘后重启），**不得用 kill -9"
                 " 代替**；单元场景由 tests/unit/d-stage-audit-fault-tests.py"
                 "执行并回写 actual_final_state / passed。未经全部场景通过的"
                 "snapshot 禁止在阶段二 release commit 中使用。",
    }
    scenario_defs = [
        ("lock_contention",
         "process A holds .snapshot.lock; start process B concurrently",
         "B acquire_snapshot_lock fails → exit 6 + clear error; B exits BEFORE "
         "creating any temp file / .txn / staging modification; A completes "
         "normally"),
        ("crash_after_staged_persist",
         "kill -9 after persist_journal(state=staged), before backup old "
         "tarball mv（等价态构造）",
         "next run parses journal=staged → validates staging_dir constraints + "
         "staging fingerprint → completes backup → backed_up → commit new pair "
         "→ verified → cleanup; final OUT_DIR = new pair"),
        ("crash_between_backup_mvs",
         "kill -9 after old tarball mv into .txn, before old sha256 mv"
         "（等价态构造）",
         "journal=staged + facts (old tar in .txn, old sha in OUT_DIR) → "
         "recovery completes remaining backup mv (guarded idempotent); final "
         "OUT_DIR = new pair"),
        ("crash_after_backed_up_persist",
         "kill -9 after persist_journal(state=backed_up), before new tarball mv"
         "（等价态构造）",
         "journal=backed_up → recovery commits new tarball + new sha256 → "
         "self-check → cleanup; final OUT_DIR = new pair"),
        ("crash_between_tarball_mv_and_journal",
         "kill -9 after new tarball mv to OUT_DIR, before "
         "persist_journal(state=tarball_committed)（等价态构造）",
         "journal=backed_up + facts (new tar in OUT_DIR) → recovery skips "
         "tarball commit, commits sha256 directly; final OUT_DIR = new pair"),
        ("selfcheck_fail",
         "write wrong sha256 content (same file size)",
         "4e sha256sum -c FAIL → persist rolling_back → classify: tampered "
         "sidecar → unknown → exit 9 keeping journal and scene (fail-closed; "
         "pre-v17 'auto rollback to old pair' expectation retired)"),
        ("out_dir_readonly",
         "chmod -w OUT_DIR before snapshot",
         "lock/txn open fails → exit 5; OUT_DIR = old pair untouched; re-run "
         "after fixing perms resumes"),
        ("journal_write_fail_at_staged",
         "make .txn read-only before persist_journal(state=staged)",
         "persist fails → exit 5; old pair NEVER moved (ordering guarantee); "
         "no data loss"),
        ("journal_corrupt",
         "echo garbage > .txn/journal before snapshot",
         "startup parse fails → exit 9 + clear error; dsh manual cleanup"),
        ("staging_dir_path_traversal",
         "rewrite journal staging_dir to point outside .txn/staging",
         "path constraint validation fails → exit 9; no mv performed"),
        ("old_pair_incomplete",
         "pre-place OUT_DIR with only d-stage-snapshot.tar.zst (no .sha256)",
         "4a completeness fail-closed → exit 9; no journal written, no files "
         "moved"),
        ("old_pair_restore_incomplete",
         "journal old_tar=present but old tarball absent in both .txn and "
         "OUT_DIR, then re-run",
         "three-way facts consistency check fails → exit 9; never silently "
         "restored to half-pair"),
        ("unknown_content_in_out_dir",
         "replace OUT_DIR tarball with a third-party file",
         "fingerprint classification = unknown → exit 9; unknown content never "
         "misjudged/deleted"),
        ("immediate_rollback_sidecar_unknown",
         "before in-process rollback, rewrite OUT_DIR .sha256 to a "
         "format-legal single line whose hash field != new_tar_sha256",
         "shared classify_out_pair classifies sidecar unknown → exit 9; "
         "sidecar deleted ONLY when precisely identified as new; journal and "
         "files preserved"),
        ("verified_half_pair",
         "manually craft journal=verified with OUT_DIR containing only the new "
         "tarball (no .sha256)",
         "state-specific invariant fails → exit 9; NO evidence cleanup"),
        ("old_pair_inconsistent",
         "pre-place OUT_DIR old pair whose tarball and .sha256 contents "
         "mismatch each other",
         "4a self-consistency check fails → exit 9; corruption evidence not "
         "silently discarded"),
        ("noop_duplicate_sidecar",
         "pre-place OUT_DIR pair whose tarball == new snapshot content, but "
         ".sha256 contains two duplicate format-legal lines",
         "idempotent no-op reuses strict sidecar validation → exit 9; "
         "malformed/duplicate sidecar NEVER accepted by no-op"),
    ]
    power_loss_defs = [
        ("power_loss_after_staged_persist",
         "poweroff after durable journal=staged and staging fsync",
         "journal=staged + staging durable → recovery completes backup → commit → verified → cleanup; OUT_DIR = new pair"),
        ("power_loss_before_staged_persist",
         "poweroff before persist_journal(state=staged)",
         "journal absent → discard .txn residue → fresh path (or 4a idempotent no-op); old pair untouched"),
        ("power_loss_after_backed_up_persist",
         "poweroff after durable journal=backed_up and old pair fsync",
         "journal=backed_up + .txn/old.* durable → recovery commits new pair → cleanup; OUT_DIR = new pair"),
        ("power_loss_after_tarball_mv",
         "poweroff after new tarball mv and before sha commit",
         "journal=backed_up + new tar durable in OUT_DIR → recovery skips tar commit, commits sha; OUT_DIR = new pair"),
        ("power_loss_after_verified_before_cleanup",
         "poweroff after durable journal=verified and before cleanup",
         "journal=verified + complete new pair → redo cleanup → exit 0"),
        ("power_loss_after_backup_mv_before_persist",
         "poweroff after old pair backup mv and before journal=backed_up",
         "journal=staged + old pair only in .txn → backup guard skips → persist(backed_up) → commit → verified; OUT_DIR = new pair"),
        ("power_loss_backup_mv_fsync_window",
         "poweroff after backup mv and before either cross-directory fsync",
         "journal=staged; three-way: consistent → auto-resume; double-existence → exit 9; both absent → exit 9 (fail-closed)"),
        ("power_loss_after_restore_mv_before_cleanup",
         "poweroff after rollback restore and before tombstone cleanup",
         "journal=rolling_back + OUT_DIR complete old pair → re-enter rollback: self-check PASS → rollback commit = atomic rename .txn→.txn.tombstone + fsync → exit 1; crash after rename → tombstone residue discarded at next startup"),
        ("power_loss_rollback_mid_delete_window",
         "poweroff while rolling_back deletes the new sidecar",
         "OUT_DIR pair = (absent,new) → whitelist allows → continue deleting new sidecar → restore old pair → exit 1 (NEVER exit 9)"),
        ("power_loss_rollback_mid_restore_window",
         "poweroff after rollback restores old tar and before old sidecar",
         "OUT_DIR pair = (old,absent) DETERMINISTIC (per-mv barrier) → whitelist allows → complete sidecar restore → exit 1"),
        ("power_loss_rollback_restore_fsync_window",
         "poweroff after one rollback restore mv and before its cross-directory fsync",
         "per-restored-file four-way matrix: tar target-only (old,absent) / source-only (absent,absent); sidecar target-only (old,old) / source-only (old,absent) → whitelist re-enter; double-existence / both-absent → exit 9 (fail-closed)"),
        ("power_loss_verified_cleanup_after_journal_rm",
         "poweroff after journal removal and before .txn cleanup",
         "journal absent + .txn without old.* → fresh path → 4a idempotent no-op exit 0"),
    ]
    for name, inj, exp in scenario_defs + power_loss_defs:
        fault_tests["snapshot_subcommand"][name] = {
            "injection": inj,
            "expected_final_state": exp,
            "actual_final_state": "<filled by O-2 tool>",
            "passed": None,
        }
    assert len(fault_tests["snapshot_subcommand"]) == 29, "fault scenario count != 29"
    row_counts = {
        "differs": sum(1 for r in main_rows if r[0] == "differs"),
        "identical": sum(1 for r in main_rows if r[0] == "identical"),
        "d_only": sum(1 for r in main_rows if r[0] == "D-only"),
        "d_stage_only": sum(1 for r in main_rows if r[0] == "D_stage-only"),
        "symlink_total": main_sym_cnt,
    }
    for c in SYMLINK_CLASSES:
        row_counts["symlink_" + c[len("symlink-"):].replace("-", "_")] = \
            sum(1 for r in main_rows if r[0] == c)
    genesis = {
        "schema_version": GENESIS_SCHEMA,
        # codex 复审 P2：genesis 路径必须绝对路径（canonicalize）
        "d_root_path": public_evidence_path(args.d_root),
        "d_stage_root_path": public_evidence_path(args.d_stage_root),
        "d_manifest_sha256": hashlib.sha256(
            build_manifest_tsv(args.d_root).encode()).hexdigest(),
        "d_stage_manifest_sha256": hashlib.sha256(
            build_manifest_tsv(args.d_stage_root).encode()).hexdigest(),
        "tool_versions": tool_versions,
        "snapshot_tool_versions": snapshot_tool_versions,
        "snapshot_boundary": "本快照 SHA 仅在 tar %s + zstd %s + jq ≥ 1.7.1 "
                            "（dpkg 包版本口径，dsh 裁定 2026-09-08）+ "
                            "本机构建环境下可复现；其他版本/环境需重新生成 "
                            "reference snapshot 并按 genesis.json "
                            "tool_versions 比对。" % (TAR_REQUIRED, ZSTD_REQUIRED),
        "fault_injection_tests": fault_tests,
        "exit_code_test_coverage": {
            "schema_version": "1.0",
            "gen-manifest": ["0", "1", "2", "3", "4", "5", "7", "8", "78"],
            "snapshot": ["0", "1", "2", "3", "4", "5", "6", "9", "78"],
            "reconcile": ["0", "1", "2", "3"],
            "notes": "每个子命令的退出码作用域契约必须由单元测试覆盖（每个"
                     "已声明退出码 + 0 成功路径）；退出码仅在子命令作用域内"
                     "解释，严禁跨子命令对照。",
        },
        "lc_all": "C",
        "started_at": started_at,
        "finished_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "row_counts": row_counts,
        "byte_counts": {
            "d-stage-audit.tsv": len(main_a.encode()),
            "d-stage-audit.symlink.tsv": len(sym_a.encode()),
            "d-stage-audit.D.manifest.tsv": len(
                build_manifest_tsv(args.d_root).encode()),
            "d-stage-audit.D_stage.manifest.tsv": len(
                build_manifest_tsv(args.d_stage_root).encode()),
        },
        "f0_reference_check": "pass (no F0 paths detected)",
        "determinism_self_check": "pass",
        "schema_check": "pass",
        "symlink_bc_check": "pass (symlink entries use SYM-N-A or UNASSIGNED only)",
    }
    genesis_path = os.path.join(args.out_dir, "d-stage-audit.genesis.json")
    atomic_write(genesis_path, json.dumps(genesis, indent=2, sort_keys=True) + "\n")
    return 0


# ---------------------------------------------------------------- reconcile

def reconcile_check(tarball, manifest):
    # 解压 tarball → 重建 4 字段 manifest → diff -u reference vs new
    # 4 字段：file_type \t relative_path \t symlink_target \t file_sha256
    scratch = tempfile.mkdtemp(prefix="r16-reconcile-")
    try:
        rc = subprocess.run(["tar", "--use-compress-program=zstd", "-xf", tarball,
                             "-C", scratch], capture_output=True, text=True,
                            env=dict(os.environ, LC_ALL="C"))
        if rc.returncode != 0:
            exit_err(3, "tarball extract failed: %s" % rc.stderr.strip())
        # codex 三轮复审 P2：顶层目录名必须恰为 d-stage（--transform 前缀），
        # 且不得是 symlink；任何其他顶层名/条目都拒绝
        tops = os.listdir(scratch)
        if tops != ["d-stage"] or os.path.islink(os.path.join(scratch, "d-stage")) \
                or not os.path.isdir(os.path.join(scratch, "d-stage")):
            exit_err(3, "tarball top-level structure unexpected: %r" % tops)
        root = os.path.join(scratch, "d-stage")
        new_manifest = build_manifest_tsv(root)
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    try:
        with open(manifest, "r") as f:
            ref = f.read()
    except OSError as e:
        exit_err(3, "manifest unreadable: %s" % e)
    # codex 复审 P2：reference manifest NF≠4 必须 exit 1（schema violation），
    # 不得落入内容 mismatch 的 exit 2
    check_nf(ref, 4, "reference manifest")
    if new_manifest == ref:
        return 0
    exit_err(2, "reconcile failed: 4-field manifest mismatch vs reference")


def cmd_reconcile(argv):
    p = argparse.ArgumentParser(prog=PROG + " reconcile", add_help=False)
    p.add_argument("--tarball", required=True)
    p.add_argument("--manifest", required=True)
    try:
        args = p.parse_args(argv)
    except SystemExit:
        exit_err(EX_CONFIG, "reconcile: unknown/malformed arguments")
    check_env()
    check_tool_versions()
    if not os.path.isfile(args.tarball):
        exit_err(3, "tarball not found: %s" % args.tarball)
    if not os.path.isfile(args.manifest):
        exit_err(3, "manifest not found: %s" % args.manifest)
    return reconcile_check(args.tarball, args.manifest)


# ---------------------------------------------------------------- snapshot

def staging_fingerprint(staging_dir):
    tar_path = os.path.join(staging_dir, TAR_BASENAME)
    sha_path = os.path.join(staging_dir, SHA_BASENAME)
    # codex 三轮复审 P1：staging 制品必须是 regular file
    for p, what in ((tar_path, "staging tar"), (sha_path, "staging sidecar")):
        if os.path.islink(p) or not os.path.isfile(p):
            exit_err(9, "%s missing or not a regular file" % what)
    new_sha = sha256_of(tar_path)
    new_size = os.path.getsize(tar_path)
    if not sidecar_is_strict(sha_path, new_sha):
        exit_err(9, "staging sidecar not strict")
    return new_sha, new_size


def build_staging(d_stage_root, staging_dir, reference_manifest):
    try:
        os.makedirs(staging_dir, exist_ok=True)
    except OSError as e:
        exit_err(5, "staging mkdir failed: %s" % e)
    # codex 六轮复审 P1 配套：staging 目录项持久化（fsync .txn）后再写文件
    fsync_dir(os.path.dirname(staging_dir))
    tar_path = os.path.join(staging_dir, TAR_BASENAME)
    sha_path = os.path.join(staging_dir, SHA_BASENAME)
    # design §5.3 可复现规范：固定 sort/mtime/owner/group/numeric-owner/
    # no-acls/no-xattrs/no-selinux + --transform d-stage + zstd -q -19
    tar_cmd = ["tar", "--sort=name", "--mtime=%s" % EPOCH, "--owner=0",
               "--group=0", "--numeric-owner", "--no-acls", "--no-xattrs",
               "--no-selinux", "--transform=s,^\\.,d-stage,", "-C", d_stage_root,
               "-cf", "-", "."]
    try:
        with open(tar_path, "wb") as out:
            t1 = subprocess.Popen(tar_cmd, stdout=subprocess.PIPE,
                                  env=dict(os.environ, LC_ALL="C"))
            t2 = subprocess.Popen(["zstd", "-q", "-19"], stdin=t1.stdout,
                                  stdout=out, env=dict(os.environ, LC_ALL="C"))
            t1.stdout.close()
            rc2 = t2.wait()
            rc1 = t1.wait()
    except OSError as e:
        exit_err(5, "staging tarball write failed: %s" % e)
    if rc1 != 0 or rc2 != 0:
        exit_err(5, "staging tarball generation failed (tar=%d zstd=%d)"
                 % (rc1, rc2))
    new_sha = sha256_of(tar_path)
    new_size = os.path.getsize(tar_path)
    atomic_write(sha_path, "%s  %s\n" % (new_sha, TAR_BASENAME))
    # staging 内 sha256sum -c 自校验
    chk = subprocess.run(["sha256sum", "-c", SHA_BASENAME], cwd=staging_dir,
                         capture_output=True, text=True,
                         env=dict(os.environ, LC_ALL="C"))
    if chk.returncode != 0:
        exit_err(1, "staging sha256sum -c failed: %s" % chk.stderr.strip())
    # staging 内 reconcile（解压 + 4 字段 manifest + diff -u reference）
    reconcile_check(tar_path, reference_manifest)
    # 注入接缝：staging 完整生成后、journal(staged) 持久化前
    inject_pause("staging_built")
    return new_sha, new_size


def facts_check(out_dir, txn_dir, j, new_sha, old_tar_sha, old_sha_sha):
    # 三者事实一致性（fail-closed）
    old_tar_path = os.path.join(txn_dir, "old.tar.zst")
    old_sha_path = os.path.join(txn_dir, "old.sha256")
    plan_tar = j["old_tar"]
    plan_sha = j["old_sha"]
    if plan_tar not in ("present", "absent") or plan_sha not in ("present", "absent"):
        exit_err(9, "journal old_tar/old_sha not in {present,absent}")
    if plan_tar != plan_sha:
        exit_err(9, "journal old_tar/old_sha inconsistent")
    tar_state, sha_state = classify_out_pair(out_dir, new_sha, old_tar_sha, old_sha_sha)
    if "unknown" in (tar_state, sha_state):
        exit_err(9, "OUT_DIR unknown content (tar=%s sha=%s)" % (tar_state, sha_state))
    # .txn/old.* 内容必须匹配指纹；symlink/非 regular 一律拒绝（codex 三轮复审 P1）
    for name, want in (("old.tar.zst", old_tar_sha), ("old.sha256", old_sha_sha)):
        p = os.path.join(txn_dir, name)
        if os.path.lexists(p):
            if os.path.islink(p) or not os.path.isfile(p):
                exit_err(9, ".txn/%s is not a regular file" % name)
            if want != "none" and sha256_of(p) != want:
                exit_err(9, ".txn/%s fingerprint mismatch" % name)
    # 计划 present → 旧文件恰好存在于 .txn 或 OUT_DIR 之一（**逐文件**：
    # tar 与 sidecar 各自独立校验，双存在 / 均缺失 / 与计划不一致 → exit 9，
    # codex 初审 P1）；计划 absent → .txn/old.* 不得存在。
    # codex 复审 P1：verified 状态是清理完成后的合法崩溃窗口（old.* 已删、
    # journal 未删），不适用该互斥校验；rolling_back 维持四态矩阵校验。
    if j["state"] != "verified":
        for name, in_out_state, path in (
                ("old.tar.zst", tar_state == "old", old_tar_path),
                ("old.sha256", sha_state == "old", old_sha_path)):
            if plan_tar == "present":
                in_txn = os.path.exists(path)
                if in_txn == in_out_state:
                    exit_err(9, "%s double-existence or both-missing "
                                "(txn=%s out=%s)" % (name, in_txn, in_out_state))
            else:
                if os.path.exists(path):
                    exit_err(9, "plan absent but .txn/%s exists" % name)
    # 统一混合判定仅对非 rolling_back 状态生效
    if j["state"] != "rolling_back":
        if tar_state == "new" and sha_state == "old" or \
           tar_state == "old" and sha_state == "new":
            exit_err(9, "mixed new/old OUT_DIR pair for state %s" % j["state"])
    return tar_state, sha_state


def state_invariants(state, tar_state, sha_state):
    if state in ("verified", "sha_committed"):
        if tar_state != "new" or sha_state != "new":
            exit_err(9, "state invariant violated: %s requires full new pair" % state)
    if state == "staged" and "new" in (tar_state, sha_state):
        exit_err(9, "state invariant violated: staged must not have new")
    if state == "backed_up" and sha_state != "absent":
        exit_err(9, "state invariant violated: backed_up sha must be absent")
    if state == "tarball_committed" and tar_state != "new":
        exit_err(9, "state invariant violated: tarball_committed tar must be new")
    if state == "rolling_back" and not rolling_back_mid_state_ok(tar_state, sha_state):
        exit_err(9, "state invariant violated: rolling_back pair (%s|%s) not in "
                    "legal mid-state whitelist" % (tar_state, sha_state))


def backup_old_pair(out_dir, txn_dir):
    old_tar = os.path.join(txn_dir, "old.tar.zst")
    old_sha = os.path.join(txn_dir, "old.sha256")
    for src, dst in ((os.path.join(out_dir, TAR_BASENAME), old_tar),
                     (os.path.join(out_dir, SHA_BASENAME), old_sha)):
        if os.path.exists(src) and not os.path.exists(dst):
            try:
                os.rename(src, dst)
            except OSError as e:
                exit_err(5, "backup mv failed (%s -> %s): %s" % (src, dst, e))
        # 注入接缝：单个备份 mv 完成后、双目录 fsync 之前（fsync 窗口）
        if dst == old_tar:
            inject_pause("backup_tar_fsync_window")
        else:
            inject_pause("backup_sha_fsync_window")
    # v19 per codex v18 P1 #1 + v20 per codex v19 P1 #1：跨目录 rename 源、目标目录都同步
    fsync_dir(txn_dir)
    fsync_dir(out_dir)


def commit_new_tar(staging_dir, out_dir):
    src = os.path.join(staging_dir, TAR_BASENAME)
    dst = os.path.join(out_dir, TAR_BASENAME)
    try:
        if os.path.exists(dst):
            os.remove(dst)
        os.rename(src, dst)
    except OSError as e:
        exit_err(5, "commit new tarball mv failed: %s" % e)
    fsync_dir(out_dir)
    # 注入接缝：新 tar 提交 + OUT_DIR fsync 后、journal(tarball_committed) 前
    inject_pause("tarball_mv_committed")


def commit_new_sha(staging_dir, out_dir):
    src = os.path.join(staging_dir, SHA_BASENAME)
    dst = os.path.join(out_dir, SHA_BASENAME)
    try:
        if os.path.exists(dst):
            os.remove(dst)
        os.rename(src, dst)
    except OSError as e:
        exit_err(5, "commit new sha256 mv failed: %s" % e)
    fsync_dir(out_dir)


def cleanup_verified(out_dir, txn_dir, journal_path):
    # rm old.* → fsync_dir(.txn)（v21 per codex v20 P1 #3，先于 rm journal）
    # codex 复审 P1：清理失败必须 fail-closed exit 5（不得 ignore_errors 后
    # 假装成功，事务证据残留时绝不报告清理完成）
    for name in ("old.tar.zst", "old.sha256"):
        p = os.path.join(txn_dir, name)
        if os.path.exists(p):
            try:
                os.remove(p)
            except OSError as e:
                exit_err(5, "verified cleanup rm %s failed: %s" % (name, e))
    fsync_dir(txn_dir)
    if os.path.exists(journal_path):
        try:
            os.remove(journal_path)
        except OSError as e:
            exit_err(5, "verified cleanup rm journal failed: %s" % e)
    # 注入接缝：rm old.* + fsync(.txn) + rm journal 后、rmtree .txn 前
    inject_pause("cleanup_journal_rm")
    try:
        shutil.rmtree(txn_dir)
    except OSError as e:
        exit_err(5, "verified cleanup rmtree .txn failed: %s" % e)
    fsync_dir(out_dir)


def rollback_segment(out_dir, txn_dir, journal_path, new_sha, old_tar_sha,
                     old_sha_sha, from_recovery=False):
    # 即时回滚（与启动恢复共用 classify_out_pair；v17 per codex v16 P1 #1）
    # codex 初审 P1：恢复重入（from_recovery=True）时 OUT_DIR 合法中途态
    # (old,absent)/(old,old) 必须被允许——old 状态文件原样保留、只删 new、
    # 继续还原 .txn 剩余 old.* 收敛；即时回滚（4e FAIL）仍拒绝 unknown/old。
    tar_state, sha_state = classify_out_pair(out_dir, new_sha, old_tar_sha, old_sha_sha)
    if tar_state == "unknown":
        exit_err(9, "OUT_DIR tarball state unknown (keep journal + scene)")
    if sha_state == "unknown":
        exit_err(9, "OUT_DIR sidecar state unknown (keep journal + scene)")
    if not from_recovery:
        if tar_state == "old":
            exit_err(9, "OUT_DIR tarball state old (not exactly new; keep "
                        "journal + scene)")
        if sha_state == "old":
            exit_err(9, "OUT_DIR sidecar state old (not exactly new; keep "
                        "journal + scene)")
    if tar_state == "new":
        try:
            os.remove(os.path.join(out_dir, TAR_BASENAME))
        except OSError as e:
            exit_err(9, "rollback rm new tarball failed: %s（journal 保持 "
                        "rolling_back，下次启动重入）" % e)
    # 注入接缝：新 tar 删除后、新 sidecar 删除前（mid-delete 窗口）
    inject_pause("rollback_deleted_tar")
    if sha_state == "new":
        try:
            os.remove(os.path.join(out_dir, SHA_BASENAME))
        except OSError as e:
            exit_err(9, "rollback rm new sha256 failed: %s（journal 保持 "
                        "rolling_back，下次启动重入）" % e)
    # v23 per codex v22 P1 #1：**每个**跨目录还原 mv 后立即 fsync 源、目标目录；
    # v24 per codex v23 P1 #1：任一 fsync 失败 exit 5（journal 保持 rolling_back 重入收敛）
    old_tar = os.path.join(txn_dir, "old.tar.zst")
    old_sha = os.path.join(txn_dir, "old.sha256")
    if os.path.exists(old_tar):
        try:
            os.rename(old_tar, os.path.join(out_dir, TAR_BASENAME))
        except OSError as e:
            exit_err(9, "回滚还原旧 tarball 失败: %s（journal 保持 rolling_back，"
                        "下次启动重入）" % e)
        # 注入接缝：旧 tar 还原 mv 后、该 mv 双目录 fsync 前（restore fsync 窗口）
        inject_pause("rollback_restore_tar_mv")
        fsync_dir(out_dir)
        fsync_dir(txn_dir)
    # 注入接缝：旧 tar 还原 + 逐次双目录 fsync 后、旧 sidecar 还原前
    # （mid-restore 窗口；旧 tar 未计划时仍在同点暂停——该场景 pre-check
    # 按 (old,absent) 判定，未计划时按 journal facts 自洽）
    inject_pause("rollback_restored_tar")
    if os.path.exists(old_sha):
        try:
            os.rename(old_sha, os.path.join(out_dir, SHA_BASENAME))
        except OSError as e:
            exit_err(9, "回滚还原旧 sha256 失败: %s（journal 保持 rolling_back，"
                        "下次启动重入）" % e)
        # 注入接缝：旧 sidecar 还原 mv 后、该 mv 双目录 fsync 前
        inject_pause("rollback_restore_sha_mv")
        fsync_dir(out_dir)
        fsync_dir(txn_dir)
    # 最终校验：旧对若存在必须自洽
    if os.path.exists(os.path.join(out_dir, TAR_BASENAME)) or \
       os.path.exists(os.path.join(out_dir, SHA_BASENAME)):
        chk = subprocess.run(["sha256sum", "-c", SHA_BASENAME], cwd=out_dir,
                             capture_output=True, text=True,
                             env=dict(os.environ, LC_ALL="C"))
        if chk.returncode != 0:
            exit_err(9, "rollback old-pair self-check failed")
    # 注入接缝：还原 + 逐次 fsync + 自校验全部完成后、tombstone rename 前
    # （restore-before-cleanup 窗口 a）
    inject_pause("rollback_restore_complete")
    # codex 五轮复审 P1：rmtree 不是原子操作（可能 journal 已删而 old.* 残留，
    # 恢复无法重入收敛）——改为目录级 rename tombstone：原子 rename 提交事务
    # 终态（.txn 整体消失），tombstone 残留为已提交残骸，启动时丢弃。
    tomb = txn_dir + ".tombstone"
    try:
        os.rename(txn_dir, tomb)
    except OSError as e:
        exit_err(9, "rollback commit rename .txn -> .txn.tombstone failed: %s"
                    "（journal 保留，下次启动重入收敛）" % e)
    fsync_dir(out_dir)
    # 注入接缝：rename + fsync_dir(OUT_DIR) 后、rmtree(.txn.tombstone) 前
    # （restore-before-cleanup 窗口 b）
    inject_pause("rollback_tombstone_rmtree")
    try:
        shutil.rmtree(tomb)
    except OSError as e:
        exit_err(5, "rollback tombstone cleanup failed（事务已提交；残骸 "
                    ".txn.tombstone 由下次启动丢弃）: %s" % e)
    # 注入接缝：rmtree 后、最终 fsync_dir(OUT_DIR) 前
    # （restore-before-cleanup 窗口 c）
    inject_pause("rollback_tombstone_final_fsync")
    fsync_dir(out_dir)


def classify_fs(fstype):
    # codex 七轮复审 P1：契约要求本地崩溃一致性文件系统（ext4/xfs 默认
    # barrier 语义）——白名单取代有限黑名单（tmpfs/overlayfs/CIFS/FUSE 等
    # 一律拒绝，tmpfs 重启丢失全部事务与输出）
    # GNU stat reports some ext-family mounts as the composite "ext2/ext3"
    # label; it still denotes the local ext filesystem family covered by the
    # contract.
    GOOD_FS = ("ext2", "ext3", "ext4", "ext2/ext3", "xfs", "btrfs")
    return fstype in GOOD_FS


def check_out_dir_fs(out_dir):
    # codex 六轮复审 P1：OUT_DIR 文件系统前置检查——非本地崩溃一致性
    # 文件系统（NFS/9P/virtiofs/tmpfs/overlayfs/FUSE 等）→ exit 78（EX_CONFIG）
    try:
        out = subprocess.run(["stat", "-f", "-c", "%T", out_dir],
                             capture_output=True, text=True,
                             env=dict(os.environ, LC_ALL="C"), timeout=30)
    except (OSError, subprocess.TimeoutExpired):
        exit_err(EX_CONFIG, "stat unavailable")
    if out.returncode != 0:
        exit_err(EX_CONFIG, "stat exit non-zero (%d)" % out.returncode)
    fstype = out.stdout.strip()
    if not classify_fs(fstype):
        exit_err(EX_CONFIG, "OUT_DIR filesystem %s is not a local crash-"
                 "consistent filesystem (require ext4/xfs-class with barrier "
                 "semantics; tmpfs/NFS/9P/virtiofs/overlayfs/FUSE rejected)"
                 % fstype)


def run_snapshot(d_stage_root, out_dir, reference_manifest):
    # codex 六轮复审 P2：锁先于一切文件系统变更——不再在加锁前 mkdir；
    # OUT_DIR 必须已存在（缺失 → 锁文件打开失败 exit 5）
    fd = acquire_snapshot_lock(out_dir)
    try:
        txn_dir = os.path.join(out_dir, ".txn")
        journal_path = os.path.join(txn_dir, "journal")
        # codex 初审 P1：.txn 本身为 symlink 时必须拒绝（防写到事务目录外部）
        if os.path.islink(txn_dir):
            exit_err(9, ".txn is a symlink (refuse to operate through it)")
        # codex 五轮复审 P1 配套：丢弃已提交的 tombstone 残骸（严格 fail-closed）
        tomb = txn_dir + ".tombstone"
        if os.path.islink(tomb):
            exit_err(9, ".txn.tombstone is a symlink (refuse to operate through it)")
        if os.path.exists(tomb):
            try:
                shutil.rmtree(tomb)
            except OSError as e:
                exit_err(9, "discard .txn.tombstone residue failed: %s" % e)
        # 1. 启动恢复检查
        j = read_journal(txn_dir) if os.path.isdir(txn_dir) else None
        if j is None and os.path.isdir(txn_dir):
            # 无 journal：old.* 不存在（已校验）→ 丢弃 .txn 残留继续全新路径
            # codex 四轮复审 P2：残留清理必须 fail-closed（不得 ignore_errors）
            try:
                shutil.rmtree(txn_dir)
            except OSError as e:
                exit_err(9, "discard .txn residue failed: %s" % e)
        out_tar = os.path.join(out_dir, TAR_BASENAME)
        out_sha = os.path.join(out_dir, SHA_BASENAME)
        if j is not None:
            # 2. staging_dir 路径约束
            staging_dir = validate_staging_dir(j["staging_dir"], txn_dir)
            new_sha = j["new_tar_sha256"]
            old_tar_sha = j.get("old_tar_sha256") or "none"
            old_sha_sha = j.get("old_sha_sha256") or "none"
            # 3. 三者事实一致性
            tar_state, sha_state = facts_check(out_dir, txn_dir, j, new_sha,
                                               old_tar_sha, old_sha_sha)
            # 3b. 状态专属不变量
            state_invariants(j["state"], tar_state, sha_state)
            if j["state"] == "verified":
                cleanup_verified(out_dir, txn_dir, journal_path)
                print("INFO: recovery complete (was verified); OUT_DIR = new pair")
                return 0
            if j["state"] == "rolling_back":
                rollback_segment(out_dir, txn_dir, journal_path, new_sha,
                                 old_tar_sha, old_sha_sha, from_recovery=True)
                print("INFO: recovery complete (was rolling_back); OUT_DIR = old pair")
                return 1
            if j["state"] == "staged":
                # 校验 staging 指纹后补做备份
                got_sha, got_size = staging_fingerprint(staging_dir)
                if got_sha != new_sha or str(got_size) != j["new_tar_size"]:
                    exit_err(9, "staging fingerprint mismatch vs journal")
                backup_old_pair(out_dir, txn_dir)
                persist_journal(journal_path, {"state": "backed_up",
                                              "staging_dir": j["staging_dir"],
                                              "new_tar_sha256": new_sha,
                                              "new_tar_size": j["new_tar_size"],
                                              "old_tar": j["old_tar"],
                                              "old_sha": j["old_sha"],
                                              "old_tar_sha256": old_tar_sha,
                                              "old_sha_sha256": old_sha_sha})
                j["state"] = "backed_up"
            if j["state"] == "backed_up":
                if tar_state != "new":
                    commit_new_tar(staging_dir, out_dir)
                persist_journal(journal_path, {"state": "tarball_committed",
                                              "staging_dir": j["staging_dir"],
                                              "new_tar_sha256": new_sha,
                                              "new_tar_size": j["new_tar_size"],
                                              "old_tar": j["old_tar"],
                                              "old_sha": j["old_sha"],
                                              "old_tar_sha256": old_tar_sha,
                                              "old_sha_sha256": old_sha_sha})
                j["state"] = "tarball_committed"
            if j["state"] == "tarball_committed":
                if sha_state != "new":
                    commit_new_sha(staging_dir, out_dir)
                persist_journal(journal_path, {"state": "sha_committed",
                                              "staging_dir": j["staging_dir"],
                                              "new_tar_sha256": new_sha,
                                              "new_tar_size": j["new_tar_size"],
                                              "old_tar": j["old_tar"],
                                              "old_sha": j["old_sha"],
                                              "old_tar_sha256": old_tar_sha,
                                              "old_sha_sha256": old_sha_sha})
                j["state"] = "sha_committed"
            # 4e 最终自校验
            chk = subprocess.run(["sha256sum", "-c", SHA_BASENAME], cwd=out_dir,
                                 capture_output=True, text=True,
                                 env=dict(os.environ, LC_ALL="C"))
            if chk.returncode == 0:
                persist_journal(journal_path, {"state": "verified",
                                              "staging_dir": j["staging_dir"],
                                              "new_tar_sha256": new_sha,
                                              "new_tar_size": j["new_tar_size"],
                                              "old_tar": j["old_tar"],
                                              "old_sha": j["old_sha"],
                                              "old_tar_sha256": old_tar_sha,
                                              "old_sha_sha256": old_sha_sha})
                # 注入接缝：journal(verified) 持久化后、清理开始前
                inject_pause("verified_persisted")
                cleanup_verified(out_dir, txn_dir, journal_path)
                print("OK: recovery committed new pair; OUT_DIR = new pair")
                return 0
            persist_journal(journal_path, {"state": "rolling_back",
                                          "staging_dir": j["staging_dir"],
                                          "new_tar_sha256": new_sha,
                                          "new_tar_size": j["new_tar_size"],
                                          "old_tar": j["old_tar"],
                                          "old_sha": j["old_sha"],
                                          "old_tar_sha256": old_tar_sha,
                                          "old_sha_sha256": old_sha_sha})
            rollback_segment(out_dir, txn_dir, journal_path, new_sha,
                             old_tar_sha, old_sha_sha)
            print("OK: rolled back to complete old pair")
            return 1
        # ---- 全新路径 ----
        try:
            os.makedirs(txn_dir, exist_ok=True)
        except OSError as e:
            exit_err(5, "cannot create .txn: %s" % e)
        # codex 六轮复审 P1：.txn 目录项必须持久化（fsync OUT_DIR）后才写
        # journal(staged)，否则断电可能丢失整个事务目录（与
        # power_loss_after_staged_persist 契约冲突）
        fsync_dir(out_dir)
        staging_dir = os.path.join(txn_dir, "staging")
        new_sha, new_size = build_staging(d_stage_root, staging_dir, reference_manifest)
        # 4a 幂等 no-op（复用严格 sidecar 校验）
        if os.path.lexists(out_tar) or os.path.lexists(out_sha):
            # codex 三轮复审 P1：OUT_DIR 制品 symlink/非 regular 一律 exit 9
            if os.path.islink(out_tar) or not os.path.isfile(out_tar) or \
               os.path.islink(out_sha) or not os.path.isfile(out_sha):
                exit_err(9, "OUT_DIR existing snapshot not regular files")
            if not (os.path.exists(out_tar) and os.path.exists(out_sha)):
                exit_err(9, "OUT_DIR existing snapshot half pair")
            old_tar_sha = sha256_of(out_tar)
            if not sidecar_is_strict(out_sha, old_tar_sha):
                exit_err(9, "OUT_DIR existing sidecar not strict")
            if old_tar_sha == new_sha:
                # codex 四轮复审 P2：no-op staging 清理必须 fail-closed
                try:
                    shutil.rmtree(staging_dir)
                except OSError as e:
                    exit_err(5, "no-op staging cleanup failed: %s" % e)
                print("OK: idempotent no-op (existing pair == new snapshot)")
                return 0
        # 备份计划 + 旧对指纹
        old_tar_present = os.path.exists(out_tar)
        old_sha_present = os.path.exists(out_sha)
        old_tar_sha = sha256_of(out_tar) if old_tar_present else "none"
        old_sha_sha = sha256_of(out_sha) if old_sha_present else "none"
        # persist(staged)：staging 文件+目录 fsync 先于 journal（v19 per codex v18 P1 #1）
        for f in (os.path.join(staging_dir, TAR_BASENAME),
                  os.path.join(staging_dir, SHA_BASENAME)):
            fsync_file(f)
        fsync_dir(staging_dir)
        persist_journal(journal_path, {"state": "staged",
                                      "staging_dir": staging_dir,
                                      "new_tar_sha256": new_sha,
                                      "new_tar_size": str(new_size),
                                      "old_tar": "present" if old_tar_present else "absent",
                                      "old_sha": "present" if old_sha_present else "absent",
                                      "old_tar_sha256": old_tar_sha,
                                      "old_sha_sha256": old_sha_sha})
        # 注入接缝：journal(staged) 持久化后、备份旧对 mv 前
        inject_pause("staged_persisted")
        backup_old_pair(out_dir, txn_dir)
        # 注入接缝：备份 mv + 双目录 fsync 后、journal(backed_up) 前
        inject_pause("backup_mv_complete")
        persist_journal(journal_path, {"state": "backed_up",
                                      "staging_dir": staging_dir,
                                      "new_tar_sha256": new_sha,
                                      "new_tar_size": str(new_size),
                                      "old_tar": "present" if old_tar_present else "absent",
                                      "old_sha": "present" if old_sha_present else "absent",
                                      "old_tar_sha256": old_tar_sha,
                                      "old_sha_sha256": old_sha_sha})
        # 注入接缝：journal(backed_up) 持久化后、新 tarball 提交前
        inject_pause("backed_up_persisted")
        commit_new_tar(staging_dir, out_dir)
        persist_journal(journal_path, {"state": "tarball_committed",
                                      "staging_dir": staging_dir,
                                      "new_tar_sha256": new_sha,
                                      "new_tar_size": str(new_size),
                                      "old_tar": "present" if old_tar_present else "absent",
                                      "old_sha": "present" if old_sha_present else "absent",
                                      "old_tar_sha256": old_tar_sha,
                                      "old_sha_sha256": old_sha_sha})
        commit_new_sha(staging_dir, out_dir)
        persist_journal(journal_path, {"state": "sha_committed",
                                      "staging_dir": staging_dir,
                                      "new_tar_sha256": new_sha,
                                      "new_tar_size": str(new_size),
                                      "old_tar": "present" if old_tar_present else "absent",
                                      "old_sha": "present" if old_sha_present else "absent",
                                      "old_tar_sha256": old_tar_sha,
                                      "old_sha_sha256": old_sha_sha})
        chk = subprocess.run(["sha256sum", "-c", SHA_BASENAME], cwd=out_dir,
                             capture_output=True, text=True,
                             env=dict(os.environ, LC_ALL="C"))
        if chk.returncode == 0:
            persist_journal(journal_path, {"state": "verified",
                                          "staging_dir": staging_dir,
                                          "new_tar_sha256": new_sha,
                                          "new_tar_size": str(new_size),
                                          "old_tar": "present" if old_tar_present else "absent",
                                          "old_sha": "present" if old_sha_present else "absent",
                                          "old_tar_sha256": old_tar_sha,
                                          "old_sha_sha256": old_sha_sha})
            cleanup_verified(out_dir, txn_dir, journal_path)
            print("OK: %s (size: %d bytes, self-check PASS, journal state machine "
                  "complete)" % (os.path.join(out_dir, TAR_BASENAME), new_size))
            return 0
        persist_journal(journal_path, {"state": "rolling_back",
                                      "staging_dir": staging_dir,
                                      "new_tar_sha256": new_sha,
                                      "new_tar_size": str(new_size),
                                      "old_tar": "present" if old_tar_present else "absent",
                                      "old_sha": "present" if old_sha_present else "absent",
                                      "old_tar_sha256": old_tar_sha,
                                      "old_sha_sha256": old_sha_sha})
        rollback_segment(out_dir, txn_dir, journal_path, new_sha,
                         old_tar_sha, old_sha_sha)
        print("OK: rolled back to complete old pair (OUT_DIR = old pair)")
        return 1
    finally:
        os.close(fd)


def cmd_snapshot(argv):
    p = argparse.ArgumentParser(prog=PROG + " snapshot", add_help=False)
    p.add_argument("--d-stage-root", required=True)
    p.add_argument("--out-dir", required=True)
    p.add_argument("--reference-manifest", required=False)
    try:
        args = p.parse_args(argv)
    except SystemExit:
        exit_err(EX_CONFIG, "snapshot: unknown/malformed arguments")
    check_env()
    check_tool_versions()
    check_out_dir(args.out_dir)
    # codex 七轮复审 P1：CLI 边界 canonicalize out_dir——journal 中
    # staging_dir 必须绝对路径（相对 --out-dir 崩溃后无法恢复）
    args.out_dir = realpath_m(args.out_dir)
    # codex 七轮复审 P2：缺失 OUT_DIR 不得先被 fs 检查拦截——目录不存在时
    # 跳过 fs 检查，由锁打开失败按契约 exit 5
    if os.path.isdir(args.out_dir):
        check_out_dir_fs(args.out_dir)
    if not os.path.isdir(args.d_stage_root):
        exit_err(3, "d-stage-root missing: %s" % args.d_stage_root)
    ref = args.reference_manifest or os.path.join(
        args.out_dir, "..", "d-stage-audit.D_stage.manifest.tsv")
    ref = realpath_m(ref)
    if not os.path.isfile(ref):
        exit_err(4, "reference manifest missing or inaccessible: %s" % ref)
    # reconcile-first：staging 内 reconcile（build_staging 步骤 7）已含 4 字段比对
    return run_snapshot(args.d_stage_root, args.out_dir, ref)


def main():
    if len(sys.argv) < 2:
        eprint("Usage: %s <gen-manifest|snapshot|reconcile> [args]" % PROG)
        sys.exit(EX_CONFIG)
    sub = sys.argv[1]
    rest = sys.argv[2:]
    if sub == "gen-manifest":
        sys.exit(gen_full_audit(rest))
    if sub == "snapshot":
        sys.exit(cmd_snapshot(rest))
    if sub == "reconcile":
        sys.exit(cmd_reconcile(rest))
    eprint("unknown subcommand: %s" % sub)
    sys.exit(EX_CONFIG)


if __name__ == "__main__":
    main()
