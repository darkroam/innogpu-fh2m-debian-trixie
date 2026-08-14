#!/bin/bash
set -euo pipefail

echo "ERROR: patched-18 is a historical test artifact with a retired mixed-payload builder." >&2
echo "Use scripts/build-patched19-deepin-coherent.sh for the Deepin 202504 full-payload build." >&2
exit 2
