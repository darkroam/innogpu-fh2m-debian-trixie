#!/bin/bash
# Historical builder retained only to prevent reuse of the validated version.

set -euo pipefail

echo "ERROR: patched-20 is the installed historical diagnostic artifact and must not be rebuilt." >&2
echo "Its validated deb predates the current xdisplay/package ownership boundary." >&2
echo "Define a new version greater than 20 before using scripts/build-deepin-coherent.sh." >&2
exit 2
