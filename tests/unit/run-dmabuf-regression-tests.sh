#!/bin/bash
# Unit tests: tools/run-dmabuf-regression-test.sh control flow with fake fixtures.
# CI-safe without /dev/dri. Fixture mode (INNOGPU_DMABUF_FIXTURE_MODE=1) uses the
# independent fixture_dmabuf_* namespace, never emits authoritative dmabuf_* lines,
# and never touches baselines/.

set -u -o pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SCRIPT="$ROOT/tools/run-dmabuf-regression-test.sh"
runtime="$(mktemp -d "${TMPDIR:-/tmp}/inno-dmabuf-tests.XXXXXX")"
trap 'rm -rf "$runtime"' EXIT

# ---- fake sysfs/dev 环境 ----
PCI_INNO="$runtime/sysfs/devices/pci0000:00/0000:00:01.2/0000:02:00.0"
PCI_OTHER="$runtime/sysfs/devices/pci0000:00/0000:00:01.2/0000:03:00.0"
mkdir -p "$PCI_INNO" "$PCI_OTHER"
printf '0x1ec8\n' > "$PCI_INNO/vendor"; printf '0x9810\n' > "$PCI_INNO/device"
printf '0x1ec8\n' > "$PCI_OTHER/vendor"; printf '0x9810\n' > "$PCI_OTHER/device"
for n in renderD128 card0; do
    mkdir -p "$runtime/sysfs/class/drm/$n"
    ln -sf "$PCI_INNO" "$runtime/sysfs/class/drm/$n/device"
done
# 多目标：renderD129 也指向同一 inno PCI
mkdir -p "$runtime/sysfs/class/drm/renderD129"
ln -sf "$PCI_INNO" "$runtime/sysfs/class/drm/renderD129/device"
# intel sysfs（身份不匹配）
mkdir -p "$runtime/sysfs-intel/class/drm/renderD128" "$runtime/sysfs-intel/devices/pci0000:00/0000:00:02.0"
printf '0x8086\n' > "$runtime/sysfs-intel/devices/pci0000:00/0000:00:02.0/vendor"
printf '0x5912\n' > "$runtime/sysfs-intel/devices/pci0000:00/0000:00:02.0/device"
ln -sf "$runtime/sysfs-intel/devices/pci0000:00/0000:00:02.0" "$runtime/sysfs-intel/class/drm/renderD128/device"

mkdir -p "$runtime/dev/by-path"
: > "$runtime/dev/renderD128"; : > "$runtime/dev/card0"; : > "$runtime/dev/renderD129"
ln -sf ../renderD128 "$runtime/dev/by-path/pci-0000:02:00.0-render"
ln -sf ../card0 "$runtime/dev/by-path/pci-0000:02:00.0-card"

# ---- 状态 fixture（真机 /proc 相同格式）----
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
sed 's/Driver Status:   OK/Driver Status: OKAY/' "$runtime/status-ok" > "$runtime/status-okay"
sed 's/^Server Errors:   0/Server Errors: bad 0/' "$runtime/status-ok" > "$runtime/status-badcount"
sed '/^Server Errors:/a Server Errors:   0' "$runtime/status-ok" > "$runtime/status-dup"
sed '/^WGP Error Count:/d' "$runtime/status-ok" > "$runtime/status-missing"
sed 's/^WGP Error Count: 0/WGP Error Count: abc/' "$runtime/status-ok" > "$runtime/status-nonnum"
sed 's/^Server Errors:   0/Server Errors:   1/' "$runtime/status-ok" > "$runtime/status-high"

# ---- 内核日志 fixture ----
: > "$runtime/klog-clean"
printf 'drm: innogpu error line\n' > "$runtime/klog-error"
printf 'drm: GPU HANG: ecode 0x0000:0x00000000, reason: Render engine hang\n' > "$runtime/klog-gpu-hang"
printf 'dma_buf: timeout waiting for fence\n' > "$runtime/klog-dmabuf-timeout"
# benign 日志：裸子串误伤反例（debug/installed/hangcheck 必须保持 clean）
printf 'drm: debug enabled\n' > "$runtime/klog-benign-debug"
printf 'drm: helper installed\n' > "$runtime/klog-benign-installed"
printf 'GPU hangcheck initialized\n' > "$runtime/klog-benign-hangcheck"
printf 'drm: line-a\ndrm: line-b\n' > "$runtime/klog-two"
printf 'drm: line-b\ndrm: line-a\n' > "$runtime/klog-two-reordered"
printf 'drm: line-a\ndrm: line-insert\ndrm: line-b\n' > "$runtime/klog-two-inserted"

mkdir -p "$runtime/bin" "$runtime/scratch"

# ---- fake gcc（编译失败测试用）----
cat > "$runtime/bin/fake-cc-fail" <<'FAKECC'
#!/bin/bash
echo "fake cc: compile failed" >&2
exit 1
FAKECC
chmod +x "$runtime/bin/fake-cc-fail"

# ---- fake self-import 探针 ----
cat > "$runtime/bin/fake-self-import" <<'FAKESELF'
#!/bin/bash
if [[ -n "${FAKE_SELF_CAPABILITY:-}" ]]; then echo "capability=$FAKE_SELF_CAPABILITY"; exit 3; fi
if [[ "${FAKE_SELF_RC:-0}" != "0" ]]; then echo "fake self-import failed" >&2; exit "$FAKE_SELF_RC"; fi
if [[ "${FAKE_SELF_SLEEP:-0}" == "1" ]]; then sleep 5; fi
iters="${3:-3}"
echo "self_import device=${FAKE_SELF_BAD_DEVICE:-$1} size=$2 iterations=$iters"
rns="${FAKE_SELF_ROUND_NUMBERS:-}"
if [[ -z "$rns" ]]; then
    rns=""
    for ((i=1; i<=${FAKE_SELF_PRINT_ROUNDS:-$iters}; i++)); do rns="${rns:+$rns }$i"; done
fi
IFS=, read -ra RNS <<<"${rns// /,}"
for rn in "${RNS[@]}"; do
    cs=${FAKE_SELF_MISSING_CREATE_SIZE:+""}; cs="${FAKE_SELF_CREATE_SIZE:-7647232}"
    ce=${FAKE_SELF_MISSING_CLOEXEC:+""}; ce="${FAKE_SELF_BAD_CLOEXEC:-${FAKE_SELF_CLOEXEC:-yes}}"
    if [[ -n "${FAKE_SELF_MISSING_CREATE_SIZE:-}" ]]; then
        echo "round=$rn handle=100 exported_fd=7 cloexec=$ce imported_handle=100 imported_same=yes ok"
    elif [[ -n "${FAKE_SELF_MISSING_CLOEXEC:-}" ]]; then
        echo "round=$rn handle=100 create_size=$cs exported_fd=7 imported_handle=100 imported_same=yes ok"
    else
        echo "round=$rn handle=100 create_size=$cs exported_fd=7 cloexec=$ce imported_handle=100 imported_same=yes ok"
    fi
done
if [[ "${FAKE_SELF_DUP_SUMMARY:-0}" == "1" ]]; then
    echo "summary self_import rounds=$iters success=${FAKE_SELF_SUCCESS:-$iters} failures=${FAKE_SELF_FAILURES:-0} fds_before=${FAKE_SELF_FDS_BEFORE:-8} fds_after=${FAKE_SELF_FDS_AFTER:-8} fd_leak=${FAKE_SELF_LEAK:-no}"
fi
if [[ "${FAKE_SELF_BAD_SUMMARY:-0}" == "1" ]]; then
    echo "summary self_import garbage"
else
    echo "summary self_import rounds=${FAKE_SELF_SUMMARY_ROUNDS:-$iters} success=${FAKE_SELF_SUCCESS:-$iters} failures=${FAKE_SELF_FAILURES:-0} fds_before=${FAKE_SELF_FDS_BEFORE:-8} fds_after=${FAKE_SELF_FDS_AFTER:-8} fd_leak=${FAKE_SELF_LEAK:-no}"
fi
exit 0
FAKESELF

# ---- fake invisible-read 探针（read/write 两模式 + 故障开关）----
cat > "$runtime/bin/fake-invisible-read" <<'FAKEREAD'
#!/bin/bash
acc="$4"
if [[ "${FAKE_READ_RC:-0}" != "0" ]]; then echo "fake invisible failed" >&2; exit "$FAKE_READ_RC"; fi
if [[ -n "${FAKE_SWAP_STATUS_LINK:-}" && -n "${FAKE_SWAP_STATUS_TARGET:-}" ]]; then
    ln -sf "$FAKE_SWAP_STATUS_TARGET" "$FAKE_SWAP_STATUS_LINK"
fi
if [[ -n "${FAKE_SWAP_KLOG_LINK:-}" && -n "${FAKE_SWAP_KLOG_TARGET:-}" ]]; then
    ln -sf "$FAKE_SWAP_KLOG_TARGET" "$FAKE_SWAP_KLOG_LINK"
