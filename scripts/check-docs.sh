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
    rg -n '/home/ok|MiWiFi|serverauth\.[[:alnum:]]' \
        README.md docs scripts tests config patches 2>/dev/null |
        rg -v '不得写死 `/home/ok`|rg -n .*/home/ok' || true
)"
if [[ -n "$personal_refs" ]]; then
    printf '%s\n' "$personal_refs" >&2
    fail "repository contains machine-specific paths or names"
fi

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
