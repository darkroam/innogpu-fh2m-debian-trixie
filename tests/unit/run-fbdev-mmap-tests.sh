#!/usr/bin/env bash
# tests/unit/run-fbdev-mmap-tests.sh — F4 新探针 probe-fbdev-mmap.c 静态自测
#
# 契约（validation-plan §〇 F4 自测行 + F4 覆盖边界声明）：夹具构建
# （-DINNOGPU_DMABUF_FIXTURE_HOOKS + INNOGPU_FBDEV_FIXTURE=1）下控制流全走通、
# 恢复逐字节一致、越界拒绝、输出全部 fixture_ 命名空间（绝不产出真机权威行）；
# 生产构建仅编译验证（不得在无设备机上运行生产路径——/dev/fb0 存在时绝不写入）。
# 退出码：0=全过 1=用例失败 2=环境错误。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SRC="$ROOT/tools/probe-fbdev-mmap.c"
cd "$ROOT"
export LC_ALL=C

[ -f "$SRC" ] || { echo "FATAL: probe-fbdev-mmap.c not found" >&2; exit 2; }
command -v gcc >/dev/null 2>&1 || { echo "FATAL: gcc required" >&2; exit 2; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/fbmmap-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT INT TERM HUP

PASS=0; FAILN=0
ok()  { PASS=$((PASS + 1)); echo "ok  $1"; }
bad() { FAILN=$((FAILN + 1)); echo "BAD $1: $2" >&2; }

# 夹具构建
gcc -O2 -Wall -Wextra -DINNOGPU_DMABUF_FIXTURE_HOOKS -o "$TMP/probe-fx" "$SRC" 2>"$TMP/cc-fx.err" \
    || { echo "FATAL: fixture build failed"; cat "$TMP/cc-fx.err" >&2; exit 2; }
# 生产构建（仅编译，绝不运行）
gcc -O2 -Wall -Wextra -o "$TMP/probe-prod" "$SRC" 2>"$TMP/cc-prod.err" \
    || { echo "FATAL: production build failed"; cat "$TMP/cc-prod.err" >&2; exit 2; }
if [[ -s "$TMP/cc-fx.err" || -s "$TMP/cc-prod.err" ]]; then
    echo "WARNING: compiler warnings present" >&2
fi

# t01 夹具运行：控制流全走通 + 全 PASS
set +e
INNOGPU_FBDEV_FIXTURE=1 "$TMP/probe-fx" > "$TMP/fx.out" 2> "$TMP/fx.err"
RC=$?
set -e
if [ "$RC" -eq 0 ] \
   && grep -Fq 'fixture_fbdev_mmap_write_readback=PASS' "$TMP/fx.out" \
   && grep -Fq 'fixture_fbdev_mmap_restore=PASS' "$TMP/fx.out" \
   && grep -Fq 'original_bytes_match=yes' "$TMP/fx.out" \
   && grep -Fq 'fixture_fbdev_mmap_bounds=PASS cases=3' "$TMP/fx.out" \
   && grep -Fq 'fixture_fbdev_mmap_cleanup=PASS abnormal_exit_restore_verified' "$TMP/fx.out" \
   && grep -Fq 'fixture_fbdev_mmap_overall=PASS' "$TMP/fx.out"; then
    ok t01
else
    bad t01 "rc=$RC out=[$(tail -2 "$TMP/fx.out" | tr '\n' ';')]"
fi

# t02 命名空间：夹具输出绝不产出无前缀权威行（覆盖边界声明）
if grep -q 'fbdev_mmap' "$TMP/fx.out" "$TMP/fx.err" \
   && ! grep -E 'fbdev_mmap' "$TMP/fx.out" "$TMP/fx.err" | grep -qv 'fixture_fbdev_mmap'; then
    ok t02
else
    bad t02 "unprefixed authoritative line leaked from fixture build"
fi

# t03 夹具构建无环境变量 → 不进入夹具路径（权威命名空间 open_failed 或安全失败）
# 仅在 /dev/fb0 不存在时执行（存在时绝不运行生产路径写真实 framebuffer）
if [[ ! -e /dev/fb0 ]]; then
    set +e
    "$TMP/probe-prod" > "$TMP/prod.out" 2> "$TMP/prod.err"
    RC3=$?
    set -e
    if [ "$RC3" -eq 1 ] && grep -Fq 'fbdev_mmap=FAIL reason=open_failed' "$TMP/prod.err" \
       && ! grep -q 'fixture_fbdev_mmap' "$TMP/prod.out" "$TMP/prod.err"; then
        ok t03
    else
        bad t03 "rc=$RC3 err=[$(head -1 "$TMP/prod.err")]"
    fi
else
    ok t03   # /dev/fb0 存在：跳过生产路径执行（保护真实 framebuffer），编译已验
fi

# t04 静态：非破坏性契约要素（FBIOGET_FSCREENINFO/保存/恢复/atexit 兜底/
# 异步信号安全处理器——处理器仅置 sig_atomic_t 标志，绝不调用 cleanup）
if grep -Fq 'FBIOGET_FSCREENINFO' "$SRC" \
   && grep -Fq 'atexit(cleanup)' "$SRC" \
   && grep -Fq 'signal(SIGINT, on_signal)' "$SRC" \
   && grep -Fq 'memcpy(mapped + map_off, saved, map_len)' "$SRC" \
   && grep -Fq 'msync(mapped, map_len, MS_SYNC)' "$SRC" \
   && grep -Fq 'restore_bytes_mismatch' "$SRC" \
   && grep -Fq 'bounds_violation' "$SRC" \
   && grep -Fq 'static volatile sig_atomic_t caught_signal' "$SRC" \
   && sed -n '/static void on_signal/,/^}/p' "$SRC" | grep -Fq 'caught_signal = sig;' \
   && ! sed -n '/static void on_signal/,/^}/p' "$SRC" | grep -Eq 'cleanup\(|memcpy|msync|munmap|free\(' \
   && grep -Fq 'return 128 + (int)caught_signal' "$SRC"; then
    ok t04
else
    bad t04 "non-destructive contract elements missing"
fi

# t05 静态：单一源码双构建（夹具宏守卫 + 环境变量开关，生产构建宏无效）
if grep -Fq '#ifdef INNOGPU_DMABUF_FIXTURE_HOOKS' "$SRC" \
   && grep -Fq 'getenv("INNOGPU_FBDEV_FIXTURE")' "$SRC" \
   && grep -Fq '#define do_mmap mmap' "$SRC" \
   && ! grep -Fq 'LD_PRELOAD' "$SRC"; then
    ok t05
else
    bad t05 "dual-build fixture macro convention missing"
fi

# t06 静态：全页读回校验（codex 初审 P2-6；不得只校验 pattern 长度子集）
if grep -Fq 'for (size_t i = 0; i < len && !mismatch; i++)' "$SRC" \
   && grep -Fq 'page[i] != pattern[i & 0xFF]' "$SRC" \
   && ! grep -Fq 'memcmp(page, pattern, sizeof(pattern))' "$SRC"; then
    ok t06
else
    bad t06 "full-page readback verify missing"
fi

# t07 负向：后半页篡改注入（夹具 CORRUPT 钩子）→ 读回 FAIL
set +e
INNOGPU_FBDEV_FIXTURE=1 INNOGPU_FBDEV_FIXTURE_CORRUPT=1 "$TMP/probe-fx" \
    > "$TMP/corrupt.out" 2> "$TMP/corrupt.err"
RC7=$?
set -e
if [ "$RC7" -eq 1 ] && grep -Fq 'write_readback_mismatch' "$TMP/corrupt.err"; then
    ok t07
else
    bad t07 "rc=$RC7 err=[$(head -1 "$TMP/corrupt.err")]"
fi

# t08 真实信号中断（codex 复审 P1-4）：脏窗口持留钩子（hold 标记文件 +
# 10ms 步进等待）→ 测试进程发 SIGTERM → 处理器仅置标志、主流程检查点
# 恢复测试页并以 128+sig 退出；断言 rc=143 + INTERRUPTED page_restored=yes +
# 无读回/恢复失配
rm -f "$TMP/hold"
set +e
INNOGPU_FBDEV_FIXTURE=1 INNOGPU_FBDEV_FIXTURE_HOLD_MS=5000 \
    INNOGPU_FBDEV_FIXTURE_HOLD_FILE="$TMP/hold" \
    "$TMP/probe-fx" > "$TMP/sig.out" 2> "$TMP/sig.err" &
SIGPID=$!
for _ in $(seq 1 250); do
    [ -f "$TMP/hold" ] && break
    sleep 0.02
done
kill -TERM "$SIGPID" 2>/dev/null || true
wait "$SIGPID" 2>/dev/null
RC8=$?
set -e
if [ "$RC8" -eq 143 ] \
   && grep -Fq 'fixture_fbdev_mmap=INTERRUPTED signal=15 page_restored=yes' "$TMP/sig.err" \
   && ! grep -Fq 'write_readback_mismatch' "$TMP/sig.err" \
   && ! grep -Fq 'restore_bytes_mismatch' "$TMP/sig.err"; then
    ok t08
else
    bad t08 "rc=$RC8 hold=[$([ -f "$TMP/hold" ] && echo yes || echo no)] err=[$(tr '\n' ';' < "$TMP/sig.err")]"
fi

echo "PASS=$PASS FAIL=$FAILN"
[ "$FAILN" -eq 0 ] || exit 1
exit 0
