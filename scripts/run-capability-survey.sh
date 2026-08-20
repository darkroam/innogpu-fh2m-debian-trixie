#!/bin/bash
# Run the FH2M capability survey (Vulkan + OpenCL + VA-API + sysfs snapshot).
# Read-only: compiles the minimal probes into a temp dir and runs them; when the
# authoritative tools (vulkaninfo/clinfo/vainfo) are installed they are used as
# the primary source for each section. Raw output is saved to a timestamped log.
# No modeset, no config change, no driver modification.
#
# Usage:
#   scripts/run-capability-survey.sh [--out DIR] [--vaapi-only]
#
#   --out DIR     write the log to DIR/capability-survey-<ts>.log (default:
#                 baselines/capability-survey-<ts>.log under the repo root)
#   --vaapi-only  run only the VA-API section (fast retest after probe changes)
#
# In a session with /dev/dri access (real desktop or TTY) the survey reports the
# Fantasy II-M device. Without DRM render nodes (e.g. unprivileged container)
# tools degrade gracefully and the failure is itself evidence.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

OUT_DIR="$ROOT/baselines"
VAAPI_ONLY=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --out)
            OUT_DIR="$2"; shift 2 ;;
        --vaapi-only)
            VAAPI_ONLY=1; shift ;;
        *)
            echo "usage: $0 [--out DIR] [--vaapi-only]" >&2; exit 2 ;;
    esac
done
mkdir -p "$OUT_DIR"

BUILD_DIR="$(mktemp -d /tmp/capability-survey.XXXXXX)"
trap 'rm -rf "$BUILD_DIR"' EXIT

for probe in probe-vulkan-devices probe-opencl-devices probe-vaapi; do
    gcc -O2 -Wall -Wextra -o "$BUILD_DIR/$probe" "tools/$probe.c" -ldl
done

OUT="$OUT_DIR/capability-survey-$(date +%Y%m%d-%H%M%S).log"
: > "$OUT"

echo "=== FH2M capability survey $(date -Is) ===" | tee -a "$OUT"
echo "render nodes: $(ls /dev/dri/ 2>/dev/null | tr '\n' ' ' || echo 'none')" | tee -a "$OUT"
echo "module: $(lsmod | awk '/^innogpu/{print $1, $2, $3}')" | tee -a "$OUT"

echo | tee -a "$OUT"
echo "=== DRM topology (connectors) ===" | tee -a "$OUT"
for c in /sys/class/drm/card0-*/status; do
    [ -e "$c" ] || continue
    echo "${c#/sys/class/drm/}: $(cat "$c")" | tee -a "$OUT"
done
echo "gpu-info: $(cat /sys/class/drm/card0/device/gpu-info 2>/dev/null || echo n/a)" | tee -a "$OUT"
echo "power: $(cat /sys/class/drm/card0/device/power/runtime_status 2>/dev/null || echo n/a)" | tee -a "$OUT"
if command -v drm_info >/dev/null 2>&1; then
    echo | tee -a "$OUT"
    echo "=== drm_info (KMS topology) ===" | tee -a "$OUT"
    drm_info 2>&1 | tee -a "$OUT" || true
fi

if [[ "$VAAPI_ONLY" -eq 1 ]]; then
    echo "=== VA-API only mode ===" | tee -a "$OUT"
    "$BUILD_DIR/probe-vaapi" | tee -a "$OUT" || true
    echo | tee -a "$OUT"
    echo "raw output saved to: $OUT" | tee -a "$OUT"
    exit 0
fi

echo | tee -a "$OUT"
echo "=== Vulkan enumeration ===" | tee -a "$OUT"
if command -v vulkaninfo >/dev/null 2>&1; then
    vulkaninfo --summary 2>&1 | tee -a "$OUT" || true
else
    echo "(vulkaninfo not installed; using minimal probe)" | tee -a "$OUT"
    "$BUILD_DIR/probe-vulkan-devices" | tee -a "$OUT" || true
fi

echo | tee -a "$OUT"
echo "=== OpenCL enumeration ===" | tee -a "$OUT"
if command -v clinfo >/dev/null 2>&1; then
    clinfo 2>&1 | tee -a "$OUT" || true
else
    echo "(clinfo not installed; using minimal probe)" | tee -a "$OUT"
    "$BUILD_DIR/probe-opencl-devices" | tee -a "$OUT" || true
fi

echo | tee -a "$OUT"
echo "=== VA-API (video codec) enumeration ===" | tee -a "$OUT"
if command -v vainfo >/dev/null 2>&1; then
    vainfo 2>&1 | tee -a "$OUT" || true
else
    echo "(vainfo not installed; using minimal probe)" | tee -a "$OUT"
    "$BUILD_DIR/probe-vaapi" | tee -a "$OUT" || true
fi

echo | tee -a "$OUT"
echo "raw output saved to: $OUT" | tee -a "$OUT"
