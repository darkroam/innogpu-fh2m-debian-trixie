#!/bin/bash
# New-architecture builder: assemble drivers/ + verified vendor payload into an
# isolated staging tree, compile the DKMS module offline, and build a coherent
# package (4.0.0-iN). The old builder remains the p27 oracle; no patch apply.
# No install, no hot-swap, no reboot.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

VERSION="${VERSION:-4.0.0-i1}"
# 可复现构建: 固定审核 epoch 必须显式提供, 禁止回退到当前时间(同源码不同时间产出不同 deb)。
# 审核 epoch 记录于 docs/planning/source-tree-migration.md(4.0.0-i1 = 1787342400)。
SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-}"
[[ "$SOURCE_DATE_EPOCH" =~ ^[0-9]+$ ]] || {
    echo "builder_repro=FAIL SOURCE_DATE_EPOCH must be the fixed audit epoch (see docs)"; exit 1; }
export SOURCE_DATE_EPOCH
KERNEL="${KERNELDIR_VER:-$(uname -r)}"
KERNELDIR="${KERNELDIR:-/lib/modules/$KERNEL/build}"
OUT_DEB="${OUT_DEB:-$ROOT/build/innogpu-fh2m-trixie_$VERSION.deb}"
[[ -d "$KERNELDIR" ]] || { echo "staging_kernel_headers=FAIL $KERNELDIR"; exit 1; }

# 0) 版本排序必须高于 p27
dpkg --compare-versions "$VERSION" gt 3.3.3.42-patched-27 || {
    echo "builder_version_ordering=FAIL $VERSION is not > patched-27"; exit 1; }
echo "builder_version_ordering=PASS $VERSION > patched-27"

# 1) manifest + vendor 就位
[[ -f binary-manifest.json ]] || { echo "staging_manifest=FAIL"; exit 1; }
bash scripts/extract-vendor-binaries.sh --check-only | grep -q 'vendor_extraction_overall=PASS' || {
    echo "staging_vendor_check=FAIL"; exit 1; }

# 2) staging 源码树
mkdir -p "$ROOT/build"
STAGE="$(mktemp -d "$ROOT/build/stage.XXXXXX")"
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/source" "$STAGE/package"

cp -r drivers/. "$STAGE/source/"
for obj in vendor/kernel/*/*.o_shipped; do
    mod=$(basename "$(dirname "$obj")")
    cp "$obj" "$STAGE/source/$mod/"
done
python3 tools/patch-gpupll-object.py "$STAGE/source/innogpu/innogpu.o_shipped" >/dev/null
echo "staging_objects=$(ls "$STAGE/source"/*/*.o_shipped | wc -l)"
echo "staging_deterministic_transform=PASS"

# 3) 离线编译
cd "$STAGE/source"
jobs=$(nproc); (( jobs > 16 )) && jobs=16
make -j"$jobs" KERNELDIR="$KERNELDIR" > "$ROOT/build/staging-build.log" 2>&1 || {
    echo "staging_dkms_build=FAIL"; tail -15 "$ROOT/build/staging-build.log"; exit 1; }
echo "staging_dkms_build=PASS"
NAME=$(/usr/sbin/modinfo -F name innogpu.ko)
VERMAGIC=$(/usr/sbin/modinfo -F vermagic innogpu.ko)
[[ "$NAME" == "innogpu" && "$VERMAGIC" == "$KERNEL "* ]] || {
    echo "staging_module_vermagic=FAIL"; exit 1; }
echo "staging_module_vermagic=PASS ($VERMAGIC)"
cd "$ROOT"

# 4) 包装配
P=$STAGE/package/root
install -d "$P/usr/src/innogpu-kernel-2.2" "$P/etc/ld.so.conf.d" \
    "$P/usr/share/innogpu-fh2m-trixie" "$P/usr/bin" "$P/usr/sbin"
cp -r drivers/. "$P/usr/src/innogpu-kernel-2.2/"
rm -f "$P/usr/src/innogpu-kernel-2.2/README.md"

