#!/bin/bash
# New-architecture builder: assemble drivers/ + verified vendor payload into an
# isolated staging tree, compile the DKMS module offline, and build a coherent
# package (4.0.x-iN). The old builder remains the p27 oracle.
# No install, no hot-swap, no reboot.

set -euo pipefail
umask 022  # 设计 §四：物化/组装目录 mode 确定性（不受调用者 umask 影响）

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

VERSION="${VERSION:-4.0.2-i3}"
case "$VERSION" in
    4.0.1-i3|4.0.1-i4) EXPECTED_SOURCE_DATE_EPOCH=1788451200 ;;
    4.0.2-i1) EXPECTED_SOURCE_DATE_EPOCH=1788624000 ;;
    4.0.2-i2) EXPECTED_SOURCE_DATE_EPOCH=1788710400 ;;
    4.0.2-i3) EXPECTED_SOURCE_DATE_EPOCH=1788796800 ;;
    5.0.0-i1) EXPECTED_SOURCE_DATE_EPOCH=1788796800 ;;  # dsh 批准：沿用 4.0.2-i3 审核 epoch
    5.0.0-i2) EXPECTED_SOURCE_DATE_EPOCH=1788796800 ;;  # dsh 批准：沿用审核 epoch
    *)
        echo "builder_version_review=FAIL unreviewed package version: $VERSION" >&2
        exit 1
        ;;
esac
# 血统：5.0.0-iN = fantgpu 血统（O_stage 物化源树，030 链 13 项，patch-000
# no-transform；i2 起含 F 血统 userspace/固件载荷）；其余 = innogpu/Deepin 血统。
FANT_LINEAGE=0
[[ "$VERSION" == 5.0.0-i* ]] && FANT_LINEAGE=1
# 保护区边界：staging 根与构建日志可注入（5.0.0-i1 实跑注入 /tmp，build/ 保护区零写入；
# 4.0.x-iN 默认保持历史行为）
STAGE_ROOT="${STAGE_ROOT:-$ROOT/build}"
BUILD_LOG="${BUILD_LOG:-$STAGE_ROOT/staging-build.log}"
# 可复现构建: 固定审核 epoch 必须显式提供, 禁止回退到当前时间(同源码不同时间产出不同 deb)。
# 审核 epoch 记录于对应的 docs/patches/ 候选说明。
SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-}"
[[ "$SOURCE_DATE_EPOCH" =~ ^[0-9]+$ ]] || {
    echo "builder_repro=FAIL SOURCE_DATE_EPOCH must be the fixed audit epoch (see docs)"; exit 1; }
[[ "$SOURCE_DATE_EPOCH" == "$EXPECTED_SOURCE_DATE_EPOCH" ]] || {
    echo "builder_repro=FAIL $VERSION requires SOURCE_DATE_EPOCH=$EXPECTED_SOURCE_DATE_EPOCH"; exit 1; }
export SOURCE_DATE_EPOCH
KERNEL="${KERNELDIR_VER:-$(uname -r)}"
KERNELDIR="${KERNELDIR:-/lib/modules/$KERNEL/build}"
# 默认输出名按血统（codex P1：5.0.0-i1 不得沿用 innogpu 名）
if [[ "$FANT_LINEAGE" == 1 ]]; then
    OUT_DEB="${OUT_DEB:-$STAGE_ROOT/fantgpu-fh2m-trixie_$VERSION.deb}"
else
    OUT_DEB="${OUT_DEB:-$STAGE_ROOT/innogpu-fh2m-trixie_$VERSION.deb}"
fi
[[ -d "$KERNELDIR" ]] || { echo "staging_kernel_headers=FAIL $KERNELDIR"; exit 1; }

# 0) 版本排序必须高于 p27
dpkg --compare-versions "$VERSION" gt 3.3.3.42-patched-27 || {
    echo "builder_version_ordering=FAIL $VERSION is not > patched-27"; exit 1; }
echo "builder_version_ordering=PASS $VERSION > patched-27"

