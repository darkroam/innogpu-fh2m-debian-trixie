#!/usr/bin/env python3
# tools/r16-f-payload-integrity.py — C3-a ③ F 载荷完整性审计（生成 + 复验）
#
# 依据 = docs/planning/5.0.0-i1-validation-plan.md §三「C3-a 放行前置」第 ③ 项
# （dsh 2026-09-11 裁决：① 来源已闭合、② 授权已定档「仅自用、不分发」、
# ③④ 交 qoder 技术执行）。本工具只实现 ③：F 载荷逐文件 SHA-256 清单 +
# 与来源包 DEBIAN/md5sums 交叉校验，证据落 docs/planning/evidence/o-stage/。
#
# 三个相互独立的证据源，两两交叉校验（任一源单独被篡改都会暴露）：
#   S1 = 来源 deb 的 data 归档   （dpkg-deb --fsys-tarfile，流式，不落盘）
#   S2 = 来源 deb 的 control 归档（dpkg-deb --ctrl-tarfile，流式，不落盘）
#   S3 = 本地解包树              （只读遍历）
# 拆分口径：S1 只含安装载荷（无 DEBIAN/），S2 只含 DEBIAN/ 控制成员，故
#   B 用 S1 ↔ S3\DEBIAN/，C 用 S2 ↔ S3 的 DEBIAN/ 子树——两侧各自双向闭合。
#
# 检查项（全部 fail-closed；任一失败 → 不写任何产物、rc=1、逐项打印失败）：
#   A 来源包身份   deb SHA-256 + 字节数 + control 五字段 == 期望常量
#   B 载荷一致     S1 与 S3\DEBIAN/ 的 {目录/常规文件/符号链接} 集合双向相等，
#                  且逐项 size / mode / sha256 / symlink target 相等
#   C 控制与校验和 S2 与 S3 的 DEBIAN/ 子树双向字节相等；S2 的 md5sums 拆为
#                  精确子集（507 条，路径在盘且 MD5 逐值相等）与重定位子集
#                  （95 条，经三段可复合重定位后逐值相等且构成严格双射）；
#                  上游 md5sums **路径列**缺陷作为有界发现记录（含 dpkg -V
#                  影响与 ④ 必须重生成 md5sums 的义务），超出该刻画即 FAIL
#   D 清单         逐文件 type/path/size/mode/sha256/md5/link_target（主产物）
#   E 血统相干     REQUIRED_F_PATHS 全部存在（常规文件或可在载荷根内解析到
#                  常规文件的符号链接）+ 全树零悬空符号链接 + 零 O 血统
#                  loader 文件名
#   F 固件对账     fh2m.fw/fh2c.fw/fh2m.sh/fh2c.sh SHA-256 == 评估文档记录值
#   G 安装期暂存   opt/fantgpu-fh2m 下 DDX ABI 变体枚举 + postinst 设备门
#                  PCI ID 锁定断言（缺失/未锁定 ID 即 FAIL）
#
# 子命令：
#   gen     生成 f-payload.manifest.tsv(+.sha256) 与 f-payload-integrity.json
#   verify  不写盘；重算并与既有产物逐字节比对（供三方独立复验）
#
# gen 提交事务（三件产物同代，杜绝混代证据；codex 初审 P1-1 / re-review
# P1-1/P1-2/P2 / re-review#2 P1-1/P1-2 / re-review#4 P1-1/P1-2 /
# re-review#5 P1/P2 修复）：
#   0. all-or-none 守卫**先于暂存**：旧产物部分存在 → rc=2 且零残留
#   1. 暂存：三件产物先写入 mkstemp 唯一临时文件（O_EXCL 随机名；固定名
#      <name>.tmp 已废弃——预置同名 symlink 无法劫持也不会被跟随）；
#      mkstemp 后**立即登记**临时路径，写入/fsync 失败统一清理并 die rc=2
#   2. 备份：既有旧产物逐个 os.replace 为 <name>.bak（os.replace 替换链接
#      自身、不跟随 symlink，预置 .bak symlink 不会写穿到链接目标）
#   3. 提交：逐个 os.replace(临时, 目标)；任一失败 → 写 journal rolling_back
#      → 按 pre_existing 规则回滚（有 .bak 恢复旧代；无 .bak 且事务前不存在
#      的目标删除——首代中途失败不残留新产物）→ rc=2
#   4. 清理：删 .bak/临时文件/journal；清理失败**不回滚**（提交点后口径，
#      同 O-4 先例），残留由下次 gen 启动恢复按 journal 状态机统一处理
# journal = f-payload.commit.journal：{"state": backing_up|committing|
# committed|rolling_back, "pre_existing": [bool×3]}——pre_existing 记录事务
# 开始前三件目标的存在性，是回滚/恢复区分「旧代在盘」与「首代新产物」的
# 唯一依据。**事务开始前强制旧产物三件全有或全无**（部分存在 → rc=2 人工
# 裁决），故 pre_existing 只有全真/全假两种合法值，`.bak` 只允许对应
# pre_existing=true 的位置。启动恢复（codex re-review P1-1）：先按原状态
# 只读验证 .bak 集合不变量（backing_up/rolling_back → 前缀，committing →
# 与 pre_existing 完全一致），**再把 journal 切换为 rolling_back（切换
# 成功后才允许修改 .bak/dst；切换失败保留原现场 die）**，随后按 2→0 反序
# 恢复——反序使恢复再次中断的残留仍是前缀集，二次启动可续恢复；同进程
# 回滚同样「切换成功才动 backup」。恢复前对可重建的旧代三件做**统一
# reconcile**（codex re-review P1-2）：sidecar 与 manifest 哈希绑定 +
# JSON 与 manifest 绑定（schema_version/manifest.file/manifest.sha256），
# committed 清理同样校验 dst 三件；不一致或 JSON 不可读 → fail-closed
# rc=2 保留现场。**无 journal 出现任何 .bak → fail-closed rc=2 保留现场**
# （.bak 无法自证属于本工具事务）。回滚失败如实报告（INCOMPLETE +
# journal 保留），绝不谎称「已恢复」。verify 只读，遇任何提交残留一律
# rc=2（先 gen 恢复再复验）。
# 故障注入（仅测试用，逗号分隔多点）：
# FPI_FAIL_INJECT=stage.N|stage-cleanup.N|backup.N|commit.N|cleanup.N|
# rollback.1..3|journal.<state>|journal-cleanup.1|
# journal-unlink.committed|journal-unlink.recovered|journal-unlink.rollback
# （如 "commit.2,rollback.2" 模拟提交失败后回滚中途失败、
# "commit.2,journal.rolling_back" 模拟回滚状态切换失败、
# "stage.2,stage-cleanup.1" 模拟暂存失败且清理也失败）
#
# 退出码：0=PASS 1=审计失败 2=用法/环境错误
#
# 边界：只读 debs/ 与 build/（保护区），只写 --out-dir；--out-dir 落在保护区
# 内即拒绝。产物不含墙钟时间戳 → 双跑字节一致（审计 provenance 由引入它的
# git commit 承载，与 o-stage.manifest.tsv 同口径）。

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

PROG = "tools/r16-f-payload-integrity.py"
SCHEMA_VERSION = "1.0"
AUDIT_ID = "c3-a-payload-integrity"
LINEAGE = "fantgpu"

EX_USAGE = 2
EX_FAIL = 1

DEFAULT_DEB = ("debs/fantgpu-fh2m_3.3.8.126-driver-linux-desktop-"
               "sp-generic_amd64.deb")
DEFAULT_UNPACK = "build/r16-fantgpu-deb"
DEFAULT_OUT = "docs/planning/evidence/o-stage"

MANIFEST_NAME = "f-payload.manifest.tsv"
MANIFEST_SHA_NAME = "f-payload.manifest.tsv.sha256"
INTEGRITY_NAME = "f-payload-integrity.json"
MANIFEST_COLUMNS = ("type", "path", "size", "mode", "sha256", "md5",
                    "link_target")

TXN_PREFIX = ".f-payload.txn."
BAK_SUFFIX = ".bak"
JOURNAL_NAME = "f-payload.commit.journal"
JOURNAL_STATES = ("backing_up", "committing", "committed", "rolling_back")

PROTECTED_TOP = ("debs", "vendor", "build", "third_party", "migration",
                 "drivers", "baselines", "patches")

DEBIAN_PREFIX = "DEBIAN/"

# --------------------------------------------------------------- 期望常量
# 每个常量的来源都在注释中标注；改动任一项都构成审计基线变更，须三方重新裁决。

# A：本轮 sha256sum/stat 实测，与 collab 既有记录及方案文档 §三 记载一致。
EXPECTED_DEB_SHA256 = ("6f0daaf79fb6b2a547138c17628bb990dff0d0c684ee1c13775b"
                       "ebc2d28fd11b")
