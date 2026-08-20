#!/bin/bash
# Build patched-26: the patched-25 patch set plus the inactive-CRTC vblank
# guard (patch-026). This produces a package only; installation and reboot
# require explicit action.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

PATCH_VERSION=26 \
SOURCE_DATE_EPOCH=1787227200 \
APPLY_LOCAL_CONNECTOR_ACPI_MAP=1 \
APPLY_LOCAL_INTERNAL_EDP=1 \
APPLY_DP_FBCON_FALLBACK=1 \
APPLY_PANEL_BACKLIGHT_FALLBACK=0 \
APPLY_PANEL_PLATFORM_FALLBACK=0 \
APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE=0 \
APPLY_FBDEV_IO_MMAP=1 \
APPLY_PVR_INIT_DIAGNOSTIC=0 \
APPLY_INVISIBLE_READ_NO_WRITEBACK=1 \
APPLY_DMA_RESV_USAGE_FIX=1 \
APPLY_INACTIVE_CRTC_VBLANK_GUARD=1 \
OUT_DEB="${OUT_DEB:-$ROOT/debs/innogpu-fh2m-trixie_3.3.3.42-patched-26.deb}" \
    scripts/build-deepin-coherent.sh
