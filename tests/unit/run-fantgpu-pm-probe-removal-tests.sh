#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
CHECK="$ROOT/scripts/check-fantgpu-pm-probe-removed.sh"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
PASS=0
FAIL=0

ok() { echo "pm-probe-removal-$1=PASS"; PASS=$((PASS + 1)); }
bad() { echo "pm-probe-removal-$1=FAIL${2:+ reason=$2}"; FAIL=$((FAIL + 1)); }

make_fixture() {
    local root=$1
    mkdir -p "$root/source" "$root/snapshot/o-stage" "$root/builder" \
             "$root/dkms" "$root/pkg/DEBIAN" "$root/pkg/usr/src/fantgpu"
    printf 'clean source\n' > "$root/source/driver.c"
    printf 'clean snapshot\n' > "$root/snapshot/o-stage/driver.c"
    tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner \
        -I 'zstd -q -19' -cf "$root/snapshot.tar.zst" -C "$root/snapshot" o-stage
    printf 'path\tsha256\n' > "$root/manifest.tsv"
    printf 'clean builder\n' > "$root/builder/driver.c"
    printf 'clean dkms\n' > "$root/dkms/driver.c"
    printf 'clean module\n' > "$root/fantgpu.ko"
    printf 'Package: fantgpu-fixture\nVersion: 1\nArchitecture: amd64\nMaintainer: fixture <fixture@example.invalid>\nDescription: fixture\n' \
        > "$root/pkg/DEBIAN/control"
    printf 'clean payload\n' > "$root/pkg/usr/src/fantgpu/driver.c"
    dpkg-deb --build --root-owner-group "$root/pkg" "$root/fixture.deb" >/dev/null
}

run_check() {
    local root=$1
    "$CHECK" "$root/source" "$root/snapshot.tar.zst" "$root/manifest.tsv" \
        "$root/builder" "$root/dkms" "$root/fantgpu.ko" "$root/fixture.deb" \
        >/dev/null 2>&1
}

make_fixture "$TMP/clean"
run_check "$TMP/clean" && ok t01_clean_all_layers || bad t01_clean_all_layers

for spec in \
    'source:source/driver.c' \
    'manifest:manifest.tsv' \
    'builder:builder/driver.c' \
    'dkms:dkms/driver.c' \
    'module:fantgpu.ko'; do
    label=${spec%%:*}
    rel=${spec#*:}
    cp -a "$TMP/clean" "$TMP/$label"
    printf 'R5_I4_DIAGNOSTIC\n' >> "$TMP/$label/$rel"
    if run_check "$TMP/$label"; then
        bad "t_${label}" accepted_diagnostic_token
    else
        ok "t_${label}"
    fi
done

cp -a "$TMP/clean" "$TMP/snapshot_bad"
printf 'fantgpu_pm_probe\n' >> "$TMP/snapshot_bad/snapshot/o-stage/driver.c"
tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner \
    -I 'zstd -q -19' -cf "$TMP/snapshot_bad/snapshot.tar.zst" \
    -C "$TMP/snapshot_bad/snapshot" o-stage
run_check "$TMP/snapshot_bad" && bad t_snapshot accepted_diagnostic_token || ok t_snapshot

cp -a "$TMP/clean" "$TMP/deb_bad"
printf 'completed_step\n' >> "$TMP/deb_bad/pkg/usr/src/fantgpu/driver.c"
dpkg-deb --build --root-owner-group "$TMP/deb_bad/pkg" "$TMP/deb_bad/fixture.deb" >/dev/null
run_check "$TMP/deb_bad" && bad t_deb accepted_diagnostic_token || ok t_deb

echo "PASS=$PASS FAIL=$FAIL"
[[ "$FAIL" -eq 0 ]]
