#!/bin/bash
# DMA-BUF regression aggregator for FH2M (Innosilicon 1ec8:9810), ~/7.md.
#
# Runs the same-device DRM PRIME self-import probe, the private invisible GEM
# READ/WRITE(+verify) probe, and the vblank sync probes, gated by a strict
# Driver/Firmware double-snapshot status check. Result namespace dmabuf_*;
# fixture mode uses the independent fixture_dmabuf_* / fixture_tests_* namespace
# and never emits an authoritative dmabuf_* line.
#
# Capability boundaries (never over-claimed):
#   - self-import: same innogpu device PRIME export/import only;
#   - foreign import / cross-device GTT export / V4L2 / second GPU stay UNVERIFIED;
#   - invisible GEM READ/WRITE is PDP-specific, not a DMA-BUF lifecycle test;
#   - vblank sync is a separate sub-item, not DMA-BUF evidence.
#
# Usage:
#   bash tools/run-dmabuf-regression-test.sh [--render-device NODE] [--card-device NODE]
#       [--size BYTES] [--iterations N] [--vblank-samples N] [--timeout SEC]
#       [--read-munmap-limit-ms MS]
#
# Exit codes: 0=PASS 1=subitem/status FAIL 2=arg/fixture/tool/compile error or overall SKIP
#            3=device/capability missing or overall UNVERIFIED 5=timeout/cleanup FAIL
#
# Fixture hooks (require INNOGPU_DMABUF_FIXTURE_MODE=1; never set in real runs):
#   FAKE_SYSFS_ROOT / FAKE_DEV_DIR            fake sysfs + dev node discovery roots
#   INNOGPU_DMABUF_SKIP_DEVICE_CHECKS=1       skip char-device/read/write checks
#   INNOGPU_DMABUF_STATUS_FILE=<file>         Driver/Firmware status + error counts source
#   INNOGPU_DMABUF_KERNEL_LOG=<file>          kernel-log source for the window diff
#   INNOGPU_DMABUF_FORCE_CLEANUP_FAIL=1       force final cleanup failure (FAIL/5, fixture-only)
#   PROBE_CC=<cc>                             compiler override (default gcc)
#   PROBE_SELF_IMPORT_BIN / PROBE_INVISIBLE_READ_BIN / PROBE_TOPOLOGY_BIN / PROBE_VBLANK_BIN
#                                             probe executable overrides (skip compile)

set -u -o pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RENDER_DEVICE="${RENDER_DEVICE:-}"
CARD_DEVICE="${CARD_DEVICE:-}"
SIZE=7646720
ITERATIONS=3
VBLANK_SAMPLES=10
# 真实探针（probe-drm-vblank.c）的列标题，strict 精确匹配（防接口漂移）
VBLANK_COLHDR='sample sequence wait_ms kernel_time_ms sequence_delta kernel_delta_ms result'
TIMEOUT=30
MUNMAP_LIMIT_MS=40
STATUS_FILE="${INNOGPU_DMABUF_STATUS_FILE:-/proc/driver/innogpu/gpu00/status}"
SYSFS_ROOT="${FAKE_SYSFS_ROOT:-/sys}"
DEV_DIR="${FAKE_DEV_DIR:-/dev/dri}"
PROBE_CC="${PROBE_CC:-gcc}"
PROBE_SELF_IMPORT_BIN="${PROBE_SELF_IMPORT_BIN:-}"
PROBE_INVISIBLE_READ_BIN="${PROBE_INVISIBLE_READ_BIN:-}"
PROBE_TOPOLOGY_BIN="${PROBE_TOPOLOGY_BIN:-}"
PROBE_VBLANK_BIN="${PROBE_VBLANK_BIN:-}"
KERNEL_LOG_FILE="${INNOGPU_DMABUF_KERNEL_LOG:-}"
FIXTURE_MODE=0
NS=""
FAIL_CODE=1

