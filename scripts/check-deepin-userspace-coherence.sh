#!/bin/bash
# Verify that hardware-acceleration probes use the Deepin 202504 userspace
# payload first, not older disabled Kylin/UOS files left in /usr.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
SRC_ROOT="${INNOGPU_DEEPIN_ROOT:-$ROOT/third_party/innogpu-fh2m-deepin-202504/root}"

section() {
    printf '\n===== %s =====\n' "$*"
}

require_file() {
    local p="$1"
    if [[ -e "$p" || -L "$p" ]]; then
        echo "ok=$p"
    else
        echo "missing=$p"
        return 1
    fi
}

section "Deepin 202504 Required Files"
missing=0
for p in \
    "$SRC_ROOT/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so" \
    "$SRC_ROOT/usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so" \
    "$SRC_ROOT/usr/lib/x86_64-linux-gnu/innogpu-fh2m/libEGL_inno.so.0.0.0" \
    "$SRC_ROOT/usr/lib/x86_64-linux-gnu/innogpu-fh2m/libGLX_inno.so.0.0.0" \
    "$SRC_ROOT/usr/lib/x86_64-linux-gnu/innogpu-fh2m/libglapi_inno.so.0.0.0" \
    "$SRC_ROOT/usr/share/glvnd/egl_vendor.d/00_inno.json"; do
    require_file "$p" || missing=1
done

section "Key File Hashes"
sha256sum \
    "$SRC_ROOT/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so" \
    "$SRC_ROOT/usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so" \
    "$SRC_ROOT/usr/lib/x86_64-linux-gnu/innogpu-fh2m/libEGL_inno.so.0.0.0" \
    "$SRC_ROOT/usr/lib/x86_64-linux-gnu/innogpu-fh2m/libGLX_inno.so.0.0.0" \
    2>/dev/null || true

section "Installed Disabled Files Are Not The Source Of Truth"
for p in \
    /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so.disabled \
    /usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so.disabled; do
    if [[ -e "$p" || -L "$p" ]]; then
        ls -l "$p"
        if [[ -f "$p" ]]; then
            sha256sum "$p" || true
        fi
    else
        echo "not_present=$p"
    fi
done

section "Probe Script Source Priority"
failed=0
for script in \
    "$ROOT/scripts/run-deepin-surfaceless-egl.sh" \
    "$ROOT/scripts/test-isolated-deepin-egl-gbm.sh" \
    "$ROOT/scripts/test-isolated-deepin-hwgl.sh" \
    "$ROOT/scripts/test-isolated-deepin-xorg-ddx.sh"; do
    echo "-- $script"
    if grep -q '"$SRC_ROOT/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so"' "$script" &&
       grep -q '/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so.disabled' "$script"; then
        first_src_line="$(grep -n '"$SRC_ROOT/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so"' "$script" | head -1 | cut -d: -f1)"
        first_usr_line="$(grep -n '/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so.disabled' "$script" | head -1 | cut -d: -f1)"
        echo "deepin_dri_line=$first_src_line disabled_dri_line=$first_usr_line"
        if (( first_src_line < first_usr_line )); then
            echo "priority=OK_DEEPIN_FIRST"
        else
            echo "priority=FAIL_DISABLED_FIRST"
            failed=1
        fi
    else
        echo "priority=SKIPPED_NO_DRI_FALLBACK_PATTERN"
    fi
done

section "Deepin DRI Exported Entry Points"
nm -D "$SRC_ROOT/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so" 2>/dev/null |
    grep -E '__driDriver(GetExtensions|Extensions)' || true

section "Result"
if [[ "$missing" == "0" && "$failed" == "0" ]]; then
    echo "RESULT: PASS_DEEPIN_USERSPACE_COHERENT"
else
    echo "RESULT: FAIL"
    exit 1
fi
