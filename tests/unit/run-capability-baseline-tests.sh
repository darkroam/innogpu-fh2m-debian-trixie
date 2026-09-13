#!/usr/bin/env bash
# tests/unit/run-capability-baseline-tests.sh — F1 lineage 参数化单测
#
# 契约（validation-plan §〇 F1 自测行）：fake root fixture 以 lineage=fantgpu
# 参数族生成假包名/模块名/sysfs/proc 路径 → 断言全部走 fantgpu 路径（用
# innogpu 诱饵路径反向证明：诱饵 firmware_en=0、fantgpu=1，PASS 即证明读的
# 是 fantgpu 路径）；非法 lineage / 非法 fake root fail-closed。
# 退出码：0=全过 1=用例失败 2=环境错误。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BASELINE="$ROOT/tests/runtime/run-capability-baseline.sh"
cd "$ROOT"
export LC_ALL=C

[ -f "$BASELINE" ] || { echo "FATAL: baseline script not found" >&2; exit 2; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/cb-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT INT TERM HUP

PASS=0; FAILN=0
ok()  { PASS=$((PASS + 1)); echo "ok  $1"; }
bad() { FAILN=$((FAILN + 1)); echo "BAD $1: $2" >&2; }

mkdir -p "$TMP/bin" "$TMP/baselines"
cat > "$TMP/bin/dpkg-query" <<'EOF'
#!/bin/bash
case " $* " in
    *" fantgpu-fh2m-trixie "*) echo "5.0.0-i2" ;;
    *" innogpu-fh2m-trixie "*) echo "4.0.2-i3" ;;
    *) exit 1 ;;
