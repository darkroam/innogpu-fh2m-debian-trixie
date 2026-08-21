#!/bin/bash
# Read-only release gate for coherent packages built from the current source tree.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
DEB="${1:-}"

if [[ -z "$DEB" || $# -ne 1 ]]; then
    echo "Usage: scripts/check-release-package.sh DEB" >&2
    exit 2
fi
[[ -f "$DEB" ]] || { echo "ERROR: package not found: $DEB" >&2; exit 1; }
command -v dpkg-deb >/dev/null 2>&1 || { echo "ERROR: dpkg-deb is required" >&2; exit 1; }
command -v tar >/dev/null 2>&1 || { echo "ERROR: tar is required" >&2; exit 1; }

package=$(dpkg-deb -f "$DEB" Package)
version=$(dpkg-deb -f "$DEB" Version)
architecture=$(dpkg-deb -f "$DEB" Architecture)
installed_size=$(dpkg-deb -f "$DEB" Installed-Size)
[[ "$package" == "innogpu-fh2m-trixie" ]] || {
    echo "ERROR: unexpected package: $package" >&2
    exit 1
}
# 接受旧架构 patched-N（N>20）与新架构 4.0.0-iN（迁移阶段 3+）
if ! { [[ "$version" =~ ^3\.3\.3\.42-patched-([0-9]+)$ ]] && (( 10#${BASH_REMATCH[1]} > 20 )); } &&
   ! [[ "$version" =~ ^4\.0\.0-i([0-9]+)$ ]]; then
    echo "ERROR: release audit only accepts patched-N (N>20) or 4.0.0-iN versions: $version" >&2
    exit 1
fi
[[ "$architecture" == "amd64" ]] || {
    echo "ERROR: unexpected architecture: $architecture" >&2
    exit 1
}
[[ "$installed_size" =~ ^[1-9][0-9]*$ ]] || {
    echo "ERROR: missing or invalid Installed-Size: $installed_size" >&2
    exit 1
}

runtime=$(mktemp -d "${TMPDIR:-/tmp}/innogpu-package-audit.XXXXXX")
trap 'rm -rf "$runtime"' EXIT HUP INT TERM
dpkg-deb --fsys-tarfile "$DEB" | tar -tf - |
    sed -e 's#^\./##' -e 's#/$##' > "$runtime/files"

required=(
    lib/firmware/innogpu/fh2m.fw
    lib/firmware/innogpu/fh2m.sh
    lib/firmware/innogpu/fh2c.fw
    lib/firmware/innogpu/fh2c.sh
    usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so
    usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so
    usr/lib/x86_64-linux-gnu/innogpu-fh2m/libgbm.so.1.0.0
    usr/lib/x86_64-linux-gnu/innogpu-fh2m/libglapi_inno.so.0.0.0
    usr/lib/xorg/modules/drivers/innogpu_drv.so
    usr/share/glvnd/egl_vendor.d/00_inno.json
    usr/share/innogpu-fh2m-trixie/restore-dp1-mode-x11.sh
    usr/share/innogpu-fh2m-trixie/xdisplay-session.sh
    usr/share/innogpu-fh2m-trixie/install-xdisplay-user.sh
)
for path in "${required[@]}"; do
    grep -Fxq "$path" "$runtime/files" || {
        echo "ERROR: required release file is missing: $path" >&2
        exit 1
    }
done

forbidden=(
    usr/share/innogpu-fh2m-trixie/xdisplay.sh
    usr/share/innogpu-fh2m-trixie/displayselect
    usr/share/innogpu-fh2m-trixie/install-kylin-userspace.sh
    usr/share/innogpu-fh2m-trixie/install-experimental-hwgl.sh
    usr/share/innogpu-fh2m-trixie/patch-skip-first-gpupll.sh
    usr/bin/innogpu-install-kylin-userspace
    usr/bin/innogpu-install-experimental-hwgl
    usr/bin/innogpu-skip-first-gpupll
    usr/sbin/innogpu-install-kylin-userspace
    usr/sbin/innogpu-install-experimental-hwgl
    usr/sbin/innogpu-skip-first-gpupll
)
for path in "${forbidden[@]}"; do
    if grep -Fxq "$path" "$runtime/files"; then
        echo "ERROR: forbidden release file is present: $path" >&2
        exit 1
    fi
done

install -d "$runtime/helpers"
for helper in restore-dp1-mode-x11.sh xdisplay-session.sh install-xdisplay-user.sh; do
    dpkg-deb --fsys-tarfile "$DEB" |
        tar -xOf - "./usr/share/innogpu-fh2m-trixie/$helper" > "$runtime/helpers/$helper"
    cmp -s "$ROOT/scripts/$helper" "$runtime/helpers/$helper" || {
        echo "ERROR: packaged integration helper differs from current source: $helper" >&2
        exit 1
    }
done

echo "RESULT: PASS_RELEASE_PACKAGE_BOUNDARIES package=$package version=$version architecture=$architecture"
