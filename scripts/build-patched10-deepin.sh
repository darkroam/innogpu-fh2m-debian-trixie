#!/bin/bash
set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

BASE_DEB=${BASE_DEB:-$ROOT/innogpu-fh2m-trixie_3.3.3.42-patched-8.deb}
if [[ -z "${DEEPIN_ROOT:-}" ]]; then
    if [[ -x "$ROOT/scripts/prepare-deepin-userspace-root.sh" ]]; then
        DEEPIN_ROOT="$("$ROOT/scripts/prepare-deepin-userspace-root.sh")"
    else
        DEEPIN_ROOT="${INNOGPU_DEEPIN_ROOT:-$ROOT/third_party/innogpu-fh2m-deepin-202504/root}"
    fi
fi
DEEPIN_SRC="$DEEPIN_ROOT/usr/src/innogpu-kernel-2.2"
PATCH_VERSION=${PATCH_VERSION:-10}
APPLY_DP_FBCON_FALLBACK=${APPLY_DP_FBCON_FALLBACK:-0}
APPLY_PANEL_BACKLIGHT_FALLBACK=${APPLY_PANEL_BACKLIGHT_FALLBACK:-0}
APPLY_PANEL_PLATFORM_FALLBACK=${APPLY_PANEL_PLATFORM_FALLBACK:-0}
APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE=${APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE:-0}
APPLY_LOCAL_CONNECTOR_ACPI_MAP=${APPLY_LOCAL_CONNECTOR_ACPI_MAP:-0}
OUT_DEB=${OUT_DEB:-innogpu-fh2m-trixie_3.3.3.42-patched-${PATCH_VERSION}.deb}

[[ -f "$BASE_DEB" ]] || { echo "missing base deb: $BASE_DEB" >&2; exit 1; }
[[ -d "$DEEPIN_SRC" ]] || { echo "missing Deepin DKMS source: $DEEPIN_SRC" >&2; exit 1; }

W=$(mktemp -d /tmp/innogpu-pkg10.XXXXXX)
mkdir -p "$W/root" "$W/DEBIAN"
dpkg-deb -x "$BASE_DEB" "$W/root"
dpkg-deb -e "$BASE_DEB" "$W/DEBIAN"

rm -rf "$W/root/usr/src/innogpu-kernel-2.2"
cp -a "$DEEPIN_SRC" "$W/root/usr/src/innogpu-kernel-2.2"

(
    cd "$W/root/usr/src/innogpu-kernel-2.2"
    patch -p6 < "$ROOT/patches/001-kernel-6.12-compat.patch"
    if [[ "$APPLY_DP_FBCON_FALLBACK" == "1" ]]; then
        patch -p1 < "$ROOT/patches/002-dp-fbdev-fallback-mode.patch"
    fi
    if [[ "$APPLY_PANEL_BACKLIGHT_FALLBACK" == "1" ]]; then
        patch -p1 < "$ROOT/patches/003-panel-backlight-fallback.patch"
    fi
    if [[ "$APPLY_PANEL_PLATFORM_FALLBACK" == "1" ]]; then
        patch -p1 < "$ROOT/patches/004-panel-platform-fallback.patch"
    fi
    if [[ "$APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE" == "1" ]]; then
        patch -p1 < "$ROOT/patches/005-backlight-force-initial-enable.patch"
    fi
    if [[ "$APPLY_LOCAL_CONNECTOR_ACPI_MAP" == "1" ]]; then
        patch -p1 < "$ROOT/patches/006-local-connector-acpi-map.patch"
    fi
    find . -name '*.orig' -o -name '*.rej' | xargs -r rm -f
)

python3 - "$W/root/usr/src/innogpu-kernel-2.2/innogpu/innogpu.o_shipped" <<'PY'
from pathlib import Path
import sys

p = Path(sys.argv[1])
b = bytearray(p.read_bytes())
old = bytes.fromhex("e8 09 fd ff ff")
new = bytes.fromhex("90 90 90 90 90")
hits = []
start = 0
while True:
    i = b.find(old, start)
    if i < 0:
        break
    hits.append(i)
    start = i + 1

if len(hits) == 1:
    off = hits[0]
    b[off:off + 5] = new
    p.write_bytes(b)
    print(f"patched Deepin object at {off:#x}")
