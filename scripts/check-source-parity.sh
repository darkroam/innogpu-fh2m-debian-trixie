#!/bin/bash
# Reproducible source-tree parity check: drivers/ vs the patched-27 generated
# source tree (Deepin raw source + the 9 enabled patches in builder order).
# Read-only: builds the reference in a temp dir, compares, outputs machine-
# readable PASS/FAIL. Does not modify drivers/, the old builder or the device.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

SRC="$ROOT/third_party/innogpu-fh2m-deepin-202504/root/usr/src/innogpu-kernel-2.2"
[[ -d "$SRC" ]] || { echo "parity_source_input=FAIL (missing $SRC)"; exit 1; }

mkdir -p "$ROOT/.build"
W="$(mktemp -d "$ROOT/.build/parity.XXXXXX")"
trap 'rm -rf "$W"' EXIT

for d in innodma innogpu innopmbus innopower innosmmu innosrvkm innovpu tools; do
    cp -r "$SRC/$d" "$W/"
done
for f in Makefile Kbuild dkms.conf dkms.post_remove dkms.pre_install cfg_detect.sh modules_config.sh test_item.sh; do
    [ -f "$SRC/$f" ] && cp "$SRC/$f" "$W/"
done

cd "$W"
patch -p6 -s < "$ROOT/patches/001-kernel-6.12-compat.patch"
for n in 002 006 009 007 023 025 026 027; do
    p="$(ls "$ROOT/patches/$n-"*.patch)"
    patch -p1 -s < "$p"
done
find . -name '*.orig' -delete
find . -name '*.rej' -delete
cd "$ROOT"

diff_lines="$(diff -rq "$ROOT/drivers" "$W" 2>/dev/null | grep -vE 'README\.md|o_shipped|\.o\.cmd' | wc -l || true)"
echo "parity_source_input=Deepin_20250421190503"
echo "parity_reference=third_party+9_enabled_patches(builder order, .orig/.rej cleaned)"
echo "parity_output=drivers/"
echo "parity_exclude=drivers/README.md, *.o_shipped, *.o.cmd(build artifacts)"
echo "parity_diff_lines=$diff_lines"
if [[ "$diff_lines" -eq 0 ]]; then
    echo "source_tree_parity_against_p27=PASS"
    exit 0
else
    echo "source_tree_parity_against_p27=FAIL"
    diff -rq "$ROOT/drivers" "$W" 2>/dev/null | grep -vE 'README\.md|o_shipped|\.o\.cmd' | head -20
    exit 1
fi
