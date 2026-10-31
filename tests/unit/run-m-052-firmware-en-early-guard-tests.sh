#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/m-052-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT
cd "$ROOT"

python3 - "$TMP" << 'PY'
import hashlib
import json
import re
import subprocess
from pathlib import Path
import sys

tmp = Path(sys.argv[1])
patch_path = Path("patches/m-052-firmware-en-early-guard.patch")
patch = patch_path.read_text()
meta = json.loads(Path("patches/m-052-firmware-en-early-guard.meta.json").read_text())
patch_sha = hashlib.sha256(patch_path.read_bytes()).hexdigest()
assert patch_sha == meta["apply"]["patch_sha256"] == meta["deepin_patch"]["sha256"]
assert "+++ b/fantsrvkm/fantaudio_drv.c" in patch and "+++ b/" in patch and patch.count("+++ b/") == 1
assert "!firmware_en" in patch
derive = Path("tools/internal/derive-fantgpu-source.sh").read_text()
builder = Path("tools/debian/build-driver.sh").read_text()
gentoo = Path("tools/gentoo/fh2m-lib.sh").read_text()
assert "m-052-firmware-en-early-guard.patch" not in derive
package = builder.split("DKMS_SRC_NAME", 1)[1]
assert package.index("m-036-bf-shf-rename.patch") < package.index("m-051-hwinfo-connector-fallback.patch") < package.index("m-052-firmware-en-early-guard.patch")
assert "!firmware_en" in builder
assert gentoo.index("m-051-hwinfo-connector-fallback.patch") < gentoo.index("m-052-firmware-en-early-guard.patch")
print("PASS: m-052 patch contract")
PY

ROOT="$ROOT" python3 - "$TMP" << 'PY'
import hashlib, importlib.util, json, os, subprocess, sys
from pathlib import Path
root = Path(os.environ["ROOT"])
tmp = Path(sys.argv[1]) / "whole"
subprocess.check_call(["rm", "-rf", str(tmp)])
tmp.mkdir()
subprocess.check_call(["tar", "--use-compress-program=zstd", "-xf",
    str(root / "docs/planning/evidence/o-stage/5.0.0-i6/o-stage-snapshot.tar.zst"), "-C", str(tmp)])
src = tmp / "o-stage"
subprocess.check_call(["bash", "-c", 'cd "$1"; source tools/internal/derive-fantgpu-source.sh; derive_fantgpu_source "$2"', "derive", str(root), str(src)])
for name in ("m-036-bf-shf-rename", "m-051-hwinfo-connector-fallback"):
    subprocess.check_call(["patch", "--batch", "--forward", "--fuzz=0", "--no-backup-if-mismatch", "-s", "-d", str(src), "-p1"], stdin=open(root / f"patches/{name}.patch"))
spec = importlib.util.spec_from_file_location("o4", root / "tools/internal/o4-f0-lock-gen.py")
o4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(o4)
def tree_hash(path):
    return hashlib.sha256(o4.manifest_text(list(o4.walk_rows(str(path)))).encode()).hexdigest()
before = tree_hash(src)
subprocess.check_call(["patch", "--batch", "--forward", "--fuzz=0", "--no-backup-if-mismatch", "-s", "-d", str(src), "-p1"], stdin=open(root / "patches/m-052-firmware-en-early-guard.patch"))
after = tree_hash(src)
meta = json.loads((root / "patches/m-052-firmware-en-early-guard.meta.json").read_text())
assert before == meta["apply"]["before_tree_hash"] == "b0a46c345c1e0d93baf51c554a1cc9dde0757feb68ce4a9951c3c01458f71c00"
assert after == meta["apply"]["after_tree_hash"] == "7a3c89e472666d73d74469f1fe52b6aa98b898eb61aa83ac32a94fa86fc8a243"
assert meta["f0_base"]["tree_hash"] == before
assert meta["ledger_ref"]["mapping"] == "docs/patches/m-052-firmware-en-early-guard.md"
assert meta["review"]["license"]["status"] == "pending-audit"
assert "m-036" in meta["review"]["license"]["evidence"]
print("PASS: m-052 whole-tree hash")
PY

mkdir -p "$TMP/tree/fantsrvkm"
tar --use-compress-program=zstd -xOf docs/planning/evidence/o-stage/5.0.0-i6/o-stage-snapshot.tar.zst o-stage/fantsrvkm/fantaudio_drv.c > "$TMP/tree/fantsrvkm/fantaudio_drv.c"
patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s \
    -d "$TMP/tree" -p1 < patches/m-052-firmware-en-early-guard.patch
python3 - "$TMP/tree" << 'PY'
import hashlib
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
meta = json.loads(Path("patches/m-052-firmware-en-early-guard.meta.json").read_text())
text = (root / "fantsrvkm/fantaudio_drv.c").read_text()
assert hashlib.sha256(text.encode()).hexdigest() == meta["apply"]["file_after_sha256"]
guard = text.index("if (!firmware_en || !fh2m_hal_get_hwinfo_finished_status")
matcher = text.index('fh2m_hal_os_release_match("NFSDesktop"')
assert guard < matcher
assert "DMA_POINTER_NORMAL" in text[guard:matcher]
print("PASS: m-052 replay")
PY

patch --batch --reverse --fuzz=0 --no-backup-if-mismatch -s \
    -d "$TMP/tree" -p1 < patches/m-052-firmware-en-early-guard.patch
python3 - "$TMP/tree" << 'PY'
import hashlib
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
meta = json.loads(Path("patches/m-052-firmware-en-early-guard.meta.json").read_text())
file_sha = hashlib.sha256((root / "fantsrvkm/fantaudio_drv.c").read_bytes()).hexdigest()
assert file_sha == meta["apply"]["file_before_sha256"]
assert "firmware_en" not in (root / "fantsrvkm/fantaudio_drv.c").read_text()
print("PASS: m-052 reverse restores audio file")
PY