elif b.find(new) >= 0:
    print("Deepin object already patched")
else:
    raise SystemExit(f"unexpected old pattern hits: {hits}")
PY

install -d "$W/root/usr/share/innogpu-fh2m-trixie" "$W/root/usr/bin" "$W/root/usr/sbin"
install -m 0755 scripts/patch-skip-first-gpupll.sh "$W/root/usr/share/innogpu-fh2m-trixie/patch-skip-first-gpupll.sh"
install -m 0755 scripts/disable-incompatible-userspace.sh "$W/root/usr/share/innogpu-fh2m-trixie/disable-incompatible-userspace.sh"
install -m 0755 scripts/install-kylin-userspace.sh "$W/root/usr/share/innogpu-fh2m-trixie/install-kylin-userspace.sh"
install -m 0755 scripts/install-experimental-hwgl.sh "$W/root/usr/share/innogpu-fh2m-trixie/install-experimental-hwgl.sh"
install -m 0755 scripts/repair-dri-nodes.sh "$W/root/usr/share/innogpu-fh2m-trixie/repair-dri-nodes.sh"
install -m 0755 scripts/test-xorg-once.sh "$W/root/usr/share/innogpu-fh2m-trixie/test-xorg-once.sh"
install -m 0755 scripts/restore-dp1-mode-x11.sh "$W/root/usr/share/innogpu-fh2m-trixie/restore-dp1-mode-x11.sh"
install -m 0755 scripts/restore-tty1-login.sh "$W/root/usr/share/innogpu-fh2m-trixie/restore-tty1-login.sh"
install -m 0755 scripts/display-recover-and-diagnose.sh "$W/root/usr/share/innogpu-fh2m-trixie/display-recover-and-diagnose.sh"
install -m 0755 scripts/prepare-soft-xorg-dwm.sh "$W/root/usr/share/innogpu-fh2m-trixie/prepare-soft-xorg-dwm.sh"
install -m 0755 scripts/start-soft-xorg-dwm-from-ssh.sh "$W/root/usr/share/innogpu-fh2m-trixie/start-soft-xorg-dwm-from-ssh.sh"
install -m 0755 scripts/check-soft-xorg-dwm.sh "$W/root/usr/share/innogpu-fh2m-trixie/check-soft-xorg-dwm.sh"
install -m 0755 scripts/install-dri-node-repair-service.sh "$W/root/usr/share/innogpu-fh2m-trixie/install-dri-node-repair-service.sh"

