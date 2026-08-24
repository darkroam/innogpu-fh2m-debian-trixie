#!/bin/bash
# Unit tests: tools/run-vaapi-decode-test.sh control flow with fake fixtures.
# CI-safe without /dev/dri. Fixture mode (INNOGPU_VAAPI_FIXTURE_MODE=1) never
# emits vaapi_decode_overall=PASS and tags every result -mode=fixture, so fake
# runs prove control flow/parsing only and can never be merged as hardware
# evidence; baselines are never touched.

set -u -o pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SCRIPT="$ROOT/tools/run-vaapi-decode-test.sh"
runtime="$(mktemp -d "${TMPDIR:-/tmp}/inno-vaapi-decode-tests.XXXXXX")"
trap 'rm -rf "$runtime"' EXIT

mkdir -p "$runtime/bin" "$runtime/scratch" "$runtime/sysfs/class/drm/renderD128/device" "$runtime/sysfs-intel/class/drm/renderD128/device"
printf '0x1ec8\n' > "$runtime/sysfs/class/drm/renderD128/device/vendor"
printf '0x9810\n' > "$runtime/sysfs/class/drm/renderD128/device/device"
printf '0x8086\n' > "$runtime/sysfs-intel/class/drm/renderD128/device/vendor"
printf '0x5912\n' > "$runtime/sysfs-intel/class/drm/renderD128/device/device"

# ---- fake ffmpeg: 真实 framemd5 格式 fixture 写入输出文件；支持故障/超时/参数日志/状态切换钩子 ----
cat > "$runtime/bin/ffmpeg" <<'FAKE'
#!/bin/bash
[[ "${FAKE_LOG_ARGS:-0}" == 1 ]] && echo "$*" >> "${FAKE_LOG_FILE:-/dev/null}"
if [[ " $* " == *" -hwaccels "* ]]; then
  [[ "${FAKE_NO_VAAPI:-0}" == 1 ]] || echo "vaapi"
  exit 0
fi
if [[ " $* " == *" -encoders "* ]]; then
  [[ "${FAKE_NO_X264:-0}" == 1 ]] || echo " V..... libx264"
  [[ "${FAKE_NO_X265:-0}" == 1 ]] || echo " V..... libx265"
  exit 0
fi
if [[ " $* " == *" -decoders "* ]]; then
  [[ "${FAKE_NO_DECODER_H264:-0}" == 1 ]] || echo " V....D h264"
  echo " V....D hevc"
  exit 0
fi
if [[ " $* " == *" -hwaccel "* ]]; then
  [[ "${FAKE_HW_FAIL:-0}" == 1 ]] && { echo "fake: vaapi init failed" >&2; exit 1; }
  if [[ "${FAKE_HW_TIMEOUT:-0}" == 1 || -n "${FAKE_HW_SLEEP:-}" ]]; then sleep "${FAKE_HW_SLEEP:-5}"; fi
  [[ "${FAKE_HEVC_ONLY_FAIL:-0}" == 1 && " $* " == *"src-hevc"* ]] && { echo "fake: hevc hw fail" >&2; exit 1; }
  if [[ -n "${FAKE_SWAP_STATUS_LINK:-}" && -n "${FAKE_SWAP_STATUS_TARGET:-}" ]]; then
    ln -sf "$FAKE_SWAP_STATUS_TARGET" "$FAKE_SWAP_STATUS_LINK"
  fi
  out="${@: -1}"; cat "${FAKE_HW_FRAMEMD5:?}" > "$out"; exit 0
fi
if [[ " $* " == *" framemd5 "* ]]; then
  [[ "${FAKE_REF_FAIL:-0}" == 1 ]] && { echo "fake: sw ref failed" >&2; exit 1; }
  [[ "${FAKE_REF_TIMEOUT:-0}" == 1 ]] && sleep 5
  out="${@: -1}"; cat "${FAKE_REF_FRAMEMD5:?}" > "$out"; exit 0
fi
if [[ " $* " == *"libx264"* || " $* " == *"libx265"* ]]; then
  [[ "${FAKE_ENCODE_FAIL:-0}" == 1 ]] && { echo "fake: encode failed" >&2; exit 1; }
  [[ "${FAKE_ENCODE_TIMEOUT:-0}" == 1 ]] && sleep 5
  [[ "${FAKE_HEVC_ONLY_FAIL:-0}" == 1 && " $* " == *"src-hevc"* ]] && { echo "fake: hevc encode fail" >&2; exit 1; }
  out="${@: -1}"; echo "fake-encoded" > "$out"; exit 0
