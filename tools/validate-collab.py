#!/usr/bin/env python3
"""Mechanical validator for the collab/ multi-agent collaboration directory.

Human authority: docs/project/multiagent-collab.md section 五 (this tool only
enforces its mechanical part). Persistent fixtures:
tests/unit/run-collab-structure-tests.sh.

collab/ is a machine-local directory (gitignored, never uploaded to GitHub);
when it does not exist at all (fresh clone, CI) the tool exits 0 without
output because there are simply no rounds to validate.

Enforced contract
-----------------
- collab/INDEX.md exists (regular file, never a symlink) and contains a pipe
  table with the header 轮次|日期|主题|状态, one separator row, and one data
  row per registered round.
- Round directories live directly under collab/ and are named
  R{id}-{YYYY-MM-DD}-{topic}; nothing else may exist at the collab/ root (no
  stray files, no symlinks anywhere under collab/, no nested directories).
- Every round directory contains only regular Markdown files and must include
  request.md and report.md (report.md is created as a template when the round
  is created, so it is always required).
- INDEX data rows and round directories correspond one-to-one by round id
  using exact trimmed equality (R01 never matches R010); the row date and topic
  must equal the directory date and topic; status must be one of 待开发/开发中/
  待审查/通过/返工/已提交/执行中.
- Every Markdown file (including collab/INDEX.md and hidden Markdown files
  that ordinary ignore-aware scans can skip) is checked case-insensitively
  against the shared pattern source tools/private-data-patterns.txt.

Exit status: 0 = valid (no output), 1 = violations (one message per line on
stderr), 2 = usage error.
"""

import argparse
import re
import sys
from datetime import date
from pathlib import Path

VALID_STATUS = ("待开发", "开发中", "待审查", "通过", "返工", "已提交", "执行中")
ROUND_RE = re.compile(r"R(\d+)-(\d{4}-\d{2}-\d{2})-(.+)")
HEADER = ["轮次", "日期", "主题", "状态"]
def _load_private_patterns():
    pattern_file = Path(__file__).resolve().parent / "private-data-patterns.txt"
    try:
        lines = pattern_file.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        sys.exit("tools/private-data-patterns.txt 无法读取: %s" % exc)
    patterns = [line.strip() for line in lines if line.strip() and not line.lstrip().startswith("#")]
    if not patterns:
        sys.exit("tools/private-data-patterns.txt 没有可用模式")
    return re.compile("|".join(patterns), re.IGNORECASE)


PRIVATE_DATA_RE = _load_private_patterns()


def cells(line):
    stripped = line.strip()
    if stripped.startswith("|"):
        stripped = stripped[1:]
    if stripped.endswith("|"):
        stripped = stripped[:-1]
    return [cell.strip() for cell in stripped.split("|")]


def is_separator(row):
    return bool(row) and all(re.fullmatch(r":?-{3,}:?", cell) for cell in row)


def is_date(value):
    if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", value):
        return False
    try:
        date.fromisoformat(value)
        return True
    except ValueError:
        return False


def parse_index(index, rows):
    errors = []
    try:
        content = index.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        return ["collab/INDEX.md 无法作为 UTF-8 Markdown 读取: %s" % exc]
    if PRIVATE_DATA_RE.search(content):
        errors.append("collab/ Markdown 包含敏感信息标记: collab/INDEX.md")
    lines = content.splitlines()
    table = []
    for line_no, line in enumerate(lines, 1):
        stripped = line.strip()
        if stripped.startswith("|"):
            table.append((line_no, cells(stripped)))
    if not table:
        errors.append("collab/INDEX.md 缺少 轮次/日期/主题/状态 表格")
        return errors
    if table[0][1] != HEADER:
        errors.append("collab/INDEX.md 表头必须为 轮次/日期/主题/状态 四列（实际: %s）" % "/".join(table[0][1]))
        return errors
    if len(table) < 2 or not is_separator(table[1][1]):
        errors.append("collab/INDEX.md 表头后缺少分隔行")
        return errors
    for line_no, row in table[2:]:
        if len(row) != 4:
            errors.append("collab/INDEX.md 第 %d 行单元格数不是 4" % line_no)
            continue
        rid, rdate, topic, status = row
        if not re.fullmatch(r"R\d+", rid):
            errors.append("collab/INDEX.md 第 %d 行轮次编号格式无效: %s" % (line_no, rid))
            continue
        if not is_date(rdate):
            errors.append("collab/INDEX.md 第 %d 行日期无效: %s" % (line_no, rdate))
            continue
        if not topic:
            errors.append("collab/INDEX.md 第 %d 行主题为空" % line_no)
            continue
        if status not in VALID_STATUS:
            errors.append("collab/INDEX.md 第 %d 行状态不在白名单: %s" % (line_no, status))
            continue
        if rid in rows:
            errors.append("collab/INDEX.md 重复登记轮次: %s" % rid)
            continue
        rows[rid] = (rdate, topic, status)
    return errors