esac
EOF
cat > "$TMP/bin/lspci" <<'EOF'
#!/bin/bash
exit 0
EOF
cat > "$TMP/bin/journalctl" <<'EOF'
#!/bin/bash
exit 0
EOF
cat > "$TMP/bin/dkms" <<'EOF'
#!/bin/bash
exit 0
EOF
cat > "$TMP/bin/modinfo" <<'EOF'
#!/bin/bash
exit 0
EOF
chmod 0755 "$TMP/bin"/*

make_fakeroot() {  # make_fakeroot <dir> <lineage-good: fantgpu|innogpu>
    local root="$1" good="$2" other
    if [[ "$good" == "fantgpu" ]]; then other="innogpu"; else other="fantgpu"; fi
    mkdir -p "$root/sys/module/$good/parameters" \
             "$root/sys/module/$other/parameters" \
             "$root/proc/driver/$good/gpu00" \
             "$root/dev/dri"
    printf '1\n' > "$root/sys/module/$good/parameters/firmware_en"
    printf '0\n' > "$root/sys/module/$other/parameters/firmware_en"
    printf 'Driver Status: OK\nFirmware Status: OK\nServer Errors: 0\n' \
        > "$root/proc/driver/$good/gpu00/status"
    touch "$root/dev/dri/card0" "$root/dev/dri/renderD128"
    touch "$root/dev/fb0"
}

run_baseline() {  # run_baseline <lineage> <expect_pkg> <fakeroot>
    set +e
    INNOGPU_LINEAGE="$1" INNOGPU_EXPECT_PKG="$2" INNOGPU_FAKE_ROOT="$3" \
        RUNTIME_BASELINE_DIR="$TMP/baselines" PATH="$TMP/bin:$PATH" \
        bash "$BASELINE" > "$TMP/b.out" 2> "$TMP/b.err"
    RC=$?
    set -e
    OUTTEXT="$(cat "$TMP/b.out")"
    ERRTEXT="$(cat "$TMP/b.err")"
}

# t01 fantgpu lineage：模块/固件参数/proc/包版本全部走 fantgpu 路径（innogpu 诱饵=0）
make_fakeroot "$TMP/fk-fant" fantgpu
run_baseline fantgpu 5.0.0-i2 "$TMP/fk-fant"
if grep -Fq 'runtime_module_loaded=PASS' "$TMP/b.out" \
   && grep -Fq 'runtime_module_param_firmware_en=PASS' "$TMP/b.out" \
   && grep -Fq 'runtime_proc_driver_status=PASS' "$TMP/b.out" \
   && grep -Fq 'runtime_proc_firmware_status=PASS' "$TMP/b.out" \
   && grep -Fq 'runtime_package_version=PASS' "$TMP/b.out" \
   && grep -Fq 'runtime_dmabuf_source_fix_present=UNVERIFIED' "$TMP/b.out" \
   && grep -Fq 'runtime_drm_nodes=PASS' "$TMP/b.out" \
   && grep -Fq 'runtime_fbdev_node=PASS' "$TMP/b.out"; then
    ok t01
else
    bad t01 "rc=$RC out=[$(grep -E 'runtime_(module|proc|package|dmabuf|drm_nodes|fbdev)' "$TMP/b.out" | tr '\n' ';')]"
fi

# t02 innogpu lineage 缺省回归：同样断言走 innogpu 路径（fantgpu 诱饵=0）
make_fakeroot "$TMP/fk-inno" innogpu
run_baseline innogpu 4.0.2-i3 "$TMP/fk-inno"
if grep -Fq 'runtime_module_loaded=PASS' "$TMP/b.out" \
   && grep -Fq 'runtime_module_param_firmware_en=PASS' "$TMP/b.out" \
   && grep -Fq 'runtime_proc_driver_status=PASS' "$TMP/b.out" \
   && grep -Fq 'runtime_package_version=PASS' "$TMP/b.out" \
   && grep -Fq 'runtime_dmabuf_source_fix_present=PASS' "$TMP/b.out"; then
    ok t02
else
    bad t02 "rc=$RC out=[$(grep -E 'runtime_(module|proc|package|dmabuf)' "$TMP/b.out" | tr '\n' ';')]"
fi

# t03 非法 lineage 值 → fail-closed rc=2
set +e
INNOGPU_LINEAGE=bogus INNOGPU_FAKE_ROOT="$TMP/fk-fant" \
    RUNTIME_BASELINE_DIR="$TMP/baselines" bash "$BASELINE" >/dev/null 2>"$TMP/t03.err"
RC3=$?
set -e
if [ "$RC3" -eq 2 ] && grep -Fq 'INNOGPU_LINEAGE must be innogpu or fantgpu' "$TMP/t03.err"; then
    ok t03
else
    bad t03 "rc=$RC3 err=[$(head -1 "$TMP/t03.err")]"
fi

# t04 非法 fake root（相对路径）→ fail-closed rc=2
set +e
INNOGPU_LINEAGE=fantgpu INNOGPU_FAKE_ROOT=relative/path \
    RUNTIME_BASELINE_DIR="$TMP/baselines" bash "$BASELINE" >/dev/null 2>"$TMP/t04.err"
RC4=$?
set -e
if [ "$RC4" -eq 2 ] && grep -Fq 'INNOGPU_FAKE_ROOT must be an existing absolute directory' "$TMP/t04.err"; then
    ok t04
else
    bad t04 "rc=$RC4 err=[$(head -1 "$TMP/t04.err")]"
fi

# t05 静态：fantgpu 参数族默认值就位（权威映射表）
if grep -Fq 'PKG_NAME="fantgpu-fh2m-trixie"' "$BASELINE" \
   && grep -Fq 'PCI_DRV="fant-drv"' "$BASELINE" \
   && grep -Fq 'DKMS_NAME="fantgpu-fh2m-kernel"' "$BASELINE" \
   && grep -Fq 'PROC_DIR="/proc/driver/fantgpu"' "$BASELINE" \
   && grep -Fq 'SYS_MOD="/sys/module/fantgpu"' "$BASELINE"; then
    ok t05
else
    bad t05 "fantgpu lineage family defaults missing"
fi

# t06 负向：fake root 的 /dev/dri 为空目录 → drm_nodes SKIP（反证不读宿主节点；
# 若读宿主 /dev/dri 可能误报 PASS/FAIL）
mkdir -p "$TMP/fk-empty/dev/dri"
make_fakeroot "$TMP/fk-empty" fantgpu
rm -f "$TMP/fk-empty/dev/dri"/*
run_baseline fantgpu 5.0.0-i2 "$TMP/fk-empty"
if grep -Fq 'runtime_drm_nodes=SKIP' "$TMP/b.out"; then
    ok t06
else
    bad t06 "out=[$(grep 'runtime_drm_nodes' "$TMP/b.out")]"
fi

# t07 静态：INNOGPU_LIB_PATH 血统分支（F 血统指向包内 fantgpu-fh2m 目录）
if grep -Fq 'EGL_LIB_PATH="/usr/lib/x86_64-linux-gnu/fantgpu-fh2m"' "$BASELINE" \
   && grep -Fq 'INNOGPU_LIB_PATH="$EGL_LIB_PATH"' "$BASELINE"; then
    ok t07
else
    bad t07 "lineage-aware lib path missing"
fi

echo "PASS=$PASS FAIL=$FAILN"
[ "$FAILN" -eq 0 ] || exit 1
exit 0