ln -sfn ../share/innogpu-fh2m-trixie/patch-skip-first-gpupll.sh "$W/root/usr/bin/innogpu-skip-first-gpupll"
ln -sfn ../share/innogpu-fh2m-trixie/patch-skip-first-gpupll.sh "$W/root/usr/sbin/innogpu-skip-first-gpupll"
ln -sfn ../share/innogpu-fh2m-trixie/disable-incompatible-userspace.sh "$W/root/usr/bin/innogpu-disable-incompatible-userspace"
ln -sfn ../share/innogpu-fh2m-trixie/disable-incompatible-userspace.sh "$W/root/usr/sbin/innogpu-disable-incompatible-userspace"
ln -sfn ../share/innogpu-fh2m-trixie/install-kylin-userspace.sh "$W/root/usr/bin/innogpu-install-kylin-userspace"
ln -sfn ../share/innogpu-fh2m-trixie/install-kylin-userspace.sh "$W/root/usr/sbin/innogpu-install-kylin-userspace"
ln -sfn ../share/innogpu-fh2m-trixie/install-experimental-hwgl.sh "$W/root/usr/bin/innogpu-install-experimental-hwgl"
ln -sfn ../share/innogpu-fh2m-trixie/install-experimental-hwgl.sh "$W/root/usr/sbin/innogpu-install-experimental-hwgl"
ln -sfn ../share/innogpu-fh2m-trixie/repair-dri-nodes.sh "$W/root/usr/bin/innogpu-repair-dri-nodes"
ln -sfn ../share/innogpu-fh2m-trixie/repair-dri-nodes.sh "$W/root/usr/sbin/innogpu-repair-dri-nodes"
ln -sfn ../share/innogpu-fh2m-trixie/test-xorg-once.sh "$W/root/usr/bin/innogpu-test-xorg-once"
ln -sfn ../share/innogpu-fh2m-trixie/test-xorg-once.sh "$W/root/usr/sbin/innogpu-test-xorg-once"
ln -sfn ../share/innogpu-fh2m-trixie/restore-dp1-mode-x11.sh "$W/root/usr/bin/innogpu-restore-dp1-mode-x11"
ln -sfn ../share/innogpu-fh2m-trixie/restore-dp1-mode-x11.sh "$W/root/usr/sbin/innogpu-restore-dp1-mode-x11"
ln -sfn ../share/innogpu-fh2m-trixie/restore-tty1-login.sh "$W/root/usr/bin/innogpu-restore-tty1-login"
ln -sfn ../share/innogpu-fh2m-trixie/restore-tty1-login.sh "$W/root/usr/sbin/innogpu-restore-tty1-login"
ln -sfn ../share/innogpu-fh2m-trixie/display-recover-and-diagnose.sh "$W/root/usr/bin/innogpu-display-recover-and-diagnose"
ln -sfn ../share/innogpu-fh2m-trixie/display-recover-and-diagnose.sh "$W/root/usr/sbin/innogpu-display-recover-and-diagnose"
ln -sfn ../share/innogpu-fh2m-trixie/prepare-soft-xorg-dwm.sh "$W/root/usr/bin/innogpu-prepare-soft-xorg-dwm"
ln -sfn ../share/innogpu-fh2m-trixie/prepare-soft-xorg-dwm.sh "$W/root/usr/sbin/innogpu-prepare-soft-xorg-dwm"
ln -sfn ../share/innogpu-fh2m-trixie/start-soft-xorg-dwm-from-ssh.sh "$W/root/usr/bin/innogpu-start-soft-xorg-dwm"
ln -sfn ../share/innogpu-fh2m-trixie/start-soft-xorg-dwm-from-ssh.sh "$W/root/usr/sbin/innogpu-start-soft-xorg-dwm"
ln -sfn ../share/innogpu-fh2m-trixie/check-soft-xorg-dwm.sh "$W/root/usr/bin/innogpu-check-soft-xorg-dwm"
ln -sfn ../share/innogpu-fh2m-trixie/check-soft-xorg-dwm.sh "$W/root/usr/sbin/innogpu-check-soft-xorg-dwm"
ln -sfn ../share/innogpu-fh2m-trixie/install-dri-node-repair-service.sh "$W/root/usr/bin/innogpu-install-dri-node-repair-service"
ln -sfn ../share/innogpu-fh2m-trixie/install-dri-node-repair-service.sh "$W/root/usr/sbin/innogpu-install-dri-node-repair-service"

python3 - "$W/DEBIAN/control" "$PATCH_VERSION" <<'PY'
from pathlib import Path
import sys

p = Path(sys.argv[1])
patch_version = sys.argv[2]
s = p.read_text()
for old_version in range(2, 16):
    s = s.replace(f"Version: 3.3.3.42-patched-{old_version}", f"Version: 3.3.3.42-patched-{patch_version}")
s = s.replace(
    "Innosilicon Fantasy II-M GPU driver (patched for Debian Trixie)",
    "Innosilicon Fantasy II-M GPU driver (Deepin 202504 DKMS baseline, patched for Debian Trixie)",
)
p.write_text(s)
PY

python3 - "$W/DEBIAN/postinst" "$W/DEBIAN/prerm" "$PATCH_VERSION" "$APPLY_DP_FBCON_FALLBACK" "$APPLY_PANEL_BACKLIGHT_FALLBACK" "$APPLY_PANEL_PLATFORM_FALLBACK" "$APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE" <<'PY'
from pathlib import Path
import sys

postinst = Path(sys.argv[1])
prerm = Path(sys.argv[2])
patch_version = sys.argv[3]
apply_dp_fbcon_fallback = sys.argv[4] == "1"
apply_panel_backlight_fallback = sys.argv[5] == "1"
apply_panel_platform_fallback = sys.argv[6] == "1"
apply_backlight_force_initial_enable = sys.argv[7] == "1"

