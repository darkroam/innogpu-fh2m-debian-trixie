#!/bin/bash
# Build the next small-step Deepin baseline: keep Deepin 202504 DKMS source,
# then add only the local display/tty fixes needed by this FH2M device.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

PATCH_VERSION=17 \
BASE_DEB="${BASE_DEB:-${INNOGPU_PATCHED8_DEB:-$ROOT/innogpu-fh2m-trixie_3.3.3.42-patched-8.deb}}" \
APPLY_LOCAL_CONNECTOR_ACPI_MAP=1 \
APPLY_DP_FBCON_FALLBACK=1 \
APPLY_PANEL_BACKLIGHT_FALLBACK=0 \
APPLY_PANEL_PLATFORM_FALLBACK=0 \
APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE=0 \
OUT_DEB=innogpu-fh2m-trixie_3.3.3.42-patched-17.deb \
    scripts/build-patched10-deepin.sh
