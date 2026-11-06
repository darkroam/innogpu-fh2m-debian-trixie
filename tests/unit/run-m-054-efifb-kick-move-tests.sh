#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/m-054-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT
cd "$ROOT"

ROOT="$ROOT" python3 - "$TMP" << 'PY'
import hashlib, importlib.util, json, os, subprocess, sys
from pathlib import Path

root = Path(os.environ["ROOT"])
tmp = Path(sys.argv[1]) / "whole"
patch_path = root / "patches/m-054-efifb-kick-move.patch"
meta = json.loads((root / "patches/m-054-efifb-kick-move.meta.json").read_text())
patch_sha = hashlib.sha256(patch_path.read_bytes()).hexdigest()
assert patch_sha == meta["apply"]["patch_sha256"] == meta["deepin_patch"]["sha256"]
derive = (root / "tools/internal/derive-fantgpu-source.sh").read_text()
builder = (root / "tools/debian/build-driver.sh").read_text()
gentoo = (root / "tools/gentoo/fh2m-lib.sh").read_text()
assert "m-054-efifb-kick-move.patch" not in derive
package = builder.split("DKMS_SRC_NAME", 1)[1]
assert package.index("m-052-firmware-en-early-guard.patch") < package.index("m-054-efifb-kick-move.patch")
assert "builder_efifb_kick_move=PASS" in package
assert gentoo.index("m-052-firmware-en-early-guard.patch") < gentoo.index("m-054-efifb-kick-move.patch") < gentoo.index("g-053-fbdev-restore-force.patch")
assert meta["verified"]["clean_build"]["status"] == "pass"
assert meta["verified"]["clean_build"]["kernel"] == "6.12.113-gentoo-x86_64"
assert meta["verified"]["clean_build"]["srcversion"] == "227E896A45E34187252CD2D"

subprocess.check_call(["rm", "-rf", str(tmp)])
tmp.mkdir()
subprocess.check_call(["tar", "--use-compress-program=zstd", "-xf",
    str(root / "docs/planning/evidence/o-stage/5.0.0-i6/o-stage-snapshot.tar.zst"), "-C", str(tmp)])
src = tmp / "o-stage"
env = os.environ.copy()
env["ROOT"] = str(root)
subprocess.check_call(["bash", "-c", 'cd "$1"; source tools/internal/derive-fantgpu-source.sh; derive_fantgpu_source "$2"',
    "derive", str(root), str(src)], env=env)
for name in ("m-036-bf-shf-rename", "m-051-hwinfo-connector-fallback", "m-052-firmware-en-early-guard"):
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
assert sha_file("fantgpu/fantgpu_pci_drv.c") == meta["apply"]["pci_before_sha256"]
assert sha_file("fantsrvkm/fantdpu_drm_fb.c") == meta["apply"]["fb_before_sha256"]
subprocess.check_call(["patch", "--batch", "--forward", "--fuzz=0", "--no-backup-if-mismatch", "-s", "-d", str(src), "-p1"],
    stdin=open(patch_path))
assert tree_hash(src) == meta["apply"]["after_tree_hash"]
assert sha_file("fantgpu/fantgpu_pci_drv.c") == meta["apply"]["pci_after_sha256"]
assert sha_file("fantsrvkm/fantdpu_drm_fb.c") == meta["apply"]["fb_after_sha256"]
pci = (src / "fantgpu/fantgpu_pci_drv.c").read_text()
fb = (src / "fantsrvkm/fantdpu_drm_fb.c").read_text()
assert "fh2m_fant_drm_fb_kick_off_efifb" not in pci
kick = fb.index("if (!fh2m_hal_get_nulldisplay())\n\t\tfh2m_fant_drm_fb_kick_off_efifb();")
config = fb.index("drm_fb_helper_initial_config(&fant_fbdev->helper);")
assert kick < config
assert fb.count("fh2m_fant_drm_fb_kick_off_efifb") == 1
subprocess.check_call(["patch", "--batch", "--forward", "--fuzz=0", "--no-backup-if-mismatch", "-s", "-d", str(src), "-p1"],
    stdin=open(root / "patches/g-053-fbdev-restore-force.patch"))
subprocess.check_call(["patch", "--batch", "--reverse", "--fuzz=0", "--no-backup-if-mismatch", "-s", "-d", str(src), "-p1"],
    stdin=open(root / "patches/g-053-fbdev-restore-force.patch"))
subprocess.check_call(["patch", "--batch", "--reverse", "--fuzz=0", "--no-backup-if-mismatch", "-s", "-d", str(src), "-p1"],
    stdin=open(patch_path))
assert tree_hash(src) == before
assert sha_file("fantgpu/fantgpu_pci_drv.c") == meta["apply"]["pci_before_sha256"]
print("PASS: m-054 move")
PY
