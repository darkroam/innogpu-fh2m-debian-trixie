#!/bin/bash
# Remove the installed innogpu-fh2m-trixie package and disable boot autoload.
#
# Also removes the source-fallback DRI repair helper symlink created by
# scripts/install-dri-node-repair-service.sh (/usr/local/sbin) and the repair
# unit, and removes them even when the deb is NOT installed (a source-tree
# fallback install never installs the package). The helper link is removed
# ONLY when its normalized target is EXACTLY this repository's
# scripts/repair-dri-nodes.sh; foreign symlinks, regular files and files owned
# by the user, the package or unknown owners are never removed. Packaged
# /usr/sbin and /usr/bin helper links are owned by the deb and are removed by
# dpkg -r, not by this script.
#
# Order of operations (no side effects before validation):
#   - if a package version is installed, the expected-version / FORCE check
#     runs FIRST; a refused version aborts with zero changes;
#   - only then are userspace, modules-load, the repair service/helper and the
#     package removed;
#   - a package-absent run only removes the DRI installer's own unit and
#     source-fallback helper - it never touches userspace config or
#     modules-load autoload.
#
# Unit-test hook (NEVER set in real runs):
#   INNOGPU_UNINSTALL_TEST=1                skip EUID check and real system
#                                            teardown (dpkg/dkms/depmod/
#                                            update-initramfs), verify the
#                                            owned-cleanup logic only;
#   INNOGPU_UNINSTALL_TEST_ROOT=<dir>       REQUIRED with the hook: must be a
#                                            non-empty absolute path != "/"
#                                            (fails closed otherwise)
#   INNOGPU_UNINSTALL_TEST_VERSION=<ver>    pretend this package version is
#                                            installed (leave empty to test the
#                                            package-absent path)

set -euo pipefail