EXPECTED_DEB_SIZE = 57607700
# A：DEBIAN/control 实读（经 C 的 S2↔S3 双向字节比对后取值）。
EXPECTED_CONTROL = {
    "Package": "fantgpu-fh2m",
    "Version": "3.3.8.126-driver-linux-desktop-sp-generic",
    "Architecture": "amd64",
    "Maintainer": "fant <service@fantasyxpu.com>",
    "Description": "Fantasy II driver",
}
# C：ls DEBIAN/ 与 dpkg-deb --ctrl-tarfile 实测，两侧均为这五个成员。
EXPECTED_CONTROL_FILES = ("control", "md5sums", "postinst", "postrm", "prerm")
# B/C：dpkg-deb -c 实测类型直方图为 602 常规 + 88 目录 + 58 符号链接，但其中
# 一个 "目录" 是归档根 `./` 自身；本工具把归档根归一为空串后跳过，故**载荷**
# 目录数为 87（解包树同为 87；`find -type d` 得 89 = 87 + DEBIAN + 树根）。
# md5sums 行数与载荷常规文件数相等——DEBIAN/ 控制文件按 Debian 规范不入
# md5sums。
EXPECTED_COUNTS = {
    "dir": 87,
    "file": 602,
    "symlink": 58,
    "md5sums_rows": 602,
}

# C：上游 md5sums **路径列**缺陷的锁定基线（本轮实测，见 RELOCATION 说明）。
# 507 条路径逐值精确命中；其余 95 条按三段可复合重定位后逐值精确命中，且与
# 「未登记的 95 个常规文件」构成严格双射 → 内容完整性得证，缺陷严格限定在
# md5sums 的路径列。任一计数变化都意味着载荷或上游打包方式改变，须重新裁决。
EXPECTED_MD5SUMS_EXACT = 507
EXPECTED_MD5SUMS_RELOCATED_TOTAL = 95
EXPECTED_MD5SUMS_RELOCATED_BY_CLASS = {
    "opt": 54,
    "opt+privlib": 4,
    "privlib": 36,
    "docdir": 1,
}
PRIVLIB_RE = re.compile(r"^(.*usr/lib/(?:x86_64|i386)-linux-gnu/)([^/]+)$")

# E：C3-a 落地所需的 F 血统关键路径（逐项 find 实测存在）。缺任一即说明载荷
# 不足以构成 coherent F stack，审计不得判 PASS。
REQUIRED_F_PATHS = (
    "etc/OpenCL/vendors/FANT_fh2m.icd",
    "etc/cdi/fantgpu-fh2m.yaml",
    "etc/modprobe.d/blacklist-fh2m.conf",
    "etc/vulkan/icd.d/fh2m_conf.json",
    "lib/firmware/fantgpu/fh2m/fh2c.fw",
    "lib/firmware/fantgpu/fh2m/fh2c.sh",
    "lib/firmware/fantgpu/fh2m/fh2m.fw",
    "lib/firmware/fantgpu/fh2m/fh2m.sh",
    "opt/fantgpu-fh2m/usr/lib/xorg/modules/drivers/fh2m_drv.so.1.19",
    "opt/fantgpu-fh2m/usr/lib/xorg/modules/drivers/fh2m_drv.so.1.20",
    "opt/fantgpu-fh2m/usr/lib/xorg/modules/drivers/fh2m_drv.so.1.21",
    "usr/lib/i386-linux-gnu/dri/fh2m_dri.so",
    "usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so",
    "usr/lib/x86_64-linux-gnu/dri/fh2m_drv_video.so",
    "usr/lib/x86_64-linux-gnu/gbm/fh2m_gbm.so",
    "usr/share/X11/xorg.conf.d/10-fh2m.conf",
    "usr/share/drirc.d/01-fh2m_drv.conf",
    "usr/share/fantgpu-fh2m-kernel-dkms/postinst",
    "usr/share/glvnd/egl_vendor.d/00_fh2m.json",
    "usr/src/fantgpu-fh2m-kernel-2.2/dkms.conf",
)

# E：O 血统 loader 文件名。命中任一即说明载荷混入 O 血统 loader，会破坏
# drmGetVersion()->name == "fh2m" 的 loader 寻径相干性。
# 注意：仅检查**文件名**，不检查文件内容——etc/modprobe.d/blacklist-fh2m.conf
# 与 DEBIAN/control 的内容**合法地**包含 innogpu 令牌（前者黑名单 O 内核模块、
# 后者 Replaces/Conflicts O 包名），按内容判定会产生假阳性。
FORBIDDEN_O_LOADER_NAMES = (
    "innogpu_dri.so",
    "innogpu_drv.so",
    "innogpu_drv_video.so",
    "innogpu_gbm.so",
    "inno_drv_video.so",
)

# F：docs/planning/fantgpu-base-update-evaluation.md:229-232 记录的 8 位前缀，
# 本轮 sha256sum 独立复算得到完整值并逐值一致。值 = (完整 SHA-256, 文档前缀,
# 文档行号)。fh2m.sh/fh2c.sh 两血统同值（评估文档记「相同」）。
EXPECTED_FIRMWARE_SHA256 = {
    "lib/firmware/fantgpu/fh2m/fh2m.fw": (
        "8d39a4056ea1e12443cbe5701db4e84940875a0bf1c29c62d293c61704a5ada3",
        "8d39a405", "fantgpu-base-update-evaluation.md:229"),
    "lib/firmware/fantgpu/fh2m/fh2m.sh": (
        "f2b0ead7edb7f06486e25b12534969f321cd28bdaf4b784b42f3c48221a6d0e5",
        "f2b0ead7", "fantgpu-base-update-evaluation.md:230"),
    "lib/firmware/fantgpu/fh2m/fh2c.fw": (
        "9604363034d0f2a73fa200aca3a5161c9055cc0bc65df88093da1e5244a59d8e",
        "96043630", "fantgpu-base-update-evaluation.md:231"),
    "lib/firmware/fantgpu/fh2m/fh2c.sh": (
        "f2b0ead7edb7f06486e25b12534969f321cd28bdaf4b784b42f3c48221a6d0e5",
        "f2b0ead7", "fantgpu-base-update-evaluation.md:232"),
}

# G：opt/fantgpu-fh2m 下 DDX ABI 变体。F 包 postinst:122-136 在安装期探测
# `Xorg -version` 后从这三个变体择一复制到 /usr/lib/xorg/modules/drivers/
# fh2m_drv.so，故三者齐备是 ④ builder 改造能在构建期预选 ABI 的前提。
EXPECTED_DDX_ABI_VARIANTS = ("1.19", "1.20", "1.21")
# G：postinst 设备门 PCI ID（codex 初审 P2 修复：锁定并断言，不只记录）。
# 真机设备 = 1ec8:9810（builder vermagic 分支与 run-dmabuf-regression-test
# .sh:130 同值；实测 postinst:154/:156 两处）。缺失或出现未锁定 ID 均 FAIL。
EXPECTED_PCI_IDS = ("1ec8:9810",)
DDX_GLOB_RE = re.compile(
    r"^opt/fantgpu-fh2m/usr/lib/xorg/modules/drivers/"
    r"fh2m_drv\.so\.(\d+\.\d+)$")
PCI_ID_RE = re.compile(r"\b([0-9a-fA-F]{4}:[0-9a-fA-F]{4})\b")
MD5SUMS_RE = re.compile(r"^([0-9a-fA-F]{32})[ \t]+(.+)$")
TYPE_KEY = {"d": "dir", "f": "file", "l": "symlink"}

# --------------------------------------------------------------- 基线机制
# 生产运行不传 --baseline，使用上面锁定的默认常量。单测用合成 deb 驱动全部
# 分支时必须提供**完整**基线（键集严格相等，缺失或多余一律拒绝），避免"部分
# 覆盖"在生产中被用来放宽断言。产物 JSON 记录 baseline_source，使入库证据
# 自证其使用的是锁定默认值。
BASELINE_KEYS = (
    "deb_sha256",
    "deb_size",
    "control",
    "control_files",
    "counts",
    "md5sums_exact",
    "md5sums_relocated_total",
    "md5sums_relocated_by_class",
    "required_f_paths",
    "forbidden_o_loader_names",
    "firmware_sha256",
    "ddx_abi_variants",
    "pci_ids",
)


def default_baseline():
    return {
        "deb_sha256": EXPECTED_DEB_SHA256,
        "deb_size": EXPECTED_DEB_SIZE,
        "control": dict(EXPECTED_CONTROL),
        "control_files": list(EXPECTED_CONTROL_FILES),
        "counts": dict(EXPECTED_COUNTS),
        "md5sums_exact": EXPECTED_MD5SUMS_EXACT,
        "md5sums_relocated_total": EXPECTED_MD5SUMS_RELOCATED_TOTAL,
        "md5sums_relocated_by_class": dict(
            EXPECTED_MD5SUMS_RELOCATED_BY_CLASS),
        "required_f_paths": list(REQUIRED_F_PATHS),
        "forbidden_o_loader_names": list(FORBIDDEN_O_LOADER_NAMES),
        "firmware_sha256": {k: list(v) for k, v in
                            EXPECTED_FIRMWARE_SHA256.items()},
        "ddx_abi_variants": list(EXPECTED_DDX_ABI_VARIANTS),
        "pci_ids": list(EXPECTED_PCI_IDS),
    }