fi
exit 0
FAKE
cat > "$runtime/bin/vainfo" <<'FAKEV'
#!/bin/bash
if [[ "${FAKE_VAINFO_FAIL:-0}" == 1 ]]; then echo "vainfo: init failed" >&2; exit 1; fi
if [[ "${FAKE_VAINFO_INTEL:-0}" == 1 ]]; then echo "vainfo: Driver version: Intel i965"; exit 0; fi
echo "vainfo: VA-API version 1.22"
echo "vainfo: Driver version: Innogpu VA driver"
echo "vainfo: Supported profile and entrypoints:"
[[ "${FAKE_VAINFO_NO_H264:-0}" == 1 ]] || echo "      VAProfileH264Main               : VAEntrypointVLD"
echo "      VAProfileHEVCMain               : VAEntrypointVLD"
exit 0
FAKEV
chmod +x "$runtime/bin/ffmpeg" "$runtime/bin/vainfo"

# ---- 真实 FFmpeg framemd5 格式 fixture（#dimensions / stream,dts,pts,duration,size,hash / 32 位 hex / 尾换行）----
gen_md5() { # <out> <nframes> [hash]
    local f="$1" n="$2" h="${3:-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa}" i
    {
        printf '#software:ffmpeg\n#tb 0: 1/30\n#media_type 0: video\n#codec_id 0: rawvideo\n#dimensions 0: 320x240\n#sar 0: 1/1\n#stream#, dts,        pts, duration,     size, hash\n'
        for ((i=0; i<n; i++)); do
            printf '0, %10d, %10d,        1,   115200, %s\n' "$i" "$i" "$h"
        done
    } > "$f"
}
gen_md5 "$runtime/ref.md5" 30
cp "$runtime/ref.md5" "$runtime/hw.md5"
gen_md5 "$runtime/hw-1frame.md5" 1
gen_md5 "$runtime/hw-diff.md5" 30 bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
gen_md5 "$runtime/hw-nonl.md5" 30; truncate -s -1 "$runtime/hw-nonl.md5"          # 缺尾换行 -> no_trailing_newline
gen_md5 "$runtime/hw-badline.md5" 30; echo '0, 1, 1, 1, 115200' >> "$runtime/hw-badline.md5"          # 5 字段
gen_md5 "$runtime/hw-badhash.md5" 30; echo '0, 30, 30, 1, 115200, zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz' >> "$runtime/hw-badhash.md5"
gen_md5 "$runtime/hw-badsize.md5" 30; echo '0, 30, 30, 1, 460800, aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' >> "$runtime/hw-badsize.md5"
gen_md5 "$runtime/hw-nodim.md5" 30; sed -i '/^#dimensions/d' "$runtime/hw-nodim.md5"
gen_md5 "$runtime/hw-zeroframes.md5" 0
gen_md5 "$runtime/ref-zero.md5" 0
: > "$runtime/hw-empty.md5"

# ---- /proc 状态 fixture（与真机 /proc/driver/innogpu/gpu00/status 相同格式）----
cat > "$runtime/status-ok" <<'ST'
Driver Status:   OK
Device ID: 0:128
Firmware Status: OK
Server Errors:   0
HWR Event Count: 0
CRR Event Count: 0
SLR Event Count: 0
WGP Error Count: 0
TRP Error Count: 0
FWF Event Count: 0
APM Event Count: 0
GPU Utilisation: -
ST
sed 's/Driver Status:   OK/Driver Status:   FAIL/' "$runtime/status-ok" > "$runtime/status-driver-bad"
sed 's/Firmware Status: OK/Firmware Status: FAIL/' "$runtime/status-ok" > "$runtime/status-fw-bad"
sed '/^WGP Error Count:/d' "$runtime/status-ok" > "$runtime/status-missing-field"   # 7 个计数（缺 WGP）
sed 's/^WGP Error Count: 0/WGP Error Count: abc/' "$runtime/status-ok" > "$runtime/status-non-numeric"
sed 's/^Driver Status:   OK/Driver Status: OKAY/' "$runtime/status-ok" > "$runtime/status-driver-okay"   # OKAY 不得当作 OK
sed 's/^Server Errors:   0/Server Errors: bad 0/' "$runtime/status-ok" > "$runtime/status-bad-count"   # 计数行带异常 token
sed '/^Server Errors:/a Server Errors:   0' "$runtime/status-ok" > "$runtime/status-dup-field"   # 重复计数字段
sed 's/^Server Errors:   0/Server Errors:   08/' "$runtime/status-ok" > "$runtime/status-ok8"   # 前导零 08
sed 's/^Server Errors:   0/Server Errors:   09/' "$runtime/status-ok" > "$runtime/status-ok9"   # 前导零 09
sed -e 's/Server Errors:   0/Server Errors:   1/' \
    -e 's/WGP Error Count: 0/WGP Error Count: 1/' \
    -e 's/FWF Event Count: 0/FWF Event Count: 1/' \
    "$runtime/status-ok" > "$runtime/status-high"