usage() { sed -n '2,19p' "${BASH_SOURCE[0]}" | sed 's/^# \?//'; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --render-device)
            [[ $# -ge 2 && -n "$2" ]] || { echo "--render-device needs a value" >&2; exit 2; }
            RENDER_DEVICE="$2"; shift 2 ;;
        --card-device)
            [[ $# -ge 2 && -n "$2" ]] || { echo "--card-device needs a value" >&2; exit 2; }
            CARD_DEVICE="$2"; shift 2 ;;
        --size)
            [[ $# -ge 2 && -n "$2" ]] || { echo "--size needs a value" >&2; exit 2; }
            SIZE="$2"; shift 2 ;;
        --iterations)
            [[ $# -ge 2 && -n "$2" ]] || { echo "--iterations needs a value" >&2; exit 2; }
            ITERATIONS="$2"; shift 2 ;;
        --vblank-samples)
            [[ $# -ge 2 && -n "$2" ]] || { echo "--vblank-samples needs a value" >&2; exit 2; }
            VBLANK_SAMPLES="$2"; shift 2 ;;
        --timeout)
            [[ $# -ge 2 && -n "$2" ]] || { echo "--timeout needs a value" >&2; exit 2; }
            TIMEOUT="$2"; shift 2 ;;
        --read-munmap-limit-ms)
            [[ $# -ge 2 && -n "$2" ]] || { echo "--read-munmap-limit-ms needs a value" >&2; exit 2; }
            MUNMAP_LIMIT_MS="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done
[[ "$SIZE" =~ ^[1-9][0-9]*$ && "$SIZE" -le 1073741824 ]] || { echo "--size must be a positive integer <= 1<<30" >&2; exit 2; }
[[ "$ITERATIONS" =~ ^[1-9][0-9]*$ && "$ITERATIONS" -le 100 ]] || { echo "--iterations must be a positive integer <= 100" >&2; exit 2; }
[[ "$VBLANK_SAMPLES" =~ ^[1-9][0-9]*$ && "$VBLANK_SAMPLES" -le 10000 ]] || { echo "--vblank-samples must be a positive integer <= 10000" >&2; exit 2; }
[[ "$TIMEOUT" =~ ^[1-9][0-9]*$ && "$TIMEOUT" -le 600 ]] || { echo "--timeout must be a positive integer <= 600" >&2; exit 2; }
[[ "$MUNMAP_LIMIT_MS" =~ ^[1-9][0-9]*$ && "$MUNMAP_LIMIT_MS" -le 60000 ]] || { echo "--read-munmap-limit-ms must be a positive integer <= 60000" >&2; exit 2; }

# ---- fixture 模式检测：任何注入钩子都必须显式声明，输出使用独立命名空间 ----
if [[ "${INNOGPU_DMABUF_FIXTURE_MODE:-0}" == "1" \
      || -n "${FAKE_SYSFS_ROOT:-}" || -n "${FAKE_DEV_DIR:-}" \
      || -n "${INNOGPU_DMABUF_SKIP_DEVICE_CHECKS:-}" \
      || -n "${INNOGPU_DMABUF_STATUS_FILE:-}" \
      || -n "${INNOGPU_DMABUF_KERNEL_LOG:-}" \
      || -n "${INNOGPU_DMABUF_FORCE_CLEANUP_FAIL:-}" \
      || "$PROBE_CC" != "gcc" \
      || -n "$PROBE_SELF_IMPORT_BIN" || -n "$PROBE_INVISIBLE_READ_BIN" \
      || -n "$PROBE_TOPOLOGY_BIN" || -n "$PROBE_VBLANK_BIN" ]]; then
    if [[ "${INNOGPU_DMABUF_FIXTURE_MODE:-0}" != "1" ]]; then
        echo "dmabuf_tool=fail reason=fixture_hooks_require_INNOGPU_DMABUF_FIXTURE_MODE=1" >&2
        exit 2
    fi
    FIXTURE_MODE=1
    NS="fixture_"
fi
tag() { if [[ "$FIXTURE_MODE" -eq 1 ]]; then printf '%s-mode=fixture' "$1"; else printf '%s' "$1"; fi; }
oktag() { [[ "$FIXTURE_MODE" -eq 1 ]] && printf ' mode=fixture'; }
# GNU timeout 超时退出码：124=TERM 生效，137=进程忽略 TERM 后被 SIGKILL（--kill-after）
is_timeout_rc() { [[ "$1" == "124" || "$1" == "137" ]]; }

# ---- 工具检查 (exit 2) ----
for t in timeout awk grep; do
    command -v "$t" >/dev/null 2>&1 || { echo "${NS}dmabuf_tool=fail reason=tool_missing:$t" >&2; exit 2; }
done
NEED_CC=0
[[ -z "$PROBE_SELF_IMPORT_BIN" || -z "$PROBE_INVISIBLE_READ_BIN" || -z "$PROBE_TOPOLOGY_BIN" || -z "$PROBE_VBLANK_BIN" ]] && NEED_CC=1
if [[ "$NEED_CC" -eq 1 ]]; then
    command -v "$PROBE_CC" >/dev/null 2>&1 || { echo "${NS}dmabuf_tool=fail reason=cc_missing:$PROBE_CC" >&2; exit 2; }
fi

# ---- 设备发现与身份 (exit 3) ----
pci_of() { local t; t="$(readlink -f "$SYSFS_ROOT/class/drm/$1/device" 2>/dev/null || true)"; [[ -n "$t" ]] && basename "$t" || true; }
vid_of() { cat "$SYSFS_ROOT/class/drm/$1/device/vendor" 2>/dev/null || true; }
did_of() { cat "$SYSFS_ROOT/class/drm/$1/device/device" 2>/dev/null || true; }
is_target() { [[ "$(vid_of "$1")" == "0x1ec8" && "$(did_of "$1")" == "0x9810" ]]; }

find_node() { # <render|card> -> prints node or "" ; "MULTIPLE" on ambiguity
    local kind="$1" found="" n
    if [[ -d "$DEV_DIR/by-path" ]]; then
        for l in "$DEV_DIR"/by-path/pci-*-"$kind"; do
            [[ -e "$l" ]] || continue
            n="$(basename "$(readlink "$l")")"
            is_target "$n" || continue
            [[ -n "$found" ]] && { echo "MULTIPLE"; return 1; }
            found="$DEV_DIR/$n"
        done
    fi
    if [[ -z "$found" ]]; then
        for l in "$DEV_DIR"/"$kind"*; do
            [[ -e "$l" ]] || continue
            n="$(basename "$l")"
            is_target "$n" || continue
            [[ -n "$found" ]] && { echo "MULTIPLE"; return 1; }
            found="$l"
        done
    fi
    [[ -n "$found" ]] && echo "$found" || return 1
}

NODE_RENDER="$RENDER_DEVICE"
NODE_CARD="$CARD_DEVICE"
if [[ -z "$NODE_RENDER" ]]; then
    NODE_RENDER="$(find_node render)"; FR_RC=$?
    if [[ "$FR_RC" -ne 0 ]]; then
        if [[ "$NODE_RENDER" == "MULTIPLE" ]]; then
            echo "${NS}dmabuf_device=fail reason=multiple_target_render_nodes" >&2; exit 3
        fi
        echo "${NS}dmabuf_device=fail reason=no_fh2m_render_node" >&2; exit 3
    fi
fi
if [[ -z "$NODE_CARD" ]]; then
    NODE_CARD="$(find_node card)"; FC_RC=$?
    if [[ "$FC_RC" -ne 0 ]]; then
        if [[ "$NODE_CARD" == "MULTIPLE" ]]; then
            echo "${NS}dmabuf_device=fail reason=multiple_target_card_nodes" >&2; exit 3
        fi
        echo "${NS}dmabuf_device=fail reason=no_fh2m_card_node" >&2; exit 3
    fi
fi
if [[ -z "${INNOGPU_DMABUF_SKIP_DEVICE_CHECKS:-}" ]]; then
    [[ -c "$NODE_RENDER" ]] || { echo "${NS}dmabuf_device=fail reason=render_not_char_device:$NODE_RENDER" >&2; exit 3; }
    [[ -r "$NODE_RENDER" && -w "$NODE_RENDER" ]] || { echo "${NS}dmabuf_device=fail reason=render_no_permission:$NODE_RENDER" >&2; exit 3; }
    [[ -c "$NODE_CARD" ]] || { echo "${NS}dmabuf_device=fail reason=card_not_char_device:$NODE_CARD" >&2; exit 3; }
    [[ -r "$NODE_CARD" && -w "$NODE_CARD" ]] || { echo "${NS}dmabuf_device=fail reason=card_no_permission:$NODE_CARD" >&2; exit 3; }
fi
RB="$(basename "$NODE_RENDER")"
CB="$(basename "$NODE_CARD")"
is_target "$RB" || { echo "${NS}dmabuf_device=fail reason=render_identity_mismatch:$NODE_RENDER" >&2; exit 3; }
is_target "$CB" || { echo "${NS}dmabuf_device=fail reason=card_identity_mismatch:$NODE_CARD" >&2; exit 3; }
RBDF="$(pci_of "$RB")"
CBDF="$(pci_of "$CB")"
[[ -n "$RBDF" && "$RBDF" == "$CBDF" ]] || { echo "${NS}dmabuf_device=fail reason=render_card_different_source render=$RBDF card=$CBDF" >&2; exit 3; }
echo "${NS}dmabuf_device=ok render=$NODE_RENDER card=$NODE_CARD bdf=$RBDF (1ec8:9810)$(oktag)"

# ---- Driver/Firmware 状态快照（严格整行解析 + 恰好一次 + 规范十进制；pre 无效解码前即 FAIL）----
status_snapshot() {
    local f="$1"
    [[ -r "$f" ]] || { echo "err:unreadable"; return 1; }
    awk '
        function canon(v,   s) {
            s = v
            while (length(s) > 1 && substr(s, 1, 1) == "0") s = substr(s, 2)
            return s
        }
        BEGIN {
            order[1] = "Server Errors:";      order[2] = "HWR Event Count:"
            order[3] = "CRR Event Count:";    order[4] = "SLR Event Count:"
            order[5] = "WGP Error Count:";    order[6] = "TRP Error Count:"
            order[7] = "FWF Event Count:";    order[8] = "APM Event Count:"
        }
        /^Driver Status:/ {
            if ($0 ~ /^Driver Status:[[:space:]]+OK[[:space:]]*$/) { driver++; next }
            print "err:malformed_line:" $0; err = 1; exit 1
        }
        /^Firmware Status:/ {
            if ($0 ~ /^Firmware Status:[[:space:]]+OK[[:space:]]*$/) { firmware++; next }
            print "err:malformed_line:" $0; err = 1; exit 1
        }
        {
            for (i = 1; i <= 8; i++) {
                if ($0 ~ "^" order[i]) {
                    if ($0 ~ "^" order[i] "[[:space:]]+[0-9]+[[:space:]]*$") {
                        seen[i]++; counts[i] = canon($NF); break
                    }
                    print "err:malformed_line:" $0; err = 1; exit 1
                }
            }
        }
        END {
            if (err) exit 1
            if (driver == 0) { print "err:driver_status_missing"; exit 1 }
            if (driver > 1)  { print "err:duplicate_driver_line"; exit 1 }
            if (firmware == 0) { print "err:firmware_status_missing"; exit 1 }
            if (firmware > 1)  { print "err:duplicate_firmware_line"; exit 1 }
            for (i = 1; i <= 8; i++) {
                if (!(i in seen)) { print "err:missing_count_fields:" i; exit 1 }
                if (seen[i] > 1) { print "err:duplicate_count_field:" order[i]; exit 1 }
            }
            for (i = 1; i <= 8; i++) printf "%s%s", (i > 1 ? " " : ""), counts[i]
            print ""
        }
    ' "$f"
}
PRE_SNAP="$(status_snapshot "$STATUS_FILE")"; PRE_RC=$?
if [[ "$PRE_RC" -ne 0 ]]; then
    echo "${NS}dmabuf_status=fail reason=$(tag pre_${PRE_SNAP#err:})" >&2
    echo "${NS}dmabuf_regression_overall=FAIL reason=$(tag pre_status_invalid)" >&2
    exit 1
fi

# ---- 内核日志窗口（innogpu/PVR/DRM/GPU/DMA-BUF/fence；无权限时 SKIP 元数据，不计数）----
# 来源正则：除驱动前缀外，覆盖不带 innogpu/pvr/drm 前缀的 dma_buf/dma_resv/fence/GPU 行
KLOG_SRC_RE='innogpu|pvr|drm|gpu|dma[-_]?buf|dma[-_]?resv|fence'
# 严重事件：词边界 + 词形覆盖（error/fail/fault/bug/hang/timeout/reset/oops/panic/deadlock/stall/corrupt/abort/warn/lockup/wedged）。
# 裸子串会误伤正常词：debug→bug、installed→stall、hangcheck→hang；必须 \b 边界且覆盖单复数与进行时。
KLOG_ERR_RE='\berrors?\b|\bfail(ed|ing|s|ure|ures)?\b|\bfault(s|ed|ing)?\b|\bbugs?\b|\bhangs?\b|\bhanging\b|\bhung\b|\btimeouts?\b|\btimed out\b|\breset(s|ting|ted)?\b|\boops(es)?\b|\bpanic(s|ked|king)?\b|\bdeadlocks?\b|\bstall(s|ed|ing)?\b|\bcorrupt(s|ed|ing|ion)?\b|\babort(s|ed|ing)?\b|\bwarn(s|ing|ings)?\b|\bwarn_on\b|\blockup(s)?\b|\bwedged\b'
kernel_log() { # 输出匹配来源的行；来源不可用返回 1
    if [[ -n "$KERNEL_LOG_FILE" ]]; then
        [[ -r "$KERNEL_LOG_FILE" ]] || return 1
        grep -iE "$KLOG_SRC_RE" "$KERNEL_LOG_FILE"
        return 0
    fi
    if command -v dmesg >/dev/null 2>&1 && dmesg >/dev/null 2>&1; then
        dmesg | grep -iE "$KLOG_SRC_RE"
        return 0
    fi
    return 1
}
klog_new_lines() { # <before> <after> -> after 中超出 before 的行（多集，保留重复）
    local b="$1" a="$2"
    # after 作为第一文件（NR==FNR 递增），before 递减：after 独有/超额的行才输出
    awk 'NR==FNR { c[$0]++; next } { c[$0]-- } END { for (l in c) if (c[l] > 0) for (i = 0; i < c[l]; i++) print l }' \
        <(printf '%s\n' "$a") <(printf '%s\n' "$b")
}
klog_continuous() { # <before> <after> -> 0=after 以 before 为完整前缀（append-only dmesg 快照语义）
    local b="$1" a="$2"
    [[ -n "$b" ]] || return 0   # before 为空 -> 无旧行可验证，视为连续
    [[ -n "$a" ]] || return 1   # before 非空但 after 空 -> 日志被重置 -> 不连续
    awk '
        NR==FNR { b[++nb] = $0; next }
        { a[++na] = $0 }
        END {
            if (na < nb) exit 1                        # after 比 before 短 -> 截断
            for (i = 1; i <= nb; i++)                  # before 必须是 after 的完整前缀
                if (a[i] != b[i]) exit 1               # 中间插入/重排 -> 来源被重写 -> 不连续
            exit 0
        }
    ' <(printf '%s\n' "$b") <(printf '%s\n' "$a")
}
KLOG_OK=1
KLOG_FAIL=0
KLOG_UNAVAILABLE=0
KLOG_BEFORE="$(kernel_log)" || { KLOG_OK=0; KLOG_UNAVAILABLE=1; }

# ---- 工作区与清理 ----
runtime="$(mktemp -d "${TMPDIR:-/tmp}/dmabuf-regression.XXXXXX")" || {
    echo "${NS}dmabuf_tool=fail reason=mktemp_failed" >&2; exit 2; }
CLEANED=0
CLEANUP_FAIL=0
cleanup() {
    [[ "$CLEANED" -eq 1 ]] && return
    CLEANED=1
    if [[ -n "${runtime:-}" && -d "$runtime" ]]; then
        rm -rf "$runtime" || CLEANUP_FAIL=1
    fi
}
# shellcheck disable=SC2154   # rc 在 trap 内赋值后读取，ShellCheck 无法跨 trap 关联
# shellcheck disable=SC2317   # cleanup 由 trap 间接调用
trap 'rc=$?; cleanup; if [[ "$CLEANUP_FAIL" -eq 1 && "$rc" -eq 0 ]]; then exit 5; fi' EXIT
trap 'cleanup; exit 129' HUP
trap 'cleanup; exit 130' INT
trap 'cleanup; exit 143' TERM
BUILD="$runtime/build"
mkdir -p "$BUILD"

# ---- 编译探针 (exit 2 on compile/env failure) ----
compile_probe() { # <bin-name> <source>
    local bin="$1" src="$2" rc
    timeout --kill-after=2 "$TIMEOUT" "$PROBE_CC" -std=c11 -Wall -Wextra -Werror -O2 \
        -o "$BUILD/$bin" "$ROOT/tools/$src" >/dev/null 2>"$BUILD/cc-$bin.log"; rc=$?
    if is_timeout_rc "$rc"; then
        echo "${NS}dmabuf_tool=fail reason=compile_timeout:$bin" >&2
        exit 5
    fi
    if [[ "$rc" -ne 0 ]]; then
        echo "${NS}dmabuf_tool=fail reason=compile_failed:$bin" >&2
        exit 2
    fi
}
resolve_probe() { # <bin-name> <override> -> path
    if [[ -n "$2" ]]; then echo "$2"; else echo "$BUILD/$1"; fi
}
if [[ -z "$PROBE_SELF_IMPORT_BIN" ]]; then compile_probe probe-dmabuf-self-import probe-dmabuf-self-import.c; fi
if [[ -z "$PROBE_INVISIBLE_READ_BIN" ]]; then compile_probe probe-pdp-invisible-read probe-pdp-invisible-read.c; fi
if [[ -z "$PROBE_TOPOLOGY_BIN" ]]; then compile_probe probe-drm-topology probe-drm-topology.c; fi
if [[ -z "$PROBE_VBLANK_BIN" ]]; then compile_probe probe-drm-vblank probe-drm-vblank.c; fi
P_SELF="$(resolve_probe probe-dmabuf-self-import "$PROBE_SELF_IMPORT_BIN")"
P_READ="$(resolve_probe probe-pdp-invisible-read "$PROBE_INVISIBLE_READ_BIN")"
P_TOP="$(resolve_probe probe-drm-topology "$PROBE_TOPOLOGY_BIN")"
P_VBL="$(resolve_probe probe-drm-vblank "$PROBE_VBLANK_BIN")"

# ---- 子项记录 ----
passed=0; failed=0; skipped=0; unverified=0; total=0
record_pass() { passed=$((passed+1)); total=$((total+1)); echo "${NS}dmabuf_$1=PASS reason=$(tag "$2")"; }
record_fail() { failed=$((failed+1)); total=$((total+1)); echo "${NS}dmabuf_$1=FAIL reason=$(tag "$2")" >&2; [[ "$3" -gt "$FAIL_CODE" ]] && FAIL_CODE="$3"; }
record_skip() { skipped=$((skipped+1)); total=$((total+1)); echo "${NS}dmabuf_$1=SKIP reason=$(tag "$2")"; }
record_unverified() { unverified=$((unverified+1)); total=$((total+1)); echo "${NS}dmabuf_$1=UNVERIFIED reason=$(tag "$2")"; }

num_re='^[0-9]+([.][0-9]+)?$'
valid_num() { [[ "$1" =~ $num_re ]]; }   # 有限非负十进制（%.3f 输出；拒绝 NaN/Inf/负数/指数）

# ---- 1. PRIME 同设备 self-import ----
self_import() {
    local out="$runtime/self-import.out" rc
    timeout --kill-after=2 "$TIMEOUT" "$P_SELF" "$NODE_CARD" "$SIZE" "$ITERATIONS" >"$out" 2>&1; rc=$?
    if is_timeout_rc "$rc"; then record_fail self_import timeout_self_import 5; return; fi
    if [[ "$rc" -eq 3 ]]; then
        local cap
        cap="$(grep -o 'capability=[^ ]*' "$out" | head -1 || true)"
        record_unverified self_import "capability_missing:${cap#capability=}"
        return
    fi
    if [[ "$rc" -eq 2 ]]; then record_fail self_import probe_usage_error 1; return; fi
    if [[ "$rc" -ne 0 ]]; then record_fail self_import probe_failed_rc=$rc 1; return; fi
    local line cs iter
    # 规范十进制：拒绝前导零（08/09 会被 Bash 按八进制解析导致比较失效）
    local canon='(0|[1-9][0-9]*)'
    local round_re="^round=${canon} handle=${canon} create_size=${canon} exported_fd=${canon} cloexec=yes imported_handle=${canon} imported_same=(yes|no) ok$"
    local summary_re="^summary self_import rounds=${canon} success=${canon} failures=${canon} fds_before=${canon} fds_after=${canon} fd_leak=(yes|no)$"
    local head_re="^self_import device=([^ ]+) size=${canon} iterations=${canon}$"
    local -a SEEN_ROUND=()
    # header 严格校验：device/size/iterations 与本次运行一致
    local hlines; hlines="$(grep -c '^self_import device=' "$out")"
    [[ "$hlines" -eq 1 ]] || { record_fail self_import missing_or_duplicate_header 1; return; }
    local hline; hline="$(grep '^self_import device=' "$out")"
    [[ "$hline" =~ $head_re ]] || { record_fail self_import malformed_header 1; return; }
    if [[ "${BASH_REMATCH[1]}" != "$NODE_CARD" || "${BASH_REMATCH[2]}" != "$SIZE" || "${BASH_REMATCH[3]}" != "$ITERATIONS" ]]; then
        record_fail self_import "header_mismatch dev=${BASH_REMATCH[1]} size=${BASH_REMATCH[2]} iters=${BASH_REMATCH[3]}" 1; return
    fi
    while IFS= read -r line; do
        case "$line" in
            self_import\ device=*) : ;;
            round=*)
                [[ "$line" =~ $round_re ]] || { record_fail self_import "malformed_round_line:$line" 1; return; }
                iter="${BASH_REMATCH[1]}"
                [[ "$iter" -ge 1 && "$iter" -le "$ITERATIONS" ]] || { record_fail self_import "round_out_of_range:$iter" 1; return; }
                [[ -z "${SEEN_ROUND[$iter]+x}" ]] || { record_fail self_import "duplicate_round:$iter" 1; return; }
                SEEN_ROUND[$iter]=1
                cs="${BASH_REMATCH[3]}"
                [[ -n "$cs" && "$cs" -ge "$SIZE" ]] || { record_fail self_import create_size_below_request 1; return; }
                ;;
            summary\ *) : ;;
            *) record_fail self_import unexpected_line 1; return ;;
        esac
    done < "$out"
    for ((i=1;i<=ITERATIONS;i++)); do
        [[ -n "${SEEN_ROUND[$i]+x}" ]] || { record_fail self_import "missing_round:$i" 1; return; }
    done
    # summary：恰好一行、严格格式（规范十进制）、rounds/success/failures/fd 计数全部一致
    local slines srounds succ fail fb fa leak
    slines="$(grep -c '^summary self_import ' "$out")"
    [[ "$slines" -eq 1 ]] || { record_fail self_import "summary_count=$slines" 1; return; }
    local sline; sline="$(grep '^summary self_import ' "$out")"
    [[ "$sline" =~ $summary_re ]] || { record_fail self_import malformed_summary_line 1; return; }
    srounds="${BASH_REMATCH[1]}"; succ="${BASH_REMATCH[2]}"; fail="${BASH_REMATCH[3]}"
    fb="${BASH_REMATCH[4]}"; fa="${BASH_REMATCH[5]}"; leak="${BASH_REMATCH[6]}"
    if [[ "$srounds" -ne "$ITERATIONS" || "$succ" -ne "$ITERATIONS" || "$fail" -ne 0 ]]; then
        record_fail self_import "summary_mismatch rounds=$srounds succ=$succ fail=$fail want=$ITERATIONS" 1; return
    fi
    if [[ "$fb" != "$fa" ]]; then record_fail self_import "fd_count_changed $fb->$fa" 1; return; fi
    if [[ "$leak" != "no" ]]; then record_fail self_import fd_leak_detected 1; return; fi
    record_pass self_import "rounds=$ITERATIONS-fd_cloexec=yes-fd_leak=no-fds=$fb->$fa-device=1ec8:9810"
}
# ---- 2/3. invisible GEM READ / WRITE+readback ----
# parse_invisible <out> <expected_iterations> <access> <require_verify> -> 0 ok, else prints "err:<msg>"
parse_invisible() {
    local out="$1" exp="$2" acc="$3" rv="$4" hdr_iter hdr_pages hdr_msize n
    local header line phase iter per_iter=0
    local timing_re='^iteration=([0-9][0-9]*) phase=([a-z_]+) wall_ms=[0-9]+([.][0-9]+)? user_ms=[0-9]+([.][0-9]+)? system_ms=[0-9]+([.][0-9]+)?$'
    local verify_re='^iteration=([0-9][0-9]*) verify=pass pages=[0-9][0-9]*$'
    local checksum_re='^checksum=[0-9][0-9]*$'
    local -a TOUCH_CNT=() MUNMAP_CNT=() VTOUCH_CNT=() VMUNMAP_CNT=() VERIFY_CNT=() TIMING_CNT=()
    local checksum_cnt=0
    if [[ "$rv" -eq 0 ]]; then per_iter=2; else per_iter=4; fi
    header="$(grep -c '^device=' "$out")"
    [[ "$header" -eq 1 ]] || { echo "err:header_missing_or_duplicate"; return 1; }
    # 全字段严格：device/requested_size/map_size/pages/offset/iterations/access/page_stride
    local head_re="^device=([^ ]+) handle=[0-9]+ requested_size=([0-9]+) map_size=([0-9]+) pages=([0-9]+) offset=0x[0-9a-f]+ iterations=([0-9]+) access=([a-z]+) page_stride=([0-9]+)$"
    local hline; hline="$(grep '^device=' "$out")"
    [[ "$hline" =~ $head_re ]] || { echo "err:bad_header"; return 1; }
    [[ "${BASH_REMATCH[1]}" == "$NODE_RENDER" ]] || { echo "err:header_device_mismatch"; return 1; }
    [[ "${BASH_REMATCH[2]}" == "$SIZE" ]] || { echo "err:requested_size_mismatch got=${BASH_REMATCH[2]}"; return 1; }
    hdr_msize="${BASH_REMATCH[3]}"; hdr_pages="${BASH_REMATCH[4]}"; hdr_iter="${BASH_REMATCH[5]}"
    [[ "$hdr_iter" == "$exp" ]] || { echo "err:iteration_mismatch ref=$exp got=$hdr_iter"; return 1; }
    [[ "${BASH_REMATCH[6]}" == "$acc" ]] || { echo "err:access_mismatch"; return 1; }
    [[ "${BASH_REMATCH[7]}" == "1" ]] || { echo "err:page_stride_mismatch"; return 1; }
    [[ "$hdr_pages" -gt 0 ]] || { echo "err:bad_pages"; return 1; }
    [[ "$hdr_msize" -ge "$SIZE" ]] || { echo "err:bad_map_size"; return 1; }
    while IFS= read -r line; do
        case "$line" in
            device=*)
                : ;;   # 跳过 header 行
            *" verify=pass "*)
                [[ "$line" =~ $verify_re ]] || { echo "err:bad_verify_line"; return 1; }
                iter="${BASH_REMATCH[1]}"
                [[ "$iter" -ge 1 && "$iter" -le "$exp" ]] || { echo "err:verify_iter_out_of_range:$iter"; return 1; }
                [[ "${line##*pages=}" == "$hdr_pages" ]] || { echo "err:verify_pages_mismatch"; return 1; }
                VERIFY_CNT[$iter]=$(( ${VERIFY_CNT[$iter]:-0} + 1 ))
                ;;
            checksum=*)
                [[ "$line" =~ $checksum_re ]] || { echo "err:bad_checksum"; return 1; }
                checksum_cnt=$((checksum_cnt+1))
                ;;
            iteration=*)
                [[ "$line" =~ $timing_re ]] || { echo "err:bad_timing_line"; return 1; }
                iter="${BASH_REMATCH[1]}"; phase="${BASH_REMATCH[2]}"
                [[ "$iter" -ge 1 && "$iter" -le "$exp" ]] || { echo "err:iteration_out_of_range:$iter"; return 1; }
                case "$phase" in
                    "${acc}_touch") TOUCH_CNT[$iter]=$(( ${TOUCH_CNT[$iter]:-0} + 1 )) ;;
                    "${acc}_munmap") MUNMAP_CNT[$iter]=$(( ${MUNMAP_CNT[$iter]:-0} + 1 )) ;;
                    verify_touch) VTOUCH_CNT[$iter]=$(( ${VTOUCH_CNT[$iter]:-0} + 1 )) ;;
                    verify_munmap) VMUNMAP_CNT[$iter]=$(( ${VMUNMAP_CNT[$iter]:-0} + 1 )) ;;
                    *) echo "err:unexpected_phase:$phase"; return 1 ;;
                esac
                TIMING_CNT[$iter]=$(( ${TIMING_CNT[$iter]:-0} + 1 ))
                ;;
            *) echo "err:unexpected_line"; return 1 ;;
        esac
    done < "$out"
    for ((i=1;i<=exp;i++)); do
        [[ "${TOUCH_CNT[$i]:-0}" -eq 1 ]] || { echo "err:iter_${i}_touch_count=${TOUCH_CNT[$i]:-0}"; return 1; }
        [[ "${MUNMAP_CNT[$i]:-0}" -eq 1 ]] || { echo "err:iter_${i}_munmap_count=${MUNMAP_CNT[$i]:-0}"; return 1; }
        [[ "${TIMING_CNT[$i]:-0}" -eq "$per_iter" ]] || { echo "err:iter_${i}_timing_count=${TIMING_CNT[$i]:-0}"; return 1; }
        [[ "$rv" -eq 0 ]] || [[ "${VERIFY_CNT[$i]:-0}" -eq 1 ]] || { echo "err:iter_${i}_verify_count=${VERIFY_CNT[$i]:-0}"; return 1; }
        [[ "$rv" -eq 0 ]] || [[ "${VTOUCH_CNT[$i]:-0}" -eq 1 ]] || { echo "err:iter_${i}_verify_touch_count=${VTOUCH_CNT[$i]:-0}"; return 1; }
        [[ "$rv" -eq 0 ]] || [[ "${VMUNMAP_CNT[$i]:-0}" -eq 1 ]] || { echo "err:iter_${i}_verify_munmap_count=${VMUNMAP_CNT[$i]:-0}"; return 1; }
    done
    [[ "$checksum_cnt" -eq 1 ]] || { echo "err:checksum_count=$checksum_cnt"; return 1; }
    return 0
}
invisible_read() {
    local out="$runtime/invisible-read.out" rc
    timeout --kill-after=2 "$TIMEOUT" "$P_READ" "$NODE_RENDER" "$SIZE" "$ITERATIONS" read >"$out" 2>&1; rc=$?
    if is_timeout_rc "$rc"; then record_fail invisible_read timeout_invisible_read 5; return; fi
    if [[ "$rc" -eq 2 ]]; then record_fail invisible_read probe_usage_error 1; return; fi
    if [[ "$rc" -ne 0 ]]; then record_fail invisible_read "probe_failed_rc=$rc" 1; return; fi
    local perr
    perr="$(parse_invisible "$out" "$ITERATIONS" read 0)" || { record_fail invisible_read "$perr" 1; return; }
    # READ munmap 性能门槛：max(read_munmap system_ms) <= limit（p22 71.9-119.4ms vs 修复后 1.7-2.6ms）
    local maxms
    maxms="$(awk -F'system_ms=' '/phase=read_munmap/{v=$2+0; if (v>m) m=v} END{printf "%.3f", m}' "$out")"
    [[ -n "$maxms" ]] || { record_fail invisible_read missing_read_munmap_timing 1; return; }
    if awk -v m="$maxms" -v l="$MUNMAP_LIMIT_MS" 'BEGIN { exit !(m <= l) }'; then
        record_pass invisible_read "iterations=$ITERATIONS-size=$SIZE-max_read_munmap_ms=$maxms-limit_ms=$MUNMAP_LIMIT_MS"
    else
        record_fail invisible_read "read_munmap_exceeds_limit max=$maxms limit=$MUNMAP_LIMIT_MS" 1
    fi
}