fi
iters="${3:-3}"
pages="${FAKE_READ_PAGES:-1867}"
msize="${FAKE_READ_MSIZE:-7647232}"
echo "device=${FAKE_READ_BAD_DEVICE:-$1} handle=200 requested_size=${FAKE_READ_BAD_SIZE:-$2} map_size=$msize pages=$pages offset=0x0 iterations=${FAKE_READ_ITER:-$iters} access=${FAKE_READ_BAD_ACCESS:-$acc} page_stride=${FAKE_READ_BAD_STRIDE:-1}"
for ((i=1; i<=iters; i++)); do
    ms="${FAKE_READ_MUNMAP_MS:-2.500}"
    [[ "${FAKE_READ_DROP:-0}" == "1" && "$i" -eq 1 ]] && : || echo "iteration=$i phase=${acc}_touch wall_ms=3.000 user_ms=1.000 system_ms=1.500"
    [[ "${FAKE_READ_DUP:-0}" == "1" && "$i" -eq 1 ]] && echo "iteration=1 phase=${acc}_touch wall_ms=3.000 user_ms=1.000 system_ms=1.500"
    if [[ "${FAKE_READ_DROP_MUNMAP:-0}" == "1" && "$i" -eq 1 ]]; then :; else
        echo "iteration=$i phase=${acc}_munmap wall_ms=$ms user_ms=0.200 system_ms=${FAKE_READ_SYSMS:-$ms}"
    fi
    if [[ "$acc" == "write" ]]; then
        echo "iteration=$i phase=verify_touch wall_ms=2.000 user_ms=1.000 system_ms=1.000"
        [[ "${FAKE_READ_DUP_VTOUCH:-0}" == "1" ]] && echo "iteration=$i phase=verify_touch wall_ms=2.000 user_ms=1.000 system_ms=1.000"
        echo "iteration=$i phase=verify_munmap wall_ms=0.500 user_ms=0.100 system_ms=0.200"
        [[ "${FAKE_READ_NOVERIFY:-0}" == "1" && "$i" -eq 1 ]] || echo "iteration=$i verify=pass pages=${FAKE_READ_VERIFY_PAGES:-$pages}"
    fi
done
echo "checksum=0"
exit 0
FAKEREAD

# ---- fake topology 探针 ----
cat > "$runtime/bin/fake-topology" <<'FAKETOP'
#!/bin/bash
if [[ "${FAKE_TOP_RC:-0}" != "0" ]]; then echo "fake topology failed" >&2; exit "$FAKE_TOP_RC"; fi
if [[ -n "${FAKE_TOP_CRTC_LIST:-}" ]]; then
    hdr="${FAKE_TOP_HDR_CRTCS:-4}"
    echo "device=$1 crtcs=$hdr connectors=1 encoders=1"
    [[ "${FAKE_TOP_DUP_HEADER:-0}" == "1" ]] && echo "device=$1 crtcs=$hdr connectors=1 encoders=1"
    echo "CRTCs:"
    IFS=, read -ra TOP_ENTRIES <<<"$FAKE_TOP_CRTC_LIST"
    for entry in "${TOP_ENTRIES[@]}"; do
        i="${entry%%=*}"; st="${entry##*=}"
        if [[ "$st" == "yes" && "${FAKE_TOP_UNNAMED_MODE:-0}" == "1" ]]; then
            echo "  index=$i id=$((10+i)) active=$st fb=1 position=0,0 size=1920x1080 mode=<unnamed> refresh=60"   # 真实形态：mode_valid=yes 但 mode 名称为空
        else
            echo "  index=$i id=$((10+i)) active=$st fb=1 position=0,0 size=1920x1080 mode=1920x1080 refresh=60"
        fi
    done
    echo "Connectors:"
    echo "  type=10 type_id=0 id=5 status=connected mm=597x336 modes=1 encoder=3 crtc_id=11 crtc_index=1"
    exit 0
fi
# 动态计数：CRTC 行数 = active + inactive 索引数，header crtcs 必须一致
n_active=0; n_inactive=0
if [[ -n "${FAKE_TOP_ACTIVE:-}" ]]; then for _ in ${FAKE_TOP_ACTIVE//,/ }; do n_active=$((n_active+1)); done; fi
if [[ -n "${FAKE_TOP_INACTIVE:-}" ]]; then for _ in ${FAKE_TOP_INACTIVE//,/ }; do n_inactive=$((n_inactive+1)); done; fi
echo "device=$1 crtcs=$((n_active+n_inactive)) connectors=1 encoders=1"
echo "CRTCs:"
if [[ "${FAKE_TOP_BAD:-0}" == "1" ]]; then
    echo "  index=1 id=11 active=maybe fb=1"
else
    if [[ -n "${FAKE_TOP_ACTIVE:-}" ]]; then for i in ${FAKE_TOP_ACTIVE//,/ }; do
        if [[ "${FAKE_TOP_UNNAMED_MODE:-0}" == "1" ]]; then
            echo "  index=$i id=$((10+i)) active=yes fb=1 position=0,0 size=1920x1080 mode=<unnamed> refresh=60"   # 真实形态：active 但 mode 名称为空
        else
            echo "  index=$i id=$((10+i)) active=yes fb=1 position=0,0 size=1920x1080 mode=1920x1080 refresh=60"
        fi
    done; fi
    if [[ -n "${FAKE_TOP_INACTIVE:-}" ]]; then for i in ${FAKE_TOP_INACTIVE//,/ }; do echo "  index=$i id=$((10+i)) active=no fb=0 position=0,0 size=0x0 mode=- refresh=0"; done; fi
fi
echo "Connectors:"
echo "  type=10 type_id=0 id=5 status=connected mm=597x336 modes=1 encoder=3 crtc_id=11 crtc_index=1"
exit 0
FAKETOP

# ---- fake vblank 探针（active/inactive 两模式）----
cat > "$runtime/bin/fake-vblank" <<'FAKEVBL'
#!/bin/bash
crtc="$2"; samples="${3:-10}"
if [[ "${FAKE_VBL_IGNORE_TERM:-0}" == "1" ]]; then trap '' TERM; fi
if [[ "${FAKE_VBL_SLEEP:-0}" == "1" ]]; then sleep 60; fi
if [[ "${FAKE_VBL_DUP_HEADER:-0}" == "1" ]]; then
    echo "device=$1 crtc=$crtc samples=$samples timeout_ms=$4"
fi
if [[ "${FAKE_VBL_BAD_DEVICE:-0}" == "1" && "$4" == "1500" ]] || [[ "${FAKE_VBL_BAD_DEVICE_INACTIVE:-0}" == "1" && "$4" == "500" ]]; then
    echo "device=/dev/dri/wrong crtc=$crtc samples=$samples timeout_ms=$4"
else echo "device=$1 crtc=$crtc samples=$samples timeout_ms=$([[ "$4" == "500" ]] && echo "${FAKE_VBL_BAD_TIMEOUT:-$4}" || echo "$4")"; fi
colhdr="sample sequence wait_ms kernel_time_ms sequence_delta kernel_delta_ms result"
[[ "${FAKE_VBL_BAD_COLHDR:-0}" == "1" ]] && colhdr="sample sequence wait_ms kernel_ms sequence_delta kernel_delta_ms result"
[[ "${FAKE_VBL_DUP_COLHEADER:-0}" == "1" ]] && echo "$colhdr"
echo "$colhdr"
if [[ ",${FAKE_VBL_FAIL_CRTCS:-}," == *",$crtc,"* ]]; then
  echo "fake: vblank failed for crtc $crtc" >&2; exit 1
fi
if [[ ",${FAKE_VBL_INACTIVE_CRTCS:-}," == *",$crtc,"* ]]; then
    mode="${FAKE_VBL_INACTIVE_MODE:-einval}"
    if [[ "${FAKE_VBL_DUP_SAMPLE:-0}" == "1" ]]; then
        echo "1 - 0.100 - - - error:Invalid argument errno=22"
        echo "1 - 0.100 - - - error:Invalid argument errno=22"
        echo "2 - 0.100 - - - error:Invalid argument errno=22"
        echo "summary success=0 failures=$samples avg_wait_ms=0.000 min_wait_ms=0.000 max_wait_ms=0.000 fast_returns=0 nonadvancing=0"
        exit 1
    fi
    for ((i=1; i<=samples; i++)); do
        case "$mode" in
            einval) echo "$i - 0.100 - - - error:Invalid argument errno=22" ;;
            timeout) echo "$i - 500.000 - - - timeout:Interrupted system call errno=4" ;;
            wrongerrno) echo "$i - 0.100 - - - error:Operation not permitted errno=1" ;;
            slow) echo "$i - 600.000 - - - error:Invalid argument errno=22" ;;
        esac
    done
    # 真实探针契约：success=0 时 min/max/avg 保持 0.0（失败样本不更新统计）
    isum_avg="0.000"; isum_min="0.000"; isum_max="0.000"
    [[ "${FAKE_VBL_INACTIVE_NONZERO:-0}" == "1" ]] && { isum_avg="0.100"; isum_min="0.100"; isum_max="0.100"; }   # 非零反例
    [[ "${FAKE_VBL_INACTIVE_BAD_FLOAT:-0}" == "1" ]] && isum_avg="1..2"   # 坏浮点反例
    if [[ "${FAKE_VBL_INACTIVE_FIELD_ORDER:-0}" == "1" ]]; then
        echo "summary success=0 failures=$samples avg_wait_ms=$isum_avg max_wait_ms=$isum_max min_wait_ms=$isum_min fast_returns=0 nonadvancing=0"   # 字段顺序反例
    else
        echo "summary success=0 failures=$samples avg_wait_ms=$isum_avg min_wait_ms=$isum_min max_wait_ms=$isum_max fast_returns=0 nonadvancing=0"
    fi
    exit 1