passed=0; failed=0; t=0
pass() { t=$((t+1)); passed=$((passed+1)); printf 'vaapi_decode_t%02d=PASS # %s\n' "$t" "$1"; }
fail() { t=$((t+1)); failed=$((failed+1)); printf 'vaapi_decode_t%02d=FAIL reason=%s\n' "$t" "$2"; }

run_rc() {
    local label="$1" want="$2" envspec="$3"; shift 3
    local out rc
    out="$(env $envspec bash "$SCRIPT" "$@" 2>&1)"
    rc=$?
    if [ "$rc" -eq "$want" ]; then pass "$label"; else fail "$label" "rc=$rc want=$want out=$(echo "$out" | tail -1)"; fi
}

OK="FFMPEG_BIN=$runtime/bin/ffmpeg VAINFO_BIN=$runtime/bin/vainfo INNOGPU_VAAPI_FIXTURE_MODE=1"
GOOD="$OK INNOGPU_VAAPI_SKIP_DEVICE_CHECKS=1 FAKE_SYSFS_ROOT=$runtime/sysfs FAKE_REF_FRAMEMD5=$runtime/ref.md5 FAKE_HW_FRAMEMD5=$runtime/hw.md5 INNOGPU_VAAPI_STATUS_FILE=$runtime/status-ok"

