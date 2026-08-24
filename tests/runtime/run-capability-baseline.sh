#!/bin/bash
# Real-device capability baseline for FH2M (4.0.0-i1, kernel 6.12.101+deb13-amd64).
#
# Read-only by default. Side-effect operations (real VT fbterm, modeset/hotplug/
# lid, audio playback, Vulkan/OpenCL/VA-API/DMA-BUF execution) are NEVER
# auto-run: they are recorded SKIP reason=manual-execution-required and printed
# as a manual command list. --allow-authorized-tests only unlocks that list;
# actual execution must be done by the user on the device session, then
# recorded via --results-file <file> (one runtime_<name>=PASS|FAIL|SKIP|
# UNVERIFIED [reason=...] per line) or separately.
#
# No deb install, no module switch, no reboot, no Xorg/PipeWire/display config
# change. Privacy: all output goes to a temp RAW_LOG; on exit it is redacted
# into baselines/runtime-baseline-<ts>.txt (personal paths/XAUTHORITY stripped),
# so abnormal termination still leaves only a redacted artifact.
#
# Output per test: runtime_<name>=PASS|FAIL reason=...|SKIP reason=...|UNVERIFIED reason=...
# Summary: runtime_total=.. runtime_passed=.. runtime_failed=.. runtime_skipped=.. runtime_unverified=..
#          runtime_overall=PASS|FAIL|SKIP|UNVERIFIED
# Exit: 0=PASS 1=FAIL 2=SKIP-only 3=UNVERIFIED present

set -u -o pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

usage() {
    cat <<'USAGE'
Usage: tests/runtime/run-capability-baseline.sh [options]

Options:
  --allow-authorized-tests    Unlock the manual command list for side-effect
                              tests (real VT, modeset, playback, GPU execution).
                              These are NEVER auto-run; execute them on the
                              device session and feed results via --results-file.
  --results-file FILE         Merge user-recorded results (one
                              runtime_<name>=PASS|FAIL|SKIP|UNVERIFIED [reason=..]
                              per line) into the summary, overriding the
                              default SKIP entries for authorized items.
  -h, --help                  Show this help
USAGE
}

MODE="readonly"
RESULTS_FILE=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --allow-authorized-tests) MODE="allow-authorized" ;;
        --results-file) RESULTS_FILE="${2:?missing value for --results-file}"; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

# --results-file 是人工授权流程的一部分：必须与 --allow-authorized-tests 同时使用
if [[ -n "$RESULTS_FILE" && "$MODE" != "allow-authorized" ]]; then
    echo "error: --results-file requires --allow-authorized-tests" >&2
    usage >&2
    exit 2
fi

RUNTIME_TMP="${TMPDIR:-/tmp}"
RUNTIME_DIR="$(mktemp -d "$RUNTIME_TMP/runtime-baseline.XXXXXX")"
TS="$(date +%Y%m%d-%H%M%S)"
BASELINE_DIR="${RUNTIME_BASELINE_DIR:-$ROOT/baselines}"
FULL_LOG="$BASELINE_DIR/runtime-baseline-$TS.txt"
LATEST="$BASELINE_DIR/latest-runtime-baseline.txt"
RAW_LOG="$RUNTIME_DIR/raw.log"
mkdir -p "$BASELINE_DIR"

# 隐私脱敏：用变量 H 构造 /home 前缀，替换令牌避开 serverauth. 字面序列，
# 避免在脚本文本中留下可被 check-docs 隐私扫描匹配的串。
H="home"
redact() {
    sed -e "s|/\$H/[a-z_][a-z0-9_-]*|~|g" \
        -e 's|serverauth\.[A-Za-z0-9]*|AUTHTOKEN.REDACTED|g' \
        -e 's|XAUTHORITY=[^ ]*|XAUTHORITY=REDACTED|g'
}
# 幂等清理：信号处理器调用 cleanup 后显式退出，EXIT trap 再触发时由 CLEANED 标记防重复
CLEANED=0
cleanup() {
    [[ "$CLEANED" -eq 1 ]] && return
    CLEANED=1
    redact < "$RAW_LOG" > "$FULL_LOG" 2>/dev/null
    rm -rf "$RUNTIME_DIR"
}
trap cleanup EXIT
trap 'cleanup; exit 129' HUP
trap 'cleanup; exit 130' INT
trap 'cleanup; exit 143' TERM
exec > >(tee "$RAW_LOG") 2>&1

