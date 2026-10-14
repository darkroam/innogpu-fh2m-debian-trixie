#!/bin/bash
# Gentoo fallback for the i12 + m-036-bf-shf-rename + m-051-hwinfo-connector-fallback module. The Debian builder stays a deb builder.
set -euo pipefail
umask 022

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
WORK=${GENTOO_WORK:-$ROOT/.build/work/gentoo-fh2m}
ROLLBACK=${GENTOO_ROLLBACK:-$ROOT/.build/work/gentoo-fh2m/rollback}
KERNEL=${KERNEL:-$(uname -r)}
KERNELDIR=${KERNELDIR:-/lib/modules/$KERNEL/build}
VENDOR=${GENTOO_VENDOR:-$ROOT/vendor/fantgpu}
SNAP=$ROOT/docs/planning/evidence/o-stage/5.0.0-i6/o-stage-snapshot.tar.zst
I12=9a8d185f2a65892a585b8f8f699f849a46ac6a4ab3c3e39a6a3a09357c2eb2ee
TREE_M036=0eda30cdce3da3d872c56e7ebb3c89b4dc23a5fead402f0e43300b2130a184ce
TREE_M051=b0a46c345c1e0d93baf51c554a1cc9dde0757feb68ce4a9951c3c01458f71c00
PREFIX=/opt/fantgpu-fh2m

log() { printf '%s %s\n' "$(date -Iseconds)" "$*"; }
die() { printf 'gentoo_fh2m=FAIL %s\n' "$*" >&2; exit 1; }

tree_hash() {
    python3 - "$ROOT" "$1" << 'PY'
import hashlib, importlib.util, sys
repo, tree = sys.argv[1:]
spec = importlib.util.spec_from_file_location("o4", repo + "/tools/internal/o4-f0-lock-gen.py")
o4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(o4)
print(hashlib.sha256(o4.manifest_text(list(o4.walk_rows(tree))).encode()).hexdigest())
PY
}

need_repo() {
    [[ -f $SNAP ]] || die "missing i6 snapshot"
    [[ -f $ROOT/patches/m-035-private-accessor.patch && -f $ROOT/patches/m-036-bf-shf-rename.patch && -f $ROOT/patches/m-051-hwinfo-connector-fallback.patch ]] || die "missing m-035, m-036 or m-051"
    [[ -f $ROOT/tools/internal/derive-fantgpu-source.sh ]] || die "derive source missing"
}

cmd_check() {
    local offline=0
    [[ ${1:-} == --offline ]] && offline=1
    need_repo
    log "repo_inputs=PASS snapshot=$(sha256sum "$SNAP" | awk '{print $1}')"
    if [[ $offline -eq 0 ]]; then
        [[ -d $KERNELDIR ]] || die "kernel headers missing: $KERNELDIR"
        log "kernel=$KERNEL headers=PASS"
    fi
    if [[ -d $VENDOR/usr/lib/x86_64-linux-gnu/fantgpu-fh2m ]]; then
        log "vendor=PASS"
    else
        log "vendor=ABSENT run the fantgpu materializer before install"
        [[ $offline -eq 1 ]] || die "vendor tree absent"
    fi
    log "gentoo_fh2m_check=PASS"
}

cmd_build() {
    need_repo
    [[ -d $KERNELDIR ]] || die "kernel headers missing: $KERNELDIR"
    local src=$WORK/src
    rm -rf "$src"
    mkdir -p "$WORK"
    tar --use-compress-program=zstd -xf "$SNAP" -C "$WORK"
    if [[ -d $WORK/o-stage && ! -d $src ]]; then
        mv "$WORK/o-stage" "$src"
    fi
    [[ -f $src/Makefile ]] || die "snapshot did not unpack a module tree"
    # shellcheck disable=SC1091
    source "$ROOT/tools/internal/derive-fantgpu-source.sh"
    derive_fantgpu_source "$src"
    [[ $(tree_hash "$src") == "$I12" ]] || die "i12 tree hash mismatch"
    patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s -d "$src" -p1 \
        < "$ROOT/patches/m-036-bf-shf-rename.patch"
    [[ $(tree_hash "$src") == "$TREE_M036" ]] || die "m-036 tree hash mismatch"
    patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s -d "$src" -p1 \
        < "$ROOT/patches/m-051-hwinfo-connector-fallback.patch"
    [[ $(tree_hash "$src") == "$TREE_M051" ]] || die "m-051 tree hash mismatch"
    make -C "$src" KERNELDIR="$KERNELDIR"
    [[ -f $src/fantgpu.ko ]] || die "make produced no fantgpu.ko"
    install -m 0644 "$src/fantgpu.ko" "$WORK/fantgpu.ko"
    log "built=$(sha256sum "$WORK/fantgpu.ko" | awk '{print $1}') srcversion=$(modinfo -F srcversion "$WORK/fantgpu.ko") tree=$TREE_M051"
    log "gentoo_fh2m_build=PASS"
}