# ---- 参数校验（设备检测前 rc=2）----
run_rc param_missing_codec 2 "" --timeout 10
run_rc param_bad_codec     2 "$OK" --codec av1
run_rc codec_missing_value 2 "$OK" --codec
run_rc timeout_bad_value   2 "$OK" --codec h264 --timeout nope
run_rc timeout_missing_val 2 "$OK" --codec h264 --timeout
# ---- fixture 模式标记强制 ----
run_rc fixture_hooks_require_mode 2 "FFMPEG_BIN=$runtime/bin/ffmpeg VAINFO_BIN=$runtime/bin/vainfo" --codec h264 --device /dev/dri/renderD128
# ---- 工具缺失 / codec 能力（rc=2）----
run_rc ffmpeg_missing      2 "FFMPEG_BIN=/nonexistent/ffmpeg VAINFO_BIN=$runtime/bin/vainfo INNOGPU_VAAPI_FIXTURE_MODE=1" --codec all
run_rc ffmpeg_no_vaapi     2 "$OK FAKE_NO_VAAPI=1" --codec all
run_rc encoder_missing     2 "$GOOD FAKE_NO_X264=1" --codec all
run_rc encoder_x265_required_for_all 2 "$GOOD FAKE_NO_X265=1" --codec all
run_rc encoder_x265_not_needed 0 "$GOOD FAKE_NO_X265=1" --codec h264 --device /dev/dri/renderD128
run_rc decoder_missing     2 "$GOOD FAKE_NO_DECODER_H264=1" --codec h264 --device /dev/dri/renderD128
run_rc decoder_hevc_only_ok 0 "$GOOD FAKE_NO_DECODER_H264=1" --codec hevc --device /dev/dri/renderD128
run_rc vainfo_missing      2 "FFMPEG_BIN=$runtime/bin/ffmpeg VAINFO_BIN=/nonexistent/vainfo INNOGPU_VAAPI_FIXTURE_MODE=1" --codec all
# ---- 设备/身份（rc=3）----
run_rc device_missing      3 "$OK" --codec all
printf 'x' > "$runtime/plain-node"
run_rc device_not_char     3 "$OK" --codec all --device "$runtime/plain-node"
run_rc pci_mismatch        3 "$OK INNOGPU_VAAPI_SKIP_DEVICE_CHECKS=1 FAKE_SYSFS_ROOT=$runtime/sysfs-intel" --codec all --device /dev/dri/renderD128
run_rc vainfo_not_inno     3 "$OK INNOGPU_VAAPI_SKIP_DEVICE_CHECKS=1 FAKE_SYSFS_ROOT=$runtime/sysfs FAKE_VAINFO_INTEL=1" --codec all --device /dev/dri/renderD128
run_rc vainfo_no_h264_profile 3 "$GOOD FAKE_VAINFO_NO_H264=1" --codec h264 --device /dev/dri/renderD128
run_rc vainfo_hevc_ok      0 "$GOOD FAKE_VAINFO_NO_H264=1" --codec hevc --device /dev/dri/renderD128
# ---- 解码链失败路径 ----
run_rc encode_fail         4 "$GOOD FAKE_ENCODE_FAIL=1" --codec h264 --device /dev/dri/renderD128
run_rc ref_fail            4 "$GOOD FAKE_REF_FAIL=1" --codec h264 --device /dev/dri/renderD128
run_rc hw_fail             1 "$GOOD FAKE_HW_FAIL=1" --codec h264 --device /dev/dri/renderD128
run_rc timeout_hw          5 "$GOOD FAKE_HW_TIMEOUT=1" --codec h264 --device /dev/dri/renderD128 --timeout 1
# ---- 输出校验：真实 framemd5 格式（rc=1）----
run_rc frame_count_mismatch 1 "$GOOD FAKE_HW_FRAMEMD5=$runtime/hw-1frame.md5" --codec h264 --device /dev/dri/renderD128
run_rc hash_mismatch       1 "$GOOD FAKE_HW_FRAMEMD5=$runtime/hw-diff.md5" --codec h264 --device /dev/dri/renderD128
run_rc malformed_line      1 "$GOOD FAKE_HW_FRAMEMD5=$runtime/hw-badline.md5" --codec h264 --device /dev/dri/renderD128
run_rc bad_hash            1 "$GOOD FAKE_HW_FRAMEMD5=$runtime/hw-badhash.md5" --codec h264 --device /dev/dri/renderD128
run_rc bad_size            1 "$GOOD FAKE_HW_FRAMEMD5=$runtime/hw-badsize.md5" --codec h264 --device /dev/dri/renderD128
run_rc no_dimensions       1 "$GOOD FAKE_HW_FRAMEMD5=$runtime/hw-nodim.md5" --codec h264 --device /dev/dri/renderD128
run_rc empty_output_both   1 "$GOOD FAKE_REF_FRAMEMD5=$runtime/ref-zero.md5 FAKE_HW_FRAMEMD5=$runtime/hw-zeroframes.md5" --codec h264 --device /dev/dri/renderD128
run_rc empty_file          1 "$GOOD FAKE_HW_FRAMEMD5=$runtime/hw-empty.md5" --codec h264 --device /dev/dri/renderD128
# ---- 状态门禁（独立 gate，rc=1；pre 无效在解码前即失败）----
run_rc status_missing      1 "$GOOD INNOGPU_VAAPI_STATUS_FILE=/nonexistent/status" --codec h264 --device /dev/dri/renderD128
run_rc status_driver_not_ok 1 "$GOOD INNOGPU_VAAPI_STATUS_FILE=$runtime/status-driver-bad" --codec h264 --device /dev/dri/renderD128
run_rc status_firmware_not_ok 1 "$GOOD INNOGPU_VAAPI_STATUS_FILE=$runtime/status-fw-bad" --codec h264 --device /dev/dri/renderD128
run_rc missing_count_field 1 "$GOOD INNOGPU_VAAPI_STATUS_FILE=$runtime/status-missing-field" --codec h264 --device /dev/dri/renderD128
run_rc non_numeric_count  1 "$GOOD INNOGPU_VAAPI_STATUS_FILE=$runtime/status-non-numeric" --codec h264 --device /dev/dri/renderD128
run_rc status_driver_okay 1 "$GOOD INNOGPU_VAAPI_STATUS_FILE=$runtime/status-driver-okay" --codec h264 --device /dev/dri/renderD128
run_rc status_bad_count   1 "$GOOD INNOGPU_VAAPI_STATUS_FILE=$runtime/status-bad-count" --codec h264 --device /dev/dri/renderD128
run_rc status_dup_field   1 "$GOOD INNOGPU_VAAPI_STATUS_FILE=$runtime/status-dup-field" --codec h264 --device /dev/dri/renderD128
run_rc mktemp_failure     2 "$GOOD TMPDIR=/nonexistent/tmpdir" --codec h264 --device /dev/dri/renderD128