apply_reviewed_source_fixes() {
    local source_tree=$1
    local scope=$2
    patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s -d "$source_tree" -p1 \
        < "$ROOT/patches/024-suspend-resume.patch"
    if [[ "$VERSION" == "4.0.1-i4" ]]; then
        patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s -d "$source_tree" -p1 \
            < "$ROOT/patches/025-suspend-resume-display.patch"
    fi
    if [[ "$VERSION" == 4.0.2-i1 || "$VERSION" == 4.0.2-i2 || "$VERSION" == 4.0.2-i3 ]]; then
        patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s -d "$source_tree" -p1 \
            < "$ROOT/patches/026-suspend-resume-dvfs-lifecycle.patch"
    fi
    if [[ "$VERSION" == "4.0.2-i2" ]]; then
        patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s -d "$source_tree" -p1 \
            < "$ROOT/patches/028-suspend-resume-hal-temp-monitor-delay.patch"
    fi
    if [[ "$VERSION" == "4.0.2-i3" ]]; then
        patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s -d "$source_tree" -p1 \
            < "$ROOT/patches/028-suspend-resume-hal-temp-monitor-delay.patch"
        patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s -d "$source_tree" -p1 \
            < "$ROOT/patches/029-suspend-resume-ddcci-panel.patch"
    fi
    reject_patch_artifacts "$source_tree" "$scope"
}

reject_patch_artifacts() {
    local tree=$1
    local scope=$2
    local artifact

    artifact=$(find "$tree" -type f \( -name '*.orig' -o -name '*.rej' \) -print -quit)
    [[ -z "$artifact" ]] || {
        echo "builder_patch_artifacts=FAIL scope=$scope path=${artifact#"$tree"/}" >&2
        exit 1
    }
}

APPLIED_SOURCE_FIXES="patch-024"
[[ "$VERSION" == "4.0.1-i4" ]] &&
    APPLIED_SOURCE_FIXES+=" patch-025-suspend-resume-display"
[[ "$VERSION" == 4.0.2-i1 || "$VERSION" == 4.0.2-i2 ]] &&
    APPLIED_SOURCE_FIXES+=" patch-026-suspend-resume-dvfs-lifecycle"
[[ "$VERSION" == "4.0.2-i2" ]] &&
    APPLIED_SOURCE_FIXES+=" patch-028-suspend-resume-hal-temp-monitor-delay"
[[ "$VERSION" == "4.0.2-i3" ]] &&
    APPLIED_SOURCE_FIXES+=" patch-026-suspend-resume-dvfs-lifecycle patch-028-suspend-resume-hal-temp-monitor-delay patch-029-suspend-resume-ddcci-panel"

# 1) manifest + vendor 就位（按血统分支：F 分支只要求 F 输入）
if [[ "$FANT_LINEAGE" == 1 ]]; then
    [[ -f binary-manifest-fantgpu.json ]] || { echo "staging_f_manifest=FAIL"; exit 1; }
    python3 tools/validate-binary-manifest-fantgpu.py || {
        echo "staging_fpayload_input=FAIL"; exit 1; }
    echo "staging_fpayload_input=PASS"
else
    [[ -f binary-manifest.json ]] || { echo "staging_manifest=FAIL"; exit 1; }
    bash scripts/extract-vendor-binaries.sh --check-only | grep -q 'vendor_extraction_overall=PASS' || {
        echo "staging_vendor_check=FAIL"; exit 1; }
fi

# 2) staging 源码树
mkdir -p "$STAGE_ROOT"
STAGE="$(mktemp -d "$STAGE_ROOT/stage.XXXXXX")"
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/source" "$STAGE/package"