def load_baseline(path):
    try:
        with open(path, "r", encoding="utf-8") as fh:
            data = json.load(fh)
    except (OSError, ValueError) as exc:
        die("baseline unreadable or not JSON: %s (%s)" % (path, exc))
    if not isinstance(data, dict):
        die("baseline must be a JSON object: %s" % path)
    missing = sorted(set(BASELINE_KEYS) - set(data))
    extra = sorted(set(data) - set(BASELINE_KEYS))
    if missing or extra:
        die("baseline key set must match exactly; missing=%s extra=%s"
            % (missing or "[]", extra or "[]"))
    return data


def eprint(msg):
    sys.stderr.write(msg + "\n")


def die(msg):
    eprint("ERROR: %s" % msg)
    sys.exit(EX_USAGE)


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def hash_bytes(data):
    return (hashlib.sha256(data).hexdigest(), hashlib.md5(data).hexdigest())


def count_types(entries):
    counts = {"d": 0, "f": 0, "l": 0}
    for e in entries.values():
        counts[e["type"]] += 1
    return {"dir": counts["d"], "file": counts["f"], "symlink": counts["l"]}


def is_payload_path(path):
    return path != "DEBIAN" and not path.startswith(DEBIAN_PREFIX)


# ------------------------------------------------------------ 证据源读取

def check_protected_out(root, out_dir):
    """拒绝把产物写进保护区；out_dir 允许在仓库外（单测 tmpfs）。"""
    out_abs = os.path.realpath(out_dir)
    root_abs = os.path.realpath(root)
    if out_abs != root_abs and not out_abs.startswith(root_abs + os.sep):
        return  # 仓库外（单测 tmpfs），不受保护区约束
    rel = os.path.relpath(out_abs, root_abs)
    if rel == ".." or rel.startswith("../") or rel == ".":
        return
    if rel.split(os.sep)[0] in PROTECTED_TOP:
        die("--out-dir must not resolve inside a protected area: %s" % rel)


def stream_deb_tar(deb, flag):
    """流式打开 deb 的 data/ctrl 归档；返回 (Popen, tarfile)。调用方负责收尾。"""
    proc = subprocess.Popen(["dpkg-deb", flag, deb], stdout=subprocess.PIPE)
    try:
        tf = tarfile.open(fileobj=proc.stdout, mode="r|")
    except Exception:
        proc.kill()
        proc.wait()
        raise
    return proc, tf


def close_deb_tar(proc, tf, deb, flag):
    tf.close()
    if proc.stdout is not None:
        proc.stdout.close()
    rc = proc.wait()
    if rc != 0:
        die("dpkg-deb %s failed (rc=%d): %s" % (flag, rc, deb))


def norm_tar_name(name):
    """归一 tar 成员名：去掉 "./" 前缀与尾斜杠；归档根（"./" 或 "."）→ 空串。"""
    name = name.strip()
    while name.startswith("./"):
        name = name[2:]
    name = name.rstrip("/")
    return "" if name == "." else name


def read_deb_fsys(deb):
    """S1：data 归档 → {path: entry}。entry 含 type/size/mode/sha256/md5/target。"""
    entries = {}
    proc, tf = stream_deb_tar(deb, "--fsys-tarfile")
    try:
        for m in tf:
            path = norm_tar_name(m.name)
            if not path:
                continue
            if path in entries:
                die("duplicate deb data member: %s" % path)
            if m.isdir():
                entries[path] = {"type": "d", "size": None, "target": None,
                                 "mode": m.mode & 0o7777,
                                 "sha256": None, "md5": None}
            elif m.issym():
                entries[path] = {"type": "l", "size": None,
                                 "target": m.linkname,
                                 "mode": m.mode & 0o7777,
                                 "sha256": None, "md5": None}
            elif m.isreg():
                fh = tf.extractfile(m)
                if fh is None:
                    die("deb data member unreadable: %s" % path)
                buf = fh.read()
                sha, md5 = hash_bytes(buf)
                entries[path] = {"type": "f", "size": len(buf),
                                 "target": None, "mode": m.mode & 0o7777,
                                 "sha256": sha, "md5": md5}
            else:
                die("unexpected deb data member type (%r) for %s"
                    % (m.type, path))
    finally:
        close_deb_tar(proc, tf, deb, "--fsys-tarfile")
    return entries


def read_deb_ctrl(deb):
    """S2：control 归档 → {成员名: bytes}。"""
    ctrl = {}
    proc, tf = stream_deb_tar(deb, "--ctrl-tarfile")
    try:
        for m in tf:
            path = norm_tar_name(m.name)
            if not path:
                continue  # 归档根
            if m.isdir():
                continue  # DEBIAN/ 目录自身，非控制成员
            if not m.isreg():
                die("unexpected deb control member type (%r) for %s"
                    % (m.type, path))
            fh = tf.extractfile(m)
            if fh is None:
                die("deb control member unreadable: %s" % path)
            if path in ctrl:
                die("duplicate deb control member: %s" % path)
            ctrl[path] = fh.read()
    finally:
        close_deb_tar(proc, tf, deb, "--ctrl-tarfile")
    return ctrl


def read_unpack(unpack):
    """S3：本地解包树 → {path: entry}。符号链接不跟随。"""
    entries = {}
    for dirpath, dirnames, filenames in os.walk(unpack, followlinks=False):
        dirnames.sort()
        filenames.sort()
        for name in dirnames + filenames:
            abs_path = os.path.join(dirpath, name)
            rel = os.path.relpath(abs_path, unpack).replace(os.sep, "/")
            if rel in entries:
                die("duplicate unpack entry: %s" % rel)
            st = os.lstat(abs_path)
            if os.path.islink(abs_path):
                entries[rel] = {"type": "l", "size": None,
                                "target": os.readlink(abs_path),
                                "mode": st.st_mode & 0o7777,
                                "sha256": None, "md5": None}
            elif os.path.isdir(abs_path):
                entries[rel] = {"type": "d", "size": None, "target": None,
                                "mode": st.st_mode & 0o7777,
                                "sha256": None, "md5": None}
            elif os.path.isfile(abs_path):
                with open(abs_path, "rb") as fh:
                    buf = fh.read()
                sha, md5 = hash_bytes(buf)
                entries[rel] = {"type": "f", "size": len(buf), "target": None,
                                "mode": st.st_mode & 0o7777,
                                "sha256": sha, "md5": md5}
            else:
                die("unexpected filesystem entry type: %s" % rel)
    return entries


def parse_control_fields(raw):
    """解析 Debian control 首段单行字段（多行续行以空格/Tab 起首）。"""
    fields = {}
    key = None
    for line in raw.decode("utf-8", "replace").split("\n"):
        if not line.strip():
            break  # 首段结束
        if line[:1] in (" ", "\t"):
            if key is not None:
                fields[key] += "\n" + line.strip()
            continue
        if ":" not in line:
            continue
        key, _, val = line.partition(":")
        fields[key.strip()] = val.strip()
    return fields


def parse_md5sums(raw):
    """解析 DEBIAN/md5sums → {path: md5}；格式非法即 die（fail-closed）。"""
    rows = {}
    for lineno, line in enumerate(
            raw.decode("utf-8", "replace").split("\n"), 1):
        if not line.strip():
            continue
        m = MD5SUMS_RE.match(line)
        if not m:
            die("malformed md5sums line %d: %r" % (lineno, line[:80]))
        path = m.group(2).strip()
        while path.startswith("./"):
            path = path[2:]
        if path in rows:
            die("duplicate md5sums path: %s" % path)
        rows[path] = m.group(1).lower()
    return rows


def apply_relocations(path):
    """上游 md5sums 路径列缺陷的三段可复合重定位模型。

    厂商的 md5sums 是对一个**重定位前**的暂存布局生成的，缺少三个组件：
    `opt/fantgpu-fh2m/`、`usr/lib/<triplet>/fantgpu-fh2m/`、
    `usr/share/doc/fantgpu-fh2m/`。三段各自独立判定、可复合（`opt/` 下的私有
    库同时命中前两段），且幂等（已含该组件则不重复插入）。

    返回 (已应用类名元组, 重写后路径)。
    """
    applied = []
    q = path
    if q.startswith("opt/") and not q.startswith("opt/fantgpu-fh2m/"):
        q = "opt/fantgpu-fh2m/" + q[len("opt/"):]
        applied.append("opt")
    m = PRIVLIB_RE.match(q)
    if m:
        # 余部被锚定为单个文件名，故二次应用时（余部含 "/"）正则不再匹配，
        # 幂等性由锚点本身保证，无需额外守卫。
        q = m.group(1) + "fantgpu-fh2m/" + m.group(2)
        applied.append("privlib")
    doc = "usr/share/doc/"
    if q.startswith(doc) and not q[len(doc):].startswith("fantgpu-fh2m/"):
        q = doc + "fantgpu-fh2m/" + q[len(doc):]
        applied.append("docdir")
    return tuple(applied), q