# 08 -> 09：规范十进制后应判定增长（防 Bash 八进制把 09 当非法而漏报为未增长）
O="$runtime/leadzero.out"
ln -sf "$runtime/status-ok8" "$runtime/status-link3"
env $GOOD INNOGPU_VAAPI_STATUS_FILE=$runtime/status-link3 FAKE_SWAP_STATUS_LINK=$runtime/status-link3 FAKE_SWAP_STATUS_TARGET=$runtime/status-ok9 \
    bash "$SCRIPT" --codec h264 --device /dev/dri/renderD128 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -qF 'driver_error_counts_increased:Server(8->9)' "$O"; then
    pass status_leading_zero_growth
else
    fail status_leading_zero_growth "rc=$rc out=$(grep -o 'fixture_vaapi_decode_status=.*' "$O")"
fi

# 尾换行缺失 = 输出截断异常 -> FAIL（no_trailing_newline）
O="$runtime/nonl.out"
env $GOOD FAKE_HW_FRAMEMD5=$runtime/hw-nonl.md5 bash "$SCRIPT" --codec h264 --device /dev/dri/renderD128 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q 'reason=hw_no_trailing_newline' "$O"; then pass no_trailing_newline; else fail no_trailing_newline "rc=$rc out=$(grep vaapi_decode_h264 "$O")"; fi

# 帧数 reason 内容
O="$runtime/fc.out"
env $GOOD FAKE_HW_FRAMEMD5=$runtime/hw-1frame.md5 bash "$SCRIPT" --codec h264 --device /dev/dri/renderD128 > "$O" 2>&1
if grep -q 'reason=hw_frame_count=1' "$O"; then pass reason_contains_frame_count; else fail reason_contains_frame_count "out=$(grep vaapi_decode_h264 "$O")"; fi

# fixture 模式成功路径：独立命名空间 fixture_*（控制流 PASS，rc=0 一致），绝不输出任何 vaapi_decode_* 权威行
O="$runtime/success.out"
env $GOOD bash "$SCRIPT" --codec all --device /dev/dri/renderD128 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 0 ] \
   && grep -q 'fixture_vaapi_decode_overall=PASS' "$O" \
   && ! grep -qE '(^| )vaapi_decode_overall=' "$O" \
   && ! grep -qE '(^| )vaapi_decode_h264=PASS' "$O" \
   && ! grep -qE '(^| )vaapi_decode_hevc=PASS' "$O" \
   && grep -q 'fixture_tests_total=2 fixture_tests_passed=2 fixture_tests_failed=0' "$O" \
   && grep -q 'fixture_vaapi_decode_h264=PASS reason=.*-mode=fixture' "$O" \
   && grep -q 'fixture_vaapi_decode_hevc=PASS reason=.*-mode=fixture' "$O" \
   && grep -q 'fixture_vaapi_decode_status=ok mode=fixture' "$O"; then
    pass success_all_fixture
else
    fail success_all_fixture "rc=$rc out=$(tail -4 "$O" | tr '\n' ';')"
fi

# 聚合：单 codec 失败 -> 整体 FAIL，计数一致
O="$runtime/agg.out"
env $GOOD FAKE_HEVC_ONLY_FAIL=1 bash "$SCRIPT" --codec all --device /dev/dri/renderD128 > "$O" 2>&1; rc=$?
if [ "$rc" -ne 0 ] && grep -q 'fixture_vaapi_decode_h264=PASS' "$O" && grep -q 'fixture_vaapi_decode_hevc=FAIL' "$O" && grep -q 'fixture_tests_total=2 fixture_tests_passed=1 fixture_tests_failed=1' "$O"; then
    pass aggregate_one_fail
else
    fail aggregate_one_fail "rc=$rc out=$(tail -2 "$O" | tr '\n' ';')"
fi

