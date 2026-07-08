#!/bin/bash
set -euo pipefail
ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
exec "$ROOT/scripts/uninstall-innogpu.sh" "3.3.3.42-patched-8"
