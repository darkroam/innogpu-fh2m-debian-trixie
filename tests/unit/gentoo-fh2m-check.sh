#!/bin/bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
bash -n "$root/tools/gentoo/fh2m-lib.sh"
bash -n "$root/tools/gentoo/build-driver.sh"
bash -n "$root/tools/gentoo/install-driver.sh"
bash -n "$root/tools/gentoo/verify.sh"
bash -n "$root/tools/gentoo/rollback.sh"
bash -n "$root/tools/gentoo/lid-external.sh"
bash -n "$root/tools/gentoo/install-kernel-patches.sh"
bash -n "$root/tools/gentoo/overlay/sys-kernel/fantgpu-kernel-patches/files/install-patches.sh"
bash -n "$root/tools/gentoo/check-kernel-patches.sh"
"$root/tools/gentoo/check.sh" --offline

work=$(mktemp -d)
fix=$(mktemp)
out=$(mktemp)
trap 'rm -rf "$work" "$fix" "$out"' EXIT
DESTDIR=$work/dest "$root/tools/gentoo/install-kernel-patches.sh" >"$out"
grep -q 'installed=12 skipped=0' "$out"
before=$(sha256sum "$work/dest/etc/portage/patches/sys-kernel/gentoo-sources/001-hygon-148c-xhci-reset-on-resume.patch")
DESTDIR=$work/dest "$root/tools/gentoo/install-kernel-patches.sh" >"$out"
grep -q 'installed=0 skipped=12' "$out"
after=$(sha256sum "$work/dest/etc/portage/patches/sys-kernel/gentoo-sources/001-hygon-148c-xhci-reset-on-resume.patch")
[[ $before == "$after" ]]
printf 'local\n' >> "$work/dest/etc/portage/patches/sys-kernel/gentoo-kernel/004-snd-hda-hygon-14c9.patch"
changed=$(sha256sum "$work/dest/etc/portage/patches/sys-kernel/gentoo-kernel/004-snd-hda-hygon-14c9.patch")
if DESTDIR=$work/dest "$root/tools/gentoo/install-kernel-patches.sh" >"$out" 2>&1; then
    echo "different sha was overwritten" >&2
    exit 1
fi
grep -q 'sha differs' "$out"
[[ $changed == $(sha256sum "$work/dest/etc/portage/patches/sys-kernel/gentoo-kernel/004-snd-hda-hygon-14c9.patch") ]]
cp "$work/dest/etc/portage/patches/sys-kernel/gentoo-sources/004-snd-hda-hygon-14c9.patch" \
    "$work/dest/etc/portage/patches/sys-kernel/gentoo-kernel/004-snd-hda-hygon-14c9.patch"
PORTAGE_PATCH_BASE=$work/dest/etc/portage/patches/sys-kernel "$root/tools/gentoo/check-kernel-patches.sh" --files-only >/dev/null
grep -q '补丁已落位，重编/升级内核生效后再装驱动' "$root/tools/gentoo/overlay/sys-kernel/fantgpu-kernel-patches/fantgpu-kernel-patches-1.ebuild"
grep -q 'RESTRICT="bindist"' "$root/tools/gentoo/overlay/sys-kernel/fantgpu-kernel-patches/fantgpu-kernel-patches-1.ebuild"
grep -q 'IUSE="+wayland"' "$root/tools/gentoo/overlay/x11-drivers/fantgpu-fh2m/fantgpu-fh2m-5.0.0_p12.ebuild"
grep -q 'dev-libs/wayland' "$root/tools/gentoo/overlay/x11-drivers/fantgpu-fh2m/fantgpu-fh2m-5.0.0_p12.ebuild"
grep -q 'media-libs/libglvnd' "$root/tools/gentoo/overlay/x11-drivers/fantgpu-fh2m/fantgpu-fh2m-5.0.0_p12.ebuild"
if PORTAGE_PATCH_BASE=$work/missing "$root/tools/gentoo/check-kernel-patches.sh" --files-only >"$out" 2>&1; then
    echo "absent portage patches were accepted" >&2
    exit 1