# 硬解参数断言：-hwaccel_output_format vaapi + hwdownload,format=nv12（无软件回退路径）
: > "$runtime/args.log"
env $GOOD FAKE_LOG_ARGS=1 FAKE_LOG_FILE=$runtime/args.log bash "$SCRIPT" --codec all --device /dev/dri/renderD128 >/dev/null 2>&1
if grep -q '\-hwaccel vaapi' "$runtime/args.log" \
   && grep -q '\-hwaccel_output_format vaapi' "$runtime/args.log" \
   && grep -q 'hwdownload,format=nv12' "$runtime/args.log"; then
    pass hw_args_assert
else
    fail hw_args_assert "log=$(tr '\n' '|' < "$runtime/args.log" | cut -c1-200)"
fi

# 错误计数增长（8 类中三类增长，before=0 after=3）-> 状态 FAIL
O="$runtime/status-inc.out"
ln -sf "$runtime/status-ok" "$runtime/status-link"
env $GOOD INNOGPU_VAAPI_STATUS_FILE=$runtime/status-link FAKE_SWAP_STATUS_LINK=$runtime/status-link FAKE_SWAP_STATUS_TARGET=$runtime/status-high \
    bash "$SCRIPT" --codec h264 --device /dev/dri/renderD128 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -qF 'reason=driver_error_counts_increased:Server(0->1) WGP(0->1) FWF(0->1)' "$O"; then
    pass status_errors_increased
else
    fail status_errors_increased "rc=$rc out=$(grep -o 'fixture_vaapi_decode_status=.*' "$O")"
fi

# post 快照无效（解码中状态文件被替换为缺字段）-> 状态 FAIL
O="$runtime/post-invalid.out"
ln -sf "$runtime/status-ok" "$runtime/status-link2"
env $GOOD INNOGPU_VAAPI_STATUS_FILE=$runtime/status-link2 FAKE_SWAP_STATUS_LINK=$runtime/status-link2 FAKE_SWAP_STATUS_TARGET=$runtime/status-missing-field \
    bash "$SCRIPT" --codec h264 --device /dev/dri/renderD128 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q 'reason=post_missing_count_fields:5' "$O"; then
    pass post_status_invalid
else
    fail post_status_invalid "rc=$rc out=$(grep -o 'fixture_vaapi_decode_status=.*' "$O")"
fi

# TERM 信号：清理后退出 143（bash 延迟 trap 至当前前台命令结束，无孤儿/残留）
O="$runtime/term.out"
env $GOOD FAKE_HW_SLEEP=2 TMPDIR=$runtime/scratch bash "$SCRIPT" --codec h264 --device /dev/dri/renderD128 > "$O" 2>&1 &
spid=$!
sleep 0.6
kill -TERM "$spid"
wait "$spid"; rc=$?
sleep 3   # 等被延迟的 trap 退出与前台命令收尾
if [ "$rc" -eq 143 ] && ! pgrep -f "$runtime/bin/ffmpeg" >/dev/null 2>&1 && [ -z "$(find "$runtime/scratch" -maxdepth 1 -name 'vaapi-decode.*' 2>/dev/null)" ]; then
    pass term_cleanup
else
    fail term_cleanup "rc=$rc"
fi

env $GOOD TMPDIR=$runtime/scratch bash "$SCRIPT" --codec all --device /dev/dri/renderD128 >/dev/null 2>&1
if ! pgrep -f "$runtime/bin/ffmpeg" >/dev/null 2>&1 && [ -z "$(find "$runtime/scratch" -maxdepth 1 -name 'vaapi-decode.*' 2>/dev/null)" ]; then
    pass no_residue
else
    fail no_residue "leftover process or vaapi-decode temp dir"
fi

BASELINE_BEFORE="$(sha256sum "$ROOT/baselines/latest-runtime-baseline.txt" 2>/dev/null | cut -d' ' -f1)"
env $GOOD bash "$SCRIPT" --codec all --device /dev/dri/renderD128 >/dev/null 2>&1
BASELINE_AFTER="$(sha256sum "$ROOT/baselines/latest-runtime-baseline.txt" 2>/dev/null | cut -d' ' -f1)"
if [ "$BASELINE_BEFORE" == "$BASELINE_AFTER" ]; then pass baseline_untouched; else fail baseline_untouched "baseline changed"; fi

printf 'tests_total=%d tests_passed=%d tests_failed=%d tests_skipped=0\n' "$t" "$passed" "$failed"
[ "$failed" -eq 0 ]