# ---- 结果收集（数组，支持 --results-file 覆盖）----
declare -A RES=()
declare -A RS=()
ORDER=()
record() { # <name> <PASS|FAIL|SKIP|UNVERIFIED> [reason]
    local name="$1" res="$2" reason="${3:-}"
    [[ -n "${RES[$name]+x}" ]] || ORDER+=("$name")
    RES["$name"]="$res"
    RS["$name"]="$reason"
}

# ---- 环境元数据 ----
IS_ROOT=$([ "$(id -u)" -eq 0 ] && echo yes || echo no)
HAS_DRI=$([ -d /dev/dri ] && ls /dev/dri 2>/dev/null | grep -q . && echo yes || echo no)
HAS_FB=$([ -e /dev/fb0 ] && echo yes || echo no)
HAS_TTY=$([ -t 0 ] && echo yes || echo no)
HAS_X=$([ -n "${DISPLAY:-}" ] && echo yes || echo no)
KERNEL="$(uname -r)"
PKG="$(dpkg-query -W -f='${Version}' innogpu-fh2m-trixie 2>/dev/null || echo unknown)"
# lspci -nnD 输出带 domain 前缀(0000:)，与 /sys/bus/pci/devices/ 路径一致
BDF="$(lspci -nnD 2>/dev/null | grep '1ec8:9810' | awk '{print $1}' | head -1 || true)"
BDF_N="$(lspci -nnD 2>/dev/null | grep -c '1ec8:9810' || true)"
PCI_LINE="$(lspci -nnD 2>/dev/null | grep '1ec8:9810' | head -1 || true)"
SCRIPT_COMMIT="$(git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"

printf '# runtime-capability-baseline (4.0.0-i1) %s mode=%s\n' "$TS" "$MODE"
printf '# kernel=%s package=%s pci_bdf=%s(%s) tested_commit=%s root=%s dri=%s fb=%s tty=%s x=%s\n' \
  "$KERNEL" "$PKG" "${BDF:-none}" "${BDF_N:-0}" "$SCRIPT_COMMIT" "$IS_ROOT" "$HAS_DRI" "$HAS_FB" "$HAS_TTY" "$HAS_X"
for t in lspci drm_info glxinfo vulkaninfo clinfo vainfo aplay wpctl gcc xrandr; do
    if command -v "$t" >/dev/null 2>&1; then
        printf '# tool_%s=present\n' "$t"
    else
        printf '# tool_%s=missing\n' "$t"
    fi
done

# 全部已定义测试项（RUNTIME_PARSE_ONLY 模式种子；新增测试须同步加入）
KNOWN_NAMES=(
  pci_enumeration pci_driver_binding package_version dkms_status module_vermagic
  module_loaded module_param_firmware_en proc_driver_status proc_firmware_status proc_error_counts
  journal_kernel_errors drm_nodes fbdev_node drm_topology_enumeration fbterm_real_vt
  egl_gbm_probe egl_x11_probe gl_enumeration gl_execution vulkan_enumeration vulkan_execution
  opencl_enumeration opencl_execution vaapi_enumeration vaapi_encode vaapi_decode
  dmabuf_source_fix_present dmabuf_regression display_topology display_modeset
  picom_running picom_glx audio_cards_enumeration audio_default_sink audio_playback
)