fi
grep -q 'portage_patches=ABSENT' "$out"
cat > "$fix" << 'EOF'
Server Errors:   0
HWR Event Count: 0
CRR Event Count: 0
SLR Event Count: 0
WGP Error Count: 0
TRP Error Count: 0
FWF Event Count: 0
APM Event Count: 0
EOF
GENTOO_COUNTERS_ONLY=1 GENTOO_STATUS_FILE=$fix "$root/tools/gentoo/verify.sh" >/dev/null
sed -i '/^APM /d' "$fix"
if GENTOO_COUNTERS_ONLY=1 GENTOO_STATUS_FILE=$fix "$root/tools/gentoo/verify.sh" >"$out" 2>&1; then
    echo "missing row was accepted" >&2
    exit 1
fi
grep -q counters_rows_missing "$out"
printf '%s\n' 'Server Errors:   1' 'HWR Event Count: 0' 'CRR Event Count: 0' 'SLR Event Count: 0' \
    'WGP Error Count: 0' 'TRP Error Count: 0' 'FWF Event Count: 0' 'APM Event Count: 0' > "$fix"
if GENTOO_COUNTERS_ONLY=1 GENTOO_STATUS_FILE=$fix "$root/tools/gentoo/verify.sh" >"$out" 2>&1; then
    echo "nonzero counter was accepted" >&2
    exit 1
fi
if grep -q counters_rows_missing "$out"; then
    echo "nonzero reported missing rows" >&2
    exit 1
fi

bash -n "$root/tools/gentoo/r5-observe.sh"
bash -n "$root/tools/gentoo/r5-cycle.sh"
if "$root/tools/gentoo/r5-cycle.sh" 5 >"$out" 2>&1; then
    echo "short wake was accepted" >&2
    exit 1
fi
grep -q "usage:" "$out"
fixdir=$(mktemp -d)
trap 'rm -rf "$work" "$fix" "$out" "$fixdir"' EXIT
printf '%s\n' \
    'boot_id=abc' 'mem_sleep=s2idle [deep]' 'pm_test=[none]' 'card0=yes' \
    'counter_missing=0' 'counter_bad=0' 'suspend_entry=1' 'suspend_exit=1' \
    'temp_work=0' 'pll=0' 'oops=0' >"$fixdir/before"
sed -e 's/suspend_entry=1/suspend_entry=2/' -e 's/suspend_exit=1/suspend_exit=2/' \
    "$fixdir/before" >"$fixdir/after"
got=$("$root/tools/gentoo/r5-observe.sh" --judge "$fixdir/before" "$fixdir/after")
[[ $got == r5_judge=RECORDED_OBSERVATION_ABSENT ]]
if "$root/tools/gentoo/r5-observe.sh" --judge "$fixdir/before" "$fixdir/before" >"$out"; then
    echo "unchanged cycle was accepted" >&2
    exit 1
fi
grep -q 'reason=no_new_cycle' "$out"
sed -e 's/^pm_test=.*/pm_test=[none] core processors platform devices freezer/' \
    -e 's/suspend_entry=1/suspend_entry=0/' -e 's/suspend_exit=1/suspend_exit=0/' \
    "$fixdir/before" >"$fixdir/before-real"
sed -e 's/suspend_entry=0/suspend_entry=1/' -e 's/suspend_exit=0/suspend_exit=1/' \
    "$fixdir/before-real" >"$fixdir/after-real"
got=$("$root/tools/gentoo/r5-observe.sh" --judge "$fixdir/before-real" "$fixdir/after-real")
[[ $got == r5_judge=RECORDED_OBSERVATION_ABSENT ]]