if [[ "$FANT_LINEAGE" == 1 ]]; then
    # fantgpu 血统：DKMS 源 = O_stage 物化快照解包（030 链 13 项已含、patch-000
    # no-transform、o_shipped 对象原样）；校验快照 SHA（meta 锁定值 + sidecar
    # 内容）与解包树 hash（O-4 契约）——不可变 provenance 三方一致（codex P1）
    OSTAGE_DIR="$ROOT/docs/planning/evidence/o-stage"
    OSTAGE_SNAP="$OSTAGE_DIR/o-stage-snapshot.tar.zst"
    [[ -f "$OSTAGE_SNAP" ]] || { echo "staging_ostage_snapshot=FAIL"; exit 1; }
    OSTAGE_TREE_HASH="937e37107f692712e6fba9b5eb93a48dd6bb034f138e5e51a2bb9c2beaa0d652"
    snap_sha="$(sha256sum "$OSTAGE_SNAP" | awk '{print $1}')"
    side_sha="$(awk '{print $1}' "$OSTAGE_DIR/o-stage-snapshot.tar.zst.sha256")"
    # meta 锁定四值（不可变 provenance；快照与 sidecar 同时被替换时以 meta 兜底）
    meta_sha="$(python3 - "$OSTAGE_DIR/5.0.0-i1.meta.json" <<'PY'
import json, sys
m = json.load(open(sys.argv[1], encoding="utf-8"))
print(m["snapshot_artifacts"]["o-stage-snapshot.tar.zst"]["sha256"])
PY
)"
    [[ "$snap_sha" == "$side_sha" && "$snap_sha" == "$meta_sha" ]] || {
        echo "staging_ostage_sha=FAIL actual=$snap_sha sidecar=$side_sha meta=$meta_sha"; exit 1; }
    side_content="$(cat "$OSTAGE_DIR/o-stage-snapshot.tar.zst.sha256")"
    [[ "$side_content" == "$snap_sha  o-stage-snapshot.tar.zst" ]] || {
        echo "staging_ostage_sidecar=FAIL"; exit 1; }
    tar --use-compress-program=zstd -xf "$OSTAGE_SNAP" -C "$STAGE/source"
    # 快照顶层必须仅含 o-stage/ 单一目录（防额外路径混入）
    [[ -d "$STAGE/source/o-stage" && "$(ls -A "$STAGE/source" | wc -l)" == 1 ]] || {
        echo "staging_ostage_prefix=FAIL"; exit 1; }
    tree_hash="$(python3 - "$STAGE/source/o-stage" <<'PY'
import importlib.util, hashlib, sys
spec = importlib.util.spec_from_file_location("o4", "tools/o4-f0-lock-gen.py")
o4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(o4)
rows = list(o4.walk_rows(sys.argv[1]))
print(hashlib.sha256(o4.manifest_text(rows).encode()).hexdigest())
PY
)"
    [[ "$tree_hash" == "$OSTAGE_TREE_HASH" ]] || {
        echo "staging_ostage_tree_hash=FAIL actual=$tree_hash"; exit 1; }
    mv "$STAGE/source/o-stage" "$STAGE/source.tree"
    rm -rf "$STAGE/source"
    mv "$STAGE/source.tree" "$STAGE/source"
    APPLIED_SOURCE_FIXES="o-stage-materialized-030-chain-13 (patch-000 no-transform)"
    echo "staging_deterministic_transform=PASS (no-transform: fantgpu objects used as-is)"
else
    cp -r drivers/. "$STAGE/source/"
    apply_reviewed_source_fixes "$STAGE/source" compile-staging
    for obj in vendor/kernel/*/*.o_shipped; do
        mod=$(basename "$(dirname "$obj")")
        cp "$obj" "$STAGE/source/$mod/"
    done
    python3 tools/patch-gpupll-object.py "$STAGE/source/innogpu/innogpu.o_shipped" >/dev/null
    echo "staging_deterministic_transform=PASS"
fi
echo "staging_objects=$(ls "$STAGE/source"/*/*.o_shipped | wc -l)"
echo "staging_source_fixes=PASS $APPLIED_SOURCE_FIXES"

