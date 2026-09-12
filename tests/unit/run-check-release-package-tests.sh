#!/usr/bin/env bash
# tests/unit/run-check-release-package-tests.sh — scripts/check-release-package.sh 单测
#
# C1-① 口径：包名白名单（innogpu-fh2m-trixie|fantgpu-fh2m-trixie）、版本正则
# （patched-N>20 | 4.0.x-iN | 5.0.0-iN）、helper 路径按 $package 参数化。
# C1-② 口径（批 3）：required/forbidden 载荷断言按血统分派；F 血统 helper
# cmp 对照 transform-fantgpu-helper.sh 输出（t10-t12）。codex 初审 P1-1/P1-2
# 修复后：required 覆盖权威私有库（validation-plan:257-260）且断言验证对象
# 类型与链接有效性（目录/悬空链接必须 fail，t13-t14）；复审 P1 后补齐
# **完整运行时链接链**（配置/ICD 引用名 + symlink 成员；缺链成员必须 fail，
# t15）；复审#2 P1 后补齐**内容断言**（ELF magic \x7fELF 与配置引用 token，
# 空文件/非 ELF/未引用必须 fail，t16-t17）；复审#3 P1 后收紧：ELF 真身与
# 配置本体**强制普通文件**（有效 symlink 同样 fail，t18-t19）；复审#4 P1 后
# 收紧：链成员 **target 逐字节 == 真品链定义**（错 target/换成普通文件
# fail，t20-t21；t14 改为链截断悬空）。
# 退出码：0=全过 1=用例失败 2=环境错误。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
GATE="$ROOT/scripts/check-release-package.sh"
cd "$ROOT"
export LC_ALL=C