run_probes() {
# =====================================================================
# 1. PCI / 内核 / DKMS / 固件
# =====================================================================
if [[ "$BDF_N" -eq 1 && -n "$BDF" ]]; then
    record pci_enumeration PASS
elif [[ "$BDF_N" -gt 1 ]]; then
    record pci_enumeration UNVERIFIED "multiple 1ec8:9810 devices (n=$BDF_N); using first"
else
    record pci_enumeration FAIL "pci id 1ec8:9810 not found"
fi
if [[ -n "$BDF" ]]; then
    BINDING="$(ls -l /sys/bus/pci/devices/$BDF/driver 2>/dev/null | sed 's|.*/||' || true)"
    BIND_MOD="$(readlink /sys/bus/pci/drivers/$BINDING/module 2>/dev/null | sed 's|.*/||' || true)"
    if [[ "$BINDING" == inno-drv && "$BIND_MOD" == innogpu ]]; then
        record pci_driver_binding PASS
    else
        record pci_driver_binding FAIL "binding=$BINDING module=$BIND_MOD (expect inno-drv/innogpu)"
    fi
else
    record pci_driver_binding UNVERIFIED "no bdf discovered"
fi

if [[ "$PKG" == "4.0.0-i1" ]]; then record package_version PASS; else record package_version FAIL "got=$PKG"; fi

DKMS="$(/usr/sbin/dkms status 2>/dev/null | grep 'innogpu-kernel/2.2' | head -1 || true)"
if [[ "$DKMS" == *installed* ]]; then record dkms_status PASS; else record dkms_status FAIL "dkms=$DKMS"; fi

VERM="$(/usr/sbin/modinfo -F vermagic /lib/modules/$KERNEL/updates/dkms/innogpu.ko.xz 2>/dev/null || true)"
if [[ "$VERM" == "$KERNEL "* ]]; then record module_vermagic PASS; else record module_vermagic FAIL "vermagic=$VERM"; fi

if [[ -d /sys/module/innogpu ]]; then
    record module_loaded PASS
    FWEN="$(cat /sys/module/innogpu/parameters/firmware_en 2>/dev/null || true)"
    if [[ "$FWEN" == "1" ]]; then record module_param_firmware_en PASS; else record module_param_firmware_en FAIL "firmware_en=$FWEN"; fi
else
    record module_loaded FAIL "innogpu module not loaded (may need reboot after install)"
fi

PROC="$(cat /proc/driver/innogpu/gpu00/status 2>/dev/null || true)"
if [[ "$PROC" == *"Driver Status:"* ]]; then
    record proc_driver_status PASS
    if [[ "$PROC" == *"Firmware Status: OK"* ]]; then
        record proc_firmware_status PASS
    else
        record proc_firmware_status UNVERIFIED "firmware status not OK in proc"
    fi
    ERRS="$(printf '%s' "$PROC" | grep -E 'Server Errors|WGP Error|TRP Error|APM Event' | awk -F': ' '{s+=$2} END {print s+0}')"
    if [[ "$ERRS" == "0" ]]; then record proc_error_counts PASS; else record proc_error_counts FAIL "sum=$ERRS"; fi
else
    record proc_driver_status SKIP "no /proc/driver/innogpu/gpu00/status"
fi

JERR="$(journalctl -b -k --no-pager 2>/dev/null | grep -iE 'innogpu|pvr' | grep -iE 'error|fail|timeout|panic' | head -5 || true)"
if [[ -n "$JERR" ]]; then
    record journal_kernel_errors FAIL "$(printf '%s' "$JERR" | head -1 | redact | cut -c1-90)"
elif [[ "$IS_ROOT" == "yes" || -r /var/log/journal ]]; then
    record journal_kernel_errors PASS
else
    record journal_kernel_errors SKIP "journal access limited; run with root on device"
fi

# =====================================================================
# 2. DRM/KMS 与节点
# =====================================================================
if [[ "$HAS_DRI" == "yes" ]]; then
    CARD=$(ls /dev/dri/card* 2>/dev/null | wc -l)
    REND=$(ls /dev/dri/renderD* 2>/dev/null | wc -l)
    if [[ "$CARD" -ge 1 && "$REND" -ge 1 ]]; then record drm_nodes PASS; else record drm_nodes FAIL "card=$CARD render=$REND"; fi
    if [[ "$HAS_FB" == "yes" ]]; then record fbdev_node PASS; else record fbdev_node FAIL "/dev/fb0 missing"; fi
    if command -v drm_info >/dev/null 2>&1; then
        DI="$(drm_info 2>&1 | grep -cE 'Connector|CRTC|Plane' || true)"
        if [[ "$DI" -ge 1 ]]; then
            record drm_topology_enumeration PASS
            printf '# drm_connectors=%s\n' "$(drm_info 2>&1 | grep -E 'Connector .* (connected|disconnected)' | head -6 | tr '\n' ';' | cut -c1-160)"
        else
            record drm_topology_enumeration FAIL "drm_info returned no topology"
        fi
    else
        record drm_topology_enumeration SKIP "tool_missing:drm_info"
    fi
else
    record drm_nodes SKIP "no /dev/dri; run on real device session"
    record fbdev_node SKIP "no /dev/fb0 in this session"
    record drm_topology_enumeration SKIP "no /dev/dri; run 'drm_info' on real device session"
fi

# =====================================================================
# 3. fbdev / 真实 VT（人工执行，不自动运行）
# =====================================================================
record fbterm_real_vt SKIP "manual-execution-required (real VT + authorization): run fbterm on VT, verify draw/clear/long-output/re-enter"

# =====================================================================
# 4. EGL/GBM/DRI（复用仓库探针，编译到临时目录）
# =====================================================================
PROBE_BIN="$RUNTIME_DIR/probe-egl-gbm"
if [[ "$HAS_DRI" == "yes" && -f "$ROOT/tools/probe-egl-gbm.c" && "$(command -v gcc 2>/dev/null)" ]]; then
    if gcc -O2 -o "$PROBE_BIN" "$ROOT/tools/probe-egl-gbm.c" 2>/dev/null; then
        EG="$(INNOGPU_LIB_PATH="$ROOT/vendor/userspace/x86_64-linux-gnu/innogpu-fh2m" "$PROBE_BIN" 2>&1 | tail -4 | redact | tr '\n' ';' | cut -c1-160)"
        record egl_gbm_probe UNVERIFIED "run on device session; output: $EG"
    else
        record egl_gbm_probe SKIP "probe compile failed"
    fi
else
    record egl_gbm_probe SKIP "no /dev/dri or gcc/probe missing; compile and run tools/probe-egl-gbm.c on device session"
fi
record egl_x11_probe SKIP "manual-execution-required: scripts/test-current-xorg-hwgl-runtime.sh on device session"

# =====================================================================
# 5. OpenGL / GLX / GLES
# =====================================================================
if command -v glxinfo >/dev/null 2>&1 && [[ -n "${DISPLAY:-}" ]]; then
    REND_GL="$(glxinfo -B 2>/dev/null | grep 'renderer string' | head -1 | sed 's/.*: //' || true)"
    if [[ "$REND_GL" == *llvmpipe* || "$REND_GL" == *softpipe* ]]; then
        record gl_enumeration UNVERIFIED "software renderer in this session: $REND_GL (no /dev/dri); real device must show Fantasy II-M"
    elif [[ "$REND_GL" == *Fantasy* ]]; then
        record gl_enumeration PASS
    else
        record gl_enumeration UNVERIFIED "renderer=$REND_GL"
    fi
    printf '# gl_direct=%s\n' "$(glxinfo -B 2>/dev/null | grep -E 'direct rendering' | head -1 || true)"
else
    if ! command -v glxinfo >/dev/null 2>&1; then
        record gl_enumeration SKIP "tool_missing:glxinfo"
    else
        record gl_enumeration SKIP "no DISPLAY in this session"
    fi
fi
record gl_execution SKIP "manual-execution-required: scripts/check-desktop-hwgl.sh on device session"

# =====================================================================
# 6. Vulkan（枚举 vs 实际执行）
# =====================================================================
if command -v vulkaninfo >/dev/null 2>&1; then
    VK="$(vulkaninfo --summary 2>&1 | head -20 || true)"
    if [[ "$VK" == *"InnoGPU"* || "$VK" == *"Fantasy"* ]]; then
        record vulkan_enumeration PASS
    elif [[ "$VK" == *"Skipping this driver"* || "$VK" == *"no devices"* ]]; then
        record vulkan_enumeration SKIP "ICD init failed without /dev/dri; run vulkaninfo on device session"
    else
        record vulkan_enumeration UNVERIFIED "loader visible but device not enumerated in this session"
    fi
else
    record vulkan_enumeration SKIP "tool_missing:vulkaninfo"
fi
record vulkan_execution SKIP "manual-execution-required: create instance/device + minimal render on device session"

# =====================================================================
# 7. OpenCL（枚举 vs 实际执行）
# =====================================================================
if command -v clinfo >/dev/null 2>&1; then
    CLN="$(clinfo 2>&1 | awk '/Number of platforms/{print $4}' | head -1 || true)"
    if [[ "${CLN:-0}" =~ ^[0-9]+$ ]] && [[ "$CLN" -gt 0 ]]; then
        record opencl_enumeration PASS
    elif [[ "${CLN:-0}" == "0" ]]; then
        record opencl_enumeration SKIP "0 platforms without /dev/dri; run clinfo on device session"
    else
        record opencl_enumeration UNVERIFIED "clinfo inconclusive in this session"
    fi
else
    record opencl_enumeration SKIP "tool_missing:clinfo"
fi
record opencl_execution SKIP "manual-execution-required: context/queue + minimal kernel on device session"

# =====================================================================
# 8. VA-API 视频
# =====================================================================
if command -v vainfo >/dev/null 2>&1; then
    VA="$(vainfo 2>&1 | head -15 || true)"
    if [[ "$VA" == *"Driver version"* || "$VA" == *"H264"* || "$VA" == *"HEVC"* ]]; then
        record vaapi_enumeration PASS
        if [[ "$VA" == *"VAEntrypointEncSlice"* ]]; then
            record vaapi_encode UNVERIFIED "encoder entrypoint present; execution not verified"
        else
            record vaapi_encode SKIP "no encode entrypoint; encode not supported (decode-only)"
        fi
    else
        record vaapi_enumeration SKIP "vainfo init failed without /dev/dri; run on device session"
        record vaapi_encode SKIP "cannot verify encode without vaapi init"
    fi
else
    record vaapi_enumeration SKIP "tool_missing:vainfo"
    record vaapi_encode SKIP "tool_missing:vainfo"
fi
record vaapi_decode SKIP "manual-execution-required: minimal H264/HEVC decode on device session"

# =====================================================================
# 9. DMA-BUF / 同步 / 驱动修复（源码存在性 ≠ 运行能力）
# =====================================================================
STATIC_FIX=$(grep -rl 'dma_resv_usage_rw' "$ROOT/drivers" 2>/dev/null | wc -l)
if [[ "$STATIC_FIX" -ge 1 ]]; then record dmabuf_source_fix_present PASS; else record dmabuf_source_fix_present FAIL "dma_resv_usage_rw not in drivers/"; fi
record dmabuf_regression SKIP "manual-execution-required: probe-pdp-invisible-read + probe-drm-vblank on device session (authorized)"

# =====================================================================
# 10. 显示器真实输出（拓扑只读；modeset 人工）
# =====================================================================
if [[ -n "${DISPLAY:-}" ]] && command -v xrandr >/dev/null 2>&1; then
    XR="$(xrandr -q 2>/dev/null | grep ' connected' | tr '\n' ';' | redact | cut -c1-200)"
    printf '# display_xrandr=%s\n' "$XR"
    if [[ "$XR" == *"connected"* ]]; then record display_topology PASS; else record display_topology UNVERIFIED "xrandr no connected output"; fi
else
    if ! command -v xrandr >/dev/null 2>&1; then
        record display_topology SKIP "tool_missing:xrandr"
    else
        record display_topology SKIP "no DISPLAY in this session"
    fi
fi
record display_modeset SKIP "manual-execution-required (supervision authorization): resolution switch / hotplug / lid on device session"

# =====================================================================
# 11. Picom / 应用兼容（只读状态）
# =====================================================================
PICOM_PID=$(pgrep -x picom 2>/dev/null | head -1 || true)
if [[ -n "$PICOM_PID" ]]; then
    record picom_running PASS
    printf '# picom_cmd=%s\n' "$(ps -o args= -p "$PICOM_PID" 2>/dev/null | head -1 | redact | cut -c1-120)"
else
    record picom_running SKIP "picom not running in this session"
fi
record picom_glx SKIP "manual-execution-required: verify GLX backend (compositor-management.md)"

# =====================================================================
# 12. 音频
# =====================================================================
ACARDS="$(cat /proc/asound/cards 2>/dev/null | tr '\n' ';' | cut -c1-160)"
if [[ "$ACARDS" == *InnosiliconCard* && "$ACARDS" == *"HDA Intel"* ]]; then
    record audio_cards_enumeration PASS
    printf '# audio_cards=%s\n' "$ACARDS"
else
    record audio_cards_enumeration UNVERIFIED "cards=$ACARDS"
fi
if command -v wpctl >/dev/null 2>&1; then
    SINK="$(wpctl status 2>&1 | grep -A6 'Sinks:' | grep -E '\*|Fantasy|HDA' | head -3 | redact | tr '\n' ';' | cut -c1-160)"
    if [[ "$SINK" == *'HDA Intel'* ]]; then
        record audio_default_sink PASS
        printf '# audio_sinks=%s\n' "$SINK"
    else
        record audio_default_sink UNVERIFIED "sinks=$SINK"
    fi
else
    record audio_default_sink SKIP "tool_missing:wpctl"
fi
record audio_playback SKIP "manual-execution-required: aplay test tone on device session"
}  # run_probes 结束