fi
okcount="${FAKE_VBL_OK:-$samples}"
printok="${FAKE_VBL_PRINT_OK:-$okcount}"
sumw=0; cnt=0; minw=""; maxw=""
for ((i=1; i<=samples; i++)); do
    if [[ "$i" -le "$okcount" ]]; then
        if [[ "$i" -gt "$printok" ]]; then continue; fi
        w="${FAKE_VBL_WAIT_MS:-16.500}"
        [[ "${FAKE_VBL_FAST:-0}" == "1" && "$i" -eq 1 ]] && w="0.500"
        delta="${FAKE_VBL_SEQDELTA:-1}"
        [[ "$i" -eq 1 ]] && delta="0"
        [[ "${FAKE_VBL_FIRST_DELTA:-0}" == "1" && "$i" -eq 1 ]] && delta="1"          # 首样本 delta 非零反例
        [[ "${FAKE_VBL_DELTA_MISMATCH:-0}" == "1" && "$i" -eq 2 ]] && delta="2"       # delta 与相邻 sequence 差不符反例
        # sequence：默认 100+i；WRAP=1 时在 2^32 边界真实回绕（合法）；SEQ_OOR=1 越界反例
        if [[ "${FAKE_VBL_WRAP:-0}" == "1" ]]; then
            seq=$(( i == 1 ? 4294967295 : i - 2 ))
            delta=0; [[ "$i" -gt 1 ]] && delta=1   # 0 - 4294967295 mod 2^32 == 1
        else
            seq=$((100+i))
        fi
        [[ "${FAKE_VBL_SEQ_OOR:-0}" == "1" && "$i" -eq 2 ]] && seq=4294967296         # uint32 越界反例
        # kernel 序列自洽：16.400 起每样本 +0.100；kernel_delta=0.100（首样本 0.000）；KD_MISMATCH 反例
        k=$(awk -v i="$i" 'BEGIN { printf "%.3f", 16.400 + (i - 1) * 0.100 }')
        kd="0.100"; [[ "$i" -eq 1 ]] && kd="0.000"
        [[ "${FAKE_VBL_KD_MISMATCH:-0}" == "1" && "$i" -eq 2 ]] && kd="0.200"         # kernel_delta 与相邻 kernel 差不符反例
        if [[ "${FAKE_VBL_K_REGRESS:-0}" == "1" && "$i" -eq 2 ]]; then
            k="15.400"; kd="1.000"   # 内核时间倒退反例（16.400 -> 15.400，delta 仍 +1）
        fi
        if [[ "${FAKE_VBL_K_MIN_REGRESS:-0}" == "1" && "$i" -eq 2 ]]; then
            k="16.399"; kd="0.000"   # 最小精度倒退反例（16.400 -> 16.399，delta 0.000）
        fi
        sn=$i
        [[ "${FAKE_VBL_SHIFT:-0}" == "1" ]] && sn=$((i+1))   # 样本号平移（越界反例）
        if [[ "${FAKE_VBL_REORDER:-0}" == "1" && "$i" -eq 1 ]]; then sn=2; fi
        if [[ "${FAKE_VBL_REORDER:-0}" == "1" && "$i" -eq 2 ]]; then sn=1; fi   # 顺序反例：1,2 对调
        echo "$sn $seq $w $k $delta $kd ok"
        sumw=$(awk -v a="$sumw" -v b="$w" 'BEGIN { printf "%.6f", a + b }')
        cnt=$((cnt+1))
        if [ -z "$minw" ] || awk -v a="$w" -v b="$minw" 'BEGIN { exit !(a < b) }'; then minw="$w"; fi
        if [ -z "$maxw" ] || awk -v a="$w" -v b="$maxw" 'BEGIN { exit !(a > b) }'; then maxw="$w"; fi
    else
        echo "$i - 500.000 - - - timeout:Interrupted system call errno=4"
    fi
done
f=0; [[ "${FAKE_VBL_FAST:-0}" == "1" ]] && f=1
na=0; [[ "${FAKE_VBL_NONADV:-0}" == "1" ]] && na=1
fail=$((samples - okcount))
avgv=$(awk -v s="$sumw" -v n="$cnt" 'BEGIN { printf "%.3f", (n > 0 ? s / n : 0) }')
minv="${minw:-0.000}"; maxv="${maxw:-0.000}"
[[ "${FAKE_VBL_BAD_FLOAT:-0}" == "1" ]] && avgv="1..2"          # 坏浮点反例
[[ "${FAKE_VBL_SUMMARY_METRICS_MISMATCH:-0}" == "1" ]] && { avgv="200.000"; minv="100.000"; maxv="300.000"; }   # 与样本不符的 summary 反例
if [[ "${FAKE_VBL_DUP_SUMMARY:-0}" == "1" ]]; then
    echo "summary success=$okcount failures=$fail avg_wait_ms=$avgv min_wait_ms=$minv max_wait_ms=$maxv fast_returns=$f nonadvancing=$na"
