#!/bin/bash
# VA-API H.264/HEVC real decode verification for FH2M (Innosilicon 1ec8:9810).
#
# Generates a fixed lavfi testsrc2 source (exactly 30 frames @ 320x240), encodes
# H.264 Main / HEVC Main with the system software encoders, decodes via the
# software reference path (NV12 framemd5) and via a FORCED VA-API hardware path
# (hwaccel vaapi + hwaccel_output_format vaapi + hwdownload,format=nv12; NO
# software fallback), then validates the REAL FFmpeg framemd5 output format:
# trailing newline, #dimensions 320x240, exactly 30 valid frame records
# (stream,dts,pts,duration,size,hash), NV12 frame size 115200, 32-hex MD5 per
# frame, and per-frame hash equality between reference and hardware outputs.
# Driver/Firmware status is an independent gate: TWO full snapshots (pre-decode
# and post-decode) are parsed strictly - Driver/Firmware must be OK, all 8
# error-count fields must be present and non-negative integers - and each field
# is compared individually for growth; any invalid/missing snapshot => FAIL.
#
# Usage:
#   bash tools/run-vaapi-decode-test.sh --codec h264|hevc|all [--device /dev/dri/renderDNN] [--timeout SEC]
#
# Exit codes: 0=requested codecs all PASS  1=decode/verify/status FAIL  2=arg/tool/codec missing
#            3=render node missing/permission/identity mismatch  4=input gen/software ref FAIL  5=timeout/cleanup
#
# Unit-fixture hooks (NEVER set in real runs; all require INNOGPU_VAAPI_FIXTURE_MODE=1):
#   FFMPEG_BIN / VAINFO_BIN                override the tool binaries (fake command injection)
#   INNOGPU_VAAPI_SKIP_DEVICE_CHECKS=1     skip char-device/read/write checks
#   FAKE_SYSFS_ROOT=<dir>                  read render-node PCI identity from this root instead of /sys
#   INNOGPU_VAAPI_STATUS_FILE=<file>       read Driver/Firmware status + error counts from this file
#   INNOGPU_VAAPI_FIXTURE_MODE=1           mandatory fixture-mode marker
# Fixture mode uses the INDEPENDENT namespace fixture_* for EVERY result line
# (fixture_vaapi_decode_h264=..., fixture_tests_total=..., fixture_vaapi_decode_overall=...),
# so it never emits any authoritative vaapi_decode_* line that could be merged
# as real hardware evidence; reasons still carry -mode=fixture for humans.

set -u -o pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FFMPEG_BIN="${FFMPEG_BIN:-ffmpeg}"
VAINFO_BIN="${VAINFO_BIN:-vainfo}"
SYSFS_ROOT="${FAKE_SYSFS_ROOT:-/sys}"
STATUS_FILE="${INNOGPU_VAAPI_STATUS_FILE:-/proc/driver/innogpu/gpu00/status}"
CODEC="" DEVICE="" TIMEOUT=30
FIXTURE_MODE=0
NS=""

