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

require_text() {
    local file=$1 text=$2
    grep -Fq "$text" "$file" || fail "$file is missing required current-state text: $text"
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

# Payload manifest schema must stay valid (read-only; works without vendor/).
if ! python3 tools/validate-binary-manifest.py >/dev/null 2>&1; then
    fail "binary-manifest.json schema validation failed"
fi
# Migration boundary: drivers/ must never carry build artifacts or black-box objects.
if git ls-files drivers | grep -E '\.o_shipped$|\.o\.cmd$' >/dev/null; then
    fail "drivers/ tracks build artifacts or black-box objects (.o_shipped/.o.cmd)"
fi

[[ -r tools/patch-gpupll-object.py ]] || fail "GPU PLL object patch tool is missing or unreadable"
grep -Fq 'tools/patch-gpupll-object.py' scripts/build-deepin-coherent.sh ||
    fail "coherent builder does not invoke the GPU PLL object patch tool"

if ! grep -Eq 'PATCH_VERSION.*<= 20' scripts/build-deepin-coherent.sh; then
    fail "coherent builder does not reject historical version numbers through patched-20"
fi
grep -Fq 'export SOURCE_DATE_EPOCH' scripts/build-deepin-coherent.sh ||
    fail "coherent builder does not export a reviewed reproducible-build epoch"
for expected_setting in \
    'PATCH_VERSION=21' \
    'SOURCE_DATE_EPOCH=1786665600' \
    'APPLY_DP_FBCON_FALLBACK=1' \
    'APPLY_PANEL_BACKLIGHT_FALLBACK=0' \
    'APPLY_PANEL_PLATFORM_FALLBACK=0' \
    'APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE=0' \
    'APPLY_LOCAL_CONNECTOR_ACPI_MAP=1' \
    'APPLY_FBDEV_IO_MMAP=1' \
    'APPLY_PVR_INIT_DIAGNOSTIC=0'; do
    grep -Fq "$expected_setting" scripts/build-patched21-deepin-release-candidate.sh ||
        fail "patched-21 wrapper is missing $expected_setting"
done

for expected_setting in \
    'PATCH_VERSION=23' \
    'SOURCE_DATE_EPOCH=1786924800' \
    'APPLY_DP_FBCON_FALLBACK=1' \
    'APPLY_LOCAL_CONNECTOR_ACPI_MAP=1' \
    'APPLY_LOCAL_INTERNAL_EDP=1' \
    'APPLY_FBDEV_IO_MMAP=1' \
    'APPLY_PVR_INIT_DIAGNOSTIC=0' \
    'APPLY_INVISIBLE_READ_NO_WRITEBACK=1'; do
    grep -Fq "$expected_setting" scripts/build-patched23-invisible-read-fix.sh ||
        fail "patched-23 wrapper is missing $expected_setting"
done
grep -Fq 'patches/023-invisible-read-no-writeback.patch' scripts/build-deepin-coherent.sh ||
    fail "coherent builder does not apply the patched-23 driver fix"

# Current-state guards: bumping the driver version requires updating the
# README current-package marker below (and the architecture require_text).
require_text README.md '当前驱动包 `4.0.0-i1`'
require_text README.md '(docs/project/licensing.md)'
require_text docs/README.md '[许可证与再分发边界](project/licensing.md)'

# The stable p21 evidence remains navigationally referenced while p22 is the
# currently booted connector-classification candidate.
require_text docs/patches/patched-21-release-candidate.md 'RUNTIME_VALIDATION: PASS_ON_CURRENT_DEVICE'
require_text docs/project/status.md "\`3.3.3.42-patched-21\` 已安装、重启"
require_text docs/patches/README.md 'p21 已在当前设备运行验收'
require_text docs/project/dependencies.md '当前设备已完成运行验收的候选'
require_text docs/project/architecture.md '4.0.0-i1` 是当前设备已安装并重启'
require_text debs/README.md '已在当前设备完成部署、重启和运行验收'
require_text docs/user/new-device-install.md 'patched-21 已完成当前设备的构建、包边界、部署、重启和运行验收'
require_text docs/user/recovery.md 'patched-22 -> patched-21 -> patched-17 -> patched-8'
require_text docs/patches/patch-009-local-internal-edp-connector.md '已在当前设备安装并重启验证'

stale_p21_state="$(
    rg -n 'p21 (仅离线验证|启用但未运行验证)|patched-21.*尚未安装或运行验收' \
        README.md docs debs --glob '!docs/archive/**' 2>/dev/null || true
)"
if [[ -n "$stale_p21_state" ]]; then
    printf '%s\n' "$stale_p21_state" >&2
    fail "current documentation still describes p21 as offline-only or uninstalled"
fi

if grep -Fq '3.3.3.42-patched-20` 是当前已安装' docs/project/architecture.md; then
    fail "project architecture still describes historical patched-20 as the current installation"
fi

if rg -q 'baselines/latest-' docs/project/status.md; then
    fail "current status must not cite unversioned historical baseline files as p21 evidence"
fi

# Legacy retention guards (Phase 5 step 1): old patched wrappers stay in
# scripts/ as permanent version guards / rollback evidence; must NOT be
# removed even after the new architecture becomes the default builder.
for historical_wrapper in \
    scripts/build-patched17-deepin-local-display.sh \
    scripts/build-patched18-deepin-local-display.sh \
    scripts/build-patched19-deepin-coherent.sh \
    scripts/build-patched20-deepin-diagnostic.sh; do
    if "$historical_wrapper" >/dev/null 2>&1; then
        fail "historical builder still succeeds: $historical_wrapper"
    fi
done

while IFS= read -r script; do
    name=${script##*/}
    grep -Fq "\`$name\`" scripts/README.md || fail "script is not registered in scripts/README.md: $script"
done < <(find scripts -maxdepth 1 -type f -name '*.sh' | sort)

while IFS= read -r tool; do
    name=${tool##*/}
    grep -Fq "\`$name\`" tools/README.md || fail "tool is not registered in tools/README.md: $tool"
done < <(find tools -maxdepth 1 -type f ! -name 'README.md' | sort)

for path in \
    docs/project/architecture.md \
    docs/project/status.md \
    docs/project/compositor-management.md \
    docs/project/display-management.md \
    docs/project/glossary.md \
    docs/project/maintenance-policy.md \
    docs/patches/patched-21-release-candidate.md \
    docs/incidents/patched-20-legacy-helper-payload.md \
    docs/planning/todo.md \
    docs/planning/history.md \
    docs/user/new-device-install.md \
    docs/user/picom-install.md \
    docs/user/verification.md \
    docs/user/recovery.md; do
    [[ -f "$path" ]] || fail "required document is missing: $path"
done

for path in \
    docs/patches/README.md \
    docs/incidents/README.md \
    scripts/README.md \
    tools/README.md \
    tests/README.md \
    tests/package/run-boundary-tests.sh \
    scripts/check-release-package.sh \
    debs/README.md; do
    [[ -f "$path" ]] || fail "required index is missing: $path"
done

if (( failures > 0 )); then
    printf 'RESULT: FAIL_DOCS (%d issue(s))\n' "$failures" >&2
    exit 1
fi

echo "RESULT: PASS_DOCS"
