#!/bin/bash
set -euo pipefail

echo "ERROR: patched-18 is a historical test artifact with a retired mixed-payload builder." >&2
echo "Define a new version greater than 20 before using scripts/build-deepin-coherent.sh." >&2
exit 2