write_readback() {
    local out="$runtime/invisible-write.out" rc
    timeout --kill-after=2 "$TIMEOUT" "$P_READ" "$NODE_RENDER" "$SIZE" "$ITERATIONS" write >"$out" 2>&1; rc=$?
    if is_timeout_rc "$rc"; then record_fail write_readback timeout_write_readback 5; return; fi
    if [[ "$rc" -eq 2 ]]; then record_fail write_readback probe_usage_error 1; return; fi
    if [[ "$rc" -ne 0 ]]; then record_fail write_readback "probe_failed_rc=$rc" 1; return; fi
    local perr pages
    perr="$(parse_invisible "$out" "$ITERATIONS" write 1)" || { record_fail write_readback "$perr" 1; return; }
    pages="$(sed -n 's/^device=.* pages=\([0-9][0-9]*\).*/\1/p' "$out" | head -1)"
    record_pass write_readback "iterations=$ITERATIONS-size=$SIZE-verify=$ITERATIONS-pages=${pages:-?}"
}

# ---- 4. vblank（topology → active/inactive）----
parse_topology() { # <out> -> prints "active=<i,...> inactive=<i,...>"; 严格：header 恰好一次且 device 一致、crtcs=N、CRTC 行全字段、索引唯一且全覆盖 0..N-1
    local out="$1" active="" inactive="" line idx hdr_crtcs="" n_crtcs=0 hdr_count=0
    local -a SEEN_IDX=()
    while IFS= read -r line; do
        case "$line" in
            device=*)
                local top_head_re='^device=([^ ]+) crtcs=([0-9]+) connectors=[0-9]+ encoders=[0-9]+$'
                [[ "$line" =~ $top_head_re ]] || { echo "err:bad_header"; return 1; }
                hdr_count=$((hdr_count+1))
                hdr_crtcs="${BASH_REMATCH[2]}"
                [[ "${BASH_REMATCH[1]}" == "$NODE_CARD" ]] || { echo "err:header_device_mismatch"; return 1; }
                ;;
            "CRTCs:"* | "Connectors:"*) : ;;
            "  index="*)
                local crtc_re='^  index=([0-9]+) id=([0-9]+) active=(yes|no) fb=[0-9]+ position=[0-9]+,[0-9]+ size=[0-9]+x[0-9]+ mode=[^ ]+ refresh=[0-9]+$'
                [[ "$line" =~ $crtc_re ]] || { echo "err:bad_crtc_line"; return 1; }
                idx="${BASH_REMATCH[1]}"
                [[ -z "${SEEN_IDX[$idx]+x}" ]] || { echo "err:duplicate_crtc_index:$idx"; return 1; }
                SEEN_IDX[$idx]=1; n_crtcs=$((n_crtcs+1))
                if [[ "${BASH_REMATCH[3]}" == "yes" ]]; then
                    active="${active:+$active,}$idx"
                else
                    inactive="${inactive:+$inactive,}$idx"
                fi
                ;;
            "  type="* | "  id="*) : ;;
            "") : ;;
            *) echo "err:unexpected_topology_line"; return 1 ;;
        esac
    done < "$out"
    [[ "$hdr_count" -eq 1 ]] || { echo "err:header_count=$hdr_count"; return 1; }
    [[ -n "$hdr_crtcs" ]] || { echo "err:missing_crtcs_header"; return 1; }
    [[ "$n_crtcs" -eq "$hdr_crtcs" ]] || { echo "err:crtc_count_mismatch got=$n_crtcs want=$hdr_crtcs"; return 1; }
    for ((i=0;i<hdr_crtcs;i++)); do
        [[ -n "${SEEN_IDX[$i]+x}" ]] || { echo "err:missing_crtc_index:$i"; return 1; }
    done
    for ((i=hdr_crtcs;i<64;i++)); do
        [[ -z "${SEEN_IDX[$i]+x}" ]] || { echo "err:out_of_range_crtc_index:$i"; return 1; }
    done
    echo "active=$active inactive=$inactive"
}
vblank_validate_samples() { # <out> <expected_samples> -> 0 ok；header/列标题/summary 各恰好一行、样本行按 1..N 顺序出现、全文件严格
    local out="$1" exp="$2" line i seq w k d kd prev_seq=-1 prev_k="" ok_lines=0 hdr_n=0 col_n=0 sum_n=0 expected=1
    local canon='(0|[1-9][0-9]*)'
    # sequence/delta 必须是规范 uint32（无前导零、≤ 2^32-1）；delta 按探针无符号减法支持 32 位回绕
    local sample_re="^([0-9]+) ${canon} ([0-9]+([.][0-9]+)?) ([0-9]+([.][0-9]+)?) ${canon} ([0-9]+([.][0-9]+)?) ok$"
    while IFS= read -r line; do
        case "$line" in
            device=*) hdr_n=$((hdr_n+1)) ;;
            "$VBLANK_COLHDR") col_n=$((col_n+1)) ;;
            summary\ *) sum_n=$((sum_n+1)) ;;
            [0-9]*" ok")
                [[ "$line" =~ $sample_re ]] || return 1
                i="${BASH_REMATCH[1]}"; seq="${BASH_REMATCH[2]}"; w="${BASH_REMATCH[3]}"
                k="${BASH_REMATCH[5]}"; d="${BASH_REMATCH[7]}"; kd="${BASH_REMATCH[8]}"
                [[ "$i" -eq "$expected" ]] || return 1   # 样本行必须按 1..N 顺序出现
                expected=$((expected+1))
                [[ "$seq" -le 4294967295 && "$d" -le 4294967295 ]] || return 1   # 规范 uint32 上限
                valid_num "$w" && valid_num "$k" && valid_num "$kd" || return 1
                awk -v w="$w" 'BEGIN { exit !(w >= 1.0) }' || return 1   # 无 fast return
                if [[ "$prev_seq" -ge 0 ]]; then
                    # 32 位回绕感知：delta == (seq - prev_seq) mod 2^32；delta 非零（推进）
                    [[ "$d" -gt 0 ]] || return 1
                    [[ "$d" -eq $(( (seq - prev_seq + 4294967296) % 4294967296 )) ]] || return 1
                    # kernel_delta_ms 与相邻 kernel_time_ms 交叉验证（%.3f 精度，容差 0.001）
                    # 输入已固定三位小数：内核时间必须严格单调不倒退（k2 >= p），容差只用于比较 delta 与重算差
                    [[ -n "$prev_k" ]] || return 1
                    awk -v a="$kd" -v k2="$k" -v p="$prev_k" 'BEGIN { if (k2 < p) exit 1; d = k2 - p; diff = a - d; if (diff < 0) diff = -diff; exit !(diff <= 0.001) }' || return 1
                else
                    [[ "$d" -eq 0 ]] || return 1   # 首样本 delta 必须为 0（探针首样本不记录 delta）
                    awk -v a="$kd" 'BEGIN { if(a<0)a=-a; exit !(a <= 0.001) }' || return 1   # 首样本 kernel_delta=0
                fi
                prev_seq="$seq"; prev_k="$k"
                ok_lines=$((ok_lines+1))
                ;;
            *) return 1 ;;   # 意外行
        esac
    done < "$out"
    [[ "$hdr_n" -eq 1 && "$col_n" -eq 1 && "$sum_n" -eq 1 ]] || return 1   # 元数据各恰好一行
    [[ "$ok_lines" -eq "$exp" && "$expected" -eq $((exp+1)) ]] || return 1  # 恰好 exp 行且 1..exp 顺序到齐
    return 0
}

