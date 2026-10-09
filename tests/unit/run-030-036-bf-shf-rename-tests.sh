#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/030-036-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT
cd "$ROOT"

tar --use-compress-program=zstd -xf \
    docs/planning/evidence/o-stage/5.0.0-i6/o-stage-snapshot.tar.zst -C "$TMP"
tree="$TMP/o-stage"

python3 - "$tree" <<'PY'
import hashlib
import json
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
patch_path = Path("patches/030-036.patch")
patch = patch_path.read_text()
meta = json.loads(Path("patches/030-036.meta.json").read_text())
patch_sha = hashlib.sha256(patch_path.read_bytes()).hexdigest()
assert patch_sha == meta["apply"]["patch_sha256"] == meta["deepin_patch"]["sha256"]
assert set(re.findall(r"^\+\+\+ b/(.+)$", patch, re.M)) == {"fantgpu/fant_math.h"}
assert patch.count("+#define __fant_bf_shf(x) (__builtin_ffsll(x) - 1)") == 1
assert patch.count("-#define __bf_shf(x) (__builtin_ffsll(x) - 1)") == 1
assert patch.count("__fant_bf_shf(_mask)") == 2
assert "#undef" not in patch and "#ifndef" not in patch
header = (root / "fantgpu/fant_math.h").read_bytes()
assert hashlib.sha256(header).hexdigest() == meta["apply"]["file_before_sha256"]
assert meta["apply"]["before_tree_hash"] == "9a8d185f2a65892a585b8f8f699f849a46ac6a4ab3c3e39a6a3a09357c2eb2ee"
assert meta["apply"]["after_tree_hash"] == "0eda30cdce3da3d872c56e7ebb3c89b4dc23a5fead402f0e43300b2130a184ce"
assert meta["apply"]["i6_replay_after_tree_hash"] != meta["apply"]["after_tree_hash"]
builder = Path("tools/build-innogpu-driver.sh").read_text()
assert "030-036" not in builder
assert meta["apply"]["before_tree_hash"] in builder
print("PASS: 030-036 patch contract")
PY

patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s \
    -d "$tree" -p1 < patches/030-036.patch

python3 - "$tree" <<'PY'
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

root = Path(sys.argv[1])
meta = json.loads(Path("patches/030-036.meta.json").read_text())
header = (root / "fantgpu/fant_math.h").read_text()
assert header.count("__fant_bf_shf") == 3
assert "__bf_shf" not in header
assert "(__builtin_ffsll(x) - 1)" in header
assert hashlib.sha256(header.encode()).hexdigest() == meta["apply"]["file_after_sha256"]
spec = importlib.util.spec_from_file_location("o4", "tools/internal/o4-f0-lock-gen.py")
o4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(o4)
digest = hashlib.sha256(o4.manifest_text(list(o4.walk_rows(root))).encode()).hexdigest()
assert digest == meta["apply"]["i6_replay_after_tree_hash"], digest
assert digest != meta["apply"]["after_tree_hash"]
print("PASS: 030-036 i6 header replay")
PY

test -z "$(find "$tree" -type f \( -name '*.orig' -o -name '*.rej' \) -print -quit)"

patch --batch --reverse --fuzz=0 --no-backup-if-mismatch -s \
    -d "$tree" -p1 < patches/030-036.patch
python3 - "$tree" <<'PY'
import hashlib
import importlib.util
from pathlib import Path
import sys

spec = importlib.util.spec_from_file_location("o4", "tools/internal/o4-f0-lock-gen.py")
o4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(o4)
digest = hashlib.sha256(o4.manifest_text(list(o4.walk_rows(Path(sys.argv[1])))).encode()).hexdigest()
assert digest == "5f6a5347c7e217ba3f7c5b71fdcad520148e0bc71231ab95fb023655865d11da", digest
print("PASS: reverse restores locked i6 tree")
PY