# 3) 离线编译
cd "$STAGE/source"
jobs=$(nproc); (( jobs > 16 )) && jobs=16
make -j"$jobs" KERNELDIR="$KERNELDIR" > "$BUILD_LOG" 2>&1 || {
    echo "staging_dkms_build=FAIL"; tail -15 "$BUILD_LOG"; exit 1; }
echo "staging_dkms_build=PASS"
if [[ "$FANT_LINEAGE" == 1 ]]; then
    NAME=$(/usr/sbin/modinfo -F name fantgpu.ko)
    VERMAGIC=$(/usr/sbin/modinfo -F vermagic fantgpu.ko)
    [[ "$NAME" == "fantgpu" && "$VERMAGIC" == "$KERNEL "* ]] || {
        echo "staging_module_vermagic=FAIL"; exit 1; }
else
    NAME=$(/usr/sbin/modinfo -F name innogpu.ko)
    VERMAGIC=$(/usr/sbin/modinfo -F vermagic innogpu.ko)
    [[ "$NAME" == "innogpu" && "$VERMAGIC" == "$KERNEL "* ]] || {
        echo "staging_module_vermagic=FAIL"; exit 1; }
fi
echo "staging_module_vermagic=PASS ($VERMAGIC)"
cd "$ROOT"

# 4) 包装配
P=$STAGE/package/root
if [[ "$FANT_LINEAGE" == 1 ]]; then
    DKMS_SRC_NAME="fantgpu-fh2m-kernel-2.2"
    SHARE_DIR="fantgpu-fh2m-trixie"
    CMD_PREFIX="fantgpu-"
    install -d "$P/usr/src" "$P/etc/ld.so.conf.d" \
        "$P/usr/share/$SHARE_DIR" "$P/usr/bin" "$P/usr/sbin"
    # DKMS 源 = 快照重新解包（干净树）——不得用被 make 污染的 $STAGE/source
    # （.o/.o.cmd 等编译产物会进入包内并破坏双构建字节一致）
    tar --use-compress-program=zstd -xf "$OSTAGE_SNAP" -C "$P/usr/src"
    # 快照顶层必须仅含 o-stage/ 单一目录（防额外路径混入，codex P1）
    [[ -d "$P/usr/src/o-stage" && "$(ls -A "$P/usr/src" | wc -l)" == 1 ]] || {
        echo "builder_ostage_prefix=FAIL"; exit 1; }
    mv "$P/usr/src/o-stage" "$P/usr/src/$DKMS_SRC_NAME"
    # no-transform：树自带 fant*.o_shipped 原样进入 DKMS 源（不跑 patch-gpupll）
    # 无 apply_reviewed_source_fixes（030 链已在 materialize 阶段应用）
else
    SHARE_DIR="innogpu-fh2m-trixie"
    CMD_PREFIX="innogpu-"
    install -d "$P/usr/src/innogpu-kernel-2.2" "$P/etc/ld.so.conf.d" \
        "$P/usr/share/$SHARE_DIR" "$P/usr/bin" "$P/usr/sbin"
    cp -r drivers/. "$P/usr/src/innogpu-kernel-2.2/"
    apply_reviewed_source_fixes "$P/usr/src/innogpu-kernel-2.2" packaged-dkms
    rm -f "$P/usr/src/innogpu-kernel-2.2/README.md"
    python3 tools/patch-gpupll-object.py "$P/usr/src/innogpu-kernel-2.2/innogpu/innogpu.o_shipped" >/dev/null
fi