fake=$(mktemp -d)
trap 'rm -rf "$work" "$fix" "$out" "$fixdir" "$fake"' EXIT
mkdir -p "$fake/vendor/usr/lib/x86_64-linux-gnu/fantgpu-fh2m" \
    "$fake/vendor/usr/lib/x86_64-linux-gnu/dri" \
    "$fake/vendor/opt/fantgpu-fh2m/usr/lib/xorg/modules/drivers" \
    "$fake/vendor/lib/firmware/fantgpu" \
    "$fake/vendor/usr/share/X11/xorg.conf.d" \
    "$fake/vendor/usr/share/glvnd/egl_vendor.d" \
    "$fake/vendor/etc/vulkan/icd.d" \
    "$fake/vendor/etc/OpenCL/vendors" \
    "$fake/vendor/usr/share/drirc.d" \
    "$fake/live" "$fake/rollback"
printf 'gbm\n' > "$fake/vendor/usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libfh2m_gbm.so"
printf 'dri\n' > "$fake/vendor/usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so"
printf 'video\n' > "$fake/vendor/usr/lib/x86_64-linux-gnu/dri/fh2m_drv_video.so"
printf 'ddx\n' > "$fake/vendor/opt/fantgpu-fh2m/usr/lib/xorg/modules/drivers/fh2m_drv.so.1.21"
printf 'fw\n' > "$fake/vendor/lib/firmware/fantgpu/fh2m.fw"
printf 'conf\n' > "$fake/vendor/usr/share/X11/xorg.conf.d/10-fh2m.conf"
printf '{}\n' > "$fake/vendor/usr/share/glvnd/egl_vendor.d/00_fh2m.json"
printf '{}\n' > "$fake/vendor/etc/vulkan/icd.d/fh2m_conf.json"
printf 'icd\n' > "$fake/vendor/etc/OpenCL/vendors/FANT_fh2m.icd"
printf 'dri\n' > "$fake/vendor/usr/share/drirc.d/01-fh2m_drv.conf"
mkdir -p "$fake/work"
printf 'old-ko\n' > "$fake/live/fantgpu.ko"
printf 'built-ko\n' > "$fake/work/fantgpu.ko"
GENTOO_VENDOR=$fake/vendor GENTOO_WORK=$fake/work GENTOO_ROLLBACK=$fake/rollback \
    GENTOO_LIVE_KO=$fake/live/fantgpu.ko DESTDIR=$fake/dest KERNEL=test \
    "$root/tools/gentoo/install-driver.sh" >"$out"
grep -q 'rollback_saved=' "$out"
first=$(sha256sum "$fake/rollback/fantgpu.ko")
printf 'replaced-ko\n' > "$fake/live/fantgpu.ko"
GENTOO_VENDOR=$fake/vendor GENTOO_WORK=$fake/work GENTOO_ROLLBACK=$fake/rollback \
    GENTOO_LIVE_KO=$fake/live/fantgpu.ko DESTDIR=$fake/dest KERNEL=test \
    "$root/tools/gentoo/install-driver.sh" >"$out"
grep -q 'rollback_kept=' "$out"
[[ $first == $(sha256sum "$fake/rollback/fantgpu.ko") ]]
[[ $(cat "$fake/dest/lib/modules/test/extra/fantgpu.ko") == built-ko ]]
printf 'fantgpu 1 0 - Live\n' > "$fake/modules"
GENTOO_MODULES_FILE=$fake/modules "$root/tools/gentoo/warn-loaded.sh" >"$out"
grep -q 'do not rmmod' "$out"
printf 'other 1 0 - Live\n' > "$fake/modules"
GENTOO_MODULES_FILE=$fake/modules "$root/tools/gentoo/warn-loaded.sh" >"$out"
grep -q 'loaded_no' "$out"
grep -q '模块已加载。不要热卸载。先重启，再把这次卸载当成完成。' \
    "$root/tools/gentoo/overlay/x11-drivers/fantgpu-fh2m/fantgpu-fh2m-5.0.0_p12.ebuild"
grep -q 'pkg_postrm' "$root/tools/gentoo/overlay/x11-drivers/fantgpu-fh2m/fantgpu-fh2m-5.0.0_p12.ebuild"
grep -q '不要热卸载。重启后才是正在运行的那份。' "$root/tools/gentoo/overlay/x11-drivers/fantgpu-fh2m/fantgpu-fh2m-5.0.0_p12.ebuild"

