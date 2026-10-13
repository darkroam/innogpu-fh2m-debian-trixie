#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/030-051-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT
cd "$ROOT"

python3 - "$TMP" << 'PY'
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

tmp = Path(sys.argv[1])
patch_path = Path("patches/030-051.patch")
patch = patch_path.read_text()
meta = json.loads(Path("patches/030-051.meta.json").read_text())
patch_sha = hashlib.sha256(patch_path.read_bytes()).hexdigest()
assert patch_sha == meta["apply"]["patch_sha256"] == meta["deepin_patch"]["sha256"]
assert set(re.findall(r"^\+\+\+ b/(.+)$", patch, re.M)) == {"fantsrvkm/fantdpu_connector.c"}
assert "fantdpu_hdmi.c" not in patch and "fantdpu_dp_common.c" not in patch
assert patch.count("return -EFAULT;") >= 1
added = "\n".join(line[1:] for line in patch.splitlines() if line.startswith("+") and not line.startswith("+++"))
removed = "\n".join(line[1:] for line in patch.splitlines() if line.startswith("-") and not line.startswith("---"))
assert 'fh2m_fantdpu_err(dev, "hwinfo version failed\\n")' in removed
assert 'fh2m_fantdpu_err(dev, "hwinfo version failed, Use default DDCCI\\n")' in removed
assert 'fh2m_fantdpu_err(dev, "get_output_mode failed!!!, Using the default output:0\\n")' in removed
assert 'fh2m_fantdpu_err(dev, "fh2m_hal_get_output_mode failed, Using 0\\n")' in removed
assert "fant_printk_once" in added
assert "fantdpu_cache_output_index(conn_module, 0)" in added
assert "FANTGPU_LOCAL_INTERNAL_EDP_OUTPUT_INDEX" in added
assert "s_hdmi_dvi" not in added
assert "030-051" not in Path("tools/build-innogpu-driver.sh").read_text()
builder = Path("tools/gentoo-fh2m.sh").read_text()
assert meta["apply"]["after_tree_hash"] in builder
assert builder.index("030-036.patch") < builder.index("030-051.patch")
raw = subprocess.check_output([
    "tar", "--use-compress-program=zstd", "-xOf",
    "docs/planning/evidence/o-stage/5.0.0-i6/o-stage-snapshot.tar.zst",
    "o-stage/fantsrvkm/fantdpu_connector.c",
])
assert hashlib.sha256(raw).hexdigest() == meta["apply"]["file_before_sha256"]
tree = tmp / "tree"
(tree / "fantsrvkm").mkdir(parents=True)
(tree / "fantsrvkm/fantdpu_connector.c").write_bytes(raw)
print("PASS: 030-051 patch contract")
PY

patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s \
    -d "$TMP/tree" -p1 < patches/030-051.patch
python3 - "$TMP/tree" << 'PY'
import hashlib
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
meta = json.loads(Path("patches/030-051.meta.json").read_text())
text = (root / "fantsrvkm/fantdpu_connector.c").read_text()
assert hashlib.sha256(text.encode()).hexdigest() == meta["apply"]["file_after_sha256"]
assert "fh2m_fantdpu_err(dev, \"hwinfo version failed\\n\")" not in text
assert "using default output 0 and DDCCI" in text
assert text.count("fh2m_hal_hwinfo_version(") == 1
print("PASS: 030-051 connector replay")
PY

patch --batch --reverse --fuzz=0 --no-backup-if-mismatch -s \
    -d "$TMP/tree" -p1 < patches/030-051.patch
python3 - "$TMP/tree" << 'PY'
import hashlib
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
meta = json.loads(Path("patches/030-051.meta.json").read_text())
file_sha = hashlib.sha256((root / "fantsrvkm/fantdpu_connector.c").read_bytes()).hexdigest()
assert file_sha == meta["apply"]["file_before_sha256"]
print("PASS: 030-051 reverse restores connector")
PY