vblank_active_one() { # <crtc> -> echoes "ok <avg>" 或 "fail:<reason>"
    local idx="$1"
    local out="$runtime/vblank-active-$idx.out" rc samples="$VBLANK_SAMPLES"
    timeout --kill-after=2 "$TIMEOUT" "$P_VBL" "$NODE_CARD" "$idx" "$samples" 1500 >"$out" 2>&1; rc=$?
    if is_timeout_rc "$rc"; then echo "fail:timeout"; return; fi
    if [[ "$rc" -eq 2 ]]; then echo "fail:usage"; return; fi
    if [[ "$rc" -ne 0 ]]; then echo "fail:probe_rc=$rc"; return; fi
    local head_re='^device=([^ ]+) crtc=([0-9]+) samples=([0-9]+) timeout_ms=([0-9]+)$'
    local hline hcrtc hsamples
    hline="$(head -1 "$out")"
    [[ "$hline" =~ $head_re ]] || { echo "fail:bad_header"; return; }
    hcrtc="${BASH_REMATCH[2]}"; hsamples="${BASH_REMATCH[3]}"
    [[ "${BASH_REMATCH[1]}" == "$NODE_CARD" && "${BASH_REMATCH[4]}" == "1500" ]] || { echo "fail:header_mismatch"; return; }
    [[ "$hcrtc" == "$idx" && "$hsamples" == "$samples" ]] || { echo "fail:header_mismatch"; return; }
    local slines; slines="$(grep -c '^summary ' "$out")"
    [[ "$slines" -eq 1 ]] || { echo "fail:summary_count=$slines"; return; }
    local sum_re='^summary success=([0-9]+) failures=([0-9]+) avg_wait_ms=([0-9.]+) min_wait_ms=([0-9.]+) max_wait_ms=([0-9.]+) fast_returns=([0-9]+) nonadvancing=([0-9]+)$'
    local sline succ fails avg fast nonadv
    sline="$(grep '^summary ' "$out")"
    [[ "$sline" =~ $sum_re ]] || { echo "fail:bad_summary"; return; }
    succ="${BASH_REMATCH[1]}"; fails="${BASH_REMATCH[2]}"; avg="${BASH_REMATCH[3]}"
    local mn="${BASH_REMATCH[4]}" mx="${BASH_REMATCH[5]}"
    fast="${BASH_REMATCH[6]}"; nonadv="${BASH_REMATCH[7]}"
    valid_num "$avg" && valid_num "$mn" && valid_num "$mx" || { echo "fail:bad_float"; return; }
    awk -v a="$avg" -v n="$mn" -v x="$mx" 'BEGIN { exit !(n <= a && a <= x) }' || { echo "fail:float_ordering"; return; }
    [[ "$succ" == "$samples" && "$fails" == "0" && "$fast" == "0" && "$nonadv" == "0" ]] || { echo "fail:summary_mismatch"; return; }
    vblank_validate_samples "$out" "$samples" || { echo "fail:sample_validation"; return; }
    # 交叉验证：从样本行重算 min/max/avg，与 summary 按探针 %.3f 精度比较（容差 0.001）
    local rmn rmx ravg
    read -r rmn rmx ravg <<<"$(vblank_sample_metrics "$out")"
    awk -v a="$avg" -v r="$ravg" 'BEGIN { d=a-r; if(d<0)d=-d; exit !(d<=0.001) }' \
        || { echo "fail:summary_metrics_mismatch avg=$avg want=$ravg"; return; }
    awk -v a="$mn" -v r="$rmn" 'BEGIN { d=a-r; if(d<0)d=-d; exit !(d<=0.001) }' \
        || { echo "fail:summary_metrics_mismatch min=$mn want=$rmn"; return; }
    awk -v a="$mx" -v r="$rmx" 'BEGIN { d=a-r; if(d<0)d=-d; exit !(d<=0.001) }' \
        || { echo "fail:summary_metrics_mismatch max=$mx want=$rmx"; return; }
    echo "ok $avg"
}