install_tree() {
    local dest=$1
    [[ -d $VENDOR/usr/lib/x86_64-linux-gnu/fantgpu-fh2m ]] || die "vendor tree absent"
    mkdir -p "$dest$PREFIX/lib" "$dest$PREFIX/dri" "$dest$PREFIX/gbm" \
        "$dest/usr/lib64/xorg/modules/drivers" "$dest/usr/lib64/va/drivers" \
        "$dest/usr/lib/x86_64-linux-gnu/dri" "$dest/usr/lib/x86_64-linux-gnu/gbm" \
        "$dest/usr/share/X11/xorg.conf.d" "$dest/usr/share/glvnd/egl_vendor.d" \
        "$dest/usr/share/vulkan/icd.d" "$dest/etc/OpenCL/vendors" \
        "$dest/etc/ld.so.conf.d" "$dest/etc/modprobe.d" \
        "$dest/lib/firmware/fantgpu" "$dest/usr/share/drirc.d"
    cp -a "$VENDOR/usr/lib/x86_64-linux-gnu/fantgpu-fh2m/." "$dest$PREFIX/lib/"
    cp -a "$VENDOR/usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so" \
        "$VENDOR/usr/lib/x86_64-linux-gnu/dri/fh2m_drv_video.so" "$dest$PREFIX/dri/"
    ln -sfn ../lib/libfh2m_gbm.so "$dest$PREFIX/gbm/fh2m_gbm.so"
    [[ -e $dest$PREFIX/gbm/fh2m_gbm.so ]] || die "gbm payload did not resolve"
    install -m 0644 "$VENDOR/opt/fantgpu-fh2m/usr/lib/xorg/modules/drivers/fh2m_drv.so.1.21" \
        "$dest/usr/lib64/xorg/modules/drivers/fh2m_drv.so"
    ln -sfn "$PREFIX/dri/fh2m_dri.so" "$dest/usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so"
    ln -sfn "$PREFIX/dri/fh2m_drv_video.so" "$dest/usr/lib/x86_64-linux-gnu/dri/fh2m_drv_video.so"
    ln -sfn "$PREFIX/gbm/fh2m_gbm.so" "$dest/usr/lib/x86_64-linux-gnu/gbm/fh2m_gbm.so"
    ln -sfn "$PREFIX/dri/fh2m_drv_video.so" "$dest/usr/lib64/va/drivers/fh2m_drv_video.so"
    cp -a "$VENDOR/lib/firmware/fantgpu/." "$dest/lib/firmware/fantgpu/"
    install -m 0644 "$VENDOR/usr/share/X11/xorg.conf.d/10-fh2m.conf" "$dest/usr/share/X11/xorg.conf.d/10-fh2m.conf"
    install -m 0644 "$VENDOR/usr/share/glvnd/egl_vendor.d/00_fh2m.json" "$dest/usr/share/glvnd/egl_vendor.d/00_fh2m.json"
    install -m 0644 "$VENDOR/etc/vulkan/icd.d/fh2m_conf.json" "$dest/usr/share/vulkan/icd.d/fh2m_conf.json"
    install -m 0644 "$VENDOR/etc/OpenCL/vendors/FANT_fh2m.icd" "$dest/etc/OpenCL/vendors/FANT_fh2m.icd"
    install -m 0644 "$VENDOR/usr/share/drirc.d/01-fh2m_drv.conf" "$dest/usr/share/drirc.d/01-fh2m_drv.conf"
    printf '%s\n' "$PREFIX/lib" > "$dest/etc/ld.so.conf.d/0-fantgpu-hwgl.conf"
    printf '%s\n' 'options fantgpu firmware_en=1' > "$dest/etc/modprobe.d/fantgpu.conf"
}

cmd_install() {
    [[ -f $WORK/fantgpu.ko ]] || die "build first"
    local dest=${DESTDIR:-}
    local moddir=$dest/lib/modules/$KERNEL/extra
    local live=${GENTOO_LIVE_KO:-}
    mkdir -p "$moddir" "$ROLLBACK"
    if [[ -z $live ]]; then
        live=/lib/modules/$KERNEL/extra/fantgpu.ko
    fi
    if [[ -n $live && -f $ROLLBACK/fantgpu.ko ]]; then
        log "rollback_kept=$(sha256sum "$ROLLBACK/fantgpu.ko" | awk '{print $1}')"
    elif [[ -n $live && -f $live ]]; then
        install -m 0644 "$live" "$ROLLBACK/fantgpu.ko"
        log "rollback_saved=$(sha256sum "$ROLLBACK/fantgpu.ko" | awk '{print $1}')"
    fi
    install -m 0644 "$WORK/fantgpu.ko" "$moddir/fantgpu.ko"
    install_tree "$dest"
    if [[ -z $dest ]]; then
        depmod -a "$KERNEL"
        ldconfig
        log "installed_ko=$(sha256sum "$moddir/fantgpu.ko" | awk '{print $1}')"
        log "reboot_required=yes hot_unplug=forbidden"
    else
        log "destdir=$dest"
    fi
    log "gentoo_fh2m_install=PASS"
}