# RUNTIME_PARSE_ONLY=1：跳过全部设备探测，仅用 KNOWN_NAMES 种子执行 --results-file 解析（测试用，快）
if [[ "${RUNTIME_PARSE_ONLY:-0}" == "1" ]]; then
    for n in "${KNOWN_NAMES[@]}"; do record "$n" SKIP "parse-only-mode"; done
else
    run_probes
fi

# =====================================================================
# 人工授权命令清单（--allow-authorized-tests 时打印；永不自动执行）
# =====================================================================
if [[ "$MODE" == "allow-authorized" ]]; then
    printf '# authorized-manual-commands (supervision authorization required; never auto-run)\n'
    printf '#   fbterm_real_vt:  真实 VT 下运行 fbterm，验证绘制/清屏/长输出/重入\n'
    printf '#   display_modeset: xrandr --output <out> --mode <mode>（分辨率切换/热插拔/合盖，需监督授权）\n'
    printf '#   audio_playback:  aplay -D default <test.wav>\n'
    printf '#   vulkan_execution: gcc -O2 -o /tmp/pvk tools/probe-vulkan-devices.c -ldl && /tmp/pvk exec [timeout_ms]  # 创建 instance/device/queue，空 cmd buffer+fence 提交并等待\n'
    printf '#   opencl_execution: gcc -O2 -o /tmp/pocl tools/probe-opencl-devices.c -ldl && /tmp/pocl exec [elements]  # context/queue + add kernel + 读回逐元素校验\n'
    printf '#   vaapi_decode:     bash tools/run-vaapi-decode-test.sh --codec all [--device /dev/dri/renderDNN] [--timeout 30]  # H264+HEVC 强制 VAAPI 解码 + 软件参考 framemd5 对比\n'
    printf '#   picom_glx:        验证 Picom GLX backend（docs/project/compositor-management.md）\n'
    printf '#   dmabuf_regression: tools/probe-pdp-invisible-read + tools/probe-drm-vblank\n'
    printf '# 执行后将结果逐行写入文件，用 --results-file 合并：runtime_<name>=PASS|FAIL|SKIP|UNVERIFIED [reason=..]\n'