vblank_sample_metrics() { # <out> -> "min max avg"（%.3f，来自 ok 样本行的 wait 值）
    awk '/ ok$/ { w=$3; s+=w; if(n==0 || w<min) min=w; if(w>max) max=w; n++ }
         END { if(n>0) printf "%.3f %.3f %.3f\n", min, max, s/n; else print "0.000 0.000 0.000" }' "$1"
}

vblank_active_all() { # <csv active indices>；记录每个 CRTC 的 ok/avg 证据（不覆盖）
    local list="$1" idx n=0 faillist="" per_crtc="" res tflag=0
    IFS=, read -ra IDXS <<<"$list"
    for idx in "${IDXS[@]}"; do
        res="$(vblank_active_one "$idx")"
        if [[ "$res" == ok* ]]; then
            n=$((n+1))
            per_crtc="${per_crtc:+$per_crtc,}crtc$idx=ok:${res#ok }"
        else
            [[ "$res" == *timeout* ]] && tflag=1
            faillist="${faillist:+$faillist,}crtc$idx(${res#fail:})"
            per_crtc="${per_crtc:+$per_crtc,}crtc$idx=fail:${res#fail:}"
        fi
    done
    if [[ -n "$faillist" ]]; then
        record_fail vblank_active "active_crtcs_tested=${#IDXS[@]}-ok=$n-failed=$faillist-per_crtc=$per_crtc" $((tflag ? 5 : 1))
    else
        record_pass vblank_active "active_crtcs_tested=${#IDXS[@]}-all-ok-samples=$VBLANK_SAMPLES-per_crtc=$per_crtc"
    fi
}

