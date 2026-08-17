#!/bin/bash
# Build patched-24: patched-23 plus the Debian 6.12.101+ PCI API fix.
# This produces a package only; installation and reboot require explicit action.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

PATCH_VERSION=24 \
SOURCE_DATE_EPOCH=1787006400 \
APPLY_LOCAL_CONNECTOR_ACPI_MAP=1 \
APPLY_LOCAL_INTERNAL_EDP=1 \
APPLY_DP_FBCON_FALLBACK=1 \
APPLY_PANEL_BACKLIGHT_FALLBACK=0 \
APPLY_PANEL_PLATFORM_FALLBACK=0 \
APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE=0 \
APPLY_FBDEV_IO_MMAP=1 \
APPLY_PVR_INIT_DIAGNOSTIC=0 \
APPLY_INVISIBLE_READ_NO_WRITEBACK=1 \
OUT_DEB="${OUT_DEB:-$ROOT/debs/innogpu-fh2m-trixie_3.3.3.42-patched-24.deb}" \
    scripts/build-deepin-coherent.sh
