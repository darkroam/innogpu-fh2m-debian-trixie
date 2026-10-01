#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/030-035-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT
cd "$ROOT"

tar --use-compress-program=zstd -xf \
    docs/planning/evidence/o-stage/5.0.0-i6/o-stage-snapshot.tar.zst -C "$TMP"
tree="$TMP/o-stage"
before_dvfs="$(sha256sum "$tree/fantsrvkm/ft_dvfs_device.c" | awk '{print $1}')"

patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s \
    -d "$tree" -p1 < patches/030-035.patch

python3 - "$tree" <<'PY'
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
patch = Path('patches/030-035.patch').read_text()
meta = json.loads(Path('patches/030-035.meta.json').read_text())

assert hashlib.sha256(Path('patches/030-035.patch').read_bytes()).hexdigest() == meta['apply']['patch_sha256']
assert set(re.findall(r'^\+\+\+ b/(.+)$', patch, re.M)) == {'fantsrvkm/ft_drm.c'}
assert patch.count('+\tstruct ft_drm_private *priv = fantgpu_drm_to_ft_private(ddev);') == 2
assert patch.count('-\tstruct ft_drm_private *priv = ddev->dev_private;') == 2

source = (root / 'fantsrvkm/ft_drm.c').read_text()
helper_source = (root / 'fantsrvkm/fantgpu_drm.c').read_text()

def body(name, text):
    match = re.search(r'^static int ' + name + r'\(.*?^}', text, re.M | re.S)
    assert match, name
    return match.group()

for name, call in [('ft_pm_suspend', 'SuspendDVFS(priv->dev_node)'),
                   ('ft_pm_resume', 'ResumeDVFS(priv->dev_node)')]:
    function = body(name, source)
    assert function.count('fantgpu_drm_to_ft_private(ddev)') == 1
    assert 'struct ft_drm_private *priv = ddev->dev_private' not in function
    assert call in function

helper = re.search(r'^void \* fantgpu_drm_to_ft_private\(.*?^}', helper_source, re.M | re.S).group()
assert 'drm_private = drm_dev->dev_private' in helper
assert 'return &(drm_private->ft_priv)' in helper

spec = importlib.util.spec_from_file_location('o4', 'tools/o4-f0-lock-gen.py')
o4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(o4)
digest = hashlib.sha256(o4.manifest_text(list(o4.walk_rows(root))).encode()).hexdigest()
assert digest == meta['apply']['after_tree_hash'], digest
print('PASS: 030-035 exact two-line private accessor fix')
PY

test "$(sha256sum "$tree/fantsrvkm/ft_dvfs_device.c" | awk '{print $1}')" = "$before_dvfs"
test -z "$(find "$tree" -type f \( -name '*.orig' -o -name '*.rej' \) -print -quit)"

patch --batch --reverse --fuzz=0 --no-backup-if-mismatch -s \
    -d "$tree" -p1 < patches/030-035.patch
python3 - "$tree" <<'PY'
import hashlib
import importlib.util
from pathlib import Path
import sys

spec = importlib.util.spec_from_file_location('o4', 'tools/o4-f0-lock-gen.py')
o4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(o4)
digest = hashlib.sha256(o4.manifest_text(list(o4.walk_rows(Path(sys.argv[1])))).encode()).hexdigest()
assert digest == '5f6a5347c7e217ba3f7c5b71fdcad520148e0bc71231ab95fb023655865d11da', digest
print('PASS: reverse restores locked i6 tree')
PY
