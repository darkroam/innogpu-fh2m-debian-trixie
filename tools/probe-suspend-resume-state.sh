#!/usr/bin/env bash
# Read-only desktop-state observer for the reviewed R07 FH2M ABI.
set -Eeuo pipefail

usage() {
    cat <<'EOF'
Usage: sudo tools/probe-suspend-resume-state.sh [--seconds N] [--output DIR]

Observe existing cursor, config-valid, and HAL register accesses. The tool
does not call driver functions, write registers, modeset, or suspend.
EOF
}

SECONDS_TO_SAMPLE=10
OUTPUT=
while [[ $# -gt 0 ]]; do
    case "$1" in
        --seconds)
            [[ $# -ge 2 ]] || { usage >&2; exit 2; }
            SECONDS_TO_SAMPLE=$2
            shift 2
            ;;
        --output)
            [[ $# -ge 2 ]] || { usage >&2; exit 2; }
            OUTPUT=$2
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

[[ $SECONDS_TO_SAMPLE =~ ^[1-9][0-9]?$ ]] || {
    echo "ERROR: --seconds must be an integer from 1 through 99" >&2
    exit 2
}
if [[ $EUID -ne 0 ]]; then
    echo "ERROR: run this observer with sudo" >&2
    exit 1
fi

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
BT="$ROOT/tools/probe-suspend-resume-observer.bt"
OBJECT="$ROOT/vendor/kernel/innosrvkm/innosrvkm.o_shipped"
DESKTOP_USER="${SUDO_USER:-}"
[[ -n $DESKTOP_USER && $DESKTOP_USER != root ]] || {
    echo "ERROR: sudo must preserve the invoking desktop user" >&2
    exit 1
}
DESKTOP_UID="$(id -u "$DESKTOP_USER")"
DESKTOP_GID="$(id -g "$DESKTOP_USER")"
EXPECTED_OBJECT_SHA=30c594629d1d0e32674e793f2f4235afd4efd3f1e92ee4e4ed1920b315618c2b
EXPECTED_KERNEL=6.12.101+deb13-amd64
EXPECTED_VERSION=4.0.1-i3
EXPECTED_MODULE_BUILD_ID=be315ad1dc8de5248bb4d29f84e0a98fbc1978ab
STAMP="$(date +%Y%m%d-%H%M%S)"
BUILD_OUTPUT_ROOT="$ROOT/build"
if [[ -L $BUILD_OUTPUT_ROOT ]]; then
    echo "ERROR: build output root must not be a symlink: $BUILD_OUTPUT_ROOT" >&2
    exit 2
fi
mkdir -p "$BUILD_OUTPUT_ROOT"
if [[ -z $OUTPUT ]]; then
    OUTPUT="$(mktemp -d "$BUILD_OUTPUT_ROOT/r07-observer-$STAMP.XXXXXX")"
else
    case "$OUTPUT" in
        /*) ;;
        *) OUTPUT="$PWD/$OUTPUT" ;;
    esac
    OUTPUT="$(readlink -m -- "$OUTPUT")"
    case "$OUTPUT" in
        "$BUILD_OUTPUT_ROOT"/*|/tmp/*) ;;
        *)
            echo "ERROR: --output must be a new directory below $BUILD_OUTPUT_ROOT or /tmp" >&2
            exit 2
            ;;
    esac
    [[ ! -e $OUTPUT && ! -L $OUTPUT ]] || {
        echo "ERROR: --output already exists: $OUTPUT" >&2
        exit 2
    }
    mkdir -p -- "$OUTPUT"
fi
LOG="$OUTPUT/control.log"
exec > >(tee "$LOG") 2>&1

finalize_output() {
    chmod -R a+rX "$OUTPUT" 2>/dev/null || true
    chown -R "$DESKTOP_UID:$DESKTOP_GID" "$OUTPUT" 2>/dev/null || true
}
trap finalize_output EXIT

fail() {
    echo "r07_observer=FAIL reason=$* output=$OUTPUT" >&2
    exit 1
}

command -v bpftrace >/dev/null || fail missing_bpftrace
command -v pahole >/dev/null || fail missing_pahole
[[ -r /sys/kernel/btf/vmlinux && -r /sys/kernel/btf/innogpu ]] || fail missing_kernel_or_module_btf
[[ $(uname -r) == "$EXPECTED_KERNEL" ]] || fail unexpected_kernel
[[ $(dpkg-query -W -f='${Version}' innogpu-fh2m-trixie 2>/dev/null) == "$EXPECTED_VERSION" ]] ||
    fail unexpected_package_version
[[ -r /sys/module/innogpu/notes/.note.gnu.build-id ]] || fail missing_loaded_module_build_id
LOADED_MODULE_BUILD_ID="$(
    od -An -j16 -N20 -tx1 /sys/module/innogpu/notes/.note.gnu.build-id | tr -d ' \n'
)"
[[ $LOADED_MODULE_BUILD_ID == "$EXPECTED_MODULE_BUILD_ID" ]] || fail loaded_module_build_id_mismatch
[[ $(sha256sum "$OBJECT" | cut -d' ' -f1) == "$EXPECTED_OBJECT_SHA" ]] || fail shipped_object_hash_mismatch

read_layout() {
    local type=$1 layout

    # This shipped relocatable object has usable DWARF, but pahole 1.30 exits
    # nonzero after printing it because the object has no standalone .BTF.
    layout="$(pahole -F dwarf -C "$type" "$OBJECT" 2>/dev/null || :)"
    [[ -n $layout ]] || fail "missing_${type}_layout"
    printf '%s\n' "$layout"
}

ABI="$(read_layout innodpu_pdp0_hw_device)"
grep -Eq 'cursor_resume.*\/\*[[:space:]]+616[[:space:]]+8 \*\/' <<<"$ABI" || fail cursor_resume_offset_mismatch
grep -Eq 'cursor_enable.*\/\*[[:space:]]+820[[:space:]]+1 \*\/' <<<"$ABI" || fail cursor_enable_offset_mismatch
grep -Eq 'cursor_fb.*\/\*[[:space:]]+824[[:space:]]+8 \*\/' <<<"$ABI" || fail cursor_fb_offset_mismatch
grep -Eq 'config_valid.*\/\*[[:space:]]+888[[:space:]]+8 \*\/' <<<"$ABI" || fail config_valid_offset_mismatch
grep -Eq 'crtc;.*\/\*[[:space:]]+904[[:space:]]+8 \*\/' <<<"$ABI" || fail crtc_offset_mismatch
PDP0_ABI="$(read_layout innodpu_pdp0_drm)"
grep -Eq 'struct drm_crtc[[:space:]]+crtc;.*\/\*[[:space:]]+32[[:space:]]+1600 \*\/' <<<"$PDP0_ABI" ||
    fail embedded_crtc_offset_mismatch
CRTC_ABI="$(read_layout drm_crtc)"
grep -Eq 'state;.*\/\*[[:space:]]+1176[[:space:]]+8 \*\/' <<<"$CRTC_ABI" || fail crtc_state_offset_mismatch
CRTC_STATE_ABI="$(read_layout drm_crtc_state)"
grep -Eq 'bool[[:space:]]+active;.*\/\*[[:space:]]+9[[:space:]]+1 \*\/' <<<"$CRTC_STATE_ABI" ||
    fail crtc_active_offset_mismatch
echo "abi_gate=PASS object_sha256=$EXPECTED_OBJECT_SHA"

for symbol in pdp0_cursor_move pdp0_cursor_set pdp0_cursor_resume \
              pdp0_cursor_is_disable innodpu_pdp0_wakeup pdp0_set_config_valid \
              fh2m_hal_reg_read32 fh2m_hal_reg_write32; do
    grep -qw "$symbol" /proc/kallsyms || fail "missing_symbol_$symbol"
done

XAUTH="$(
    find /tmp -maxdepth 1 -type f -user "$DESKTOP_USER" -name 'serverauth.*' \
        -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -1 | cut -d' ' -f2-
)"
[[ -n $XAUTH && -r $XAUTH ]] || fail missing_xauthority
runuser -u "$DESKTOP_USER" -- env DISPLAY=:0 XAUTHORITY="$XAUTH" \
    xrandr --current > "$OUTPUT/xrandr.txt" || fail xrandr_unavailable

{
    echo "timestamp=$(date --iso-8601=seconds)"
    echo "kernel=$(uname -r)"
    echo "package_version=$(dpkg-query -W -f='${Version}' innogpu-fh2m-trixie)"
    echo "mem_sleep=$(</sys/power/mem_sleep)"
    for connector in card0-HDMI-A-1 card0-HDMI-A-2 card0-eDP-1; do
        [[ -e /sys/class/drm/$connector/status ]] || continue
        echo "$connector.status=$(<"/sys/class/drm/$connector/status")"
        echo "$connector.enabled=$(<"/sys/class/drm/$connector/enabled")"
        echo "$connector.dpms=$(<"/sys/class/drm/$connector/dpms")"
    done
    echo "hdmi2_edid_sha256=$(sha256sum /sys/class/drm/card0-HDMI-A-2/edid | cut -d' ' -f1)"
} > "$OUTPUT/display-state.txt"
cp -- /proc/driver/innogpu/gpu00/status "$OUTPUT/pvr-status.txt"
find /sys/kernel/debug/dri -maxdepth 3 -type f -name state -print0 2>/dev/null |
    while IFS= read -r -d '' state; do
        cp -- "$state" "$OUTPUT/drm-state-$(basename "$(dirname "$state")").txt"
    done
drm_info > "$OUTPUT/drm-info.txt" 2>&1 || true

mapfile -t DRM_STATE_FILES < <(
    find "$OUTPUT" -maxdepth 1 -type f -name 'drm-state-*.txt' -print | sort
)
DRM_ACTIVE_CRTCS=UNAVAILABLE
if (( ${#DRM_STATE_FILES[@]} > 0 )); then
    DRM_ACTIVE_CRTCS="$(awk '
        /^crtc\[/ { name = $2 }
        /^[[:space:]]+active=1$/ {
            if (active != "") active = active ","
            active = active name
        }
        END { print active == "" ? "none" : active }
    ' "${DRM_STATE_FILES[@]}")"
fi

RAW="$OUTPUT/bpftrace.txt"
echo "sampling_seconds=$SECONDS_TO_SAMPLE"
echo "ACTION: move the pointer continuously over HDMI-2 during this non-suspend sample"
bpftrace "$BT" > "$RAW" 2>&1 &
BPF_PID=$!
cleanup() {
    local rc=$?
    trap - HUP INT TERM EXIT
    set +e
    if kill -0 "$BPF_PID" 2>/dev/null; then
        kill -INT "$BPF_PID" 2>/dev/null || true
        wait "$BPF_PID" 2>/dev/null || true
    fi
    finalize_output
    return "$rc"
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
sleep "$SECONDS_TO_SAMPLE"
kill -INT "$BPF_PID"
set +e
wait "$BPF_PID"
BPF_RC=$?
set -e
trap - HUP INT TERM
(( BPF_RC == 0 || BPF_RC == 130 )) || fail "bpftrace_rc_$BPF_RC"
grep -Fq 'r07_observer=START' "$RAW" || fail bpftrace_did_not_start
grep -Fq 'r07_observer=STOP' "$RAW" || fail bpftrace_did_not_stop_cleanly

STATE_EVENTS="$(grep -c '^r07_event=state ' "$RAW" || true)"
ACTIVE_VALID_EVENTS="$(grep -c ' active_valid=1 ' "$RAW" || true)"
CURSOR_ENABLED_EVENTS="$(grep -c ' cursor_enable=1 ' "$RAW" || true)"
CURSOR_RESUME_EVENTS="$(grep -c 'source=kprobe:innogpu:pdp0_cursor_resume ' "$RAW" || true)"
REG_READ_EVENTS="$(grep -c '^r07_event=hal_read ' "$RAW" || true)"
REG_WRITE_EVENTS="$(grep -c '^r07_event=hal_write ' "$RAW" || true)"

{
    echo "r07_state_events=$STATE_EVENTS"
    echo "r07_active_valid_events=$ACTIVE_VALID_EVENTS"
    echo "r07_drm_active_crtcs=$DRM_ACTIVE_CRTCS"
    echo "r07_cursor_enabled_events=$CURSOR_ENABLED_EVENTS"
    echo "r07_cursor_resume_events=$CURSOR_RESUME_EVENTS"
    echo "r07_cursor_register_reads=$REG_READ_EVENTS"
    echo "r07_cursor_register_writes=$REG_WRITE_EVENTS"
    if (( CURSOR_ENABLED_EVENTS > 0 )); then
        echo "r07_cursor_branch=OBSERVED"
    elif (( STATE_EVENTS > 0 )); then
        echo "r07_cursor_branch=NOT_ACTIVE_IN_SAMPLE"
    else
        echo "r07_cursor_branch=UNVERIFIED_NO_DRIVER_CURSOR_EVENTS"
    fi
    echo "r07_register_readback=$([[ $REG_READ_EVENTS -gt 0 ]] && echo OBSERVED || echo UNAVAILABLE_NO_NATURAL_READS)"
    echo "r07_primary_fb_content_crc=UNAVAILABLE_NO_READ_ONLY_KERNEL_INTERFACE"
    echo "r07_observer_result=PASS"
} | tee "$OUTPUT/summary.txt"

echo "r07_observer=PASS output=$OUTPUT"
