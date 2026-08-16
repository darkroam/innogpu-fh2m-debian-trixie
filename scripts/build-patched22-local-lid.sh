#!/bin/bash
# Build the local connector classification candidate for lid/power validation.
# This produces a package only; deployment and reboot require explicit operator action.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

PATCH_VERSION=22 \
SOURCE_DATE_EPOCH=1786838400 \
APPLY_LOCAL_CONNECTOR_ACPI_MAP=1 \
APPLY_LOCAL_INTERNAL_EDP=1 \
APPLY_DP_FBCON_FALLBACK=1 \
APPLY_PANEL_BACKLIGHT_FALLBACK=0 \
APPLY_PANEL_PLATFORM_FALLBACK=0 \
APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE=0 \
APPLY_FBDEV_IO_MMAP=1 \
APPLY_PVR_INIT_DIAGNOSTIC=0 \
OUT_DEB="${OUT_DEB:-$ROOT/debs/innogpu-fh2m-trixie_3.3.3.42-patched-22.deb}" \
    scripts/build-deepin-coherent.sh
