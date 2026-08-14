#!/bin/bash
# Historical builder retained only to prevent reuse of a published version.

set -euo pipefail

echo "ERROR: patched-19 is a historical artifact and its version number must not be rebuilt." >&2
echo "The current package helper boundary differs from the original patched-19 payload." >&2
echo "Define a new version greater than 20 before using scripts/build-deepin-coherent.sh." >&2
exit 2