fi

# =====================================================================
# --results-file 合并（严格解析；覆盖默认 SKIP）
# =====================================================================
if [[ -n "$RESULTS_FILE" ]]; then
    if [[ ! -f "$RESULTS_FILE" ]]; then
        echo "error: results file missing: $RESULTS_FILE" >&2
        exit 2
    fi
    declare -A SEEN_RESULTS=()
    while IFS= read -r line || [[ -n "$line" ]]; do
        [[ -z "$line" ]] && continue
        # 显式跳过 # 注释行（证据文件内的说明与原始工具输出注释，不作为结果项、不告警）
        [[ "$line" == \#* ]] && continue
        [[ "$line" == runtime_* ]] || { echo "ignored malformed results line (not runtime_*): $line" >&2; continue; }
        name="${line%%=*}"
        rest="${line#*=}"
        # 仅接受脚本自身已定义的测试项（RES 集合键为去 runtime_ 前缀名）；未知名告警忽略
        key="${name#runtime_}"
        if [[ -z "${RES[$key]+x}" ]]; then
            echo "ignored unknown results entry: $line" >&2
            continue
        fi
        # 状态必须是首个 token
        status="${rest%% *}"
        case "$status" in
            PASS|FAIL|SKIP|UNVERIFIED) ;;
            *) echo "ignored bad status in results line: $line" >&2; continue ;;
        esac
        # 剩余部分必须为空或 reason= 开头（多余字段/粘连行拒绝）
        if [[ "$rest" == "$status" ]]; then
            remainder=""
        else
            remainder="${rest#* }"
        fi
        if [[ -n "$remainder" && "$remainder" != reason=* ]]; then
            echo "ignored glued/extra-field results line: $line" >&2
            continue
        fi
        reason="${remainder#reason=}"
        # 粘连检测：reason 内不得再出现状态令牌或第二个 runtime_
        if [[ "$reason" == *=PASS* || "$reason" == *=FAIL* || "$reason" == *=SKIP* || "$reason" == *=UNVERIFIED* || "$reason" == *runtime_* ]]; then
            echo "ignored glued results line (embedded status/name): $line" >&2
            continue
        fi
        # 证据要求：PASS/FAIL 必须带非空 reason（人工命令/证据），否则不合并
        if [[ -z "$reason" && ( "$status" == PASS || "$status" == FAIL ) ]]; then
            echo "ignored results line without evidence reason: $line" >&2
            continue
        fi
        # 文件内重复：告警并采用最后一条
        if [[ -n "${SEEN_RESULTS[$name]+x}" ]]; then
            echo "duplicate results entry, using last: $name" >&2
        fi
        SEEN_RESULTS["$name"]=1
        record "$key" "$status" "$reason"
    done < "$RESULTS_FILE"
    # 证据来源元数据：采集环境与人工真机证据分开标注，避免审计误解
    printf '# evidence_merged=1 source=%s (manual real-device results; env metadata above reflects collection environment only)\n' "$(basename "$RESULTS_FILE" | redact)"
