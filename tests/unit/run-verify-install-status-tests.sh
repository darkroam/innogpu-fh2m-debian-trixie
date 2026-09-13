#!/usr/bin/env bash
# tests/unit/run-verify-install-status-tests.sh — F2 lineage 参数化单测
#
# 契约（validation-plan §〇 F2 自测行）：fake root + fantgpu lineage 参数族
# 断言（innogpu 路径不提供、以诱饵日志反证命令参数/路径全部走 fantgpu）；
# 非法 lineage fail-closed。
# 退出码：0=全过 1=用例失败 2=环境错误。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOL="$ROOT/scripts/verify-install-status.sh"
cd "$ROOT"
export LC_ALL=C

[ -f "$TOOL" ] || { echo "FATAL: verify-install-status.sh not found" >&2; exit 2; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/vis-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT INT TERM HUP

PASS=0; FAILN=0
ok()  { PASS=$((PASS + 1)); echo "ok  $1"; }
bad() { FAILN=$((FAILN + 1)); echo "BAD $1: $2" >&2; }

mkdir -p "$TMP/bin"
KERNEL="$(uname -r)"
CALLS="$TMP/calls.log"
cat > "$TMP/bin/dpkg-query" <<EOF
#!/bin/bash
printf 'dpkg-query %s\n' "\$*" >> "$CALLS"
case " \$* " in
    *" fantgpu-fh2m-trixie "*) echo "5.0.0-i2" ;;
    *" innogpu-fh2m-trixie "*) echo "4.0.2-i3" ;;
    *) exit 1 ;;
esac
EOF
cat > "$TMP/bin/dkms" <<EOF
#!/bin/bash
printf 'dkms %s\n' "\$*" >> "$CALLS"
if [[ "\$2" == "fantgpu-fh2m-kernel" || "\$2" == "innogpu-kernel" ]]; then
    echo "\$2/2.2, $KERNEL, x86_64: installed"