fi
echo "summary success=$okcount failures=$fail avg_wait_ms=$avgv min_wait_ms=$minv max_wait_ms=$maxv fast_returns=$f nonadvancing=$na"
exit $((fail > 0 || f > 0 || na > 0 ? 1 : 0))
FAKEVBL
chmod +x "$runtime"/bin/*

# ---- 公共环境 ----
OK="INNOGPU_DMABUF_FIXTURE_MODE=1"
GOOD="$OK FAKE_SYSFS_ROOT=$runtime/sysfs FAKE_DEV_DIR=$runtime/dev INNOGPU_DMABUF_SKIP_DEVICE_CHECKS=1 \
INNOGPU_DMABUF_STATUS_FILE=$runtime/status-ok INNOGPU_DMABUF_KERNEL_LOG=$runtime/klog-clean \
PROBE_SELF_IMPORT_BIN=$runtime/bin/fake-self-import PROBE_INVISIBLE_READ_BIN=$runtime/bin/fake-invisible-read \
PROBE_TOPOLOGY_BIN=$runtime/bin/fake-topology PROBE_VBLANK_BIN=$runtime/bin/fake-vblank \
FAKE_TOP_ACTIVE=1 FAKE_TOP_INACTIVE=0,2 FAKE_VBL_INACTIVE_CRTCS=0,2"
# 编译测试用：vblank 探针不覆盖（走真实编译），其余覆盖
GOOD_CC="$OK FAKE_SYSFS_ROOT=$runtime/sysfs FAKE_DEV_DIR=$runtime/dev INNOGPU_DMABUF_SKIP_DEVICE_CHECKS=1 \
INNOGPU_DMABUF_STATUS_FILE=$runtime/status-ok INNOGPU_DMABUF_KERNEL_LOG=$runtime/klog-clean \
PROBE_SELF_IMPORT_BIN=$runtime/bin/fake-self-import PROBE_INVISIBLE_READ_BIN=$runtime/bin/fake-invisible-read \
PROBE_TOPOLOGY_BIN=$runtime/bin/fake-topology FAKE_TOP_ACTIVE=1 FAKE_TOP_INACTIVE=0,2 FAKE_VBL_INACTIVE_CRTCS=0,2"

passed=0; failed=0; t=0
pass() { t=$((t+1)); passed=$((passed+1)); printf 'dmabuf_t%02d=PASS # %s\n' "$t" "$1"; }
fail() { t=$((t+1)); failed=$((failed+1)); printf 'dmabuf_t%02d=FAIL reason=%s\n' "$t" "$2"; }

run_rc() { # <label> <want> <envspec> <args...>
    local label="$1" want="$2" envspec="$3"; shift 3
    local out rc
    out="$(env $envspec bash "$SCRIPT" "$@" 2>&1)"
    rc=$?
    if [ "$rc" -eq "$want" ]; then pass "$label"; else fail "$label" "rc=$rc want=$want out=$(echo "$out" | tail -1)"; fi
}

# ================= 1. 参数校验（设备检测前 rc=2） =================
run_rc param_unknown_opt 2 "$OK" --bogus
run_rc param_missing_size 2 "$OK" --size
run_rc param_size_zero 2 "$OK" --size 0
run_rc param_size_negative 2 "$OK" --size -5
run_rc param_iterations_alpha 2 "$OK" --iterations nope
run_rc param_timeout_zero 2 "$OK" --timeout 0
run_rc param_munmap_zero 2 "$OK" --read-munmap-limit-ms 0
run_rc param_munmap_huge 2 "$OK" --read-munmap-limit-ms 999999999
run_rc param_vblank_overflow 2 "$OK" --vblank-samples 10001

# ================= 2. fixture 门禁 + 命名空间 =================
run_rc fixture_hooks_require_mode 2 "FAKE_SYSFS_ROOT=$runtime/sysfs" --size 7646720
O="$runtime/success.out"
env $GOOD TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 --iterations 3 --vblank-samples 10 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 0 ] \
   && grep -q 'fixture_dmabuf_regression_overall=PASS' "$O" \
   && ! grep -qE '(^| )dmabuf_' "$O" \
   && grep -q 'fixture_tests_total=5 fixture_tests_passed=5 fixture_tests_failed=0 fixture_tests_skipped=0 fixture_tests_unverified=0' "$O" \
   && grep -q 'fixture_dmabuf_self_import=PASS reason=.*-mode=fixture' "$O" \
   && grep -q 'fixture_dmabuf_invisible_read=PASS reason=.*-mode=fixture' "$O" \
   && grep -q 'fixture_dmabuf_write_readback=PASS reason=.*-mode=fixture' "$O" \
   && grep -q 'fixture_dmabuf_vblank_active=PASS reason=.*-mode=fixture' "$O" \
   && grep -q 'fixture_dmabuf_vblank_inactive_guard=PASS reason=.*-mode=fixture' "$O" \
   && grep -q 'fixture_dmabuf_status=ok mode=fixture' "$O"; then
    pass fixture_success_namespace
else
    fail fixture_success_namespace "rc=$rc out=$(tail -4 "$O" | tr '\n' ';')"
fi

# ================= 3. 工具/编译失败 (rc=2) =================
run_rc cc_missing 2 "$GOOD_CC PROBE_CC=/nonexistent/gcc" --size 7646720
run_rc compile_failed 2 "$GOOD_CC PROBE_CC=$runtime/bin/fake-cc-fail" --size 7646720
run_rc probe_bin_missing 1 "$GOOD PROBE_VBLANK_BIN=/nonexistent/fake-vblank" --size 7646720

# ================= 4. 设备发现与身份 (rc=3) =================
run_rc device_missing 3 "$OK INNOGPU_DMABUF_STATUS_FILE=$runtime/status-ok" --size 7646720
run_rc render_identity_mismatch 3 "$OK FAKE_SYSFS_ROOT=$runtime/sysfs-intel FAKE_DEV_DIR=$runtime/dev INNOGPU_DMABUF_SKIP_DEVICE_CHECKS=1 INNOGPU_DMABUF_STATUS_FILE=$runtime/status-ok" --render-device /dev/dri/renderD128 --card-device /dev/dri/card0
run_rc not_char_device 3 "$OK FAKE_SYSFS_ROOT=$runtime/sysfs INNOGPU_DMABUF_STATUS_FILE=$runtime/status-ok" --render-device "$runtime/dev/renderD128" --card-device /dev/dri/card0
mkdir -p "$runtime/dev-multi"
: > "$runtime/dev-multi/renderD128"; : > "$runtime/dev-multi/renderD129"; : > "$runtime/dev-multi/card0"
O="$runtime/multi.out"
env $OK FAKE_SYSFS_ROOT=$runtime/sysfs FAKE_DEV_DIR=$runtime/dev-multi INNOGPU_DMABUF_SKIP_DEVICE_CHECKS=1 INNOGPU_DMABUF_STATUS_FILE=$runtime/status-ok bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 3 ] && grep -q 'dmabuf_device=fail reason=multiple_target_render_nodes' "$O"; then pass multiple_targets_reason; else fail multiple_targets_reason "rc=$rc out=$(tail -1 "$O")"; fi

# card/render 不同源：card0 指向另一个 PCI（vendor/device 相同，BDF 不同）
mkdir -p "$runtime/sysfs-div/class/drm/renderD128" "$runtime/sysfs-div/class/drm/card0"
ln -sf "$PCI_INNO" "$runtime/sysfs-div/class/drm/renderD128/device"
ln -sf "$PCI_OTHER" "$runtime/sysfs-div/class/drm/card0/device"
run_rc render_card_diff_source 3 "$OK FAKE_SYSFS_ROOT=$runtime/sysfs-div FAKE_DEV_DIR=$runtime/dev INNOGPU_DMABUF_SKIP_DEVICE_CHECKS=1 INNOGPU_DMABUF_STATUS_FILE=$runtime/status-ok" --render-device /dev/dri/renderD128 --card-device /dev/dri/card0

# ================= 5. self-import 控制流 =================
run_rc self_export_fail 1 "$GOOD FAKE_SELF_RC=1" --size 7646720
run_rc self_capability_missing 3 "$GOOD FAKE_SELF_CAPABILITY=no-prime-export" --size 7646720
O="$runtime/short.out"
env $GOOD FAKE_SELF_PRINT_ROUNDS=3 TMPDIR=$runtime/scratch bash "$SCRIPT" --iterations 5 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -qE 'missing_round|round_count ref=5' "$O"; then pass self_round_count_short; else fail self_round_count_short "rc=$rc out=$(grep fixture_dmabuf_self_import "$O")"; fi

run_rc self_fd_leak 1 "$GOOD FAKE_SELF_LEAK=yes" --size 7646720
run_rc self_cloexec_no 1 "$GOOD FAKE_SELF_CLOEXEC=no" --size 7646720
run_rc self_create_size_below 1 "$GOOD FAKE_SELF_CREATE_SIZE=100" --size 7646720
run_rc self_missing_create_size 1 "$GOOD FAKE_SELF_MISSING_CREATE_SIZE=1" --size 7646720
run_rc self_missing_cloexec 1 "$GOOD FAKE_SELF_MISSING_CLOEXEC=1" --size 7646720
run_rc self_bad_cloexec 1 "$GOOD FAKE_SELF_BAD_CLOEXEC=1" --size 7646720
run_rc self_bad_device 1 "$GOOD FAKE_SELF_BAD_DEVICE=/dev/dri/wrong" --size 7646720
run_rc self_dup_round 1 "$GOOD FAKE_SELF_ROUND_NUMBERS=1,1,3" --iterations 3 --size 7646720
run_rc self_missing_round 1 "$GOOD FAKE_SELF_ROUND_NUMBERS=1,3" --iterations 3 --size 7646720
run_rc self_out_of_range_round 1 "$GOOD FAKE_SELF_ROUND_NUMBERS=1,2,9" --iterations 3 --size 7646720
run_rc self_summary_rounds_mismatch 1 "$GOOD FAKE_SELF_SUMMARY_ROUNDS=999" --iterations 3 --size 7646720
run_rc self_fd_count_mismatch 1 "$GOOD FAKE_SELF_FDS_AFTER=9" --size 7646720
run_rc self_dup_summary 1 "$GOOD FAKE_SELF_DUP_SUMMARY=1" --size 7646720
run_rc self_malformed_summary 1 "$GOOD FAKE_SELF_BAD_SUMMARY=1" --size 7646720
run_rc self_summary_leading_zero_rounds 1 "$GOOD FAKE_SELF_SUMMARY_ROUNDS=08" --iterations 3 --size 7646720
run_rc self_summary_leading_zero_success 1 "$GOOD FAKE_SELF_SUCCESS=08" --size 7646720
run_rc self_summary_leading_zero_failures 1 "$GOOD FAKE_SELF_FAILURES=09" --size 7646720
O="$runtime/selfcap.out"
env $GOOD FAKE_SELF_CAPABILITY=no-dumb-buffer TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 3 ] && grep -q 'fixture_dmabuf_self_import=UNVERIFIED reason=capability_missing:no-dumb-buffer' "$O"; then pass self_capability_unverified; else fail self_capability_unverified "rc=$rc out=$(grep fixture_dmabuf_self_import "$O")"; fi

# ================= 6. READ 输出严格解析 =================
run_rc read_missing_munmap_line 1 "$GOOD FAKE_READ_DROP_MUNMAP=1" --size 7646720
run_rc read_duplicate_line 1 "$GOOD FAKE_READ_DUP=1" --size 7646720
run_rc read_nan_system_ms 1 "$GOOD FAKE_READ_SYSMS=nan" --size 7646720
run_rc read_negative_system_ms 1 "$GOOD FAKE_READ_SYSMS=-1.5" --size 7646720
run_rc read_bad_wall_ms 1 "$GOOD FAKE_READ_MUNMAP_MS=abc" --size 7646720
run_rc read_iteration_mismatch 1 "$GOOD FAKE_READ_ITER=7" --size 7646720
run_rc read_perf_over_limit 1 "$GOOD FAKE_READ_MUNMAP_MS=50.000" --size 7646720
O="$runtime/perf.out"
env $GOOD FAKE_READ_MUNMAP_MS=5.000 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 0 ] && grep -q 'fixture_dmabuf_invisible_read=PASS reason=.*max_read_munmap_ms=5.000' "$O"; then pass read_perf_in_limit; else fail read_perf_in_limit "rc=$rc out=$(grep fixture_dmabuf_invisible_read "$O")"; fi
O="$runtime/perfoverride.out"
env $GOOD FAKE_READ_MUNMAP_MS=5.000 TMPDIR=$runtime/scratch bash "$SCRIPT" --read-munmap-limit-ms 4 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q 'read_munmap_exceeds_limit max=5.000 limit=4' "$O"; then pass read_perf_override_recorded; else fail read_perf_override_recorded "rc=$rc out=$(grep fixture_dmabuf_invisible_read "$O")"; fi

# ================= 7. WRITE verify =================
run_rc write_verify_missing 1 "$GOOD FAKE_READ_NOVERIFY=1" --size 7646720
run_rc write_verify_pages_mismatch 1 "$GOOD FAKE_READ_VERIFY_PAGES=100" --size 7646720
run_rc write_dup_verify_touch 1 "$GOOD FAKE_READ_DUP_VTOUCH=1" --size 7646720
run_rc read_header_bad_device 1 "$GOOD FAKE_READ_BAD_DEVICE=/dev/dri/wrong" --size 7646720
run_rc read_header_bad_size 1 "$GOOD FAKE_READ_BAD_SIZE=1000" --size 7646720
run_rc read_header_bad_access 1 "$GOOD FAKE_READ_BAD_ACCESS=write" --size 7646720
run_rc read_header_bad_stride 1 "$GOOD FAKE_READ_BAD_STRIDE=7" --size 7646720
run_rc write_probe_rc1 1 "$GOOD FAKE_READ_RC=1" --size 7646720

# ================= 8. topology =================
run_rc top_no_active 3 "$GOOD FAKE_TOP_ACTIVE= FAKE_TOP_INACTIVE=0,1,2 FAKE_VBL_INACTIVE_CRTCS=0,1,2" --size 7646720
O="$runtime/top-multi.out"
env $GOOD FAKE_TOP_ACTIVE=1,2 FAKE_TOP_INACTIVE=0 FAKE_VBL_INACTIVE_CRTCS=0 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 0 ] && grep -q 'fixture_dmabuf_vblank_active=PASS reason=active_crtcs_tested=2-all-ok' "$O"; then pass top_multiple_active_all_tested; else fail top_multiple_active_all_tested "rc=$rc out=$(grep fixture_dmabuf_vblank_active "$O")"; fi
# 多 active 中任一失败 -> 整体 FAIL
O="$runtime/multi-fail.out"
env $GOOD FAKE_TOP_ACTIVE=1,2 FAKE_TOP_INACTIVE=0 FAKE_VBL_INACTIVE_CRTCS=0 FAKE_VBL_FAIL_CRTCS=2 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q 'active_crtcs_tested=2-ok=1-failed=crtc2' "$O"; then pass top_multiple_active_one_fails; else fail top_multiple_active_one_fails "rc=$rc out=$(grep fixture_dmabuf_vblank_active "$O")"; fi
run_rc top_bad_format 1 "$GOOD FAKE_TOP_BAD=1" --size 7646720
run_rc top_probe_rc 1 "$GOOD FAKE_TOP_RC=1" --size 7646720
run_rc top_dup_index 1 "$GOOD FAKE_TOP_CRTC_LIST=0=no,1=yes,1=yes FAKE_TOP_HDR_CRTCS=3" --size 7646720
run_rc top_missing_index 1 "$GOOD FAKE_TOP_CRTC_LIST=0=no,2=yes FAKE_TOP_HDR_CRTCS=3" --size 7646720
run_rc top_out_of_range_index 1 "$GOOD FAKE_TOP_CRTC_LIST=0=no,1=yes,9=yes FAKE_TOP_HDR_CRTCS=3" --size 7646720
run_rc top_dup_header 1 "$GOOD FAKE_TOP_CRTC_LIST=0=no,1=yes,2=yes FAKE_TOP_HDR_CRTCS=3 FAKE_TOP_DUP_HEADER=1" --size 7646720
# 真实形态：active CRTC mode_valid=yes 但 mode 名称为空（mode=<unnamed>）-> 解析通过且 active 仍执行 vblank
O="$runtime/top-unnamed.out"
env $GOOD FAKE_TOP_ACTIVE=1 FAKE_TOP_INACTIVE=0,2 FAKE_VBL_INACTIVE_CRTCS=0,2 FAKE_TOP_UNNAMED_MODE=1 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 0 ] && grep -q 'fixture_dmabuf_vblank_active=PASS reason=active_crtcs_tested=1-all-ok' "$O"; then pass top_unnamed_mode_active_runs_vblank; else fail top_unnamed_mode_active_runs_vblank "rc=$rc out=$(grep -E 'vblank_active|overall' "$O" | head -2)"; fi
# 探针输出契约单元级：直接编译生产 C 探针（fixture 宏构建），验证三态选择——
# inactive -> mode=-；active 具名 -> 保留原 mode 名称；active 无名 -> <unnamed>
TOPO_BIN="$runtime/topo-real"
TOPO_OUT="$runtime/topo-c-contract.out"
if gcc -std=c11 -Wall -Wextra -Werror -O2 -DINNOGPU_DMABUF_FIXTURE_HOOKS -o "$TOPO_BIN" "$ROOT/tools/probe-drm-topology.c" 2>"$runtime/cc-topo.log"; then
    INNOGPU_DMABUF_TOPOLOGY_FIXTURE=1 "$TOPO_BIN" /dev/dri/card0 > "$TOPO_OUT" 2>&1
    if grep -q 'index=0 id=10 active=no.*mode=- refresh=0' "$TOPO_OUT" \
       && grep -q 'index=1 id=11 active=yes.*mode=1920x1080 refresh=60' "$TOPO_OUT" \
       && grep -q 'index=2 id=12 active=yes.*mode=<unnamed> refresh=60' "$TOPO_OUT" \
       && ! grep -qE 'mode= refresh=' "$TOPO_OUT"; then
        pass top_unnamed_mode_output_contract
    else
        fail top_unnamed_mode_output_contract "out=$(grep '^  index=' "$TOPO_OUT" | head -3)"
    fi
else
    fail top_unnamed_mode_output_contract "gcc: $(head -2 "$runtime/cc-topo.log")"
fi

# ================= 9. active vblank =================
run_rc vbl_active_timeout 1 "$GOOD FAKE_VBL_OK=5" --size 7646720
run_rc vbl_active_fast_return 1 "$GOOD FAKE_VBL_FAST=1" --size 7646720
run_rc vbl_active_nonadvancing 1 "$GOOD FAKE_VBL_NONADV=1" --size 7646720
run_rc vbl_active_dup_summary 1 "$GOOD FAKE_VBL_DUP_SUMMARY=1" --size 7646720
run_rc vbl_active_shifted_samples 1 "$GOOD FAKE_VBL_SHIFT=1" --vblank-samples 10 --size 7646720
run_rc vbl_active_dup_colheader 1 "$GOOD FAKE_VBL_DUP_COLHEADER=1" --size 7646720
run_rc vbl_active_bad_float 1 "$GOOD FAKE_VBL_BAD_FLOAT=1" --size 7646720
# 列标题接口漂移（kernel_ms 而非真实 kernel_time_ms）-> 必须拒绝
run_rc vbl_active_bad_colheader 1 "$GOOD FAKE_VBL_BAD_COLHDR=1" --size 7646720
# 样本行乱序（1,2 对调）-> 必须拒绝（要求按 1..N 顺序出现）
run_rc vbl_active_out_of_order 1 "$GOOD FAKE_VBL_REORDER=1" --size 7646720
# 重复 header -> 必须拒绝
run_rc vbl_active_dup_header 1 "$GOOD FAKE_VBL_DUP_HEADER=1" --size 7646720
# 首样本 delta 非零 -> 必须拒绝（探针首样本不记录 delta）
run_rc vbl_active_first_delta_nonzero 1 "$GOOD FAKE_VBL_FIRST_DELTA=1" --size 7646720
# delta 与相邻 sequence 差不符 -> 必须拒绝（数值自洽）
run_rc vbl_active_delta_mismatch 1 "$GOOD FAKE_VBL_DELTA_MISMATCH=1" --size 7646720
# summary 指标与样本重算不符（样本 ~16ms，summary 写 100/200/300）-> 必须拒绝
run_rc vbl_active_summary_metrics_mismatch 1 "$GOOD FAKE_VBL_SUMMARY_METRICS_MISMATCH=1" --size 7646720
# kernel_delta 与相邻 kernel_time 差不符 -> 必须拒绝
run_rc vbl_active_kernel_delta_mismatch 1 "$GOOD FAKE_VBL_KD_MISMATCH=1" --size 7646720
# 真实 32 位 sequence 回绕（4294967295 -> 0，delta=1 无符号减法）-> 合法，必须 PASS
O="$runtime/vbl-wrap.out"
env $GOOD FAKE_VBL_WRAP=1 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 0 ] && grep -q 'fixture_dmabuf_vblank_active=PASS' "$O"; then pass vbl_active_wraparound_ok; else fail vbl_active_wraparound_ok "rc=$rc out=$(grep fixture_dmabuf_vblank_active "$O")"; fi
# sequence 越出 uint32（4294967296）-> 必须拒绝
run_rc vbl_active_seq_out_of_range 1 "$GOOD FAKE_VBL_SEQ_OOR=1" --size 7646720
# 内核时间倒退（16.400 -> 15.400，kernel_delta 仍 +1）-> 必须拒绝（时间必须单调）
run_rc vbl_active_kernel_regression 1 "$GOOD FAKE_VBL_K_REGRESS=1" --size 7646720
# 最小精度内核时间倒退（16.400 -> 16.399，kernel_delta=0.000）-> 必须拒绝（%.3f 输入不允许 0.001 容差）
run_rc vbl_active_kernel_min_regression 1 "$GOOD FAKE_VBL_K_MIN_REGRESS=1" --size 7646720
O="$runtime/vbl-active-baddev.out"
env $GOOD FAKE_VBL_BAD_DEVICE=1 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q "fixture_dmabuf_vblank_active=FAIL reason=.*header_mismatch" "$O"; then pass vbl_active_bad_device; else fail vbl_active_bad_device "rc=$rc out=$(grep fixture_dmabuf_vblank_active "$O")"; fi
O="$runtime/vbl-inactive-badtimeout.out"
env $GOOD FAKE_TOP_ACTIVE=1 FAKE_TOP_INACTIVE=0 FAKE_VBL_INACTIVE_CRTCS=0 FAKE_VBL_BAD_TIMEOUT=9999 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q "fixture_dmabuf_vblank_inactive_guard=FAIL reason=.*header_mismatch" "$O"; then pass vblank_inactive_bad_timeout; else fail vblank_inactive_bad_timeout "rc=$rc out=$(grep fixture_dmabuf_vblank_inactive_guard "$O")"; fi
O="$runtime/vbl-samplecount.out"
env $GOOD FAKE_VBL_OK=10 FAKE_VBL_PRINT_OK=8 TMPDIR=$runtime/scratch bash "$SCRIPT" --vblank-samples 10 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -qE 'n=8|sample_validation' "$O"; then pass vbl_active_sample_count; else fail vbl_active_sample_count "rc=$rc out=$(grep fixture_dmabuf_vblank_active "$O")"; fi

# ================= 10. inactive vblank guard =================
O="$runtime/inactive.out"
env $GOOD FAKE_TOP_ACTIVE=1 FAKE_TOP_INACTIVE=0,2 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 0 ] && grep -q 'fixture_dmabuf_vblank_inactive_guard=PASS reason=inactive_crtcs_tested=2-all-einval-fast' "$O"; then pass inactive_einval_fast; else fail inactive_einval_fast "rc=$rc out=$(grep fixture_dmabuf_vblank_inactive_guard "$O")"; fi
# inactive 样本号重复 -> 拒绝（原三个重复 sample 1 反例）
O="$runtime/inactive-dup.out"
env $GOOD FAKE_TOP_ACTIVE=1 FAKE_TOP_INACTIVE=0 FAKE_VBL_INACTIVE_CRTCS=0 FAKE_VBL_DUP_SAMPLE=1 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q 'duplicate_sample' "$O"; then pass inactive_dup_sample_rejected; else fail inactive_dup_sample_rejected "rc=$rc out=$(grep fixture_dmabuf_vblank_inactive_guard "$O")"; fi
# inactive 重复 header -> 必须拒绝（与 active 共用严格元数据解析）
O="$runtime/inactive-dup-hdr.out"
env $GOOD FAKE_TOP_ACTIVE=1 FAKE_TOP_INACTIVE=0 FAKE_VBL_INACTIVE_CRTCS=0 FAKE_VBL_DUP_HEADER=1 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q "fixture_dmabuf_vblank_inactive_guard=FAIL reason=.*metadata" "$O"; then pass inactive_dup_header; else fail inactive_dup_header "rc=$rc out=$(grep fixture_dmabuf_vblank_inactive_guard "$O")"; fi
# inactive 重复列标题 -> 必须拒绝
O="$runtime/inactive-dup-colhdr.out"
env $GOOD FAKE_TOP_ACTIVE=1 FAKE_TOP_INACTIVE=0 FAKE_VBL_INACTIVE_CRTCS=0 FAKE_VBL_DUP_COLHEADER=1 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q "fixture_dmabuf_vblank_inactive_guard=FAIL reason=.*metadata" "$O"; then pass inactive_dup_colheader; else fail inactive_dup_colheader "rc=$rc out=$(grep fixture_dmabuf_vblank_inactive_guard "$O")"; fi
# inactive 坏浮点 summary -> 必须拒绝
O="$runtime/inactive-bad-float.out"
env $GOOD FAKE_TOP_ACTIVE=1 FAKE_TOP_INACTIVE=0 FAKE_VBL_INACTIVE_CRTCS=0 FAKE_VBL_INACTIVE_BAD_FLOAT=1 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q "fixture_dmabuf_vblank_inactive_guard=FAIL reason=.*bad_float" "$O"; then pass inactive_bad_float; else fail inactive_bad_float "rc=$rc out=$(grep fixture_dmabuf_vblank_inactive_guard "$O")"; fi
# inactive summary 字段顺序反例 -> 必须拒绝
O="$runtime/inactive-field-order.out"
env $GOOD FAKE_TOP_ACTIVE=1 FAKE_TOP_INACTIVE=0 FAKE_VBL_INACTIVE_CRTCS=0 FAKE_VBL_INACTIVE_FIELD_ORDER=1 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q "fixture_dmabuf_vblank_inactive_guard=FAIL reason=.*bad_summary" "$O"; then pass inactive_field_order; else fail inactive_field_order "rc=$rc out=$(grep fixture_dmabuf_vblank_inactive_guard "$O")"; fi
# inactive success=0 时 summary 指标必须全零（真实探针失败样本不更新统计）-> 非零拒绝
O="$runtime/inactive-nonzero-summary.out"
env $GOOD FAKE_TOP_ACTIVE=1 FAKE_TOP_INACTIVE=0 FAKE_VBL_INACTIVE_CRTCS=0 FAKE_VBL_INACTIVE_NONZERO=1 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q "fixture_dmabuf_vblank_inactive_guard=FAIL reason=.*inactive_nonzero_summary" "$O"; then pass inactive_nonzero_summary; else fail inactive_nonzero_summary "rc=$rc out=$(grep fixture_dmabuf_vblank_inactive_guard "$O")"; fi
O="$runtime/inactive-timeout.out"
env $GOOD FAKE_TOP_ACTIVE=1 FAKE_TOP_INACTIVE=0 FAKE_VBL_INACTIVE=1 FAKE_VBL_INACTIVE_MODE=timeout TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q 'crtc0(einval=0,to=3' "$O"; then pass inactive_timeout_fails; else fail inactive_timeout_fails "rc=$rc out=$(grep fixture_dmabuf_vblank_inactive_guard "$O")"; fi
O="$runtime/inactive-errno.out"
env $GOOD FAKE_TOP_ACTIVE=1 FAKE_TOP_INACTIVE=0 FAKE_VBL_INACTIVE=1 FAKE_VBL_INACTIVE_MODE=wrongerrno TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q 'other=3' "$O"; then pass inactive_wrong_errno; else fail inactive_wrong_errno "rc=$rc out=$(grep fixture_dmabuf_vblank_inactive_guard "$O")"; fi
O="$runtime/inactive-slow.out"
env $GOOD FAKE_TOP_ACTIVE=1 FAKE_TOP_INACTIVE=0 FAKE_VBL_INACTIVE=1 FAKE_VBL_INACTIVE_MODE=slow TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q 'slow_einval' "$O"; then pass inactive_slow_einval; else fail inactive_slow_einval "rc=$rc out=$(grep fixture_dmabuf_vblank_inactive_guard "$O")"; fi
O="$runtime/vbl-inactive-baddev.out"
env $GOOD FAKE_TOP_ACTIVE=1 FAKE_TOP_INACTIVE=0 FAKE_VBL_INACTIVE_CRTCS=0 FAKE_VBL_BAD_DEVICE_INACTIVE=1 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q "fixture_dmabuf_vblank_inactive_guard=FAIL reason=.*header_mismatch" "$O" && grep -q "fixture_dmabuf_vblank_active=PASS" "$O"; then pass inactive_bad_device; else fail inactive_bad_device "rc=$rc out=$(grep -E "vblank_active|vblank_inactive" "$O")"; fi
O="$runtime/noinactive.out"
env $GOOD FAKE_TOP_ACTIVE=0,1,2 FAKE_TOP_INACTIVE= FAKE_VBL_INACTIVE_CRTCS= TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 0 ] && grep -q 'fixture_dmabuf_vblank_inactive_guard=SKIP reason=no_inactive_crtc' "$O"; then pass inactive_none_skipped; else fail inactive_none_skipped "rc=$rc out=$(grep fixture_dmabuf_vblank_inactive_guard "$O")"; fi

# ================= 11. 状态门禁 =================
run_rc status_pre_okay 1 "$GOOD INNOGPU_DMABUF_STATUS_FILE=$runtime/status-okay" --size 7646720
run_rc status_pre_badcount 1 "$GOOD INNOGPU_DMABUF_STATUS_FILE=$runtime/status-badcount" --size 7646720
run_rc status_pre_dup 1 "$GOOD INNOGPU_DMABUF_STATUS_FILE=$runtime/status-dup" --size 7646720
run_rc status_pre_missing_field 1 "$GOOD INNOGPU_DMABUF_STATUS_FILE=$runtime/status-missing" --size 7646720
run_rc status_pre_nonnum 1 "$GOOD INNOGPU_DMABUF_STATUS_FILE=$runtime/status-nonnum" --size 7646720
O="$runtime/status-growth.out"
ln -sf "$runtime/status-ok" "$runtime/status-link"
env $GOOD INNOGPU_DMABUF_STATUS_FILE=$runtime/status-link FAKE_SWAP_STATUS_LINK=$runtime/status-link FAKE_SWAP_STATUS_TARGET=$runtime/status-high \
    TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -qF 'driver_error_counts_changed:Server(0->1)' "$O"; then pass status_growth; else fail status_growth "rc=$rc out=$(grep fixture_dmabuf_status "$O")"; fi
# 计数减少同样 FAIL（任务要求不可减少或增长）
O="$runtime/status-shrink.out"
ln -sf "$runtime/status-high" "$runtime/status-link-s"
env $GOOD INNOGPU_DMABUF_STATUS_FILE=$runtime/status-link-s FAKE_SWAP_STATUS_LINK=$runtime/status-link-s FAKE_SWAP_STATUS_TARGET=$runtime/status-ok \
    TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -qF 'driver_error_counts_changed:Server(1->0)' "$O"; then pass status_shrink_fails; else fail status_shrink_fails "rc=$rc out=$(grep fixture_dmabuf_status "$O")"; fi
# post 快照失效（pre 有效、运行中被替换为不存在文件）-> FAIL
O="$runtime/status-post-unreadable.out"
ln -sf "$runtime/status-ok" "$runtime/status-link-pu"
env $GOOD INNOGPU_DMABUF_STATUS_FILE=$runtime/status-link-pu FAKE_SWAP_STATUS_LINK=$runtime/status-link-pu FAKE_SWAP_STATUS_TARGET=/nonexistent/status \
    TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q 'reason=post_unreadable' "$O"; then pass status_post_unreadable; else fail status_post_unreadable "rc=$rc out=$(grep fixture_dmabuf_status "$O")"; fi
# pre 快照不可读（独立负例）
run_rc status_pre_unreadable 1 "$GOOD INNOGPU_DMABUF_STATUS_FILE=/nonexistent/status" --size 7646720

# ================= 12. 内核日志元数据 =================
O="$runtime/klog.out"
ln -sf "$runtime/klog-clean" "$runtime/klog-link"
env $GOOD INNOGPU_DMABUF_KERNEL_LOG=$runtime/klog-link FAKE_SWAP_KLOG_LINK=$runtime/klog-link FAKE_SWAP_KLOG_TARGET=$runtime/klog-error TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q 'fixture_dmabuf_kernel_log=fail reason=new_kernel_error_lines:1' "$O" && grep -q 'fixture_dmabuf_regression_overall=FAIL reason=kernel_error_lines' "$O"; then pass klog_error_fails_overall; else fail klog_error_fails_overall "rc=$rc out=$(grep -E 'kernel_log|overall' "$O")"; fi
# GPU hang（严重词扩充反例）-> 必须 FAIL
O="$runtime/klog-gpuhang.out"
ln -sf "$runtime/klog-clean" "$runtime/klog-link-gh"
env $GOOD INNOGPU_DMABUF_KERNEL_LOG=$runtime/klog-link-gh FAKE_SWAP_KLOG_LINK=$runtime/klog-link-gh FAKE_SWAP_KLOG_TARGET=$runtime/klog-gpu-hang TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q 'fixture_dmabuf_kernel_log=fail reason=new_kernel_error_lines:1' "$O"; then pass klog_gpu_hang_fails; else fail klog_gpu_hang_fails "rc=$rc out=$(grep fixture_dmabuf_kernel_log "$O")"; fi
# dma_buf timeout（来源扩充反例：不带 innogpu/pvr/drm 前缀）-> 必须 FAIL
O="$runtime/klog-dmabuf-timeout.out"
ln -sf "$runtime/klog-clean" "$runtime/klog-link-dt"
env $GOOD INNOGPU_DMABUF_KERNEL_LOG=$runtime/klog-link-dt FAKE_SWAP_KLOG_LINK=$runtime/klog-link-dt FAKE_SWAP_KLOG_TARGET=$runtime/klog-dmabuf-timeout TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q 'fixture_dmabuf_kernel_log=fail reason=new_kernel_error_lines:1' "$O"; then pass klog_dmabuf_timeout_fails; else fail klog_dmabuf_timeout_fails "rc=$rc out=$(grep fixture_dmabuf_kernel_log "$O")"; fi
# benign 日志（debug/installed/hangcheck 裸子串误伤反例）-> 必须保持 clean 且整体 PASS
for benign in debug installed hangcheck; do
    O="$runtime/klog-benign-$benign.out"
    ln -sf "$runtime/klog-clean" "$runtime/klog-link-b$benign"
    env $GOOD INNOGPU_DMABUF_KERNEL_LOG=$runtime/klog-link-b$benign FAKE_SWAP_KLOG_LINK=$runtime/klog-link-b$benign FAKE_SWAP_KLOG_TARGET=$runtime/klog-benign-$benign TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
    if [ "$rc" -eq 0 ] && grep -q 'fixture_dmabuf_kernel_log=clean' "$O" && grep -q 'fixture_dmabuf_regression_overall=PASS' "$O"; then pass klog_benign_$benign; else fail klog_benign_$benign "rc=$rc out=$(grep -E 'kernel_log|overall' "$O")"; fi
done
# 严重词表驱动：逐词验证聚合器 KLOG_ERR_RE 命中/不命中（防漏报与误报回归）
KLOG_RE="$(sed -n 's/^KLOG_ERR_RE=//p' "$SCRIPT" | head -1 | tr -d "\'")"
klog_table_fail=0
for entry in \
    pos:error pos:errors pos:failure pos:failures pos:failed \
    pos:fault pos:faults pos:bug pos:bugs \
    pos:hang pos:hangs pos:hanging pos:hung \
    pos:timeout pos:timeouts pos:timed\ out \
    pos:reset pos:resets pos:resetting \
    pos:oops pos:oopses pos:panic pos:panics pos:panicked pos:panicking \
    pos:deadlock pos:deadlocks pos:stall pos:stalls pos:stalled \
    pos:corrupt pos:corrupts pos:corrupted pos:corruption \
    pos:abort pos:aborts pos:aborted pos:aborting \
    pos:warn pos:warns pos:warning pos:warnings pos:WARN_ON \
    pos:lockup pos:lockups pos:wedged \
    neg:debug neg:installed neg:hangcheck neg:enabled neg:initialized neg:helper; do
    expect="${entry%%:*}"; word="${entry#*:}"
    hit=0
    printf '%s
' "$word" | grep -qiE "$KLOG_RE" && hit=1
    if { [ "$expect" = pos ] && [ "$hit" -eq 1 ]; } || { [ "$expect" = neg ] && [ "$hit" -eq 0 ]; }; then :; else
        klog_table_fail=$((klog_table_fail+1))
        printf '  severity-table miss: %s %s
' "$expect" "$word"
    fi
done
if [ "$klog_table_fail" -eq 0 ]; then pass klog_severity_table; else fail klog_severity_table "misses=$klog_table_fail"; fi
# post 内核日志源不可用（窗口不连续）-> UNVERIFIED + 整体 FAIL
O="$runtime/klog-post-unavail.out"
ln -sf "$runtime/klog-clean" "$runtime/klog-link-pu"
env $GOOD INNOGPU_DMABUF_KERNEL_LOG=$runtime/klog-link-pu FAKE_SWAP_KLOG_LINK=$runtime/klog-link-pu FAKE_SWAP_KLOG_TARGET=/nonexistent/klog \
    TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q 'fixture_dmabuf_kernel_log=UNVERIFIED reason=post_kernel_log_unavailable' "$O" && grep -q 'fixture_dmabuf_regression_overall=FAIL reason=kernel_error_lines' "$O"; then pass klog_post_unavailable_fails; else fail klog_post_unavailable_fails "rc=$rc out=$(grep -E 'kernel_log|overall' "$O")"; fi
# 截断：before 有行、after 缺失 -> 窗口不连续 -> UNVERIFIED + overall FAIL
O="$runtime/klog-trunc.out"
ln -sf "$runtime/klog-two" "$runtime/klog-link-t"
env $GOOD INNOGPU_DMABUF_KERNEL_LOG=$runtime/klog-link-t FAKE_SWAP_KLOG_LINK=$runtime/klog-link-t FAKE_SWAP_KLOG_TARGET=$runtime/klog-clean \
    TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q 'fixture_dmabuf_kernel_log=UNVERIFIED reason=kernel_log_discontinuous' "$O"; then pass klog_truncated_unverified; else fail klog_truncated_unverified "rc=$rc out=$(grep fixture_dmabuf_kernel_log "$O")"; fi
# 重排：after 与 before 行相同但顺序不同 -> 不连续 -> UNVERIFIED
O="$runtime/klog-reorder.out"
ln -sf "$runtime/klog-two" "$runtime/klog-link-r"
env $GOOD INNOGPU_DMABUF_KERNEL_LOG=$runtime/klog-link-r FAKE_SWAP_KLOG_LINK=$runtime/klog-link-r FAKE_SWAP_KLOG_TARGET=$runtime/klog-two-reordered \
    TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q 'fixture_dmabuf_kernel_log=UNVERIFIED reason=kernel_log_discontinuous' "$O"; then pass klog_reordered_unverified; else fail klog_reordered_unverified "rc=$rc out=$(grep fixture_dmabuf_kernel_log "$O")"; fi
# 中间插入：after 在 before 行之间插入新行 -> 前缀被破坏 -> 不连续
O="$runtime/klog-insert.out"
ln -sf "$runtime/klog-two" "$runtime/klog-link-i"
env $GOOD INNOGPU_DMABUF_KERNEL_LOG=$runtime/klog-link-i FAKE_SWAP_KLOG_LINK=$runtime/klog-link-i FAKE_SWAP_KLOG_TARGET=$runtime/klog-two-inserted \
    TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 1 ] && grep -q 'fixture_dmabuf_kernel_log=UNVERIFIED reason=kernel_log_discontinuous' "$O"; then pass klog_middle_insertion_unverified; else fail klog_middle_insertion_unverified "rc=$rc out=$(grep fixture_dmabuf_kernel_log "$O")"; fi
O="$runtime/klog-skip.out"
env $GOOD INNOGPU_DMABUF_KERNEL_LOG= TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 3 ] && grep -q 'fixture_dmabuf_kernel_log=SKIP reason=dmesg_unavailable' "$O" && grep -q 'fixture_dmabuf_regression_overall=UNVERIFIED reason=kernel_log_unavailable' "$O"; then pass klog_unavailable_skip; else fail klog_unavailable_skip "rc=$rc out=$(grep -E 'kernel_log|overall' "$O")"; fi

# ================= 12b. mktemp/超时/信号/清理 =================
run_rc mktemp_failure 2 "$GOOD TMPDIR=/nonexistent/tmpdir" --size 7646720
O="$runtime/tout.out"
env $GOOD FAKE_VBL_SLEEP=1 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 --timeout 1 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 5 ] && grep -qE 'crtc1\(timeout\)|timeout_vblank_active' "$O"; then pass external_timeout_exit5; else fail external_timeout_exit5 "rc=$rc out=$(tail -2 "$O" | tr '\n' ';')"; fi
# 进程忽略 TERM、timeout 最终 SIGKILL（rc=137）同样归类为超时 -> rc=5
O="$runtime/tout-kill.out"
env $GOOD FAKE_VBL_SLEEP=1 FAKE_VBL_IGNORE_TERM=1 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 --timeout 1 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 5 ] && grep -qE 'crtc1\(timeout\)|timeout_vblank_active' "$O"; then pass external_timeout_sigkill_exit5; else fail external_timeout_sigkill_exit5 "rc=$rc out=$(tail -2 "$O" | tr '\n' ';')"; fi

O="$runtime/term.out"
env $GOOD FAKE_SELF_SLEEP=1 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 --timeout 2 > "$O" 2>&1 &
spid=$!
sleep 0.5
kill -TERM "$spid"
wait "$spid"; rc=$?
sleep 3
if [ "$rc" -eq 143 ] && ! pgrep -f "$runtime/bin/fake-" >/dev/null 2>&1 && [ -z "$(find "$runtime/scratch" -maxdepth 1 -name 'dmabuf-regression.*' 2>/dev/null)" ]; then
    pass term_cleanup
else
    fail term_cleanup "rc=$rc"
fi

env $GOOD TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 >/dev/null 2>&1
if ! pgrep -f "$runtime/bin/fake-" >/dev/null 2>&1 && [ -z "$(find "$runtime/scratch" -maxdepth 1 -name 'dmabuf-regression.*' 2>/dev/null)" ]; then
    pass no_residue
else
    fail no_residue "leftover process or dmabuf-regression temp dir"
fi

# cleanup 失败（强制）-> 明确 FAIL/5，绝不输出权威 PASS
O="$runtime/cleanup-fail.out"
env $GOOD INNOGPU_DMABUF_FORCE_CLEANUP_FAIL=1 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 5 ] && grep -q 'fixture_dmabuf_regression_overall=FAIL reason=cleanup_failed' "$O" && ! grep -q 'fixture_dmabuf_regression_overall=PASS' "$O"; then pass cleanup_failure_fails; else fail cleanup_failure_fails "rc=$rc out=$(tail -2 "$O" | tr '\n' ';')"; fi

# ================= 13. 不污染 baseline =================
BASELINE_BEFORE="$(sha256sum "$ROOT/baselines/latest-runtime-baseline.txt" 2>/dev/null | cut -d' ' -f1)"
env $GOOD TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 >/dev/null 2>&1
BASELINE_AFTER="$(sha256sum "$ROOT/baselines/latest-runtime-baseline.txt" 2>/dev/null | cut -d' ' -f1)"
if [ "$BASELINE_BEFORE" == "$BASELINE_AFTER" ]; then pass baseline_untouched; else fail baseline_untouched "baseline changed"; fi

# ================= 14. 汇总恒等式 =================
O="$runtime/agg.out"
env $GOOD FAKE_TOP_ACTIVE=0,1,2 FAKE_TOP_INACTIVE= FAKE_VBL_INACTIVE_CRTCS= TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 0 ] \
   && grep -q 'fixture_tests_total=5 fixture_tests_passed=4 fixture_tests_failed=0 fixture_tests_skipped=1 fixture_tests_unverified=0' "$O" \
   && grep -q 'fixture_dmabuf_regression_overall=PASS' "$O"; then
    pass totals_identity_skip
else
    fail totals_identity_skip "rc=$rc out=$(tail -2 "$O" | tr '\n' ';')"
fi
O="$runtime/agg-unv.out"
env $GOOD FAKE_TOP_ACTIVE= FAKE_TOP_INACTIVE=0,1,2 FAKE_VBL_INACTIVE_CRTCS=0,1,2 TMPDIR=$runtime/scratch bash "$SCRIPT" --size 7646720 > "$O" 2>&1; rc=$?
if [ "$rc" -eq 3 ] && grep -q 'fixture_dmabuf_regression_overall=UNVERIFIED' "$O"; then pass overall_unverified_rc3; else fail overall_unverified_rc3 "rc=$rc out=$(tail -2 "$O" | tr '\n' ';')"; fi


# ================= 15. 真实 C 探针契约（编译 + 参数/设备/能力路径，验证资源清理与 fd 计数） =================
PROBE_BIN="$runtime/probe-real"
# 契约测试构建：定义 INNOGPU_DMABUF_FIXTURE_HOOKS 才能注入 open-fail fd 计数钩子
if gcc -std=c11 -Wall -Wextra -Werror -O2 -DINNOGPU_DMABUF_FIXTURE_HOOKS -o "$PROBE_BIN" "$ROOT/tools/probe-dmabuf-self-import.c" 2>"$runtime/cc-real.log"; then
    pass real_probe_compiles
else
    fail real_probe_compiles "gcc: $(head -3 "$runtime/cc-real.log")"
fi
if [ -x "$PROBE_BIN" ]; then
    "$PROBE_BIN" /dev/dri/card0 7646720 3 extra >/dev/null 2>&1; rc=$?
    if [ "$rc" -eq 2 ]; then pass real_probe_usage_rc2; else fail real_probe_usage_rc2 "rc=$rc"; fi
    O="$runtime/real-missing.out"
    "$PROBE_BIN" /nonexistent/dri 7646720 3 > "$O" 2>&1; rc=$?
    if [ "$rc" -eq 3 ] \
       && [ "$(grep -c '^summary self_import ' "$O")" -eq 1 ] \
       && grep -q 'rounds=0 success=0 failures=0' "$O" \
       && grep -qE 'fds_before=[0-9]+ fds_after=[0-9]+ fd_leak=no' "$O"; then pass real_probe_missing_device_rc3; else fail real_probe_missing_device_rc3 "rc=$rc out=$(tail -2 "$O" | tr '\n' ';')"; fi
    # FIFO：open 成功、CREATE_DUMB 返回 ENOTTY -> capability 路径；必须无 fd 泄漏（fds_before==fds_after）
    mkfifo "$runtime/fake-dri"
    O="$runtime/real-fifo.out"
    "$PROBE_BIN" "$runtime/fake-dri" 7646720 3 > "$O" 2>&1; rc=$?
    if [ "$rc" -eq 3 ] && grep -q "^capability=no-dumb-buffer$" "$O" \
       && grep -q "fds_before=[0-9]* fds_after=[0-9]* fd_leak=no" "$O"; then
        pass real_probe_capability_no_leak
    else
        fail real_probe_capability_no_leak "rc=$rc out=$(tail -2 "$O" | tr '\n' ';')"
    fi
    # open 失败但 after 计数不可用（-1）-> 不得宣称无泄漏：rc=1 + fd_leak=unknown
    O="$runtime/real-openfail-fdcount-unknown.out"
    INNOGPU_DMABUF_FIXTURE_OPEN_FAIL_FDCOUNT=-1 "$PROBE_BIN" /nonexistent/dri 7646720 3 > "$O" 2>&1; rc=$?
    if [ "$rc" -eq 1 ] && grep -q 'fds_after=-1 fd_leak=unknown' "$O"; then
        pass real_probe_openfail_fdcount_unknown
    else
        fail real_probe_openfail_fdcount_unknown "rc=$rc out=$(tail -2 "$O" | tr '\n' ';')"
    fi
    # open 失败但 after 计数变化（泄漏）-> rc=1 + fd_leak=yes（不能宣称无泄漏）
    O="$runtime/real-openfail-fdcount-leak.out"
    INNOGPU_DMABUF_FIXTURE_OPEN_FAIL_FDCOUNT=9999 "$PROBE_BIN" /nonexistent/dri 7646720 3 > "$O" 2>&1; rc=$?
    if [ "$rc" -eq 1 ] && grep -q 'fds_after=9999 fd_leak=yes' "$O"; then
        pass real_probe_openfail_fdcount_leak
    else
        fail real_probe_openfail_fdcount_leak "rc=$rc out=$(tail -2 "$O" | tr '\n' ';')"
    fi
    # 正常路径（capability 经 out:）after 计数不可用（-1）-> 不得宣称无泄漏：fd_leak=unknown
    # （capability 缺失仍返回 3=UNVERIFIED，但 summary 必须输出 unknown 而非 no）
    O="$runtime/real-normal-fdcount-unknown.out"
    INNOGPU_DMABUF_FIXTURE_FDCOUNT_AFTER=-1 "$PROBE_BIN" "$runtime/fake-dri" 7646720 3 > "$O" 2>&1; rc=$?
    if [ "$rc" -eq 3 ] && grep -q 'fds_after=-1 fd_leak=unknown' "$O" && ! grep -q 'fd_leak=no' "$O"; then
        pass real_probe_normal_fdcount_unknown
    else
        fail real_probe_normal_fdcount_unknown "rc=$rc out=$(tail -2 "$O" | tr '\n' ';')"
    fi
    # 生产构建（不定义 fixture 宏）：注入钩子必须被编译剔除，环境变量无效 -> open 失败仍 rc=3 + fd_leak=no
    PROBE_PROD="$runtime/probe-prod"
    if gcc -std=c11 -Wall -Wextra -Werror -O2 -o "$PROBE_PROD" "$ROOT/tools/probe-dmabuf-self-import.c" 2>"$runtime/cc-prod.log"; then
        O="$runtime/real-prod-gate.out"
        # 同时注入两个 fixture 钩子：生产构建必须忽略二者（OPEN_FAIL_FDCOUNT 与 FDCOUNT_AFTER）
        INNOGPU_DMABUF_FIXTURE_OPEN_FAIL_FDCOUNT=9999 INNOGPU_DMABUF_FIXTURE_FDCOUNT_AFTER=-1 \
            "$PROBE_PROD" /nonexistent/dri 7646720 3 > "$O" 2>&1; rc=$?
        if [ "$rc" -eq 3 ] && grep -qE 'fds_before=[0-9]+ fds_after=[0-9]+ fd_leak=no' "$O" && ! grep -qE 'fd_leak=(unknown|yes)' "$O"; then
            pass real_probe_prod_ignores_fixture_hook
        else
            fail real_probe_prod_ignores_fixture_hook "rc=$rc out=$(tail -2 "$O" | tr '\n' ';')"
        fi
    else
        fail real_probe_prod_ignores_fixture_hook "prod gcc: $(head -3 "$runtime/cc-prod.log")"
    fi
fi

printf 'tests_total=%d tests_passed=%d tests_failed=%d tests_skipped=0\n' "$t" "$passed" "$failed"
[ "$failed" -eq 0 ]