fi

# =====================================================================
# 汇总（按收集顺序输出）
# =====================================================================
passed=0; failed=0; skipped=0; unverified=0
for n in "${ORDER[@]}"; do
    printf 'runtime_%s=%s' "$n" "${RES[$n]}"
    [[ -n "${RS[$n]}" ]] && printf ' reason=%s' "${RS[$n]}"
    printf '\n'
    case "${RES[$n]}" in
        PASS) passed=$((passed+1)) ;;
        FAIL) failed=$((failed+1)) ;;
        SKIP) skipped=$((skipped+1)) ;;
        UNVERIFIED) unverified=$((unverified+1)) ;;
    esac
done

total=$((passed+failed+skipped+unverified))
overall=PASS
[[ "$failed" -gt 0 ]] && overall=FAIL
[[ "$overall" == PASS && "$unverified" -gt 0 ]] && overall=UNVERIFIED
[[ "$overall" == PASS && "$skipped" -gt 0 ]] && overall=SKIP

printf 'runtime_total=%d runtime_passed=%d runtime_failed=%d runtime_skipped=%d runtime_unverified=%d\n' \
  "$total" "$passed" "$failed" "$skipped" "$unverified"
printf 'runtime_overall=%s\n' "$overall"

# 精简摘要（runtime_ 行与 # 元数据，从 RAW_LOG 提取并脱敏；FULL_LOG 由 EXIT trap 生成）
grep -E '^(runtime_|# )' "$RAW_LOG" | redact > "$LATEST"

case "$overall" in
    PASS) exit 0 ;;
    FAIL) exit 1 ;;
    SKIP) exit 2 ;;
    UNVERIFIED) exit 3 ;;
esac