usage() { sed -n '2,18p' "${BASH_SOURCE[0]}" | sed 's/^# \?//'; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --codec)
            [[ $# -ge 2 && -n "$2" ]] || { echo "--codec needs a value" >&2; exit 2; }
            CODEC="$2"; shift 2 ;;
        --device)
            [[ $# -ge 2 && -n "$2" ]] || { echo "--device needs a value" >&2; exit 2; }
            DEVICE="$2"; shift 2 ;;
        --timeout)
            [[ $# -ge 2 && -n "$2" ]] || { echo "--timeout needs a value" >&2; exit 2; }
            TIMEOUT="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done
case "$CODEC" in h264|hevc|all) ;; *) echo "--codec required: h264|hevc|all" >&2; exit 2 ;; esac
[[ "$TIMEOUT" =~ ^[1-9][0-9]*$ ]] || { echo "--timeout must be a positive integer: $TIMEOUT" >&2; exit 2; }

# ---- fixture 模式检测：任何注入钩子都必须显式声明；fixture 模式使用独立命名空间 fixture_* ----
if [[ "${INNOGPU_VAAPI_FIXTURE_MODE:-0}" == "1" \
      || "$FFMPEG_BIN" != "ffmpeg" \
      || "$VAINFO_BIN" != "vainfo" \
      || -n "${FAKE_SYSFS_ROOT:-}" \
      || -n "${INNOGPU_VAAPI_SKIP_DEVICE_CHECKS:-}" \
      || -n "${INNOGPU_VAAPI_STATUS_FILE:-}" ]]; then
    if [[ "${INNOGPU_VAAPI_FIXTURE_MODE:-0}" != "1" ]]; then
        echo "vaapi_decode_tool=fail reason=fixture_hooks_require_INNOGPU_VAAPI_FIXTURE_MODE=1" >&2
        exit 2
    fi
    FIXTURE_MODE=1
    NS="fixture_"
fi
tag() { if [[ "$FIXTURE_MODE" -eq 1 ]]; then printf '%s-mode=fixture' "$1"; else printf '%s' "$1"; fi; }
oktag() { [[ "$FIXTURE_MODE" -eq 1 ]] && printf ' mode=fixture'; }

# ---- 工具检查 (exit 2)。先捕获完整输出再匹配，避免 pipefail 下 grep -q 令工具收 SIGPIPE ----
command -v "$FFMPEG_BIN" >/dev/null 2>&1 || { echo "${NS}vaapi_decode_tool=fail reason=ffmpeg_missing" >&2; exit 2; }
HWACCELS="$("$FFMPEG_BIN" -hide_banner -hwaccels 2>/dev/null || true)"
echo "$HWACCELS" | grep -q '^vaapi' || { echo "${NS}vaapi_decode_tool=fail reason=ffmpeg_no_vaapi" >&2; exit 2; }
ENCODERS="$("$FFMPEG_BIN" -hide_banner -encoders 2>/dev/null || true)"
DECODERS="$("$FFMPEG_BIN" -hide_banner -decoders 2>/dev/null || true)"
need_h264=0; need_hevc=0
[[ "$CODEC" == all || "$CODEC" == h264 ]] && need_h264=1
[[ "$CODEC" == all || "$CODEC" == hevc ]] && need_hevc=1
if [[ "$need_h264" -eq 1 ]]; then
    echo "$ENCODERS" | grep -qw 'libx264' || { echo "${NS}vaapi_decode_tool=fail reason=encoder_missing:libx264" >&2; exit 2; }
    echo "$DECODERS" | grep -qw 'h264' || { echo "${NS}vaapi_decode_tool=fail reason=decoder_missing:h264" >&2; exit 2; }
fi
if [[ "$need_hevc" -eq 1 ]]; then
    echo "$ENCODERS" | grep -qw 'libx265' || { echo "${NS}vaapi_decode_tool=fail reason=encoder_missing:libx265" >&2; exit 2; }
    echo "$DECODERS" | grep -qw 'hevc' || { echo "${NS}vaapi_decode_tool=fail reason=decoder_missing:hevc" >&2; exit 2; }
fi
command -v "$VAINFO_BIN" >/dev/null 2>&1 || { echo "${NS}vaapi_decode_tool=fail reason=vainfo_missing" >&2; exit 2; }

# ---- render node 定位与身份 (exit 3) ----
resolve_node() {
    if [[ -n "$DEVICE" ]]; then echo "$DEVICE"; return 0; fi
    for n in /dev/dri/renderD*; do
        [[ -e "$n" ]] || continue
        local b v d; b="$(basename "$n")"
        v="$(cat "$SYSFS_ROOT/class/drm/$b/device/vendor" 2>/dev/null || true)"
        d="$(cat "$SYSFS_ROOT/class/drm/$b/device/device" 2>/dev/null || true)"
        if [[ "$v" == "0x1ec8" && "$d" == "0x9810" ]]; then echo "$n"; return 0; fi
    done
    return 1;
}
NODE="$(resolve_node)" || { echo "${NS}vaapi_decode_node=fail reason=$(tag no_fh2m_render_node)" >&2; exit 3; }
if [[ -z "${INNOGPU_VAAPI_SKIP_DEVICE_CHECKS:-}" ]]; then
    [[ -c "$NODE" ]] || { echo "${NS}vaapi_decode_node=fail reason=$(tag not_char_device:$NODE)" >&2; exit 3; }
    [[ -r "$NODE" && -w "$NODE" ]] || { echo "${NS}vaapi_decode_node=fail reason=$(tag no_permission:$NODE)" >&2; exit 3; }
fi
NB="$(basename "$NODE")"
NV="$(cat "$SYSFS_ROOT/class/drm/$NB/device/vendor" 2>/dev/null || true)"
ND="$(cat "$SYSFS_ROOT/class/drm/$NB/device/device" 2>/dev/null || true)"
if [[ "$NV" != "0x1ec8" || "$ND" != "0x9810" ]]; then
    echo "${NS}vaapi_decode_node=fail reason=$(tag pci_identity_mismatch:vendor=$NV-device=$ND)" >&2; exit 3;
fi
echo "${NS}vaapi_decode_node=ok $NODE (1ec8:9810)$(oktag)"

# ---- vainfo 身份 + 按 codec 的 VLD profile (exit 3) ----
VAINFO_OUT="$("$VAINFO_BIN" --display drm --device "$NODE" 2>&1)"; VAINFO_RC=$?
if [[ "$VAINFO_RC" -ne 0 ]] || ! echo "$VAINFO_OUT" | grep -qiE 'innosilicon|innogpu'; then
    echo "${NS}vaapi_decode_vainfo=fail reason=$(tag vainfo_init_failed_or_not_innogpu)" >&2; exit 3;
fi
if [[ "$need_h264" -eq 1 ]] && ! echo "$VAINFO_OUT" | grep -qE 'VAProfileH264Main[^:]*:[[:space:]]*VAEntrypointVLD'; then
    echo "${NS}vaapi_decode_vainfo=fail reason=$(tag no_h264_vld_profile)" >&2; exit 3;
fi
if [[ "$need_hevc" -eq 1 ]] && ! echo "$VAINFO_OUT" | grep -qE 'VAProfileHEVCMain[^:]*:[[:space:]]*VAEntrypointVLD'; then
    echo "${NS}vaapi_decode_vainfo=fail reason=$(tag no_hevc_vld_profile)" >&2; exit 3;
fi
echo "${NS}vaapi_decode_vainfo=ok$(oktag)"

# ---- Driver/Firmware 状态快照（严格解析：整行匹配 + 恰好一次 + 规范十进制）----
status_snapshot() {
    local f="$1"
    [[ -r "$f" ]] || { echo "err:unreadable"; return 1; }
    awk '
        # 规范十进制：剥离前导零，避免 Bash -gt 按八进制解析 08/09 出错
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
# 解码前快照无效（缺失/非 OK/缺字段/非数字）=> 直接 FAIL，不做解码
PRE_SNAP="$(status_snapshot "$STATUS_FILE")"; PRE_RC=$?
if [[ "$PRE_RC" -ne 0 ]]; then
    echo "${NS}vaapi_decode_status=fail reason=$(tag pre_${PRE_SNAP#err:})" >&2
    echo "${NS}vaapi_decode_overall=FAIL reason=$(tag pre_status_invalid)" >&2
    exit 1
fi

runtime="$(mktemp -d "${TMPDIR:-/tmp}/vaapi-decode.XXXXXX")" || {
    echo "${NS}vaapi_decode_tool=fail reason=mktemp_failed" >&2; exit 2; }
# 幂等清理；HUP/INT/TERM 清理后分别退出 129/130/143（bash 会等当前前台命令结束后再执行 trap，
# 因此不会有孤儿 ffmpeg 残留；trap 在下一个外部命令前触发，不会继续后续解码阶段）
cleanup() { [[ -n "${runtime:-}" && -d "$runtime" ]] && rm -rf "$runtime"; }
trap cleanup EXIT
trap 'cleanup; exit 129' HUP
trap 'cleanup; exit 130' INT
trap 'cleanup; exit 143' TERM

# ---- 校验真实 FFmpeg framemd5 输出格式 ----
# 成功：打印 30 行 hash（每行 32 位十六进制）。失败：打印一行 "err:<msg>" 并返回 1。
verify_framemd5() {
    local f="$1"
    [[ -s "$f" ]] || { echo "err:empty_file"; return 1; }
    # 命令替换会吞掉尾部换行，不能用 $(tail -c1) 比较；wc -l 对最后字节是否换行敏感
    [[ "$(tail -c1 "$f" | wc -l)" -eq 1 ]] || { echo "err:no_trailing_newline"; return 1; }
    awk '
        /^#dimensions 0: 320x240$/ { dim_ok = 1; next }
        /^#/ { next }
        /^[[:space:]]*$/ { next }
        {
            n = split($0, a, ",")
            if (n != 6) { print "err:invalid_frame_line"; err = 1; exit 1 }
            for (j = 1; j <= 6; j++) gsub(/[[:space:]]/, "", a[j])
            if (a[1] != 0 || a[2] !~ /^-?[0-9]+$/ || a[3] !~ /^-?[0-9]+$/ || a[4] !~ /^[0-9]+$/) { print "err:invalid_frame_line"; err = 1; exit 1 }
            if (a[6] !~ /^[0-9a-f]{32}$/) { print "err:invalid_hash"; err = 1; exit 1 }
            if (a[5] != 115200) { print "err:bad_frame_size:" a[5]; err = 1; exit 1 }
            hashes[++cnt] = a[6]
        }
        END {
            if (err) exit 1
            if (!dim_ok) { print "err:dimensions_metadata_missing"; exit 1 }
            if (cnt != 30) { print "err:frame_count=" cnt; exit 1 }
            for (i = 1; i <= cnt; i++) print hashes[i]
        }
    ' "$f"
}

# ---- 单 codec 解码链 ----
passed=0; failed=0; skipped=0; total=0
fail_code=0
record_fail() { # <codec> <reason> <code>
    failed=$((failed+1))
    echo "${NS}vaapi_decode_$1=FAIL reason=$(tag "$2")" >&2
    [[ "$3" -gt "$fail_code" ]] && fail_code="$3"
}
decode_one() { # <codec> <encoder> <ext>
    local codec="$1" enc="$2" ext="$3" rc=0
    total=$((total+1))
    local input="$runtime/src-$codec.$ext" ref="$runtime/ref-$codec.md5" hw="$runtime/hw-$codec.md5"
    # 1. 输入生成 (输入/参考失败 -> 4, 超时 -> 5)
    timeout --kill-after=2 "$TIMEOUT" "$FFMPEG_BIN" -y -f lavfi -i "testsrc2=size=320x240:rate=30:duration=1" \
        -c:v "$enc" -profile:v main -pix_fmt yuv420p "$input" >/dev/null 2>&1; rc=$?
    if [[ "$rc" -ne 0 ]]; then
        if [[ "$rc" -eq 124 ]]; then record_fail "$codec" timeout_input_generation 5; else record_fail "$codec" input_generation_failed 4; fi
        return
    fi
    # 2. 软件参考 (-> 4/5)
    timeout --kill-after=2 "$TIMEOUT" "$FFMPEG_BIN" -y -i "$input" -pix_fmt nv12 -f framemd5 "$ref" >/dev/null 2>&1; rc=$?
    if [[ "$rc" -ne 0 ]]; then
        if [[ "$rc" -eq 124 ]]; then record_fail "$codec" timeout_software_reference 5; else record_fail "$codec" software_reference_failed 4; fi
        return
    fi
    # 3. 强制 VAAPI 硬解 (解码失败 -> 1, 超时 -> 5; 无软件回退)
    timeout --kill-after=2 "$TIMEOUT" "$FFMPEG_BIN" -y -hwaccel vaapi -hwaccel_device "$NODE" -hwaccel_output_format vaapi \
        -i "$input" -vf 'hwdownload,format=nv12' -f framemd5 "$hw" >/dev/null 2>&1; rc=$?
    if [[ "$rc" -ne 0 ]]; then
        if [[ "$rc" -eq 124 ]]; then record_fail "$codec" timeout_vaapi_decode 5; else record_fail "$codec" vaapi_decode_failed 1; fi
        return
    fi
    # 4. 输出校验 (-> 1)：30 帧 / 320x240 / NV12 115200 / 32 位 hash / 尾换行，全部通过才比对 hash
    local ref_out hw_out ref_rc hw_rc ok=1 diff_reason=""
    ref_out="$(verify_framemd5 "$ref")"; ref_rc=$?
    hw_out="$(verify_framemd5 "$hw")"; hw_rc=$?
    if [[ "$ref_rc" -ne 0 ]]; then ok=0; diff_reason="ref_${ref_out#err:}"
    elif [[ "$hw_rc" -ne 0 ]]; then ok=0; diff_reason="hw_${hw_out#err:}"
    elif [[ "$ref_out" != "$hw_out" ]]; then
        ok=0; diff_reason="framemd5_mismatch"
        diff <(printf '%s\n' "$ref_out") <(printf '%s\n' "$hw_out") | head -3 >&2
    fi
    if [[ "$ok" -eq 1 ]]; then
        passed=$((passed+1))
        echo "${NS}vaapi_decode_$codec=PASS reason=$(tag frames=30-format=nv12-size=115200-hashes-match-device=1ec8:9810)"
    else
        record_fail "$codec" "$diff_reason" 1
    fi
}

if [[ "$CODEC" == all || "$CODEC" == h264 ]]; then decode_one h264 libx264 mp4; fi
if [[ "$CODEC" == all || "$CODEC" == hevc ]]; then decode_one hevc libx265 mp4; fi

# ---- 状态门禁（独立 gate，不计入 codec 计数）：post 快照严格解析 + 8 字段逐项比较 ----
POST_SNAP="$(status_snapshot "$STATUS_FILE")"; POST_RC=$?
status_ok=1; status_reason=""
if [[ "$POST_RC" -ne 0 ]]; then
    status_ok=0; status_reason="post_${POST_SNAP#err:}"
else
    read -ra PRE_A <<<"$PRE_SNAP"; read -ra POST_A <<<"$POST_SNAP"
    names=(Server HWR CRR SLR WGP TRP FWF APM)
    grown=""
    for ((i=0;i<8;i++)); do
        if [[ "${POST_A[$i]}" -gt "${PRE_A[$i]}" ]]; then
            grown+="${grown:+ }${names[$i]}(${PRE_A[$i]}->${POST_A[$i]})"
        fi
    done
    if [[ -n "$grown" ]]; then status_ok=0; status_reason="driver_error_counts_increased:$grown"; fi
fi
if [[ "$status_ok" -eq 1 ]]; then
    echo "${NS}vaapi_decode_status=ok$(oktag)"
else
    echo "${NS}vaapi_decode_status=fail reason=$(tag "$status_reason")"
fi

printf '%stests_total=%d %stests_passed=%d %stests_failed=%d %stests_skipped=%d\n' "$NS" "$total" "$NS" "$passed" "$NS" "$failed" "$NS" "$skipped"
if [[ "$failed" -gt 0 ]]; then
    echo "${NS}vaapi_decode_overall=FAIL reason=$(tag decode_failures=$failed)"
    exit "$fail_code"
elif [[ "$status_ok" -ne 1 ]]; then
    echo "${NS}vaapi_decode_overall=FAIL reason=$(tag "$status_reason")"
    exit 1
else
    echo "${NS}vaapi_decode_overall=PASS reason=$(tag all-requested-codecs-passed)"
    exit 0
fi