vblank_inactive_one() { # <crtc> -> "ok" 或 "fail:<reason>"；与 active 共用严格元数据解析
    local idx="$1"
    local out="$runtime/vblank-inactive-$idx.out" rc samples=3
    timeout --kill-after=2 "$TIMEOUT" "$P_VBL" "$NODE_CARD" "$idx" "$samples" 500 >"$out" 2>&1; rc=$?
    if is_timeout_rc "$rc"; then echo "fail:timeout"; return; fi
    if [[ "$rc" -eq 2 ]]; then echo "fail:usage"; return; fi
    if [[ "$rc" -ne 1 ]]; then echo "fail:expected_rc1_got=$rc"; return; fi   # inactive EINVAL -> probe rc=1 预期
    local head_re='^device=([^ ]+) crtc=([0-9]+) samples=([0-9]+) timeout_ms=([0-9]+)$'
    local hline hcrtc hsamples
    hline="$(head -1 "$out")"
    [[ "$hline" =~ $head_re ]] || { echo "fail:bad_header"; return; }
    hcrtc="${BASH_REMATCH[2]}"; hsamples="${BASH_REMATCH[3]}"
    [[ "${BASH_REMATCH[1]}" == "$NODE_CARD" && "${BASH_REMATCH[4]}" == "500" ]] || { echo "fail:header_mismatch"; return; }
    [[ "$hcrtc" == "$idx" && "$hsamples" == "$samples" ]] || { echo "fail:header_mismatch"; return; }
    # 与 active 共用严格元数据解析：header/列标题/summary 各恰好一行，样本行按 1..N 顺序出现，无意外行
    local line hdr_n=0 col_n=0 sum_n=0 n_seen=0 einval=0 to=0 other=0 expected=1 w sn
    local -a SEEN=()
    local err_re='^([0-9]+) - ([0-9]+([.][0-9]+)?) - - - error:[^:]* errno=22$'
    local to_re='^([0-9]+) - ([0-9]+([.][0-9]+)?) - - - timeout:.*$'
    while IFS= read -r line; do
        case "$line" in
            device=*) hdr_n=$((hdr_n+1)) ;;
            "$VBLANK_COLHDR") col_n=$((col_n+1)) ;;
            summary\ *) sum_n=$((sum_n+1)) ;;
            [0-9]*" - "*)
                if [[ "$line" =~ $to_re ]]; then to=$((to+1))
                elif [[ "$line" =~ $err_re ]]; then
                    sn="${BASH_REMATCH[1]}"; w="${BASH_REMATCH[2]}"
                    [[ -z "${SEEN[$sn]+x}" ]] || { echo "fail:duplicate_sample:$sn"; return; }
                    SEEN[$sn]=1
                    [[ "$sn" -eq "$expected" ]] || { echo "fail:sample_order:$sn"; return; }  # 按 1..N 顺序
                    expected=$((expected+1))
                    n_seen=$((n_seen+1))
                    valid_num "$w" || { echo "fail:bad_wait_ms"; return; }
                    awk -v w="$w" 'BEGIN { exit !(w < 500) }' || { echo "fail:slow_einval"; return; }
                    einval=$((einval+1))
                else other=$((other+1)); fi
                ;;
            *) echo "fail:unexpected_line"; return ;;
        esac
    done < "$out"
    [[ "$hdr_n" -eq 1 && "$col_n" -eq 1 && "$sum_n" -eq 1 ]] || { echo "fail:metadata hdr=$hdr_n col=$col_n sum=$sum_n"; return; }
    [[ "$n_seen" -eq "$samples" && "$einval" -eq "$samples" && "$to" -eq 0 && "$other" -eq 0 && "$expected" -eq $((samples+1)) ]] \
        || { echo "fail:einval=$einval,to=$to,other=$other,n=$n_seen"; return; }
    # summary 严格格式：浮点字段 valid_num + min≤avg≤max（与 active 同规）
    local sline; sline="$(grep '^summary ' "$out")"
    local sum_re='^summary success=0 failures=([0-9]+) avg_wait_ms=([0-9.]+) min_wait_ms=([0-9.]+) max_wait_ms=([0-9.]+) fast_returns=0 nonadvancing=0$'
    [[ "$sline" =~ $sum_re ]] || { echo "fail:bad_summary"; return; }
    [[ "${BASH_REMATCH[1]}" == "$samples" ]] || { echo "fail:summary_failures_mismatch"; return; }
    local avg="${BASH_REMATCH[2]}" mn="${BASH_REMATCH[3]}" mx="${BASH_REMATCH[4]}"
    valid_num "$avg" && valid_num "$mn" && valid_num "$mx" || { echo "fail:bad_float"; return; }
    awk -v a="$avg" -v n="$mn" -v x="$mx" 'BEGIN { exit !(n <= a && a <= x) }' || { echo "fail:float_ordering"; return; }
    # 真实探针契约：success=0（全失败样本不更新统计）时 avg/min/max 必须全为零
    awk -v a="$avg" -v n="$mn" -v x="$mx" 'BEGIN { exit !(a == 0 && n == 0 && x == 0) }' \
        || { echo "fail:inactive_nonzero_summary avg=$avg min=$mn max=$mx"; return; }
    echo "ok"
}

