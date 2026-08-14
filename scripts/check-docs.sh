#!/bin/bash
# Check repository documentation links, compatibility entries, and local data.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

failures=0

fail() {
    printf 'FAIL: %s\n' "$*" >&2
    failures=$((failures + 1))
}

while IFS= read -r -d '' file; do
    while IFS= read -r target; do
        [[ -n "$target" ]] || continue
        resolved="$(realpath -m "$(dirname "$file")/$target")"
        [[ -e "$resolved" ]] || fail "$file links to missing $target"
    done < <(
        perl -ne '
            while (/\[[^]]*\]\(([^)#]+)(?:#[^)]*)?\)/g) {
                print "$1\n" unless $1 =~ m{^(?:https?:|mailto:|/)};
            }
        ' "$file"
    )
done < <(find README.md docs scripts baselines tests -type f -name '*.md' -print0)

personal_refs="$(
    rg -n --pcre2 \
        '(/home/[a-z_][a-z0-9_-]*(?:/|$)|MiWiFi|serverauth\.[[:alnum:]]|-----BEGIN (?:OPENSSH |RSA |EC )?PRIVATE KEY-----|ssh-(?:rsa|ed25519) AAAA|(?:ghp|github_pat)_[A-Za-z0-9_]+)' \
        README.md docs scripts tests config patches tools \
        --glob '!scripts/check-docs.sh' 2>/dev/null |
        rg -v '不得写死 `/home/ok`' || true
)"
if [[ -n "$personal_refs" ]]; then
    printf '%s\n' "$personal_refs" >&2
    fail "repository contains a home path, machine identity, authentication file, or secret marker"
fi

for removed_copy in \
    scripts/xdisplay.sh \
    scripts/displayselect \
    tests/xdisplay/run-stage2-tests.sh \
    tests/xdisplay/run-stage2-watch-tests.sh \
    tests/xdisplay/run-stage4-regression-tests.sh; do
    [[ ! -e "$removed_copy" ]] || fail "dotconfig-owned xdisplay copy returned: $removed_copy"
done

stale_xdisplay_refs="$(
    rg -n \
        'scripts/xdisplay\.sh|scripts/displayselect|tests/xdisplay/run-stage[24](-watch|-regression)?-tests\.sh' \
        README.md docs scripts tests baselines \
        --glob '!docs/archive/**' \
        --glob '!docs/planning/history.md' \
        --glob '!docs/planning/display-integration.md' \
        --glob '!docs/project/display-management.md' \
        --glob '!scripts/check-docs.sh' 2>/dev/null || true
)"
if [[ -n "$stale_xdisplay_refs" ]]; then
    printf '%s\n' "$stale_xdisplay_refs" >&2
    fail "current documentation references a removed xdisplay implementation"
fi

[[ -r tools/patch-gpupll-object.py ]] || fail "GPU PLL object patch tool is missing or unreadable"
grep -Fq 'tools/patch-gpupll-object.py' scripts/build-deepin-coherent.sh ||
    fail "coherent builder does not invoke the GPU PLL object patch tool"
for expected_setting in \
    'PATCH_VERSION=20' \
    'APPLY_DP_FBCON_FALLBACK=1' \
    'APPLY_PANEL_BACKLIGHT_FALLBACK=0' \
    'APPLY_PANEL_PLATFORM_FALLBACK=0' \
    'APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE=0' \
    'APPLY_LOCAL_CONNECTOR_ACPI_MAP=1' \
    'APPLY_FBDEV_IO_MMAP=1' \
    'APPLY_PVR_INIT_DIAGNOSTIC=1'; do
    grep -Fq "$expected_setting" scripts/build-patched20-deepin-diagnostic.sh ||
        fail "patched-20 wrapper is missing $expected_setting"
done

for path in \
    docs/project/architecture.md \
    docs/project/status.md \
    docs/project/compositor-management.md \
    docs/project/display-management.md \
    docs/project/maintenance-policy.md \
    docs/planning/todo.md \
    docs/planning/history.md \
    docs/user/new-device-install.md \
    docs/user/picom-install.md \
    docs/user/verification.md \
    docs/user/recovery.md; do
    [[ -f "$path" ]] || fail "required document is missing: $path"
done

for path in docs/patches/README.md docs/incidents/README.md scripts/README.md debs/README.md; do
    [[ -f "$path" ]] || fail "required index is missing: $path"
done

if (( failures > 0 )); then
    printf 'RESULT: FAIL_DOCS (%d issue(s))\n' "$failures" >&2
    exit 1
fi

echo "RESULT: PASS_DOCS"