# ------------------------------------------------------------------ 检查

def check_a_source_identity(bl, deb_rel, deb_sha, deb_size, s2, failures):
    want_control = bl["control"]
    ctrl_fields = parse_control_fields(s2["control"]) if "control" in s2 else {}
    diff = {k: {"expected": v, "actual": ctrl_fields.get(k)}
            for k, v in sorted(want_control.items())
            if ctrl_fields.get(k) != v}
    a = {
        "deb_path": deb_rel,
        "deb_sha256_expected": bl["deb_sha256"],
        "deb_sha256_actual": deb_sha,
        "deb_sha256_match": deb_sha == bl["deb_sha256"],
        "deb_size_expected": bl["deb_size"],
        "deb_size_actual": deb_size,
        "deb_size_match": deb_size == bl["deb_size"],
        "control_fields_expected": want_control,
        "control_fields_actual": {k: ctrl_fields.get(k)
                                  for k in sorted(want_control)},
        "control_diff": diff,
    }
    a["pass"] = (a["deb_sha256_match"] and a["deb_size_match"] and not diff)
    if not a["deb_sha256_match"]:
        failures.append("A: deb SHA-256 mismatch (expected %s, got %s)"
                        % (bl["deb_sha256"], deb_sha))
    if not a["deb_size_match"]:
        failures.append("A: deb size mismatch (expected %d, got %d)"
                        % (bl["deb_size"], deb_size))
    for k, v in sorted(diff.items()):
        failures.append("A: control field %s mismatch (expected %r, got %r)"
                        % (k, v["expected"], v["actual"]))
    return a


def check_b_payload_structure(bl, s1, s3_payload, failures):
    want_counts = bl["counts"]
    only_deb = sorted(set(s1) - set(s3_payload))
    only_unpack = sorted(set(s3_payload) - set(s1))
    field_diffs = []
    for path in sorted(set(s1) & set(s3_payload)):
        for field in ("type", "size", "mode", "sha256", "target"):
            if s1[path][field] != s3_payload[path][field]:
                field_diffs.append({"path": path, "field": field,
                                    "deb": s1[path][field],
                                    "unpack": s3_payload[path][field]})
    counts_deb = count_types(s1)
    counts_unpack = count_types(s3_payload)
    count_diff = {}
    for kind in ("dir", "file", "symlink"):
        if (counts_deb[kind] != want_counts[kind]
                or counts_unpack[kind] != want_counts[kind]):
            count_diff[kind] = {"expected": want_counts[kind],
                                "deb": counts_deb[kind],
                                "unpack": counts_unpack[kind]}
    b = {
        "counts_expected": {k: want_counts[k]
                            for k in ("dir", "file", "symlink")},
        "counts_deb": counts_deb,
        "counts_unpack": counts_unpack,
        "count_diff": count_diff,
        "entries_deb": len(s1),
        "entries_unpack": len(s3_payload),
        "only_in_deb": only_deb,
        "only_in_unpack": only_unpack,
        "field_diffs": field_diffs,
    }
    b["pass"] = not (only_deb or only_unpack or field_diffs or count_diff)
    if only_deb:
        failures.append("B: %d entries only in deb data archive (first: %s)"
                        % (len(only_deb), only_deb[0]))
    if only_unpack:
        failures.append("B: %d entries only in unpack tree (first: %s)"
                        % (len(only_unpack), only_unpack[0]))
    if field_diffs:
        failures.append("B: %d field mismatches (first: %s.%s deb=%r "
                        "unpack=%r)" % (len(field_diffs),
                                        field_diffs[0]["path"],
                                        field_diffs[0]["field"],
                                        field_diffs[0]["deb"],
                                        field_diffs[0]["unpack"]))
    for kind, v in sorted(count_diff.items()):
        failures.append("B: %s count mismatch (expected %d, deb %d, "
                        "unpack %d)" % (kind, v["expected"], v["deb"],
                                        v["unpack"]))
    return b


def check_c_control_and_md5sums(bl, s2, s3, failures):
    # ---- C-1 控制成员：S2 ↔ S3 的 DEBIAN/ 子树，双向字节相等
    s3_ctrl = {p[len(DEBIAN_PREFIX):]: e for p, e in s3.items()
               if p.startswith(DEBIAN_PREFIX) and e["type"] == "f"}
    ctrl_only_deb = sorted(set(s2) - set(s3_ctrl))
    ctrl_only_unpack = sorted(set(s3_ctrl) - set(s2))
    ctrl_bytes_mismatch = sorted(
        n for n in set(s2) & set(s3_ctrl)
        if hashlib.sha256(s2[n]).hexdigest() != s3_ctrl[n]["sha256"])
    ctrl_expected_diff = sorted(
        (set(bl["control_files"]) ^ set(s2))
        | (set(bl["control_files"]) ^ set(s3_ctrl)))

    # ---- C-2 md5sums：精确子集 + 重定位子集
    md5sums = parse_md5sums(s2["md5sums"]) if "md5sums" in s2 else {}
    payload_files = {p: e for p, e in s3.items()
                     if e["type"] == "f" and is_payload_path(p)}
    rows_diff = len(md5sums) != bl["counts"]["md5sums_rows"]

    exact_mismatch = []
    absent = {}
    exact_count = 0
    for path in sorted(md5sums):
        if path in payload_files:
            exact_count += 1
            if payload_files[path]["md5"] != md5sums[path]:
                exact_mismatch.append(path)
        else:
            absent[path] = md5sums[path]
    unlisted = {p: e["md5"] for p, e in payload_files.items()
                if p not in md5sums}

    relocated = {}
    reloc_unexplained = []
    reloc_md5_mismatch = []
    by_class = {}
    for path in sorted(absent):
        classes, rewritten = apply_relocations(path)
        if not classes or rewritten == path or rewritten not in payload_files:
            reloc_unexplained.append({"md5sums_path": path,
                                      "classes": list(classes),
                                      "rewritten": rewritten})
            continue
        if payload_files[rewritten]["md5"] != absent[path]:
            reloc_md5_mismatch.append({"md5sums_path": path,
                                       "rewritten": rewritten})
            continue
        relocated[path] = {"classes": list(classes), "rewritten": rewritten}
        key = "+".join(classes)
        by_class[key] = by_class.get(key, 0) + 1

    images = [v["rewritten"] for v in relocated.values()]
    injective = len(set(images)) == len(images)
    leftover = sorted(set(unlisted) - set(images))
    bijection = injective and not leftover and len(images) == len(unlisted)
    counts_ok = (exact_count == bl["md5sums_exact"]
                 and len(relocated) == bl["md5sums_relocated_total"]
                 and by_class == bl["md5sums_relocated_by_class"])

    c = {
        "control_files_expected": list(bl["control_files"]),
        "control_files_deb": sorted(s2),
        "control_files_unpack": sorted(s3_ctrl),
        "control_only_in_deb": ctrl_only_deb,
        "control_only_in_unpack": ctrl_only_unpack,
        "control_bytes_mismatch": ctrl_bytes_mismatch,
        "control_set_diff_vs_expected": ctrl_expected_diff,
        "md5sums_rows": len(md5sums),
        "md5sums_rows_expected": bl["counts"]["md5sums_rows"],
        "payload_files": len(payload_files),
        "md5sums_exact_count": exact_count,
        "md5sums_exact_expected": bl["md5sums_exact"],
        "md5sums_exact_mismatch": exact_mismatch,
        "md5sums_relocated_count": len(relocated),
        "md5sums_relocated_expected": bl["md5sums_relocated_total"],
        "md5sums_relocated_by_class": by_class,
        "md5sums_relocated_by_class_expected":
            bl["md5sums_relocated_by_class"],
        "md5sums_relocated_unexplained": reloc_unexplained,
        "md5sums_relocated_md5_mismatch": reloc_md5_mismatch,
        "unlisted_payload_files": len(unlisted),
        "unlisted_leftover": leftover,
        "relocation_injective": injective,
        "relocation_bijection": bijection,
        "relocation_counts_match": counts_ok,
        "upstream_md5sums_path_defect": {
            "verdict": "confirmed-and-bounded",
            "characterization": (
                "DEBIAN/md5sums was generated against a pre-relocation "
                "staging layout that lacks three path components: "
                "opt/fantgpu-fh2m/, usr/lib/<triplet>/fantgpu-fh2m/ and "
                "usr/share/doc/fantgpu-fh2m/. %d rows match exactly; the "
                "remaining %d rows match exactly after applying that "
                "relocation, the mapping is injective, and its image equals "
                "the set of %d otherwise-unlisted payload files. Content "
                "integrity is therefore established; the defect is confined "
                "to the md5sums path column."
                % (exact_count, len(relocated), len(unlisted))),
            "content_integrity_basis": (
                "check B proves the unpack is byte-faithful to the deb data "
                "archive for all payload entries (type/size/mode/sha256/"
                "symlink target), and check D pins every file by SHA-256; "
                "the vendor md5sums is not the integrity authority here"),
            "dpkg_v_impact": (
                "dpkg -V on a real machine reports these %d paths as missing "
                "and leaves %d real files unowned; this is an upstream "
                "packaging artifact and must not be misattributed to the 030 "
                "chain during R-item verification"
                % (len(relocated), len(unlisted))),
            "obligation_for_audit_item_4": (
                "the C3-a builder rework must regenerate md5sums from the "
                "payload it actually installs, and must not inherit the "
                "vendor file verbatim"),
        },
    }
    c["pass"] = not (ctrl_only_deb or ctrl_only_unpack or ctrl_bytes_mismatch
                     or ctrl_expected_diff or rows_diff or exact_mismatch
                     or reloc_unexplained or reloc_md5_mismatch or leftover) \
        and injective and counts_ok
    if ctrl_only_deb:
        failures.append("C: control members only in deb: %s"
                        % ", ".join(ctrl_only_deb))
    if ctrl_only_unpack:
        failures.append("C: control members only in unpack DEBIAN/: %s"
                        % ", ".join(ctrl_only_unpack))
    if ctrl_bytes_mismatch:
        failures.append("C: DEBIAN/ bytes differ from deb control archive: %s"
                        % ", ".join(ctrl_bytes_mismatch))
    if ctrl_expected_diff:
        failures.append("C: control member set differs from expected: %s"
                        % ", ".join(ctrl_expected_diff))
    if rows_diff:
        failures.append("C: md5sums rows %d != expected %d"
                        % (len(md5sums), bl["counts"]["md5sums_rows"]))
    if exact_mismatch:
        failures.append("C: %d exact-path MD5 value mismatches (first: %s)"
                        % (len(exact_mismatch), exact_mismatch[0]))
    if reloc_unexplained:
        failures.append("C: %d md5sums paths not explained by the relocation "
                        "model (first: %s)"
                        % (len(reloc_unexplained),
                           reloc_unexplained[0]["md5sums_path"]))
    if reloc_md5_mismatch:
        failures.append("C: %d relocated md5sums MD5 value mismatches "
                        "(first: %s)"
                        % (len(reloc_md5_mismatch),
                           reloc_md5_mismatch[0]["md5sums_path"]))
    if not injective:
        failures.append("C: relocation mapping is not injective")
    if leftover:
        failures.append("C: %d payload files unlisted in md5sums and not "
                        "reached by the relocation (first: %s)"
                        % (len(leftover), leftover[0]))
    if not counts_ok:
        failures.append("C: md5sums subset counts differ from locked "
                        "baseline (exact %d/%d, relocated %d/%d, by_class "
                        "%s vs %s)"
                        % (exact_count, bl["md5sums_exact"],
                           len(relocated),
                           bl["md5sums_relocated_total"],
                           json.dumps(by_class, sort_keys=True),
                           json.dumps(bl["md5sums_relocated_by_class"],
                                      sort_keys=True)))
    return c


