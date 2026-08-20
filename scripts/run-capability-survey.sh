#!/bin/bash
# Run the FH2M capability survey probes (Vulkan + OpenCL enumeration).
# Read-only: compiles the two minimal probes into a temp dir, runs them with the
# vendor ICD environment, and tees the raw output to /tmp. No modeset, no config
# change, no driver modification.
#
# Usage:
#   scripts/run-capability-survey.sh
#
# In a session with /dev/dri access (real desktop or TTY) the probes report the
# Fantasy II-M device. Without DRM render nodes (e.g. unprivileged container)
# they degrade gracefully and report the failure, which is itself evidence.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

BUILD_DIR="$(mktemp -d /tmp/capability-survey.XXXXXX)"
trap 'rm -rf "$BUILD_DIR"' EXIT

for probe in probe-vulkan-devices probe-opencl-devices; do
    gcc -O2 -Wall -Wextra -o "$BUILD_DIR/$probe" "tools/$probe.c" -ldl
done

OUT="/tmp/capability-survey-$(date +%Y%m%d-%H%M%S).log"
: > "$OUT"

echo "=== FH2M capability survey $(date -Is) ===" | tee -a "$OUT"
echo "render nodes: $(ls /dev/dri/ 2>/dev/null | tr '\n' ' ' || echo 'none')" | tee -a "$OUT"

echo | tee -a "$OUT"
echo "=== Vulkan enumeration ===" | tee -a "$OUT"
"$BUILD_DIR/probe-vulkan-devices" | tee -a "$OUT" || true

echo | tee -a "$OUT"
echo "=== OpenCL enumeration ===" | tee -a "$OUT"
"$BUILD_DIR/probe-opencl-devices" | tee -a "$OUT" || true

echo | tee -a "$OUT"
echo "raw output saved to: $OUT" | tee -a "$OUT"