rmdest=$(mktemp -d)
trap 'rm -rf "$work" "$fix" "$out" "$fixdir" "$fake" "$rmdest"' EXIT
DESTDIR=$rmdest "$root/tools/gentoo/install-kernel-patches.sh" >/dev/null
printf 'local\n' >> "$rmdest/etc/portage/patches/sys-kernel/gentoo-sources/003-pinctrl-amd-hygon-gpio.patch"
kept=$(sha256sum "$rmdest/etc/portage/patches/sys-kernel/gentoo-sources/003-pinctrl-amd-hygon-gpio.patch")
PATCH_REMOVE=1 DESTDIR=$rmdest "$root/tools/gentoo/install-kernel-patches.sh" >"$out"
grep -q 'removed=11 kept=1' "$out"
[[ -f $rmdest/etc/portage/patches/sys-kernel/gentoo-sources/003-pinctrl-amd-hygon-gpio.patch ]]
[[ $kept == $(sha256sum "$rmdest/etc/portage/patches/sys-kernel/gentoo-sources/003-pinctrl-amd-hygon-gpio.patch") ]]
[[ ! -f $rmdest/etc/portage/patches/sys-kernel/gentoo-sources/001-hygon-148c-xhci-reset-on-resume.patch ]]
grep -q 'pkg_prerm' "$root/tools/gentoo/overlay/sys-kernel/fantgpu-kernel-patches/fantgpu-kernel-patches-1.ebuild"

lid=$(mktemp -d)
trap 'rm -rf "$work" "$fix" "$out" "$fixdir" "$fake" "$rmdest" "$lid"' EXIT
GENTOO_LID_CONF=$lid/90-lid-external.conf "$root/tools/gentoo/lid-external.sh" --apply >"$out"
grep -q 'lid_conf=APPLIED' "$out"
GENTOO_LID_CONF=$lid/90-lid-external.conf "$root/tools/gentoo/lid-external.sh" --apply >"$out"
grep -q 'lid_conf=SKIP identical' "$out"
printf 'local\n' >> "$lid/90-lid-external.conf"
if GENTOO_LID_CONF=$lid/90-lid-external.conf "$root/tools/gentoo/lid-external.sh" --revert >"$out" 2>&1; then
    echo "different lid conf was removed" >&2
    exit 1
fi
grep -q 'lid_conf=FAIL differs' "$out"
ko=$(mktemp -d)
trap 'rm -rf "$work" "$fix" "$out" "$fixdir" "$fake" "$rmdest" "$lid" "$ko"' EXIT
mkdir -p "$ko/work" "$ko/rollback" "$ko/live"
printf 'built-ko\n' > "$ko/work/fantgpu.ko"
printf 'old-ko\n' > "$ko/live/fantgpu.ko"
GENTOO_WORK=$ko/work GENTOO_ROLLBACK=$ko/rollback GENTOO_LIVE_KO=$ko/live/fantgpu.ko \
    DESTDIR=$ko/dest KERNEL=test "$root/tools/gentoo/install-ko.sh" >"$out"
grep -q 'rollback_saved=' "$out"
grep -q 'gentoo_fh2m_install_ko=PASS' "$out"
[[ $(cat "$ko/dest/lib/modules/test/extra/fantgpu.ko") == built-ko ]]
[[ ! -e $ko/dest/opt/fantgpu-fh2m ]]
printf 'other-ko\n' > "$ko/live/fantgpu.ko"
GENTOO_WORK=$ko/work GENTOO_ROLLBACK=$ko/rollback GENTOO_LIVE_KO=$ko/live/fantgpu.ko \
    DESTDIR=$ko/dest KERNEL=test "$root/tools/gentoo/install-ko.sh" >"$out"
grep -q 'rollback_kept=' "$out"
[[ $(cat "$ko/rollback/fantgpu.ko") == old-ko ]]