[ -f "$GATE" ] || { echo "FATAL: gate not found" >&2; exit 2; }
command -v dpkg-deb >/dev/null 2>&1 || { echo "FATAL: dpkg-deb required" >&2; exit 2; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/crp-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT INT TERM HUP

PASS=0; FAILN=0
ok()  { PASS=$((PASS + 1)); echo "ok  $1"; }
bad() { FAILN=$((FAILN + 1)); echo "BAD $1: $2" >&2; }

mkdeb() {  # mkdeb <name> <package> <version>
    local name="$1" package="$2" version="$3"
    local root="$TMP/$name"
    mkdir -p "$root/DEBIAN" "$root/usr/share/$package"
    printf 'Package: %s\nVersion: %s\nArchitecture: amd64\n' \
        "$package" "$version" > "$root/DEBIAN/control"
    printf 'Maintainer: fixture\nDescription: fixture\nInstalled-Size: 1\n' \
        >> "$root/DEBIAN/control"
    touch "$root/usr/share/$package/placeholder"
    dpkg-deb --root-owner-group -b "$root" "$TMP/$name.deb" >/dev/null
    echo "DEB=$TMP/$name.deb"
}

run_gate() {  # run_gate <deb>
    set +e
    bash "$GATE" "$1" > "$TMP/g.out" 2> "$TMP/g.err"
    RC=$?
    set -e
    ERRTEXT="$(cat "$TMP/g.err")"
}

# t01 O 白名单 + 版本正则：4.0.2-i3 通过白名单后失败于 required 内容
eval "$(mkdeb t01 innogpu-fh2m-trixie 4.0.2-i3)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "required release file is missing" "$TMP/g.err"; then
    ok t01
else
    bad t01 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t02 F 白名单 + 5.0.0-iN 正则：fantgpu-fh2m-trixie 5.0.0-i2 通过白名单
eval "$(mkdeb t02 fantgpu-fh2m-trixie 5.0.0-i2)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "required release file is missing" "$TMP/g.err"; then
    ok t02
else
    bad t02 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t03 未知包名 → unexpected package
eval "$(mkdeb t03 other-fh2m-trixie 4.0.2-i3)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "unexpected package" "$TMP/g.err"; then
    ok t03
else
    bad t03 "rc=$RC"
fi

# t04 F 包 + 未审核版本 → 血统配对拒绝
eval "$(mkdeb t04 fantgpu-fh2m-trixie 9.9.9)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "only accepts 5.0.0-iN" "$TMP/g.err"; then
    ok t04
else
    bad t04 "rc=$RC"
fi

# t05 patched-N 旧口径回归：patched-28 通过白名单后失败于 required
eval "$(mkdeb t05 innogpu-fh2m-trixie 3.3.3.42-patched-28)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "required release file is missing" "$TMP/g.err"; then
    ok t05
else
    bad t05 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t06 无参数 → rc=2
set +e
bash "$GATE" >/dev/null 2>&1
RC6=$?
set -e
if [ "$RC6" -eq 2 ]; then ok t06; else bad t06 "rc=$RC6 (want 2)"; fi

# t07 deb 缺失 → rc=1
set +e
bash "$GATE" "$TMP/nope.deb" >/dev/null 2>&1
RC7=$?
set -e
if [ "$RC7" -eq 1 ]; then ok t07; else bad t07 "rc=$RC7 (want 1)"; fi

# t08 交叉负向：innogpu 包 + 5.0.0-i2 → 拒绝（codex 初审 P1-4）
eval "$(mkdeb t08 innogpu-fh2m-trixie 5.0.0-i2)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "only accepts patched-N" "$TMP/g.err"; then
    ok t08
else
    bad t08 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t09 交叉负向：fantgpu 包 + 4.0.2-i3 → 拒绝（codex 初审 P1-4）
eval "$(mkdeb t09 fantgpu-fh2m-trixie 4.0.2-i3)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "only accepts 5.0.0-iN" "$TMP/g.err"; then
    ok t09
else
    bad t09 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# ---- C1-②（批 3）----

mkfdeb() {  # mkfdeb <name> <mode: full|forbidden|rawhelper|dirtype|dangling|droplink|emptyelf|badcfg|elflink|cfglink|wrongtarget|plainlink>
    local name="$1" mode="${2:-full}" pkg=fantgpu-fh2m-trixie
    local root="$TMP/$name"
    mkdir -p "$root/DEBIAN"
    printf 'Package: %s\nVersion: 5.0.0-i2\nArchitecture: amd64\n' "$pkg" \
        > "$root/DEBIAN/control"
    printf 'Maintainer: fixture\nDescription: fixture\nInstalled-Size: 1\n' \
        >> "$root/DEBIAN/control"
    local req=(
        lib/firmware/fantgpu/fh2m/fh2m.fw
        lib/firmware/fantgpu/fh2m/fh2m.sh
        lib/firmware/fantgpu/fh2m/fh2c.fw
        lib/firmware/fantgpu/fh2m/fh2c.sh
        usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so
        usr/lib/x86_64-linux-gnu/dri/fh2m_drv_video.so
        usr/lib/x86_64-linux-gnu/gbm/fh2m_gbm.so
        usr/lib/xorg/modules/drivers/fh2m_drv.so
        usr/share/glvnd/egl_vendor.d/00_fh2m.json
        etc/vulkan/icd.d/fh2m_conf.json
        etc/OpenCL/vendors/FANT_fh2m.icd
        usr/share/drirc.d/01-fh2m_drv.conf
        etc/modprobe.d/blacklist-fh2m.conf
        usr/share/X11/xorg.conf.d/10-fh2m.conf
        etc/ld.so.conf.d/0-fantgpu-hwgl.conf
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libfh2m_gbm.so.1.0.0
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libgbm.so.1.0.0
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libVK_FANT_fh2m.so.1
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libEGL_fh2m.so.1
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libEGL_fh2m_l.so.0.0.0
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libFTOCL_fh2m.so.1
    )
    # 完整链接链成员（codex 复审 P1：配置/ICD 引用名 + 各 symlink 必须与
    # 真品一致；fixture 按「名 目标」成对创建）。
    local links=(
        "libfh2m_gbm.so libfh2m_gbm.so.1"
        "libfh2m_gbm.so.1 libfh2m_gbm.so.1.0.0"
        "libgbm.so libgbm.so.1"
        "libgbm.so.1 libgbm.so.1.0.0"
        "libVK_FANT_fh2m.so libVK_FANT_fh2m.so.1"
        "libEGL_fh2m.so libEGL_fh2m.so.1"
        "libEGL_fh2m.so.0 libEGL_fh2m.so"
        "libEGL_fh2m_l.so libEGL_fh2m_l.so.0"
        "libEGL_fh2m_l.so.0 libEGL_fh2m_l.so.0.0.0"
        "libFTOCL_fh2m.so libFTOCL_fh2m.so.1"
    )
    local p
    for p in "${req[@]}"; do
        mkdir -p "$root/$(dirname "$p")"
        touch "$root/$p"
    done
    # ELF 真身条目写入 \x7fELF magic（codex 复审#2 P1：空文件必须被内容断言拒绝）
    # gbm/fh2m_gbm.so 与真品一致是 M2 shim symlink，不入 ELF 清单（见 links）。
    local elf f
    for f in \
        usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so \
        usr/lib/x86_64-linux-gnu/dri/fh2m_drv_video.so \
        usr/lib/xorg/modules/drivers/fh2m_drv.so \
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libfh2m_gbm.so.1.0.0 \
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libgbm.so.1.0.0 \
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libVK_FANT_fh2m.so.1 \
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libEGL_fh2m.so.1 \
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libEGL_fh2m_l.so.0.0.0 \
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libFTOCL_fh2m.so.1
    do
        printf '\177ELF' > "$root/$f"
    done
    rm -f "$root/usr/lib/x86_64-linux-gnu/gbm/fh2m_gbm.so"
    ln -s ../fantgpu-fh2m/libfh2m_gbm.so "$root/usr/lib/x86_64-linux-gnu/gbm/fh2m_gbm.so"
    # 配置引用 token 与真品一致（codex 复审#2 P1：内容必须实际引用对应库）
    printf '%s\n' '{' '    "ICD": {' \
        '        "library_path": "libVK_FANT_fh2m.so"' '    }' '}' \
        > "$root/etc/vulkan/icd.d/fh2m_conf.json"
    printf '%s\n' '{' '    "ICD" : {' \
        '        "library_path" : "libEGL_fh2m.so.0"' '    }' '}' \
        > "$root/usr/share/glvnd/egl_vendor.d/00_fh2m.json"
    printf '%s\n' 'libFTOCL_fh2m.so' \
        > "$root/etc/OpenCL/vendors/FANT_fh2m.icd"
    local pair name target
    for pair in "${links[@]}"; do
        name="${pair%% *}"; target="${pair#* }"
        ln -sfn "$target" "$root/usr/lib/x86_64-linux-gnu/fantgpu-fh2m/$name"
    done
    if [[ "$mode" == dirtype ]]; then
        rm -f "$root/usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so"
        mkdir -p "$root/usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so"
    elif [[ "$mode" == dangling ]]; then
        # 链截断：target 正确但链末真身缺失 → 悬空（非内容断言条目）
        rm -f "$root/usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libgbm.so.1.0.0"
    elif [[ "$mode" == droplink ]]; then
        rm -f "$root/usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libVK_FANT_fh2m.so"
    elif [[ "$mode" == emptyelf ]]; then
        : > "$root/usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so"
    elif [[ "$mode" == badcfg ]]; then
        printf '%s\n' '{"ICD": {"library_path": "libSOMETHING_ELSE.so"}}' \
            > "$root/etc/vulkan/icd.d/fh2m_conf.json"
    elif [[ "$mode" == elflink ]]; then
        rm -f "$root/usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libVK_FANT_fh2m.so.1"
        ln -s libEGL_fh2m.so.1 \
            "$root/usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libVK_FANT_fh2m.so.1"
    elif [[ "$mode" == cfglink ]]; then
        rm -f "$root/etc/vulkan/icd.d/fh2m_conf.json"
        ln -s ../../../usr/share/glvnd/egl_vendor.d/00_fh2m.json \
            "$root/etc/vulkan/icd.d/fh2m_conf.json"
    elif [[ "$mode" == wrongtarget ]]; then
        # 有效 ELF 但 target 错误（codex 复审#4 P1）
        rm -f "$root/usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libVK_FANT_fh2m.so"
        ln -s libEGL_fh2m.so.1 \
            "$root/usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libVK_FANT_fh2m.so"
    elif [[ "$mode" == plainlink ]]; then
        # 链成员被替换为普通文件（codex 复审#4 P1）
        rm -f "$root/usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libVK_FANT_fh2m.so"
        touch "$root/usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libVK_FANT_fh2m.so"
    fi
    local h
    mkdir -p "$root/usr/share/$pkg"
    for h in restore-dp1-mode-x11.sh xdisplay-session.sh install-xdisplay-user.sh; do
        if [[ "$mode" == rawhelper && "$h" == restore-dp1-mode-x11.sh ]]; then
            printf '%s\n' '#!/bin/bash' 'innogpu-repair-dri-nodes placeholder' \
                > "$root/usr/share/$pkg/$h"
        else
            bash "$ROOT/tools/transform-fantgpu-helper.sh" < "$ROOT/scripts/$h" \
                > "$root/usr/share/$pkg/$h"
        fi
        chmod 0755 "$root/usr/share/$pkg/$h"
    done
    if [[ "$mode" == forbidden ]]; then
        mkdir -p "$root/usr/lib/x86_64-linux-gnu/dri"
        touch "$root/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so"
    fi
    dpkg-deb --root-owner-group -b "$root" "$TMP/$name.deb" >/dev/null
    echo "DEB=$TMP/$name.deb"
}

# t10 F 正向：完整 F required 载荷 + 变换后 helper → PASS_RELEASE_PACKAGE_BOUNDARIES
eval "$(mkfdeb t10 full)"
run_gate "$DEB"
if [ "$RC" -eq 0 ] && grep -Fq "RESULT: PASS_RELEASE_PACKAGE_BOUNDARIES" "$TMP/g.out"; then
    ok t10
else
    bad t10 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t11 F 负向：F 包混入 O loader → forbidden 拒绝
eval "$(mkfdeb t11 forbidden)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "forbidden release file is present" "$TMP/g.err"; then
    ok t11
else
    bad t11 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t12 F 负向：helper 未变换（含 innogpu-repair-dri-nodes token）→ 变换对照拒绝
eval "$(mkfdeb t12 rawhelper)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "differs from transformed source" "$TMP/g.err"; then
    ok t12
else
    bad t12 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t13 F 负向：required 路径是目录（codex 初审 P1-2）→ 类型拒绝
eval "$(mkfdeb t13 dirtype)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "required release file is a directory" "$TMP/g.err"; then
    ok t13
else
    bad t13 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t14 F 负向：required 链截断悬空（codex 初审 P1-2）→ 链接有效性拒绝
eval "$(mkfdeb t14 dangling)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "required release file is a dangling symlink" "$TMP/g.err" \
   && grep -Fq 'libgbm.so.1 ->' "$TMP/g.err"; then
    ok t14
else
    bad t14 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t15 F 负向：删除配置引用的链接链成员（codex 复审 P1：删 libVK_FANT_fh2m.so
# 后旧门禁仍 PASS，现必须 FAIL）→ missing 拒绝
eval "$(mkfdeb t15 droplink)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "required release file is missing" "$TMP/g.err" \
   && grep -Fq "libVK_FANT_fh2m.so" "$TMP/g.err"; then
    ok t15
else
    bad t15 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t16 F 负向：ELF 真身为空文件（codex 复审#2 P1）→ 内容断言拒绝
eval "$(mkfdeb t16 emptyelf)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "required release file is not an ELF object" "$TMP/g.err" \
   && grep -Fq "fh2m_dri.so" "$TMP/g.err"; then
    ok t16
else
    bad t16 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t17 F 负向：配置内容未引用对应库（codex 复审#2 P1）→ 引用断言拒绝
eval "$(mkfdeb t17 badcfg)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "does not reference" "$TMP/g.err" \
   && grep -Fq "fh2m_conf.json" "$TMP/g.err"; then
    ok t17
else
    bad t17 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t18 F 负向：ELF 真身被替换为有效 symlink（codex 复审#3 P1）→ 强制普通文件拒绝
eval "$(mkfdeb t18 elflink)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "must be a regular file, not a symlink" "$TMP/g.err" \
   && grep -Fq "libVK_FANT_fh2m.so.1" "$TMP/g.err"; then
    ok t18
else
    bad t18 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t19 F 负向：配置被替换为有效 symlink（codex 复审#3 P1）→ 强制普通文件拒绝
eval "$(mkfdeb t19 cfglink)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "must be a regular file, not a symlink" "$TMP/g.err" \
   && grep -Fq "fh2m_conf.json" "$TMP/g.err"; then
    ok t19
else
    bad t19 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t20 F 负向：链成员 target 为包内合法 ELF 但错误（codex 复审#4 P1）→ 目标映射拒绝
eval "$(mkfdeb t20 wrongtarget)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "symlink target mismatch" "$TMP/g.err" \
   && grep -Fq 'libVK_FANT_fh2m.so ->' "$TMP/g.err"; then
    ok t20
else
    bad t20 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

# t21 F 负向：链成员被替换为普通文件（codex 复审#4 P1）→ 类型拒绝
eval "$(mkfdeb t21 plainlink)"
run_gate "$DEB"
if [ "$RC" -eq 1 ] && grep -Fq "must be a symlink, found regular file" "$TMP/g.err" \
   && grep -Fq "libVK_FANT_fh2m.so" "$TMP/g.err"; then
    ok t21
else
    bad t21 "rc=$RC err=[$(head -1 "$TMP/g.err")]"
fi

echo "PASS=$PASS FAIL=$FAILN"
[ "$FAILN" -eq 0 ] || exit 1
exit 0