vblank_inactive_all() { # <csv inactive indices>
    local list="$1" idx n=0 faillist="" tflag=0 res
    IFS=, read -ra IDXS <<<"$list"
    for idx in "${IDXS[@]}"; do
        res="$(vblank_inactive_one "$idx")"
        if [[ "$res" == ok ]]; then n=$((n+1))
        else
            [[ "$res" == *timeout* ]] && tflag=1
            faillist="${faillist:+$faillist,}crtc$idx(${res#fail:})"
        fi
    done
    if [[ -n "$faillist" ]]; then
        record_fail vblank_inactive_guard "inactive_crtcs_tested=${#IDXS[@]}-ok=$n-failed=$faillist" $((tflag ? 5 : 1))
    else
        record_pass vblank_inactive_guard "inactive_crtcs_tested=${#IDXS[@]}-all-einval-fast"
    fi
}
topology() {
    local out="$runtime/topology.out" rc tparse active inactive
    timeout --kill-after=2 "$TIMEOUT" "$P_TOP" "$NODE_CARD" >"$out" 2>&1; rc=$?
    if is_timeout_rc "$rc"; then
        record_fail vblank_active timeout_topology 5
        record_fail vblank_inactive_guard timeout_topology 5
        return
    fi
    if [[ "$rc" -ne 0 ]]; then
        record_fail vblank_active "topology_probe_rc=$rc" 1
        record_fail vblank_inactive_guard "topology_probe_rc=$rc" 1
        return
    fi
    tparse="$(parse_topology "$out")" || {
        record_fail vblank_active "topology_parse_failed:$tparse" 1
        record_fail vblank_inactive_guard "topology_parse_failed:$tparse" 1
        return
    }
    active="${tparse#active=}"; active="${active%% inactive=*}"
    inactive="${tparse##*inactive=}"
    if [[ -n "$active" ]]; then
        vblank_active_all "$active"
    else
        record_unverified vblank_active no_active_crtc
    fi
    if [[ -n "$inactive" ]]; then
        vblank_inactive_all "$inactive"
    else
        record_skip vblank_inactive_guard no_inactive_crtc
    fi
}
self_import
invisible_read
write_readback
topology