def resolve_payload_symlink(s3, path, max_hops=40):
    """在载荷根内逐跳解析符号链接 → (最终路径, 是否解析到常规文件, 链)。"""
    cur = path
    chain = []
    for _ in range(max_hops):
        ent = s3.get(cur)
        if ent is None:
            return None, False, chain  # 悬空：目标不存在
        if ent["type"] == "f":
            return cur, True, chain
        if ent["type"] != "l":
            return None, False, chain  # 指向目录/非常规文件
        target = ent["target"]
        chain.append(target)
        if target.startswith("/"):
            # 绝对目标：按载荷根解释（安装后载荷根即系统根）
            nxt = target.lstrip("/")
        else:
            base = os.path.dirname(cur)
            nxt = os.path.normpath(os.path.join(base, target))
            nxt = nxt.replace(os.sep, "/")
        if nxt == ".." or nxt.startswith("../"):
            return None, False, chain  # 逃出载荷根
        cur = nxt
    return None, False, chain  # 环或链过长


def check_e_lineage_coherence(bl, s3, failures):
    resolved = {}
    missing = []
    broken = []
    wrong_type = []
    for path in bl["required_f_paths"]:
        ent = s3.get(path)
        if ent is None:
            missing.append(path)
        elif ent["type"] == "f":
            resolved[path] = {"kind": "regular"}
        elif ent["type"] == "l":
            final, ok, chain = resolve_payload_symlink(s3, path)
            if ok:
                resolved[path] = {"kind": "symlink", "chain": chain,
                                  "resolves_to": final}
            else:
                broken.append({"path": path, "chain": chain})
        else:
            wrong_type.append({"path": path, "type": ent["type"]})

    dangling = []
    for path in sorted(p for p, e in s3.items() if e["type"] == "l"):
        final, ok, chain = resolve_payload_symlink(s3, path)
        if not ok:
            dangling.append({"path": path, "chain": chain})

    basenames = {os.path.basename(p) for p in s3}
    forbidden = sorted(basenames & set(bl["forbidden_o_loader_names"]))
    e = {
        "required_f_paths": list(bl["required_f_paths"]),
        "required_resolved": {k: resolved[k] for k in sorted(resolved)},
        "required_missing": missing,
        "required_broken_symlink": broken,
        "required_wrong_type": wrong_type,
        "symlink_count": sum(1 for x in s3.values() if x["type"] == "l"),
        "dangling_symlinks": dangling,
        "forbidden_o_loader_names": list(bl["forbidden_o_loader_names"]),
        "forbidden_present": forbidden,
        "scope_note": ("filename-level only; blacklist-fh2m.conf and "
                       "DEBIAN/control legitimately carry innogpu tokens in "
                       "their contents, so a content-level scan would "
                       "false-positive"),
    }
    e["pass"] = not (missing or broken or wrong_type or dangling or forbidden)
    for p in missing:
        failures.append("E: required F-lineage path missing: %s" % p)
    for b in broken:
        failures.append("E: required F-lineage symlink unresolvable: %s "
                        "(chain: %s)" % (b["path"], " -> ".join(b["chain"])))
    for w in wrong_type:
        failures.append("E: required F-lineage path has unexpected type %s: %s"
                        % (w["type"], w["path"]))
    for d in dangling:
        failures.append("E: dangling symlink in payload: %s (chain: %s)"
                        % (d["path"], " -> ".join(d["chain"])))
    for p in forbidden:
        failures.append("E: O-lineage loader filename present: %s" % p)
    return e


def check_f_firmware(bl, s3, failures):
    fw = {}
    mismatched = []
    for path, spec in sorted(bl["firmware_sha256"].items()):
        want, prefix, ref = spec[0], spec[1], spec[2]
        got = s3.get(path, {}).get("sha256")
        ok = got == want
        prefix_ok = want.startswith(prefix)
        fw[path] = {"sha256_expected": want, "sha256_actual": got,
                    "match": ok, "eval_doc_prefix": prefix,
                    "eval_doc_ref": ref, "prefix_consistent": prefix_ok}
        if not ok or not prefix_ok:
            mismatched.append(path)
    f = {"firmware": fw, "mismatched": mismatched}
    f["pass"] = not mismatched
    for path in mismatched:
        failures.append("F: firmware SHA-256 mismatch vs evaluation-doc "
                        "record: %s" % path)
    return f


def check_g_install_time_staging(bl, s3, unpack, failures):
    ddx = {}
    opt_files = 0
    for path, ent in s3.items():
        if ent["type"] != "f":
            continue
        m = DDX_GLOB_RE.match(path)
        if m:
            ddx[m.group(1)] = path
        if path.startswith("opt/fantgpu-fh2m/"):
            opt_files += 1
    ddx_missing = sorted(set(bl["ddx_abi_variants"]) - set(ddx))
    pci_ids = []
    postinst = os.path.join(unpack, DEBIAN_PREFIX + "postinst")
    if os.path.isfile(postinst) and not os.path.islink(postinst):
        with open(postinst, "r", encoding="utf-8", errors="replace") as fh:
            pci_ids = sorted(set(x.lower() for x in PCI_ID_RE.findall(
                fh.read())))
    pci_missing = sorted(set(bl["pci_ids"]) - set(pci_ids))
    pci_unexpected = sorted(set(pci_ids) - set(bl["pci_ids"]))
    g = {
        "opt_staging_files": opt_files,
        "ddx_abi_variants_expected": list(bl["ddx_abi_variants"]),
        "ddx_abi_variants_found": {k: ddx[k] for k in sorted(ddx)},
        "ddx_abi_missing": ddx_missing,
        "postinst_pci_ids_expected": list(bl["pci_ids"]),
        "postinst_pci_ids": pci_ids,
        "postinst_pci_missing": pci_missing,
        "postinst_pci_unexpected": pci_unexpected,
        "note": ("opt/fantgpu-fh2m is install-time staging: DEBIAN/postinst "
                 "probes Xorg -version (drv_install, :122-136), "
                 "/etc/os-release ID+VERSION_ID, /usr/share/alsa/ucm2 "
                 "presence and pulseaudio --version (ucm_install, :64-120), "
                 "and libwayland-client0 version (update_wayland, :29-48) to "
                 "materialize DDX / ALSA UCM / wayland compat libs from "
                 "/opt into /usr at install time. Recorded as a fact for "
                 "C3-a audit item 4 (reproducible build input); it is not a "
                 "defect of audit item 3. The postinst device gate must "
                 "carry exactly the locked PCI ID(s): a missing or "
                 "non-locked ID fails this check."),
    }
    g["pass"] = not ddx_missing and not pci_missing and not pci_unexpected
    for v in ddx_missing:
        failures.append("G: DDX ABI variant missing: fh2m_drv.so.%s" % v)
    for v in pci_missing:
        failures.append("G: postinst PCI ID missing: %s" % v)
    for v in pci_unexpected:
        failures.append("G: postinst PCI ID unexpected: %s" % v)
    return g