def scan_round(directory, rel, errors):
    has_request = False
    has_report = False
    try:
        entries = sorted(directory.iterdir(), key=lambda entry: entry.name)
    except OSError:
        errors.append("%s 无法枚举" % rel)
        return
    for entry in entries:
        child = "%s/%s" % (rel, entry.name)
        if entry.is_symlink():
            errors.append("collab/ 内不允许符号链接: %s" % child)
            continue
        if entry.is_dir():
            errors.append("collab/ 轮次目录内不允许嵌套目录: %s" % child)
            continue
        if not entry.is_file() or not entry.name.endswith(".md"):
            errors.append("collab/ 轮次目录只允许 Markdown 文件: %s" % child)
            continue
        try:
            content = entry.read_text(encoding="utf-8")
        except (OSError, UnicodeError) as exc:
            errors.append("%s 无法作为 UTF-8 Markdown 读取: %s" % (child, exc))
            continue
        if PRIVATE_DATA_RE.search(content):
            errors.append("collab/ Markdown 包含敏感信息标记: %s" % child)
        if entry.name == "request.md":
            has_request = True
        if entry.name == "report.md":
            has_report = True
    if not has_request:
        errors.append("collab/ 轮次缺少 request.md: %s" % rel)
    if not has_report:
        errors.append("collab/ 轮次缺少 report.md: %s" % rel)


def scan_root(collab, round_dirs, errors):
    try:
        entries = sorted(collab.iterdir(), key=lambda entry: entry.name)
    except OSError:
        errors.append("collab/ 无法枚举")
        return
    for entry in entries:
        rel = "collab/%s" % entry.name
        if entry.is_symlink():
            errors.append("collab/ 内不允许符号链接: %s" % rel)
            continue
        if entry.name == "INDEX.md":
            continue
        if not entry.is_dir():
            errors.append("collab/ 根目录只允许 INDEX.md 与轮次目录: %s" % rel)
            continue
        match = ROUND_RE.fullmatch(entry.name)
        if not match:
            errors.append("collab/ 轮次目录命名无效（应为 R{编号}-{YYYY-MM-DD}-{主题}）: %s" % rel)
            continue
        rid = "R" + match.group(1)
        rdate = match.group(2)
        if not is_date(rdate):
            errors.append("collab/ 轮次目录日期无效: %s" % rel)
            continue
        if rid in round_dirs:
            errors.append("collab/ 轮次编号重复: %s" % rid)
            continue
        round_dirs[rid] = (rel, rdate, match.group(3))
        scan_round(entry, rel, errors)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=None,
                        help="repository root (default: parent directory of tools/)")
    args = parser.parse_args(argv)
    root = Path(args.root).resolve() if args.root else Path(__file__).resolve().parent.parent
    collab = root / "collab"
    errors = []
    if collab.is_symlink():
        errors.append("collab/ 根目录不允许是符号链接")
    elif not collab.exists():
        # Local-only directory (gitignored): absent on a fresh clone and in
        # CI, which is a valid state with zero rounds.
        return 0
    elif not collab.is_dir():
        errors.append("collab/ 存在但不是目录")
    else:
        index = collab / "INDEX.md"
        rows = {}
        if index.is_file() and not index.is_symlink():
            errors.extend(parse_index(index, rows))
        else:
            errors.append("collab/INDEX.md 不存在或不是普通文件")
        round_dirs = {}
        scan_root(collab, round_dirs, errors)
        row_ids = set(rows)
        dir_ids = set(round_dirs)
        for rid in sorted(row_ids - dir_ids):
            errors.append("collab/INDEX.md 登记了无对应目录的轮次: %s" % rid)
        for rid in sorted(dir_ids - row_ids):
            rel = round_dirs[rid][0]
            errors.append("%s 未登记在 collab/INDEX.md" % rel)
        for rid in sorted(row_ids & dir_ids):
            rel, dir_date, dir_topic = round_dirs[rid]
            row_date, row_topic = rows[rid][:2]
            if dir_date != row_date:
                errors.append("%s 与 collab/INDEX.md 登记日期不一致（目录 %s / INDEX %s）" % (rel, dir_date, row_date))
            if dir_topic != row_topic:
                errors.append("%s 与 collab/INDEX.md 登记主题不一致（目录 %s / INDEX %s）" % (rel, dir_topic, row_topic))
    if not errors:
        return 0
    for message in errors:
        print(message, file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