# vendor payload -> install 路径
while IFS= read -r vp; do
    case "$vp" in
        kernel/*)                          dst="$P/usr/src/innogpu-kernel-2.2/${vp#kernel/}" ;;
        userspace/x86_64-linux-gnu/*)      dst="$P/usr/lib/x86_64-linux-gnu/${vp#userspace/x86_64-linux-gnu/}" ;;
        userspace/i386-linux-gnu/*)        dst="$P/usr/lib/i386-linux-gnu/${vp#userspace/i386-linux-gnu/}" ;;
        userspace/xorg/*)                  dst="$P/usr/lib/xorg/${vp#userspace/xorg/}" ;;
        userspace/usr/lib/kgc/*)           dst="$P/usr/lib/kgc/${vp#userspace/usr/lib/kgc/}" ;;
        userspace/usr/sbin/*)              dst="$P/usr/sbin/${vp#userspace/usr/sbin/}" ;;
        userspace/share/*)                 dst="$P/usr/share/${vp#userspace/share/}" ;;
        userspace/etc/*)                   dst="$P/etc/${vp#userspace/etc/}" ;;
        userspace/lib/systemd/system/*)    dst="$P/lib/systemd/system/${vp#userspace/lib/systemd/system/}" ;;
        opt/innogpu/*)                     dst="$P/opt/${vp#opt/}" ;;
        firmware/*)                        dst="$P/lib/firmware/innogpu/${vp#firmware/}" ;;
        *) echo "builder_payload_map=FAIL $vp"; exit 1 ;;
    esac
    mkdir -p "$(dirname "$dst")"
    if [[ -L "vendor/$vp" ]]; then
        ln -sfn "$(readlink "vendor/$vp")" "$dst"
    else
        cp "vendor/$vp" "$dst"
    fi
done < <(python3 -c "import json;m=json.load(open('binary-manifest.json'));[print(e['vendor_path']) for e in m['entries']]")
python3 tools/patch-gpupll-object.py "$P/usr/src/innogpu-kernel-2.2/innogpu/innogpu.o_shipped" >/dev/null
# 包边界守卫: .o.cmd 是内核构建产物, 不入发布包(监督评审边界裁定)。
if find "$P" -name '*.o.cmd' | grep -q .; then
    echo "builder_package_boundary=FAIL .o.cmd build artifacts must not enter the package"; exit 1
fi
echo "builder_payload_assemble=PASS"
echo "builder_package_boundary=PASS (no .o.cmd build artifacts)"

printf '%s
' '/usr/lib/x86_64-linux-gnu/innogpu-fh2m' > "$P/etc/ld.so.conf.d/0-innogpu-hwgl.conf"

helpers=(disable-incompatible-userspace.sh repair-dri-nodes.sh test-xorg-once.sh
    restore-dp1-mode-x11.sh xdisplay-session.sh install-xdisplay-user.sh
    restore-tty1-login.sh display-recover-and-diagnose.sh prepare-soft-xorg-dwm.sh
    start-soft-xorg-dwm-from-ssh.sh check-soft-xorg-dwm.sh install-dri-node-repair-service.sh)
for h in "${helpers[@]}"; do
    install -m 0755 "scripts/$h" "$P/usr/share/innogpu-fh2m-trixie/$h"
done
declare -A cmds=( [innogpu-disable-incompatible-userspace]=disable-incompatible-userspace.sh
    [innogpu-repair-dri-nodes]=repair-dri-nodes.sh [innogpu-test-xorg-once]=test-xorg-once.sh
    [innogpu-restore-dp1-mode-x11]=restore-dp1-mode-x11.sh [innogpu-restore-tty1-login]=restore-tty1-login.sh
    [innogpu-display-recover-and-diagnose]=display-recover-and-diagnose.sh
    [innogpu-prepare-soft-xorg-dwm]=prepare-soft-xorg-dwm.sh
    [innogpu-start-soft-xorg-dwm]=start-soft-xorg-dwm-from-ssh.sh
    [innogpu-check-soft-xorg-dwm]=check-soft-xorg-dwm.sh
    [innogpu-install-dri-node-repair-service]=install-dri-node-repair-service.sh)
for c in "${!cmds[@]}"; do
    ln -sfn "../share/innogpu-fh2m-trixie/${cmds[$c]}" "$P/usr/bin/$c"
    ln -sfn "../share/innogpu-fh2m-trixie/${cmds[$c]}" "$P/usr/sbin/$c"
done

install -d "$P/DEBIAN"
installed_size=$(du -sk --exclude=DEBIAN "$P" | awk '{print $1}')
cat > "$P/DEBIAN/control" <<EOF
Package: innogpu-fh2m-trixie
Version: $VERSION
Section: graphics
Priority: optional
Architecture: amd64
Installed-Size: ${installed_size}
Depends: dkms, build-essential, libdrm2, libepoxy0, libpixman-1-0, libwayland-server0, libxcb-randr0
Recommends: linux-headers-amd64, libegl1, libgles2, libgl1, libglx0, libgles1, libglvnd0
Conflicts: innogpu-fh2m, innogpu-fh2m-kernel-dkms, innogpu-kernel-dkms
Replaces: innogpu-fh2m, innogpu-fh2m-kernel-dkms, innogpu-kernel-dkms
Maintainer: Tim Hant <tthantclaw@outlook.com>
Homepage: https://github.com/timhant/innogpu-fh2m-debian-trixie
Description: Innosilicon Fantasy II-M driver (migrated source tree, version $VERSION)
 New-architecture build: drivers/ source tree + manifest-managed black-box
 payload, equivalent to patched-27 without patch application.
EOF

cat > "$P/DEBIAN/postinst" <<EOF
#!/bin/bash
set -e

PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export PATH

case "\${1:-}" in
    configure)
        ;;
    abort-upgrade|abort-remove|abort-deconfigure)
        exit 0
        ;;
    *)
        echo "postinst called with unknown argument \${1:-}" >&2
        exit 1
        ;;
esac

kernel_ver=\$(uname -r)
echo "Configuring innogpu-fh2m-trixie ${VERSION} from Deepin 202504..."

install -d /etc/modprobe.d
printf '%s\n' 'options innogpu firmware_en=1' > /etc/modprobe.d/innogpu.conf

dkms add -m innogpu-kernel -v 2.2 2>/dev/null || true
dkms build -m innogpu-kernel -v 2.2 -k "\$kernel_ver" --force
dkms install -m innogpu-kernel -v 2.2 -k "\$kernel_ver" --force

# The package ships a complete, matching Deepin DRI/GBM/GLAPI/DDX set.
# Never move or restore an individual vendor library here.
if [ ! -f /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so ]; then
    echo "ERROR: coherent Deepin innogpu_dri.so is missing" >&2
    exit 1
fi

ldconfig
depmod -a "\$kernel_ver"
if command -v update-initramfs >/dev/null 2>&1; then
    update-initramfs -u -k "\$kernel_ver"
fi

echo "Installed coherent Deepin 202504 userspace; module autoload policy was not changed."
exit 0
EOF

cat > "$P/DEBIAN/prerm" <<'PEOF'
#!/bin/bash
set -e

case "${1:-}" in
    remove|upgrade|deconfigure)
        dkms remove -m innogpu-kernel -v 2.2 --all 2>/dev/null || true
        ;;
    failed-upgrade)
        ;;
    *)
        echo "prerm called with unknown argument ${1:-}" >&2
        exit 1
        ;;
esac

exit 0
PEOF

cat > "$P/DEBIAN/postrm" <<'PEOF'
#!/bin/bash
set -e

case "${1:-}" in
    remove|purge|upgrade|failed-upgrade|abort-install|abort-upgrade|disappear)
        ldconfig
        ;;
    *)
        echo "postrm called with unknown argument ${1:-}" >&2
        ;;
esac

exit 0
PEOF

chmod 0755 "$P/DEBIAN/postinst" "$P/DEBIAN/prerm" "$P/DEBIAN/postrm"
find "$P" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +

mkdir -p "$(dirname "$OUT_DEB")"
dpkg-deb --root-owner-group --build "$P" "$OUT_DEB"
echo "builder_package_build=PASS $OUT_DEB"

# 5) 边界检查
scripts/check-release-package.sh "$OUT_DEB" || { echo "builder_package_boundary=FAIL"; exit 1; }
echo "builder_package_boundary=PASS"
echo "builder_overall=PASS"
echo "builder_out_deb=$OUT_DEB"