# ------------------------------------------------------------------ 产物

def build_manifest_tsv(s3):
    rows = []
    for path in sorted(s3):
        e = s3[path]
        rows.append("\t".join((
            e["type"],
            path,
            "" if e["size"] is None else str(e["size"]),
            "%04o" % e["mode"],
            e["sha256"] or "",
            e["md5"] or "",
            e["target"] or "",
        )))
    return ("\n".join(rows) + "\n").encode("utf-8")


def build_integrity_json(deb_rel, unpack_rel, deb_sha, deb_size, checks,
                         manifest_bytes, s3, baseline_source):
    s3_payload = {p: e for p, e in s3.items() if is_payload_path(p)}
    manifest_sha = hashlib.sha256(manifest_bytes).hexdigest()
    payload_files = {p: e for p, e in s3_payload.items() if e["type"] == "f"}
    return {
        "schema_version": SCHEMA_VERSION,
        "audit_id": AUDIT_ID,
        "lineage": LINEAGE,
        "generator": PROG,
        "baseline_source": baseline_source,
        "source_deb": {"path": deb_rel, "sha256": deb_sha, "size": deb_size},
        "unpack_tree": unpack_rel,
        "entry_counts_full_tree": count_types(s3),
        "entry_counts_payload": count_types(s3_payload),
        "payload_files": len(payload_files),
        "payload_bytes": sum(e["size"] for e in payload_files.values()),
        "manifest": {
            "file": MANIFEST_NAME,
            "columns": list(MANIFEST_COLUMNS),
            "rows": len(s3),
            "row_scope": "full unpack tree including DEBIAN/",
            "sha256": manifest_sha,
        },
        "checks": checks,
        "overall_pass": all(c["pass"] for c in checks.values()),
        "audit_scope": {
            "closed_by_this_audit": "C3-a precondition 3 (integrity)",
            "not_closed_by_this_audit": [
                "C3-a precondition 1 (source chain): closed by dsh 2026-09-11 "
                "at level 'official channel confirmed + metadata consistent'",
                "C3-a precondition 2 (closed-source blob authorization): "
                "ruled self-use-only, no redistribution, by dsh 2026-09-11",
                "C3-a precondition 4 (reproducible build input): pending - "
                "binary-manifest.json F entry family + builder rework + "
                "double-build byte identity",
            ],
        },
        "reproducibility_note": (
            "Artifacts carry no wall-clock timestamp; provenance is the git "
            "commit that introduces them. gen is byte-idempotent and verify "
            "recomputes from the deb plus the unpack tree without writing."),
    }


def render_json(integrity_obj):
    return (json.dumps(integrity_obj, indent=2, sort_keys=True,
                       ensure_ascii=False) + "\n").encode("utf-8")


# ------------------------------------------------------------ 提交事务
# 三件产物必须同代：任一时刻目标目录要么全部旧代、要么全部新代（或残留由
# 下次启动恢复），杜绝「manifest 新 + sidecar/JSON 旧」的混代证据。
# 事务顺序：暂存(mkstemp) → journal backing_up → 备份 → journal committing
# → 逐个 replace → journal committed → 清理 .bak/临时/journal。清理失败不
# 回滚（O-4 先例：提交点后口径），残留由下次 gen 的 recover_txn_residue 处理。