# vendor payload -> install 路径（F 分支走 manifest 物化 M1-M6；O 分支保持 case 映射）
if [[ "$FANT_LINEAGE" == 1 ]]; then
    F_XORG_ABI="${F_XORG_ABI:-1.21}" F_UCM_LAYOUT="${F_UCM_LAYOUT:-ucm2}" \
    F_WAYLAND_COMPAT="${F_WAYLAND_COMPAT:-off}" \
        python3 tools/materialize-fantgpu-payload.py \
        --manifest binary-manifest-fantgpu.json --vendor vendor/fantgpu \
        --pkg-root "$P" --trace "$STAGE/materialize-trace.tsv" \
        --ostage-manifest docs/planning/evidence/o-stage/o-stage.manifest.tsv \
        || { echo "builder_fpayload_materialize=FAIL"; exit 1; }
    # trace 逐行复核（codex 初审 P1-5：destination/kind/SHA/mode/target +
    # locked 来源零出现 + 行数 == f + ostage）
    python3 tools/materialize-fantgpu-payload.py --verify-trace \
        "$STAGE/materialize-trace.tsv" \
        --manifest binary-manifest-fantgpu.json --vendor vendor/fantgpu \
        --pkg-root "$P" \
        --ostage-manifest docs/planning/evidence/o-stage/o-stage.manifest.tsv \
        || { echo "builder_materialize_trace=FAIL"; exit 1; }
    trace_rows="$(wc -l < "$STAGE/materialize-trace.tsv")"
    echo "builder_materialize_trace=PASS trace_entries=$trace_rows"
