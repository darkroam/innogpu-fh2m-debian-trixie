#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PATCH="$ROOT/components/linux/002-kaitian-x7h-battery-notification-delay.patch"
EXPECTED=d7d6740eaab078eeaa40d87306c4d77fc61436f50ef31061416f0d1689899e1a

[[ $(sha256sum "$PATCH" | awk '{print $1}') == "$EXPECTED" ]]
[[ $(grep -Fc 'DMI_MATCH(DMI_SYS_VENDOR, "KaiTian")' "$PATCH") -eq 1 ]]
[[ $(grep -Fc 'DMI_MATCH(DMI_PRODUCT_NAME, "X7h G1e")' "$PATCH") -eq 1 ]]
[[ $(grep -Fc '.callback = battery_notification_delay_quirk' "$PATCH") -eq 1 ]]
grep -Fq 'Baseline: Debian linux-source-6.12 6.12.107-1.' "$PATCH"

printf 'battery_delay_static=PASS\n'
printf 'tests_total=1 tests_passed=1 tests_failed=0 tests_skipped=0\n'