def _fsync_dir(path):
    try:
        fd = os.open(path, os.O_RDONLY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
    except OSError:
        pass  # 目录 fsync 失败不回滚事务；原子性由 replace 序列保证


def _txn_paths(out_dir):
    return [os.path.join(out_dir, n)
            for n in (MANIFEST_NAME, MANIFEST_SHA_NAME, INTEGRITY_NAME)]


def _txn_temps(out_dir):
    return sorted(os.path.join(out_dir, n) for n in os.listdir(out_dir)
                  if n.startswith(TXN_PREFIX))


def _maybe_inject(point):
    want = os.environ.get("FPI_FAIL_INJECT", "")
    if want and point in [p for p in want.split(",") if p]:
        raise OSError("injected failure at %s" % point)


def _set_journal_state(out_dir, state, pre_existing):
    fd, tmp = tempfile.mkstemp(prefix=TXN_PREFIX, dir=out_dir)
    try:
        with os.fdopen(fd, "wb") as fh:
            fh.write((json.dumps({"state": state,
                                  "pre_existing": pre_existing},
                                 sort_keys=True) + "\n").encode("ascii"))
            fh.flush()
            os.fsync(fh.fileno())
        _maybe_inject("journal." + state)
        os.replace(tmp, os.path.join(out_dir, JOURNAL_NAME))
    except BaseException:
        # 清理 best-effort：清理异常不得覆盖原始异常（codex re-review#7 P2），
        # 残留临时文件由下次 gen 启动恢复统一处理。
        try:
            _maybe_inject("journal-cleanup.1")
            if os.path.lexists(tmp):
                os.unlink(tmp)
        except OSError as cleanup_exc:
            eprint("WARNING: journal temp cleanup failed (%s); residue "
                   "left for next-gen startup recovery: %s"
                   % (cleanup_exc, tmp))
        raise
    _fsync_dir(out_dir)


def _read_journal(out_dir):
    path = os.path.join(out_dir, JOURNAL_NAME)
    if os.path.islink(path):
        die("commit journal is a symlink (refusing to follow): %s" % path)
    try:
        with open(path, "r", encoding="ascii") as fh:
            data = json.load(fh)
    except (OSError, ValueError) as exc:
        die("commit journal unreadable: %s (%s)" % (path, exc))
    if not isinstance(data, dict) or data.get("state") not in JOURNAL_STATES:
        die("commit journal has unknown state: %s" % path)
    pre = data.get("pre_existing")
    if (not isinstance(pre, list) or len(pre) != 3
            or not all(isinstance(x, bool) for x in pre)):
        die("commit journal has invalid pre_existing field: %s" % path)
    return data["state"], pre


def _trio_die(msg, out_dir):
    die("commit residue: %s; preserving: %s" % (msg, out_dir))


def _check_sidecar_binding(out_dir, man_bytes, sha_bytes, where):
    man_sha = hashlib.sha256(man_bytes).hexdigest()
    want = ("%s  %s\n" % (man_sha, MANIFEST_NAME)).encode("ascii")
    if sha_bytes != want:
        _trio_die("%s sidecar not bound to manifest" % where, out_dir)


def _reconcile_trio(out_dir, man_bytes, sha_bytes, json_bytes, where):
    """三件同代自洽：sidecar↔manifest 哈希绑定 + JSON↔manifest 绑定。"""
    man_sha = hashlib.sha256(man_bytes).hexdigest()
    want = ("%s  %s\n" % (man_sha, MANIFEST_NAME)).encode("ascii")
    if sha_bytes != want:
        _trio_die("%s sidecar not bound to manifest" % where, out_dir)
    try:
        obj = json.loads(json_bytes.decode("utf-8"))
    except (ValueError, UnicodeDecodeError):
        _trio_die("%s JSON unreadable" % where, out_dir)
    if not isinstance(obj, dict) or obj.get("schema_version") != SCHEMA_VERSION:
        _trio_die("%s JSON has wrong schema_version" % where, out_dir)
    m = obj.get("manifest")
    if (not isinstance(m, dict) or m.get("file") != MANIFEST_NAME
            or m.get("sha256") != man_sha):
        _trio_die("%s JSON not bound to manifest" % where, out_dir)


def _read_candidate(path):
    if os.path.lexists(path):
        with open(path, "rb") as fh:
            return fh.read()
    return None


def _validate_bak_set(out_dir, state, pre_existing, present, baks):
    """按状态 + pre_existing 验证 .bak 集合不变量（codex re-review P1-1/#4 P1-2）。

    pre_existing 由事务开始时的「三件全有或全无」守卫保证只有全真/全假；
    journal 中的部分 pre_existing 视为篡改现场。备份 0→2 顺序进行、恢复
    2→0 反序进行，故 backing_up 与 rolling_back 的中断残留均为前缀集。
    """
    if any(pre_existing) and not all(pre_existing):
        _trio_die("journal pre_existing is neither all-true nor all-false",
                  out_dir)
    for i in range(3):
        if present[i] and not pre_existing[i]:
            _trio_die(".bak exists for a path that did not pre-exist (%s)"
                      % os.path.basename(baks[i]), out_dir)
    if not any(pre_existing):
        return  # 首代事务：任何 .bak 已被上一循环拒绝，其余无需校验
    if state in ("backing_up", "rolling_back"):
        for i in (1, 2):
            if present[i] and not present[i - 1]:
                _trio_die("backup set violates prefix order in %s state "
                          "(missing %s)" % (state,
                                            os.path.basename(baks[i - 1])),
                          out_dir)
    elif state == "committing":
        if not all(present):
            _trio_die("backup set incomplete in committing state", out_dir)


def _restore_old_generation(out_dir, state, pre_existing, baks, paths,
                            temps, validate):
    """按 pre_existing 规则把目标恢复到事务开始前的状态。

    - 有 .bak → 恢复 .bak 到目标（旧代）；
    - 无 .bak 且事务前不存在 → 删除目标（首代提交的新产物，必须清除）；
    - 无 .bak 且事务前存在 → 目标仍是旧代原地未动，保留。
    validate=True（启动恢复路径）：先按状态 + pre_existing 验证 .bak 集合
    不变量，再对可重建的旧代三件做 reconcile（sidecar/JSON 与 manifest
    绑定），违者 fail-closed 保留现场；validate=False（同进程回滚路径）：
    .bak 是本进程刚由 os.replace 生成、内容即事务前字节，无需 reconcile。
    **恢复按 2→0 反序进行**——中途失败残留为前缀集，二次启动可续恢复。
    OSError 上抛，由调用方定口径。
    """
    present = [os.path.lexists(b) for b in baks]
    if validate:
        _validate_bak_set(out_dir, state, pre_existing, present, baks)
        old_pos = [i for i in range(3) if present[i] or pre_existing[i]]
        cand = [(_read_candidate(baks[i]) if present[i]
                 else _read_candidate(paths[i])) for i in range(3)]
        if old_pos == [0, 1, 2]:
            if any(c is None for c in cand):
                _trio_die("cannot reconstruct old trio", out_dir)
            _reconcile_trio(out_dir, cand[0], cand[1], cand[2],
                            "backup trio")
        elif old_pos == [0, 1]:
            if cand[0] is None or cand[1] is None:
                _trio_die("cannot reconstruct old manifest+sidecar", out_dir)
            _check_sidecar_binding(out_dir, cand[0], cand[1],
                                   "backup sidecar")
    for i in range(2, -1, -1):
        _maybe_inject("rollback.%d" % (i + 1))
        if present[i]:
            os.replace(baks[i], paths[i])
        elif not pre_existing[i]:
            if os.path.lexists(paths[i]):
                os.unlink(paths[i])
    for tmp in temps:
        if os.path.lexists(tmp):
            os.unlink(tmp)


def _journal_unlink_best_effort(out_dir, journal, inject_point):
    """best-effort 删除 journal（codex re-review#9 P2）：
    失败 WARNING + 保留残留（下次启动可继续），不抛异常、不绕过统一口径。"""
    if os.path.lexists(journal):
        try:
            _maybe_inject(inject_point)
            os.unlink(journal)
        except OSError as exc:
            eprint("WARNING: journal cleanup failed (%s); residue kept "
                   "for next-gen startup recovery: %s" % (exc, JOURNAL_NAME))


def _rollback_txn(out_dir, baks, paths, temps, pre_existing):
    """回滚本次事务；返回 True=旧代已恢复，False=回滚未完成（现场保留）。

    必须先成功把 journal 切换为 rolling_back 才允许修改 .bak/dst；
    切换失败 → 不修改任何文件、返回 False（原现场 committing+完整备份集
    仍可由下次启动恢复，codex re-review#4 P1-1）。恢复完成但 journal
    删除失败 → 旧代确已恢复，报 restored 并 WARNING 残留（不谎称
    INCOMPLETE）。
    """
    try:
        _set_journal_state(out_dir, "rolling_back", pre_existing)
    except OSError:
        return False
    try:
        _restore_old_generation(out_dir, "rolling_back", pre_existing,
                                baks, paths, temps, validate=False)
        _journal_unlink_best_effort(out_dir,
                                    os.path.join(out_dir, JOURNAL_NAME),
                                    "journal-unlink.rollback")
        _fsync_dir(out_dir)
        return True
    except OSError:
        return False


def recover_txn_residue(out_dir):
    """gen 启动恢复：把上次中断的提交恢复到同代一致状态并清理残留。"""
    journal = os.path.join(out_dir, JOURNAL_NAME)
    paths = _txn_paths(out_dir)
    baks = [p + BAK_SUFFIX for p in paths]
    temps = _txn_temps(out_dir)

    if os.path.lexists(journal):
        state, pre_existing = _read_journal(out_dir)
        if state == "committed":
            # 提交已完成、清理被中断：三件 reconcile → 清残留；不自洽 → 保留现场
            if not all(os.path.isfile(p) for p in paths):
                _trio_die("artifacts missing after committed state", out_dir)
            _reconcile_trio(out_dir, _read_candidate(paths[0]),
                            _read_candidate(paths[1]),
                            _read_candidate(paths[2]), "artifact trio")
            cleanup_failed = []
            for p in baks + temps:
                if os.path.lexists(p):
                    try:
                        os.unlink(p)
                    except OSError as exc:
                        cleanup_failed.append("%s (%s)"
                                              % (os.path.basename(p), exc))
            if cleanup_failed:
                # journal 保留 committed：残留可被下次启动继续清理，
                # 清理异常不覆盖、不升级（codex re-review#7 P2 同口径）
                eprint("WARNING: commit cleanup incomplete (%s); journal "
                       "kept for next-gen startup recovery"
                       % "; ".join(cleanup_failed))
                _fsync_dir(out_dir)
                return
            _journal_unlink_best_effort(out_dir, journal,
                                        "journal-unlink.committed")
            _fsync_dir(out_dir)
            return
        # 其余态：先按原状态只读验证现场合法（不动任何文件）
        _validate_bak_set(out_dir, state, pre_existing,
                          [os.path.lexists(b) for b in baks], baks)
        # 切换 rolling_back 成功后才允许修改 .bak/dst；失败保留原现场
        try:
            _set_journal_state(out_dir, "rolling_back", pre_existing)
        except OSError as exc:
            die("commit residue: cannot switch journal to rolling_back "
                "(%s); original field preserved: %s" % (exc, out_dir))
        try:
            _restore_old_generation(out_dir, "rolling_back", pre_existing,
                                    baks, paths, temps, validate=True)
        except OSError as exc:
            die("commit residue: recovery incomplete (%s); journal "
                "preserved (rolling_back) for retry: %s" % (exc, out_dir))
        _journal_unlink_best_effort(out_dir, journal,
                                    "journal-unlink.recovered")
        _fsync_dir(out_dir)
        return

    if any(os.path.lexists(b) for b in baks):
        die("commit residue: .bak without journal — cannot prove it "
            "belongs to this tool's transaction; preserving: %s" % out_dir)
    for tmp in temps:
        if os.path.lexists(tmp):
            try:
                os.unlink(tmp)
            except OSError as exc:
                eprint("WARNING: stale temp cleanup failed (%s): %s"
                       % (exc, tmp))
    _fsync_dir(out_dir)


def _check_no_txn_residue(out_dir):
    """verify 只读：遇任何提交残留一律 fail-closed（先 gen 恢复再复验）。"""
    if not os.path.isdir(out_dir):
        return
    residue = []
    if os.path.lexists(os.path.join(out_dir, JOURNAL_NAME)):
        residue.append(JOURNAL_NAME)
    for n in (MANIFEST_NAME, MANIFEST_SHA_NAME, INTEGRITY_NAME):
        if os.path.lexists(os.path.join(out_dir, n + BAK_SUFFIX)):
            residue.append(n + BAK_SUFFIX)
    residue += [os.path.basename(t) for t in _txn_temps(out_dir)]
    if residue:
        die("commit residue present (%s); run `gen` first to recover: %s"
            % (", ".join(residue), out_dir))


def write_artifacts(out_dir, manifest_bytes, integrity_obj):
    man_sha = hashlib.sha256(manifest_bytes).hexdigest()
    paths = _txn_paths(out_dir)
    payload = (
        manifest_bytes,
        ("%s  %s\n" % (man_sha, MANIFEST_NAME)).encode("ascii"),
        render_json(integrity_obj),
    )

    # all-or-none 守卫必须**先于任何暂存文件创建**（codex re-review#5 P1）：
    # 部分旧代 → rc=2 且不留任何 .f-payload.txn.* 残留。
    pre_existing = [os.path.lexists(p) for p in paths]
    if any(pre_existing) and not all(pre_existing):
        die("refusing to commit: artifact trio partially present "
            "(all-or-none required); adjudicate manually: %s" % out_dir)

    temps = []
    try:
        for i, data in enumerate(payload):
            fd, tmp = tempfile.mkstemp(prefix=TXN_PREFIX, dir=out_dir)
            temps.append(tmp)  # mkstemp 后立即登记：写入/fsync 失败也能清理
            _maybe_inject("stage.%d" % (i + 1))
            with os.fdopen(fd, "wb") as fh:
                fh.write(data)
                fh.flush()
                os.fsync(fh.fileno())
    except OSError as exc:
        # 清理 best-effort：逐文件隔离，清理异常不得覆盖原始异常、
        # 不得绕过统一 rc=2 控制路径（codex re-review#7 P2-1）。
        cleanup_failed = []
        for i, tmp in enumerate(temps):
            try:
                _maybe_inject("stage-cleanup.%d" % (i + 1))
                if os.path.lexists(tmp):
                    os.unlink(tmp)
            except OSError as cleanup_exc:
                cleanup_failed.append("%s (%s)"
                                      % (os.path.basename(tmp), cleanup_exc))
        if cleanup_failed:
            die("failed to stage artifacts (%s); cleanup INCOMPLETE (%s) — "
                "residue left for next-gen startup recovery"
                % (exc, "; ".join(cleanup_failed)))
        die("failed to stage artifacts (%s)" % exc)

    journal = os.path.join(out_dir, JOURNAL_NAME)
    baks = [p + BAK_SUFFIX for p in paths]

    try:
        _set_journal_state(out_dir, "backing_up", pre_existing)
        for i, path in enumerate(paths):
            _maybe_inject("backup.%d" % (i + 1))
            if pre_existing[i]:
                os.replace(path, baks[i])
        _set_journal_state(out_dir, "committing", pre_existing)
        for i, path in enumerate(paths):
            _maybe_inject("commit.%d" % (i + 1))
            os.replace(temps[i], path)
            temps[i] = None
        _set_journal_state(out_dir, "committed", pre_existing)
    except OSError as exc:
        restored = _rollback_txn(out_dir, baks, paths,
                                 [t for t in temps if t is not None],
                                 pre_existing)
        if restored:
            die("failed to commit artifacts (%s); previous generation "
                "restored" % exc)
        die("failed to commit artifacts (%s); rollback INCOMPLETE — field "
            "preserved for next-startup recovery" % exc)

    # 清理失败不回滚：journal 保持 committed，残留由下次启动统一处理。
    residue_note = False
    try:
        _maybe_inject("cleanup.1")
        for bak in baks:
            if os.path.lexists(bak):
                os.unlink(bak)
        _maybe_inject("cleanup.2")
        for tmp in temps:
            if tmp is not None and os.path.lexists(tmp):
                os.unlink(tmp)
        if os.path.lexists(journal):
            os.unlink(journal)
    except OSError:
        residue_note = True
    _fsync_dir(out_dir)
    if residue_note:
        eprint("WARNING: commit cleanup interrupted; residue will be "
               "resolved by next-gen startup recovery")
    return paths


def compare_existing(out_dir, manifest_bytes, integrity_obj):
    """verify：把重算结果与磁盘既有产物逐字节比对。"""
    man_sha = hashlib.sha256(manifest_bytes).hexdigest()
    want = (
        (MANIFEST_NAME, manifest_bytes),
        (MANIFEST_SHA_NAME,
         ("%s  %s\n" % (man_sha, MANIFEST_NAME)).encode("ascii")),
        (INTEGRITY_NAME, render_json(integrity_obj)),
    )
    diffs = []
    for name, want_bytes in want:
        path = os.path.join(out_dir, name)
        if not os.path.isfile(path):
            diffs.append("missing artifact: %s" % path)
            continue
        with open(path, "rb") as fh:
            if fh.read() != want_bytes:
                diffs.append("artifact bytes differ: %s" % path)
    return diffs


# ------------------------------------------------------------------- main

def baseline_fingerprint(bl):
    return hashlib.sha256(
        json.dumps(bl, sort_keys=True, ensure_ascii=False)
        .encode("utf-8")).hexdigest()


def main():
    ap = argparse.ArgumentParser(prog=PROG)
    ap.add_argument("command", choices=("gen", "verify"))
    ap.add_argument("--root", default=os.getcwd(),
                    help="repository root (default: cwd)")
    ap.add_argument("--deb", default=DEFAULT_DEB,
                    help="source deb, relative to --root (read-only)")
    ap.add_argument("--unpack", default=DEFAULT_UNPACK,
                    help="unpacked payload tree, relative to --root "
                         "(read-only)")
    ap.add_argument("--out-dir", default=DEFAULT_OUT,
                    help="artifact output directory")
    ap.add_argument("--baseline", default=None,
                    help="JSON file fully replacing the locked baseline "
                         "(unit tests only; production runs must omit it so "
                         "the committed evidence self-certifies "
                         "baseline_source=default)")
    args = ap.parse_args()

    root = os.path.realpath(args.root)
    if not os.path.isdir(root):
        die("--root is not a directory: %s" % root)
    deb = args.deb if os.path.isabs(args.deb) else os.path.join(root, args.deb)
    unpack = (args.unpack if os.path.isabs(args.unpack)
              else os.path.join(root, args.unpack))
    out_dir = (args.out_dir if os.path.isabs(args.out_dir)
               else os.path.join(root, args.out_dir))

    # 写保护前置：先于任何输入存在性检查，使守卫可被独立验证。
    check_protected_out(root, out_dir)

    if shutil.which("dpkg-deb") is None:
        die("dpkg-deb is required")
    if not os.path.isfile(deb):
        die("source deb not found: %s" % deb)
    if not os.path.isdir(unpack):
        die("unpack tree not found: %s" % unpack)
    if args.command == "gen" and not os.path.isdir(out_dir):
        die("--out-dir does not exist: %s" % out_dir)
    if args.command == "gen":
        recover_txn_residue(out_dir)
    else:
        _check_no_txn_residue(out_dir)

    if args.baseline is None:
        bl = default_baseline()
        baseline_source = "default"
    else:
        bl = load_baseline(args.baseline)
        baseline_source = "override"

    deb_rel = os.path.relpath(deb, root) if deb.startswith(root + os.sep) \
        else deb
    unpack_rel = os.path.relpath(unpack, root) \
        if unpack.startswith(root + os.sep) else unpack

    deb_sha = sha256_file(deb)
    deb_size = os.path.getsize(deb)
    s1 = read_deb_fsys(deb)
    s2 = read_deb_ctrl(deb)
    s3 = read_unpack(unpack)
    s3_payload = {p: e for p, e in s3.items() if is_payload_path(p)}

    failures = []
    checks = {
        "A_source_identity": check_a_source_identity(
            bl, deb_rel, deb_sha, deb_size, s2, failures),
        "B_payload_structure": check_b_payload_structure(
            bl, s1, s3_payload, failures),
        "C_control_and_md5sums": check_c_control_and_md5sums(
            bl, s2, s3, failures),
        "E_lineage_coherence": check_e_lineage_coherence(bl, s3, failures),
        "F_firmware_reconciliation": check_f_firmware(bl, s3, failures),
        "G_install_time_staging": check_g_install_time_staging(
            bl, s3, unpack, failures),
    }
    manifest_bytes = build_manifest_tsv(s3)
    integrity_obj = build_integrity_json(
        deb_rel, unpack_rel, deb_sha, deb_size, checks, manifest_bytes, s3,
        {"source": baseline_source, "sha256": baseline_fingerprint(bl)})

    if failures:
        for msg in failures:
            eprint("FAIL: %s" % msg)
        eprint("RESULT: FAIL_F_PAYLOAD_INTEGRITY failures=%d checks=%d"
               % (len(failures), len(checks)))
        return EX_FAIL

    if args.command == "verify":
        diffs = compare_existing(out_dir, manifest_bytes, integrity_obj)
        if diffs:
            for d in diffs:
                eprint("FAIL: %s" % d)
            eprint("RESULT: FAIL_F_PAYLOAD_INTEGRITY_VERIFY diffs=%d"
                   % len(diffs))
            return EX_FAIL
        print("RESULT: PASS_F_PAYLOAD_INTEGRITY_VERIFY rows=%d "
              "payload_files=%d manifest_sha256=%s"
              % (len(s3), integrity_obj["payload_files"],
                 integrity_obj["manifest"]["sha256"]))
        return 0

    written = write_artifacts(out_dir, manifest_bytes, integrity_obj)
    print("RESULT: PASS_F_PAYLOAD_INTEGRITY rows=%d payload_files=%d "
          "deb_sha256=%s manifest_sha256=%s"
          % (len(s3), integrity_obj["payload_files"], deb_sha,
             integrity_obj["manifest"]["sha256"]))
    for path in written:
        print("artifact=%s" % path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