cmd_warn_loaded() {
    local modules=${GENTOO_MODULES_FILE:-/proc/modules}
    if [[ -f $modules ]] && grep -q '^fantgpu ' "$modules"; then
        log "gentoo_fh2m_uninstall=WARN loaded; reboot before this counts as removed; do not rmmod"
        return 0
    fi
    log "gentoo_fh2m_uninstall=loaded_no"
}

cmd_rollback() {
    local saved=$ROLLBACK/fantgpu.ko
    [[ -f $saved ]] || die "no rollback ko at $saved"
    [[ ${EUID:-$(id -u)} -eq 0 ]] || die "rollback needs root"
    install -m 0644 "$saved" "/lib/modules/$KERNEL/extra/fantgpu.ko"
    depmod -a "$KERNEL"
    log "restored=$(sha256sum "/lib/modules/$KERNEL/extra/fantgpu.ko" | awk '{print $1}') reboot_required=yes"
}

check_counters() {
    awk '
        BEGIN { split("Server HWR CRR SLR WGP TRP FWF APM", names, " ") }
        $1=="Server" || $1=="HWR" || $1=="CRR" || $1=="SLR" || $1=="WGP" || $1=="TRP" || $1=="FWF" || $1=="APM" {
            seen[$1] = 1
            if ($NF != 0) bad = 1
        }
        END {
            missing = ""
            for (i = 1; i <= 8; i++) if (!seen[names[i]]) missing = missing " " names[i]
            if (missing != "") {
                print "counters_rows_missing" missing
                exit 1
            }
            exit bad ? 1 : 0
        }
    ' "$1" || die "eight counters not zero"
}

cmd_verify() {
    local srcversion ko=/lib/modules/$KERNEL/extra/fantgpu.ko
    local status=${GENTOO_STATUS_FILE:-/proc/driver/fantgpu/gpu00/status}
    [[ -f $status ]] || die "no gpu status"
    if [[ ${GENTOO_COUNTERS_ONLY:-} == 1 ]]; then
        check_counters "$status"
        log "eight_counters=0"
        exit 0
    fi
    [[ -f $ko ]] || die "module file missing"
    srcversion=$(modinfo -F srcversion "$ko")
    log "ko_srcversion=$srcversion"
    [[ -e /dev/dri/card0 ]] || die "no DRM card"
    check_counters "$status"
    log "eight_counters=0 drm=card0"
    log "hwinfo_gap=known_not_pass"
    log "gentoo_fh2m_verify=PASS"
}

cmd_install_ko() {
    [[ -f $WORK/fantgpu.ko ]] || die "build first"
    local dest=${DESTDIR:-}
    local moddir=$dest/lib/modules/$KERNEL/extra
    local live=${GENTOO_LIVE_KO:-}
    mkdir -p "$moddir" "$ROLLBACK"
    if [[ -z $live ]]; then
        live=/lib/modules/$KERNEL/extra/fantgpu.ko
    fi
    if [[ -f $ROLLBACK/fantgpu.ko ]]; then
        log "rollback_kept=$(sha256sum "$ROLLBACK/fantgpu.ko" | awk '{print $1}')"
    elif [[ -f $live ]]; then
        install -m 0644 "$live" "$ROLLBACK/fantgpu.ko"
        log "rollback_saved=$(sha256sum "$ROLLBACK/fantgpu.ko" | awk '{print $1}')"
    fi
    install -m 0644 "$WORK/fantgpu.ko" "$moddir/fantgpu.ko"
    if [[ -z $dest ]]; then
        depmod -a "$KERNEL"
        log "installed_ko=$(sha256sum "$moddir/fantgpu.ko" | awk '{print $1}')"
        log "reboot_required=yes hot_unplug=forbidden"
    else
        log "destdir=$dest"
    fi
    log "gentoo_fh2m_install_ko=PASS"
}

usage() { die "use check [--offline] | build | install | install-ko | verify | rollback | warn-loaded"; }

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    case ${1:-} in
        check) cmd_check "${2:-}" ;;
        build) cmd_build ;;
        install) cmd_install ;;
        install-ko) cmd_install_ko ;;
        verify) cmd_verify ;;
        rollback) cmd_rollback ;;
        warn-loaded) cmd_warn_loaded ;;
        *) usage ;;
    esac
fi