fi
EOF
cat > "$TMP/bin/lsmod" <<EOF
#!/bin/bash
printf 'lsmod\n' >> "$CALLS"
echo "Module Size Used by"
echo "fantgpu 123456 0"
echo "innogpu 123456 0"
EOF
cat > "$TMP/bin/modinfo" <<EOF
#!/bin/bash
printf 'modinfo %s\n' "\$*" >> "$CALLS"
if [[ "\$*" == *fantgpu* ]]; then echo "/lib/modules/$KERNEL/updates/dkms/fantgpu.ko.xz"; fi
if [[ "\$*" == *innogpu* ]]; then echo "/lib/modules/$KERNEL/updates/dkms/innogpu.ko.xz"; fi
EOF
chmod 0755 "$TMP/bin"/*

make_fakeroot() {  # make_fakeroot <dir> <lineage-good: fantgpu|innogpu>
    local root="$1" good="$2"
    if [[ "$good" == "fantgpu" ]]; then
        local pkg=fantgpu-fh2m-trixie mod=fantgpu ddx=fh2m
        local dri=fh2m_dri.so gbm=fh2m_gbm.so drv=fh2m_drv.so json=00_fh2m.json ldconf=0-fantgpu-hwgl.conf
    else
        local pkg=innogpu-fh2m-trixie mod=innogpu ddx=innogpu
        local dri=innogpu_dri.so gbm=innogpu_gbm.so drv=innogpu_drv.so json=00_inno.json ldconf=0-innogpu-hwgl.conf
    fi
    mkdir -p "$root/var/lib/dpkg/info" "$root/proc/driver/$mod/gpu00" \
             "$root/etc/modules-load.d" "$root/etc/X11" \
             "$root/etc/ld.so.conf.d" \
             "$root/usr/lib/x86_64-linux-gnu/dri" \
             "$root/usr/lib/x86_64-linux-gnu/gbm" \
             "$root/usr/lib/xorg/modules/drivers" \
             "$root/usr/share/glvnd/egl_vendor.d" "$root/dev/dri"
    : > "$root/var/lib/dpkg/info/$pkg.list"
    printf 'Driver Status: OK\nFirmware Status: OK\n' > "$root/proc/driver/$mod/gpu00/status"
    printf '%s\n' "$mod" > "$root/etc/modules-load.d/$mod.conf"
    printf 'Section "Device"\n    Driver "%s"\nEndSection\n' "$ddx" > "$root/etc/X11/xorg.conf"
    : > "$root/etc/ld.so.conf.d/$ldconf"
    : > "$root/usr/lib/x86_64-linux-gnu/dri/$dri"
    : > "$root/usr/lib/x86_64-linux-gnu/gbm/$gbm"
    : > "$root/usr/lib/xorg/modules/drivers/$drv"
    : > "$root/usr/share/glvnd/egl_vendor.d/$json"
    touch "$root/dev/dri/card0" "$root/dev/dri/renderD128" "$root/dev/fb0"
}

run_tool() {  # run_tool <lineage> <fakeroot> [--require-reboot]
    set +e
    INNOGPU_LINEAGE="$1" INNOGPU_FAKE_ROOT="$2" PATH="$TMP/bin:/usr/bin:/bin" \
        bash "$TOOL" "${3:-}" > "$TMP/v.out" 2> "$TMP/v.err"
    RC=$?
    set -e
    OUTTEXT="$(cat "$TMP/v.out")"
    ERRTEXT="$(cat "$TMP/v.err")"
}

# t01 fantgpu lineage 全 PASS：包/DKMS/模块/proc/DDX/hwgl 全部走 fantgpu 参数族
make_fakeroot "$TMP/fk-fant" fantgpu
run_tool fantgpu "$TMP/fk-fant"
if [ "$RC" -eq 0 ] \
   && grep -Fq 'fantgpu-fh2m-trixie is installed' "$TMP/v.out" \
   && grep -Fq 'fantgpu module is loaded' "$TMP/v.out" \
   && grep -Fq 'Xorg persistent config uses fh2m DDX' "$TMP/v.out" \
   && grep -Fq 'present: /usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so' "$TMP/v.out" \
   && grep -Fq 'RESULT: PASS_INSTALL_STATUS' "$TMP/v.out" \
   && grep -Fq 'dkms status fantgpu-fh2m-kernel' "$CALLS" \
   && ! grep -Fq 'innogpu-kernel' "$CALLS" \
   && ! grep -Fq 'innogpu-fh2m-trixie' "$CALLS"; then
    ok t01
else
    bad t01 "rc=$RC out=[$(grep -E 'PASS|FAIL' "$TMP/v.out" | head -3 | tr '\n' ';')]"
fi

# t02 innogpu lineage 缺省回归：同断言走 innogpu 参数族
rm -f "$CALLS"
make_fakeroot "$TMP/fk-inno" innogpu
run_tool innogpu "$TMP/fk-inno"
if [ "$RC" -eq 0 ] \
   && grep -Fq 'innogpu-fh2m-trixie is installed' "$TMP/v.out" \
   && grep -Fq 'innogpu module is loaded' "$TMP/v.out" \
   && grep -Fq 'Xorg persistent config uses innogpu DDX' "$TMP/v.out" \
   && grep -Fq 'present: /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so' "$TMP/v.out" \
   && grep -Fq 'RESULT: PASS_INSTALL_STATUS' "$TMP/v.out" \
   && grep -Fq 'dkms status innogpu-kernel' "$CALLS"; then
    ok t02
else
    bad t02 "rc=$RC out=[$(grep -E 'RESULT|FAIL' "$TMP/v.out" | head -3 | tr '\n' ';')]"
fi

# t03 非法 lineage → fail-closed rc=2
set +e
INNOGPU_LINEAGE=bogus INNOGPU_FAKE_ROOT="$TMP/fk-fant" PATH="$TMP/bin:/usr/bin:/bin" \
    bash "$TOOL" >/dev/null 2>"$TMP/t03.err"
RC3=$?
set -e
if [ "$RC3" -eq 2 ] && grep -Fq 'INNOGPU_LINEAGE must be innogpu or fantgpu' "$TMP/t03.err"; then
    ok t03
else
    bad t03 "rc=$RC3 err=[$(head -1 "$TMP/t03.err")]"
fi

# t04 静态：fantgpu 参数族默认值就位（F2 同族参数）
if grep -Fq 'PKG_NAME="fantgpu-fh2m-trixie"' "$TOOL" \
   && grep -Fq 'DKMS_NAME="fantgpu-fh2m-kernel"' "$TOOL" \
   && grep -Fq 'MOD_NAME="fantgpu"' "$TOOL" \
   && grep -Fq 'PROC_DIR="/proc/driver/fantgpu"' "$TOOL" \
   && grep -Fq 'XORG_DRV="fh2m"' "$TOOL"; then
    ok t04
else
    bad t04 "fantgpu family defaults missing"
fi

echo "PASS=$PASS FAIL=$FAILN"
[ "$FAILN" -eq 0 ] || exit 1
exit 0
