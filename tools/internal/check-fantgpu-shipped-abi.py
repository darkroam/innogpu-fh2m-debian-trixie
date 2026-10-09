#!/usr/bin/env python3
"""Check pahole text for the frozen dev_rsrc ABI (never read live kernel BTF)."""
import hashlib
import re
import subprocess
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


def read_module(module, output):
    # pahole 1.30's DWARF -C early-stop path returns EINVAL even for a tiny
    # valid ELF. Prefix filtering scans to completion and keeps rc mandatory.
    result = subprocess.run(["pahole", "-F", "dwarf", "-y", "dev_rsrc", str(module)],
                            capture_output=True, text=True)
    output.with_suffix(".all.pahole").write_text(result.stdout)
    output.with_suffix(".stderr").write_text(result.stderr)
    result.check_returncode()
    layouts = re.findall(r"^struct dev_rsrc \{\n.*?^};$", result.stdout, re.M | re.S)
    if re.sub(r"^struct dev_rsrc \{\n.*?^};$", "", result.stdout, flags=re.M | re.S).strip():
        raise ValueError("unexpected ABI type output")
    # The frozen shipped object also carries its original 114-member layout.
    # Exempt only that exact dump; every other definition must pass the existing
    # 115-member gate. Never select a convenient first definition or ignore rc.
    legacy = "bff704b1537f1642b0ecc5a5634c6b8147c01aaf0d7f5603372dd6a2eca85e5b"
    current = [s for s in layouts if hashlib.sha256(s.encode()).hexdigest() != legacy]
    if len(layouts) not in (1, 2) or len(current) != 1:
        raise ValueError("missing or ambiguous dev_rsrc definitions")
    text = current[0] + "\n"
    check(text)
    output.write_text(text)


if __name__ == "__main__":
    try:
        if len(sys.argv) == 4 and sys.argv[1] == "--module":
            read_module(Path(sys.argv[2]), Path(sys.argv[3]))
        elif len(sys.argv) == 2:
            check(Path(sys.argv[1]).read_text())
        else:
            raise ValueError("usage: check-fantgpu-shipped-abi.py PAHOLE_TEXT | --module MODULE OUTPUT")
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        raise SystemExit(str(exc))
    print("shipped_abi=PASS size=140536 members=115 offset=140528")
