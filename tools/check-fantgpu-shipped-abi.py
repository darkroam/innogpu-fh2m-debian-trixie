#!/usr/bin/env python3
"""Check pahole text for the frozen dev_rsrc ABI (never read live kernel BTF)."""
import re
import sys
from pathlib import Path

OFFSETS = {
    "debugfs_dir": 136616,
    "debugfs_hwdir": 136624,
    "debugfs_name": 136632,
    "syspll_debugfs": 136640,
    "test_temp_debugfs": 136648,
    "hdmi_dev": 136656,
    "pvr_resume_count": 140528,
}


def check(text):
    summaries = re.findall(r"/\* size: (\d+),.*members: (\d+) \*/", text)
    if summaries != [("140536", "115")]:
        raise ValueError("dev_rsrc size/member count missing, ambiguous or changed")
    for field, offset in OFFSETS.items():
        matches = re.findall(rf"^.*\b{field}(?:\[[^]]+\])?;\s*/\*\s*(\d+)", text, re.M)
        if matches != [str(offset)]:
            raise ValueError(f"dev_rsrc offset missing, ambiguous or changed: {field}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: check-fantgpu-shipped-abi.py PAHOLE_TEXT")
    try:
        check(Path(sys.argv[1]).read_text())
    except (OSError, ValueError) as exc:
        raise SystemExit(str(exc))
    print("shipped_abi=PASS size=140536 members=115 offset=140528")
