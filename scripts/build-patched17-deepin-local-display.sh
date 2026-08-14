#!/bin/bash
# Historical builder retained only to make the retired interface fail clearly.

set -euo pipefail

echo "ERROR: patched-17 is a historical result and must not be rebuilt as a new baseline." >&2
echo "Use scripts/build-patched19-deepin-coherent.sh for the Deepin 202504 full-payload build." >&2
exit 2
