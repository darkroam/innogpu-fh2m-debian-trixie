#!/bin/bash
# Historical builder retained only to make the retired interface fail clearly.

set -euo pipefail

echo "ERROR: patched-17 is a historical result and must not be rebuilt as a new baseline." >&2
echo "Define a new version greater than 20 before using scripts/build-deepin-coherent.sh." >&2
exit 2
