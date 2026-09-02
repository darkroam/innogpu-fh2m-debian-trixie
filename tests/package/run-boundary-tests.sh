#!/bin/bash

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
CHECK="$ROOT/scripts/check-release-package.sh"
runtime=$(mktemp -d "${TMPDIR:-/tmp}/innogpu-package-tests.XXXXXX")
tests=0
skipped=0
SUITE=package_boundary
trap 'rm -rf "$runtime"' EXIT HUP INT TERM

fail() {
    printf '%s_t%02d=FAIL reason=%s\n' "$SUITE" "$tests" "$1" >&2
    exit 1
}

pass() {
    tests=$((tests + 1))
    printf '%s_t%02d=PASS # %s\n' "$SUITE" "$tests" "$1"
}

make_package() {
    local name=$1
    local version=$2
    local variant=${3:-valid}
    local root="$runtime/$name-root"
    local deb="$runtime/$name.deb"

    install -d \
        "$root/DEBIAN" \
        "$root/lib/firmware/innogpu" \
        "$root/usr/lib/x86_64-linux-gnu/dri" \
        "$root/usr/lib/x86_64-linux-gnu/gbm" \
        "$root/usr/lib/x86_64-linux-gnu/innogpu-fh2m" \
        "$root/usr/lib/xorg/modules/drivers" \
        "$root/usr/share/glvnd/egl_vendor.d" \
        "$root/usr/share/innogpu-fh2m-trixie"
    printf 'Package: innogpu-fh2m-trixie\nVersion: %s\nArchitecture: amd64\nInstalled-Size: 1\nMaintainer: test <test@example.invalid>\nDescription: fixture\n' \
        "$version" > "$root/DEBIAN/control"
    : > "$root/lib/firmware/innogpu/fh2m.fw"
    : > "$root/lib/firmware/innogpu/fh2m.sh"
    : > "$root/lib/firmware/innogpu/fh2c.fw"
    : > "$root/lib/firmware/innogpu/fh2c.sh"
    : > "$root/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so"
    : > "$root/usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so"
    : > "$root/usr/lib/x86_64-linux-gnu/innogpu-fh2m/libgbm.so.1.0.0"
    : > "$root/usr/lib/x86_64-linux-gnu/innogpu-fh2m/libglapi_inno.so.0.0.0"
    : > "$root/usr/lib/xorg/modules/drivers/innogpu_drv.so"
    : > "$root/usr/share/glvnd/egl_vendor.d/00_inno.json"
    for helper in restore-dp1-mode-x11.sh xdisplay-session.sh install-xdisplay-user.sh; do
        cp "$ROOT/scripts/$helper" "$root/usr/share/innogpu-fh2m-trixie/$helper"
    done
    case "$variant" in
        legacy)
            : > "$root/usr/share/innogpu-fh2m-trixie/xdisplay.sh"
            ;;
        stale-helper)
            printf '\n# stale fixture\n' >> "$root/usr/share/innogpu-fh2m-trixie/install-xdisplay-user.sh"
            ;;
        missing-shader)
            rm "$root/lib/firmware/innogpu/fh2c.sh"
            ;;
        wrong-architecture)
            sed -i 's/Architecture: amd64/Architecture: arm64/' "$root/DEBIAN/control"
            ;;
        missing-installed-size)
            sed -i '/^Installed-Size:/d' "$root/DEBIAN/control"
            ;;
    esac
    dpkg-deb --root-owner-group --build "$root" "$deb" >/dev/null
    printf '%s\n' "$deb"
}

valid=$(make_package valid 3.3.3.42-patched-21)
"$CHECK" "$valid" >/dev/null || fail 'T01 valid package failed the boundary check'
pass 'clean new-version package passes'

candidate=$(make_package candidate 4.0.1-i2)
"$CHECK" "$candidate" >/dev/null || fail 'T02 current 4.0.1-i2 candidate failed the boundary check'
pass 'current new-architecture candidate passes'

noncanonical=$(make_package noncanonical 4.0.1-i0)
if "$CHECK" "$noncanonical" >/dev/null 2>&1; then
    fail 'T03 noncanonical new-architecture version passed'
fi
pass 'noncanonical new-architecture version is rejected'

legacy=$(make_package legacy 3.3.3.42-patched-21 legacy)
if "$CHECK" "$legacy" >/dev/null 2>&1; then
    fail 'T04 package with private xdisplay copy passed'
fi
pass 'private xdisplay payload is rejected'

historical=$(make_package historical 3.3.3.42-patched-20)
if "$CHECK" "$historical" >/dev/null 2>&1; then
    fail 'T05 historical version reuse passed'
fi
pass 'patched-20 version reuse is rejected'

stale=$(make_package stale 3.3.3.42-patched-21 stale-helper)
if "$CHECK" "$stale" >/dev/null 2>&1; then
    fail 'T06 stale integration helper passed'
fi
pass 'packaged integration helpers must match current source'

missing_shader=$(make_package missing-shader 3.3.3.42-patched-21 missing-shader)
if "$CHECK" "$missing_shader" >/dev/null 2>&1; then
    fail 'T07 package without complete shader firmware passed'
fi
pass 'complete firmware and shader payload is required'

wrong_arch=$(make_package wrong-arch 3.3.3.42-patched-21 wrong-architecture)
if "$CHECK" "$wrong_arch" >/dev/null 2>&1; then
    fail 'T08 package with wrong architecture passed'
fi
pass 'non-amd64 package is rejected'

missing_size=$(make_package missing-size 3.3.3.42-patched-21 missing-installed-size)
if "$CHECK" "$missing_size" >/dev/null 2>&1; then
    fail 'T09 package without Installed-Size passed'
fi
pass 'Installed-Size is required'

printf 'tests_total=%d tests_passed=%d tests_failed=%d tests_skipped=%d\n' "$tests" "$tests" 0 "$skipped"
