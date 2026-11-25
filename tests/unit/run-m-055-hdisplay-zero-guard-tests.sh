#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/m-055-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT
cd "$ROOT"

ROOT="$ROOT" python3 - "$TMP" << 'PY'
import hashlib, importlib.util, json, os, subprocess, sys
from pathlib import Path

root = Path(os.environ["ROOT"])
tmp = Path(sys.argv[1]) / "whole"
patch_path = root / "patches/m-055-hdisplay-zero-guard.patch"
meta = json.loads((root / "patches/m-055-hdisplay-zero-guard.meta.json").read_text())
patch_sha = hashlib.sha256(patch_path.read_bytes()).hexdigest()
assert patch_sha == meta["apply"]["patch_sha256"] == meta["deepin_patch"]["sha256"]
derive = (root / "tools/internal/derive-fantgpu-source.sh").read_text()
builder = (root / "tools/debian/build-driver.sh").read_text()
gentoo = (root / "tools/gentoo/fh2m-lib.sh").read_text()
assert "m-055-hdisplay-zero-guard.patch" not in derive
package = builder.split("DKMS_SRC_NAME", 1)[1]
assert package.index("m-054-efifb-kick-move.patch") < package.index("m-055-hdisplay-zero-guard.patch")
assert "builder_hdisplay_zero_guard=PASS" in package
assert gentoo.index("m-054-efifb-kick-move.patch") < gentoo.index("m-055-hdisplay-zero-guard.patch") < gentoo.index("g-053-fbdev-restore-force.patch")
assert meta["verified"]["clean_build"]["status"] == "pass"
assert meta["verified"]["clean_build"]["kernel"] == "6.12.115-gentoo-x86_64"
assert meta["verified"]["clean_build"]["srcversion"] == "B72875836C2992A4FD97B66"

subprocess.check_call(["rm", "-rf", str(tmp)])
tmp.mkdir()
subprocess.check_call(["tar", "--use-compress-program=zstd", "-xf",
    str(root / "docs/planning/evidence/o-stage/5.0.0-i6/o-stage-snapshot.tar.zst"), "-C", str(tmp)])
src = tmp / "o-stage"
env = os.environ.copy()
env["ROOT"] = str(root)
subprocess.check_call(["bash", "-c", 'cd "$1"; source tools/internal/derive-fantgpu-source.sh; derive_fantgpu_source "$2"',
    "derive", str(root), str(src)], env=env)
for name in ("m-036-bf-shf-rename", "m-051-hwinfo-connector-fallback", "m-052-firmware-en-early-guard", "m-054-efifb-kick-move"):
    subprocess.check_call(["patch", "--batch", "--forward", "--fuzz=0", "--no-backup-if-mismatch", "-s", "-d", str(src), "-p1"],
        stdin=open(root / f"patches/{name}.patch"))
spec = importlib.util.spec_from_file_location("o4", root / "tools/internal/o4-f0-lock-gen.py")
o4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(o4)

def tree_hash(path):
    return hashlib.sha256(o4.manifest_text(list(o4.walk_rows(str(path)))).encode()).hexdigest()

def sha_file(rel):
    return hashlib.sha256((src / rel).read_bytes()).hexdigest()

before = tree_hash(src)
assert before == meta["apply"]["before_tree_hash"] == meta["f0_base"]["tree_hash"]
assert sha_file("fantsrvkm/pdp0_crtc.c") == meta["apply"]["crtc_before_sha256"]
subprocess.check_call(["patch", "--batch", "--forward", "--fuzz=0", "--no-backup-if-mismatch", "-s", "-d", str(src), "-p1"],
    stdin=open(patch_path))
assert tree_hash(src) == meta["apply"]["after_tree_hash"]
assert sha_file("fantsrvkm/pdp0_crtc.c") == meta["apply"]["crtc_after_sha256"]
crtc = (src / "fantsrvkm/pdp0_crtc.c").read_text()
guard = crtc.index("if (state->adjusted_mode.hdisplay == 0 ||\n\t\t    state->adjusted_mode.vdisplay == 0)")
false = crtc.index("s->enhancer_enable = false;", guard)
div = crtc.index("div_u64", guard)
assert guard < false < div
subprocess.check_call(["patch", "--batch", "--forward", "--fuzz=0", "--no-backup-if-mismatch", "-s", "-d", str(src), "-p1"],
    stdin=open(root / "patches/g-053-fbdev-restore-force.patch"))
subprocess.check_call(["patch", "--batch", "--reverse", "--fuzz=0", "--no-backup-if-mismatch", "-s", "-d", str(src), "-p1"],
    stdin=open(root / "patches/g-053-fbdev-restore-force.patch"))
subprocess.check_call(["patch", "--batch", "--reverse", "--fuzz=0", "--no-backup-if-mismatch", "-s", "-d", str(src), "-p1"],
    stdin=open(patch_path))
assert tree_hash(src) == before
assert sha_file("fantsrvkm/pdp0_crtc.c") == meta["apply"]["crtc_before_sha256"]
print("PASS: m-055 guard")
PY