# ---- 状态门禁（post 快照 + 8 字段逐项比较）+ 内核日志 ----
POST_SNAP="$(status_snapshot "$STATUS_FILE")"; POST_RC=$?
status_ok=1; status_reason=""
if [[ "$POST_RC" -ne 0 ]]; then
    status_ok=0; status_reason="post_${POST_SNAP#err:}"
else
    read -ra PRE_A <<<"$PRE_SNAP"; read -ra POST_A <<<"$POST_SNAP"
    names=(Server HWR CRR SLR WGP TRP FWF APM)
    changed=""
    for ((i=0;i<8;i++)); do
        if [[ "${POST_A[$i]}" -ne "${PRE_A[$i]}" ]]; then
            changed+="${changed:+ }${names[$i]}(${PRE_A[$i]}->${POST_A[$i]})"
        fi
    done
    if [[ -n "$changed" ]]; then status_ok=0; status_reason="driver_error_counts_changed:$changed"; fi
fi
if [[ "$status_ok" -eq 1 ]]; then
    echo "${NS}dmabuf_status=ok$(oktag)"
else
    echo "${NS}dmabuf_status=fail reason=$(tag "$status_reason")"
fi

if [[ "$KLOG_OK" -eq 1 ]]; then
    KLOG_AFTER="$(kernel_log)" || {
        echo "${NS}dmabuf_kernel_log=UNVERIFIED reason=post_kernel_log_unavailable"
        KLOG_FAIL=1
    }
    if [[ "$KLOG_FAIL" -eq 0 ]]; then
        # 有序连续关系校验：after 必须以 before 为完整前缀（append-only dmesg 快照）
        if ! klog_continuous "$KLOG_BEFORE" "$KLOG_AFTER"; then
            echo "${NS}dmabuf_kernel_log=UNVERIFIED reason=kernel_log_discontinuous"
            KLOG_FAIL=1
        else
            # 连续窗口新增行判断（多集差集：旧行轮转消失不抵消新增错误）
            new_lines="$(klog_new_lines "$KLOG_BEFORE" "$KLOG_AFTER")"
            new_errors="$(printf '%s\n' "$new_lines" | grep -icE "$KLOG_ERR_RE" || true)"
            if [[ "$new_errors" -gt 0 ]]; then
                echo "${NS}dmabuf_kernel_log=fail reason=new_kernel_error_lines:$new_errors"
                KLOG_FAIL=1
            else
                echo "${NS}dmabuf_kernel_log=clean"
            fi
        fi
    fi
else
    echo "${NS}dmabuf_kernel_log=SKIP reason=dmesg_unavailable"
fi

printf '%stests_total=%d %stests_passed=%d %stests_failed=%d %stests_skipped=%d %stests_unverified=%d\n' \
    "$NS" "$total" "$NS" "$passed" "$NS" "$failed" "$NS" "$skipped" "$NS" "$unverified"
# ---- 最终清理（先于 overall 输出；清理失败 -> 明确 FAIL/5，绝不输出权威 PASS）----
if [[ -n "$runtime" && -d "$runtime" ]]; then
    if [[ "${INNOGPU_DMABUF_FORCE_CLEANUP_FAIL:-0}" == "1" ]] || ! rm -rf "$runtime" 2>/dev/null; then
        echo "${NS}dmabuf_regression_overall=FAIL reason=$(tag cleanup_failed)" >&2
        exit 5
    fi
fi
if [[ "$failed" -gt 0 ]]; then
    echo "${NS}dmabuf_regression_overall=FAIL reason=$(tag subitem_failures=$failed)"
    exit "$FAIL_CODE"
elif [[ "$status_ok" -ne 1 ]]; then
    echo "${NS}dmabuf_regression_overall=FAIL reason=$(tag "$status_reason")"
    exit 1
elif [[ "$KLOG_FAIL" -eq 1 ]]; then
    echo "${NS}dmabuf_regression_overall=FAIL reason=$(tag kernel_error_lines)"
    exit 1
elif [[ "$KLOG_UNAVAILABLE" -eq 1 ]]; then
    # 内核日志不可用（pre 快照缺失）-> 无法确认窗口无错误 -> 整体至少 UNVERIFIED
    echo "${NS}dmabuf_regression_overall=UNVERIFIED reason=$(tag kernel_log_unavailable)"
    exit 3
elif [[ "$failed" -eq 0 && "$unverified" -eq 0 ]]; then
    # SKIP（如无 inactive CRTC 的诚实分级）不阻止整体 PASS
    echo "${NS}dmabuf_regression_overall=PASS reason=$(tag self_import+read+write+vblank-ok-status-ok-device=1ec8:9810)"
    exit 0
elif [[ "$unverified" -gt 0 ]]; then
    echo "${NS}dmabuf_regression_overall=UNVERIFIED reason=$(tag "capability_or_device_unverified:unverified=$unverified")"
    exit 3
else
    echo "${NS}dmabuf_regression_overall=SKIP reason=$(tag "skipped:$skipped")"
    exit 2
fi