s = postinst.read_text()
s = s.replace("innogpu-fh2m-trixie postinst (patched-8)", f"innogpu-fh2m-trixie postinst (patched-{patch_version})")
s = s.replace("innogpu-fh2m-trixie postinst (patched-9)", f"innogpu-fh2m-trixie postinst (patched-{patch_version})")
s = s.replace("innogpu-fh2m-trixie postinst (patched-10)", f"innogpu-fh2m-trixie postinst (patched-{patch_version})")
s = s.replace(
    "patched-6 applies the G0M first-GPU-PLL workaround to the DKMS source object.",
    f"patched-{patch_version} uses the Deepin 202504 DKMS baseline plus Debian 6.12 fixes, G0M PLL workaround, and DRM node repair.",
)
s = s.replace(
    "patched-9 includes the G0M first-GPU-PLL workaround, safe userspace cleanup, and DRM node repair.",
    f"patched-{patch_version} uses the Deepin 202504 DKMS baseline plus Debian 6.12 fixes, G0M PLL workaround, and DRM node repair.",
)
s = s.replace(
    "patched-10 uses the Deepin 202504 DKMS baseline plus Debian 6.12 fixes, G0M PLL workaround, and DRM node repair.",
    f"patched-{patch_version} uses the Deepin 202504 DKMS baseline plus Debian 6.12 fixes, G0M PLL workaround, and DRM node repair.",
)
if apply_dp_fbcon_fallback and "DP fbcon fallback mode" not in s:
    s = s.replace(
        "and DRM node repair.",
        "DRM node repair, and DP fbcon fallback mode.",
        1,
    )
if apply_panel_backlight_fallback and "panel backlight fallback" not in s:
    s = s.replace(
        "DP fbcon fallback mode.",
        "DP fbcon fallback mode, and panel backlight fallback.",
        1,
    )
if apply_panel_platform_fallback and "panel platform fallback" not in s:
    s = s.replace(
        "panel backlight fallback.",
        "panel backlight fallback, and panel platform fallback.",
        1,
    )
if apply_backlight_force_initial_enable and "initial backlight enable" not in s:
    s = s.replace(
        "panel platform fallback.",
        "panel platform fallback, and initial backlight enable.",
        1,
    )
postinst.write_text(s)

s = prerm.read_text()
needle = "rm -f /usr/bin/innogpu-disable-incompatible-userspace /usr/sbin/innogpu-disable-incompatible-userspace\n"
extra = (
    "rm -f /usr/bin/innogpu-install-kylin-userspace /usr/sbin/innogpu-install-kylin-userspace\n"
    "rm -f /usr/bin/innogpu-install-experimental-hwgl /usr/sbin/innogpu-install-experimental-hwgl\n"
    "rm -f /usr/bin/innogpu-repair-dri-nodes /usr/sbin/innogpu-repair-dri-nodes\n"
    "rm -f /usr/bin/innogpu-test-xorg-once /usr/sbin/innogpu-test-xorg-once\n"
    "rm -f /usr/bin/innogpu-restore-dp1-mode-x11 /usr/sbin/innogpu-restore-dp1-mode-x11\n"
    "rm -f /usr/bin/innogpu-restore-tty1-login /usr/sbin/innogpu-restore-tty1-login\n"
    "rm -f /usr/bin/innogpu-display-recover-and-diagnose /usr/sbin/innogpu-display-recover-and-diagnose\n"
    "rm -f /usr/bin/innogpu-prepare-soft-xorg-dwm /usr/sbin/innogpu-prepare-soft-xorg-dwm\n"
    "rm -f /usr/bin/innogpu-start-soft-xorg-dwm /usr/sbin/innogpu-start-soft-xorg-dwm\n"
    "rm -f /usr/bin/innogpu-check-soft-xorg-dwm /usr/sbin/innogpu-check-soft-xorg-dwm\n"
    "rm -f /usr/bin/innogpu-install-dri-node-repair-service /usr/sbin/innogpu-install-dri-node-repair-service\n"
)
if extra not in s:
    s = s.replace(needle, needle + extra)
prerm.write_text(s)
PY

rm -rf "$W/root/DEBIAN"
cp -a "$W/DEBIAN" "$W/root/DEBIAN"
dpkg-deb --root-owner-group --build "$W/root" "$OUT_DEB"
echo "Built: $OUT_DEB"
