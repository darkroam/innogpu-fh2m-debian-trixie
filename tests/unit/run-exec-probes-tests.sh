#!/bin/bash
# Unit tests: Vulkan/OpenCL execution probe compile + failure paths.
# CI-safe without /dev/dri: missing loader -> rc=2, no GPU device -> rc=3,
# both interpretable (never a hardware PASS). Each probe exec runs at most
# once; outputs are reused across assertions to keep the suite fast.

set -u -o pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
runtime="$(mktemp -d "${TMPDIR:-/tmp}/inno-exec-probes.XXXXXX")"
trap 'rm -rf "$runtime"' EXIT

PVK="$runtime/probe-vulkan"
POCL="$runtime/probe-opencl"

passed=0; failed=0; t=0
pass() { passed=$((passed+1)); printf 'exec_probes_t%02d=PASS # %s\n' "$passed" "$1"; }
fail() { failed=$((failed+1)); printf 'exec_probes_t%02d=FAIL reason=%s\n' "$passed" "$2"; }

# 1/2. 编译
gcc -O2 -Wall -o "$PVK" "$ROOT/tools/probe-vulkan-devices.c" -ldl 2> "$runtime/vk-cc.log"
t=$((t+1)); if [ -x "$PVK" ]; then pass vk_compiles; else fail vk_compiles "gcc vulkan failed: $(head -2 "$runtime/vk-cc.log")"; fi
gcc -O2 -Wall -o "$POCL" "$ROOT/tools/probe-opencl-devices.c" -ldl 2> "$runtime/ocl-cc.log"
t=$((t+1)); if [ -x "$POCL" ]; then pass ocl_compiles; else fail ocl_compiles "gcc opencl failed: $(head -2 "$runtime/ocl-cc.log")"; fi

# 3. 缺失 loader -> rc=2（dlopen 立即失败，很快）
PROBE_VULKAN_LOADER=/nonexistent/libvulkan.so timeout 10 "$PVK" exec >/dev/null 2>&1
rc=$?; t=$((t+1)); if [ "$rc" -eq 2 ]; then pass vk_missing_loader; else fail vk_missing_loader "rc=$rc"; fi
PROBE_OPENCL_LOADER=/nonexistent/libOpenCL.so timeout 10 "$POCL" exec >/dev/null 2>&1
rc=$?; t=$((t+1)); if [ "$rc" -eq 2 ]; then pass ocl_missing_loader; else fail ocl_missing_loader "rc=$rc"; fi

# 4. 无 /dev/dri 环境 -> 可解释非 PASS（rc=3），不伪造硬件 PASS。
#    每个探针 exec 仅运行一次，输出复用于后续断言。
timeout 15 "$PVK" exec > "$runtime/vk.out" 2>&1; VK_RC=$?
t=$((t+1))
if [ "$VK_RC" -eq 3 ] && grep -q 'vulkan_exec_.*fail' "$runtime/vk.out"; then
    pass vk_no_device_interpretable
else
    fail vk_no_device_interpretable "rc=$VK_RC out=$(tail -2 "$runtime/vk.out" | tr '\n' ';')"
fi
timeout 15 "$POCL" exec > "$runtime/ocl.out" 2>&1; OCL_RC=$?
t=$((t+1))
if [ "$OCL_RC" -eq 3 ] && grep -q 'opencl_exec_.*fail' "$runtime/ocl.out"; then
    pass ocl_no_device_interpretable
else
    fail ocl_no_device_interpretable "rc=$OCL_RC out=$(tail -2 "$runtime/ocl.out" | tr '\n' ';')"
fi

# 5. 枚举模式仍可用（只读、rc=0），各运行一次
timeout 15 "$PVK" > "$runtime/vk-enum.out" 2>&1; rc=$?
t=$((t+1)); if [ "$rc" -eq 0 ]; then pass vk_enumeration_runs; else fail vk_enumeration_runs "rc=$rc"; fi
timeout 15 "$POCL" > "$runtime/ocl-enum.out" 2>&1; rc=$?
t=$((t+1)); if [ "$rc" -eq 0 ]; then pass ocl_enumeration_runs; else fail ocl_enumeration_runs "rc=$rc"; fi

# 6. 超时/异常退出不留后台进程与临时文件（复用 4 的 exec 运行）
t=$((t+1))
if ! pgrep -f 'probe-vulkan-devices|probe-opencl-devices' >/dev/null 2>&1; then
    pass exec_no_leftover_process
else
    fail exec_no_leftover_process "lingering probe process"
fi
t=$((t+1))
leftovers="$(find "$runtime" -maxdepth 1 -type f ! -name 'probe-*' ! -name '*.out' ! -name '*.log' 2>/dev/null)"
if [ -z "$leftovers" ]; then pass exec_no_temp_files; else fail exec_no_temp_files "unexpected files: $leftovers"; fi

# 7. 机器可读输出格式（复用 4 的输出）
t=$((t+1))
if grep -qE 'vulkan_exec_[a-z_]+=(ok|fail)' "$runtime/vk.out"; then pass vk_machine_readable; else fail vk_machine_readable "no vulkan_exec_ lines"; fi
t=$((t+1))
if grep -qE 'opencl_exec_[a-z_]+=(ok|fail)' "$runtime/ocl.out"; then pass ocl_machine_readable; else fail ocl_machine_readable "no opencl_exec_ lines"; fi

printf 'tests_total=%d tests_passed=%d tests_failed=%d tests_skipped=0\n' "$t" "$passed" "$failed"
[ "$failed" -eq 0 ]
