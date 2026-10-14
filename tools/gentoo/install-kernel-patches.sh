#!/bin/bash
# No-portage fallback. The ebuild is the normal path and calls the same script.
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
export ROOT
exec "$ROOT/tools/gentoo/overlay/sys-kernel/fantgpu-kernel-patches/files/install-patches.sh"
