#!/bin/bash

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PATCH="$ROOT/components/linux/001-hygon-148c-xhci-reset-on-resume.patch"
EXPECTED=299eb3f3d688a48518278c1ac5c6e97be89c8cc8f2c50bdcbfc4881bd47a5614

[[ "$(sha256sum "$PATCH" | awk '{print $1}')" == "$EXPECTED" ]]
grep -Fq 'pdev->vendor == PCI_VENDOR_ID_HYGON' "$PATCH"
grep -Fq 'pdev->device == PCI_DEVICE_ID_AMD_STARSHIP_XHCI' "$PATCH"
grep -Fq 'xhci->quirks |= XHCI_RESET_ON_RESUME;' "$PATCH"

printf 'xhci_quirk_static=PASS\n'
printf 'tests_total=1 tests_passed=1 tests_failed=0 tests_skipped=0\n'