TEST_MODE=0
PREFIX=""
if [[ "${INNOGPU_UNINSTALL_TEST:-0}" == "1" ]]; then
    TEST_MODE=1
    TEST_ROOT="${INNOGPU_UNINSTALL_TEST_ROOT:-}"
    NORM="$(readlink -f "$TEST_ROOT" 2>/dev/null || true)"
    if [[ -z "$NORM" || "$NORM" == "/" || "$TEST_ROOT" != /* ]]; then
        echo "ERROR: INNOGPU_UNINSTALL_TEST=1 requires INNOGPU_UNINSTALL_TEST_ROOT (absolute path != /)" >&2
        exit 1
    fi
    PREFIX="$NORM"
fi

EXPECTED_VERSION="${1:-}"
ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"

if [[ ${EUID:-$(id -u)} -ne 0 && "$TEST_MODE" -ne 1 ]]; then
    echo "ERROR: run as root: sudo $0 [expected-version]" >&2
    exit 1
fi

kernel="$(uname -r)"
if [[ "$TEST_MODE" -eq 1 ]]; then
    installed="${INNOGPU_UNINSTALL_TEST_VERSION:-}"
else
    installed="$(dpkg-query -W -f='${Version}' innogpu-fh2m-trixie 2>/dev/null || true)"
fi

# ---- validation before ANY side effect -
if [[ -n "$installed" ]]; then
    if [[ -n "$EXPECTED_VERSION" && "$installed" != "$EXPECTED_VERSION" ]]; then
        echo "ERROR: expected $EXPECTED_VERSION, but installed version is $installed" >&2
        echo "Set FORCE=1 to remove anyway." >&2
        if [[ "${FORCE:-0}" != "1" ]]; then
            exit 1
        fi
    fi
    cat <<EOF
Removing innogpu-fh2m-trixie $installed.

Recovery if this is not intended:
  sudo dpkg -i "$ROOT/debs/innogpu-fh2m-trixie_3.3.3.42-patched-17.deb"
  sudo reboot

EOF

    if command -v innogpu-disable-incompatible-userspace >/dev/null 2>&1 && [[ "$TEST_MODE" -ne 1 ]]; then
        innogpu-disable-incompatible-userspace || true
    fi
    rm -f "$PREFIX/etc/modules-load.d/innogpu.conf"
elif [[ "$TEST_MODE" -eq 1 ]]; then
    # package-absent test path: no userspace / modules-load side effects
    echo "[test-mode] package absent: only DRI repair unit/helper cleanup"
fi

# DRI installer owned paths are cleaned regardless of package presence, but a
# same-named unit is only touched when it is a regular, non-symlink file that
# this project demonstrably owns: it must carry the project's dedicated
# managed-by marker, or match the full legacy structure (project description +
# ExecStart pointing at this project's helper). An unrecognized (foreign) unit
# is preserved and reported.
UNIT_MANAGED_MARKER='# Managed by innogpu-fh2m-debian-trixie install-dri-node-repair-service.sh'
UNIT_LEGACY_DESC='Description=Repair Innogpu DRM/fbdev device nodes'
expected_helper="$(readlink -f "$ROOT/scripts/repair-dri-nodes.sh" 2>/dev/null || true)"
pkg_helper="$PREFIX/usr/share/innogpu-fh2m-trixie/repair-dri-nodes.sh"
is_project_unit() {
    local f="$1" exec_line resolved cand
    [[ -f "$f" && ! -L "$f" ]] || return 1
    grep -Fq "$UNIT_MANAGED_MARKER" "$f" 2>/dev/null && return 0
    grep -Fq "$UNIT_LEGACY_DESC" "$f" 2>/dev/null || return 1
    exec_line="$(sed -n 's/^ExecStart=//p' "$f" 2>/dev/null | head -1)"
    [[ -n "$exec_line" ]] || return 1
    resolved="$(readlink -f "$exec_line" 2>/dev/null || true)"
    for cand in "$expected_helper" "$pkg_helper"; do
        if [[ -n "$resolved" && "$resolved" == "$(readlink -f "$cand" 2>/dev/null || true)" ]]; then
            return 0
        fi
    done
    return 1
}
remove_owned_unit() {
    local unit="$1"
    if [[ -e "$unit" || -L "$unit" ]]; then
        if is_project_unit "$unit"; then
            systemctl disable --now innogpu-repair-dri-nodes.service 2>/dev/null || true
            rm -f "$unit"
            echo "Removed repair unit: $unit"
        else
            echo "Kept $unit (not this project's unit; left untouched)" >&2
        fi
    fi
    true
}
remove_owned_unit "$PREFIX/etc/systemd/system/innogpu-repair-dri-nodes.service"
systemctl daemon-reload 2>/dev/null || true

# Remove the source-fallback helper ONLY if this project created it: the link
# must be a symlink whose normalized target is EXACTLY this repository's
# scripts/repair-dri-nodes.sh (never a same-named path from another checkout).
remove_owned_symlink() {
    local link="$1" expected="$2" target
    if [[ -L "$link" ]]; then
        target="$(readlink -f "$link" 2>/dev/null || true)"
        if [[ -n "$target" && "$target" == "$expected" ]]; then
            rm -f "$link"
            echo "Removed source-fallback helper symlink: $link"
        else
            echo "Kept $link (target $target does not equal $expected)" >&2
        fi
    fi
    true
}
expected_helper="$(readlink -f "$ROOT/scripts/repair-dri-nodes.sh" 2>/dev/null || true)"
if [[ -n "$expected_helper" ]]; then
    remove_owned_symlink "$PREFIX/usr/local/sbin/innogpu-repair-dri-nodes" "$expected_helper"
fi

if [[ -z "$installed" ]]; then
    echo "innogpu-fh2m-trixie is not installed; removed repair service/helper if present."
    exit 0
fi

if [[ "$TEST_MODE" -eq 1 ]]; then
    echo "[test-mode] skipped dpkg -r / dkms remove / depmod / update-initramfs"
else
    dpkg -r innogpu-fh2m-trixie

    if command -v dkms >/dev/null 2>&1; then
        dkms remove innogpu-kernel/2.2 --all 2>/dev/null || true
    fi

    depmod -a "$kernel" || true
    if command -v update-initramfs >/dev/null 2>&1; then
        update-initramfs -u -k "$kernel" || true
    fi
fi

echo
echo "Uninstall complete. Reboot to ensure no old innogpu module remains loaded:"
echo "  sudo reboot"