else
    while IFS= read -r vp; do
        case "$vp" in
            kernel/*)
                dst="$P/usr/src/innogpu-kernel-2.2/${vp#kernel/}" ;;
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
fi
if [[ "$FANT_LINEAGE" != 1 ]]; then
    python3 tools/patch-gpupll-object.py "$P/usr/src/innogpu-kernel-2.2/innogpu/innogpu.o_shipped" >/dev/null
fi
# 包边界守卫: 构建产物和 patch 备份/拒绝文件不得进入发布包。
# 注意：find|grep -q 在 pipefail 下会因 grep 提前退出触发 SIGPIPE（find rc=141）
# 使条件反转为假——必须用 -print -quit 让 find 在首个命中即退出。
# *.o/*.ko 精确拒绝（codex P2）；*.o 不匹配合法 o_shipped 后缀。
if find "$P" \( -name '*.o' -o -name '*.ko' \) -print -quit | grep -q .; then
    echo "builder_package_boundary=FAIL .o/.ko build artifacts must not enter the package"; exit 1
fi
if find "$P" -name '*.o.cmd' -print -quit | grep -q .; then
    echo "builder_package_boundary=FAIL .o.cmd build artifacts must not enter the package"; exit 1
fi
reject_patch_artifacts "$P" package-payload
echo "builder_payload_assemble=PASS"
echo "builder_package_boundary=PASS (no .o.cmd/.orig/.rej artifacts)"

# 血统参数：share 目录与命令前缀（C1-① 按改后实际内容断言；包装配开头已按血统
# 初始化，此处仅对 4.0.x-iN 兜底——避免重复定义冲突）
: "${SHARE_DIR:=innogpu-fh2m-trixie}"
: "${CMD_PREFIX:=innogpu-}"
if [[ "$FANT_LINEAGE" == 1 ]]; then
    printf '%s\n' '/usr/lib/x86_64-linux-gnu/fantgpu-fh2m' > "$P/etc/ld.so.conf.d/0-fantgpu-hwgl.conf"
else
    printf '%s\n' '/usr/lib/x86_64-linux-gnu/innogpu-fh2m' > "$P/etc/ld.so.conf.d/0-innogpu-hwgl.conf"
fi

helpers=(disable-incompatible-userspace.sh repair-dri-nodes.sh test-xorg-once.sh
    restore-dp1-mode-x11.sh xdisplay-session.sh install-xdisplay-user.sh
    restore-tty1-login.sh display-recover-and-diagnose.sh prepare-soft-xorg-dwm.sh
    start-soft-xorg-dwm-from-ssh.sh check-soft-xorg-dwm.sh install-dri-node-repair-service.sh)
for h in "${helpers[@]}"; do
    if [[ "$FANT_LINEAGE" == 1 ]]; then
        # F 血统：变换 helper 内部调用链 token（命令名/share 路径/模块条件）；
        # O 血统保持脚本原始字节（check-release-package cmp 契约）
        bash tools/transform-fantgpu-helper.sh < "scripts/$h" \
            > "$P/usr/share/$SHARE_DIR/$h"
        chmod 0755 "$P/usr/share/$SHARE_DIR/$h"
    else
        install -m 0755 "scripts/$h" "$P/usr/share/$SHARE_DIR/$h"
    fi
done
declare -A cmds=( [${CMD_PREFIX}disable-incompatible-userspace]=disable-incompatible-userspace.sh
    [${CMD_PREFIX}repair-dri-nodes]=repair-dri-nodes.sh [${CMD_PREFIX}test-xorg-once]=test-xorg-once.sh
    [${CMD_PREFIX}restore-dp1-mode-x11]=restore-dp1-mode-x11.sh [${CMD_PREFIX}restore-tty1-login]=restore-tty1-login.sh
    [${CMD_PREFIX}display-recover-and-diagnose]=display-recover-and-diagnose.sh
    [${CMD_PREFIX}prepare-soft-xorg-dwm]=prepare-soft-xorg-dwm.sh
    [${CMD_PREFIX}start-soft-xorg-dwm]=start-soft-xorg-dwm-from-ssh.sh
    [${CMD_PREFIX}check-soft-xorg-dwm]=check-soft-xorg-dwm.sh
    [${CMD_PREFIX}install-dri-node-repair-service]=install-dri-node-repair-service.sh)
for c in "${!cmds[@]}"; do
    ln -sfn "../share/$SHARE_DIR/${cmds[$c]}" "$P/usr/bin/$c"
    ln -sfn "../share/$SHARE_DIR/${cmds[$c]}" "$P/usr/sbin/$c"
done

install -d "$P/DEBIAN"
installed_size=$(du -sk --exclude=DEBIAN "$P" | awk '{print $1}')
if [[ "$FANT_LINEAGE" == 1 ]]; then
    PKG_NAME="fantgpu-fh2m-trixie"
    PKG_DESC="Innosilicon Fantasy II-M driver (fantgpu lineage $VERSION, O_stage materialized tree)"
    DKMS_MOD="fantgpu-fh2m-kernel"
    DKMS_VER="2.2"
    KERNEL_MOD="fantgpu"
    PKG_CONFLICTS="innogpu-fh2m, innogpu-fh2m-kernel-dkms, innogpu-kernel-dkms, innogpu-fh2m-trixie"
    DESC_BODY=$(printf ' fantgpu lineage: O_stage materialized source tree (030 chain of 13,\n patch-000 no-transform), coherent fantgpu (F) userspace payload (binary-manifest-fantgpu.json locked; build-time DDX/UCM/wayland selection).')
else
    PKG_NAME="innogpu-fh2m-trixie"
    PKG_DESC="Innosilicon Fantasy II-M driver (migrated source tree, version $VERSION)"
    DKMS_MOD="innogpu-kernel"
    DKMS_VER="2.2"
    KERNEL_MOD="innogpu"
    PKG_CONFLICTS="innogpu-fh2m, innogpu-fh2m-kernel-dkms, innogpu-kernel-dkms"
    DESC_BODY=$(printf ' New-architecture build: drivers/ source tree + manifest-managed black-box\n payload, with the reviewed %s suspend/resume fixes.' "$APPLIED_SOURCE_FIXES")
fi
cat > "$P/DEBIAN/control" <<EOF
Package: $PKG_NAME
Version: $VERSION
Section: graphics
Priority: optional
Architecture: amd64
Installed-Size: ${installed_size}
Depends: dkms, build-essential, libdrm2, libepoxy0, libpixman-1-0, libwayland-server0, libxcb-randr0
Recommends: linux-headers-amd64, libegl1, libgles2, libgl1, libglx0, libgles1, libglvnd0
Conflicts: $PKG_CONFLICTS
Replaces: $PKG_CONFLICTS
Maintainer: Tim Hant <tthantclaw@outlook.com>
Homepage: https://github.com/timhant/innogpu-fh2m-debian-trixie
Description: $PKG_DESC
${DESC_BODY}
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
echo "Configuring $PKG_NAME $VERSION..."

install -d /etc/modprobe.d
if [[ "$FANT_LINEAGE" == 1 ]]; then
    # fantgpu 血统：模块参数名未经设备审核，不写 modprobe options（待 O_stage
    # 运行时矩阵确认后另行裁决）
    :
else
    printf '%s\n' 'options innogpu firmware_en=1' > /etc/modprobe.d/innogpu.conf
fi

dkms add -m $DKMS_MOD -v $DKMS_VER 2>/dev/null || true
dkms build -m $DKMS_MOD -v $DKMS_VER -k "\$kernel_ver" --force
dkms install -m $DKMS_MOD -v $DKMS_VER -k "\$kernel_ver" --force

# The package ships a complete, matching coherent userspace set.
# Never move or restore an individual vendor library here.
if [[ "$FANT_LINEAGE" == 1 ]]; then
    DRI_SO=/usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so
    DRI_NAME=fh2m_dri.so
    # 设备门（③ G 检查锁定 PCI ID）：缺失 WARNING 不中止；lspci 不可用同样
    # 必须 WARNING（codex 初审 P2：不得静默跳过）——安装器在无设备机可完成
    # 构建/测试场景；真机批步骤 3 的设备存在性由 R 项权威判定。
    if ! command -v lspci >/dev/null 2>&1; then
        echo "WARNING: lspci unavailable; 1ec8:9810 device gate not executed" >&2
    elif ! lspci -n -d 1ec8:9810 | grep -q .; then
        echo "WARNING: no 1ec8:9810 device present" >&2
    fi
else
    DRI_SO=/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so
    DRI_NAME=innogpu_dri.so
fi
if [ ! -f "\$DRI_SO" ]; then
    echo "ERROR: coherent \$DRI_NAME is missing" >&2
    exit 1
fi

ldconfig
depmod -a "\$kernel_ver"
if command -v update-initramfs >/dev/null 2>&1; then
    update-initramfs -u -k "\$kernel_ver"
fi

if [[ "$FANT_LINEAGE" == 1 ]]; then
    echo "Installed coherent fantgpu (F) userspace payload; module autoload policy was not changed."
else
    echo "Installed coherent Deepin 202504 userspace; module autoload policy was not changed."
fi
exit 0
EOF

cat > "$P/DEBIAN/prerm" <<'PEOF'
#!/bin/bash
set -e

case "${1:-}" in
    remove|upgrade|deconfigure)
        dkms remove -m fantgpu-fh2m-kernel -v 2.2 --all 2>/dev/null || true
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
# F 分支：md5sums 按实际安装载荷重生成（④ 强制项 1；不得继承厂商 95 条路径错配）
if [[ "$FANT_LINEAGE" == 1 ]]; then
    python3 tools/gen-package-md5sums.py --root "$P" || {
        echo "builder_md5sums=FAIL"; exit 1; }
    echo "builder_md5sums=PASS"
fi
find "$P" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +

mkdir -p "$(dirname "$OUT_DEB")"
dpkg-deb --root-owner-group --build "$P" "$OUT_DEB"
echo "builder_package_build=PASS $OUT_DEB"

# 5) 边界检查
if [[ "$FANT_LINEAGE" == 1 ]]; then
    # fantgpu 血统：C1-②（required 载荷断言按 F 路径）与 release 审计门禁恢复
    # 调用在 ④ 落地 + 重构建之后同批交付（批 3）——本分支保留 .o.cmd/.orig/.rej
    # 包边界守卫，见上。
    echo "builder_package_boundary=PASS (fantgpu lineage; release-audit gate pending batch-3 C1-2)"
else
    scripts/check-release-package.sh "$OUT_DEB" || { echo "builder_package_boundary=FAIL"; exit 1; }
    echo "builder_package_boundary=PASS"
fi
echo "builder_overall=PASS"
echo "builder_out_deb=$OUT_DEB"
echo "builder_lineage=$([[ "$FANT_LINEAGE" == 1 ]] && echo fantgpu || echo innogpu)"
