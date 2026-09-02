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

# Link scan based on git ls-files so that EVERY tracked Markdown file is
# covered, never a hand-maintained directory list (local-only collab/ is not
# tracked and therefore not scanned here).
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
done < <(git ls-files -z -- '*.md')

# Privacy patterns come from the single shared source
# tools/private-data-patterns.txt (loaded by tools/validate-collab.py too);
# both consumers scan case-insensitively.
privacy_patterns="tools/private-data-patterns.txt"
personal_refs="$(
    {
        # Repository privacy roots: --hidden --no-ignore also covers hidden
        # and ignored local files under these explicitly selected paths.
        rg -n --hidden --no-ignore --ignore-case --pcre2 -f "$privacy_patterns" \
            README.md docs scripts tests config patches tools \
            --glob '!scripts/check-docs.sh' \
            --glob '!tools/validate-collab.py' \
            --glob '!tools/private-data-patterns.txt' 2>/dev/null
        # collab/ is gitignored, so include hidden and ignored Markdown files
        # explicitly instead of relying on ripgrep's default traversal.
        rg -n --hidden --no-ignore --ignore-case --pcre2 -f "$privacy_patterns" collab \
            --glob '*.md' 2>/dev/null
    } | rg -v '不得写死 `/home/ok`' || true
)"
if [[ -n "$personal_refs" ]]; then
    printf '%s\n' "$personal_refs" >&2
    fail "repository contains a home path, machine identity, authentication file, or secret marker"
fi

# collab/ round-structure mechanical checks (multiagent-collab.md §五):
# directory naming, unique ids, request.md/report.md, INDEX registration with
# a valid status, markdown-only round dirs, and exact INDEX <-> directory
# one-to-one correspondence. Implemented in tools/validate-collab.py (exact id
# matching, so R01 never matches R010); persistent fixtures live in
# tests/unit/run-collab-structure-tests.sh. collab/ is a local-only directory
# (.gitignore): when it does not exist (fresh clone / CI) the tool passes.
if ! python3 tools/validate-collab.py; then
    fail "collab/ round structure or INDEX registration is invalid"
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
if ! python3 tools/audit-licenses.py >/dev/null 2>&1; then
    fail "source/payload license inventory or policy is stale"
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

# Current-state guards: changing the installed package or R06/R07 conclusion
# requires updating the summary and patch analysis together.
require_text README.md '当前驱动包为 `4.0.1-i3`'
require_text README.md '当前桌面 `cursor_enable=0`'
require_text docs/patches/025-suspend-resume-display.md 'patch-025 仍为 **UNVERIFIED**'
require_text README.md '(docs/project/licensing.md)'
require_text docs/README.md '[许可证与再分发边界](project/licensing.md)'
require_text docs/README.md '[驱动源码许可证审计](project/source-license-audit.md)'
require_text docs/README.md '[project-tools 允许清单](project/project-tools-allowlist.txt)'
require_text docs/README.md '[driver-source 允许清单](project/driver-source-allowlist.txt)'
require_text README.md 'THIRD_PARTY_NOTICES.md'
require_text docs/project/licensing.md 'license_release_gate=BLOCKED'
require_text README.md 'docs/project/licensing.md'
require_text LICENSE 'license_scope=original-layer-only'
require_text THIRD_PARTY_NOTICES.md 'Copyright (c) 2026 Tim Hant'
require_text THIRD_PARTY_NOTICES.md 'MIT License'
require_text LICENSES/MIT.txt 'Copyright (c) 2026 Tim Hant'

# Current README and status must describe the same documentation snapshot.
readme_date="$(sed -n 's/^> 最后更新：\([0-9-]*\).*/\1/p' README.md | head -1)"
status_date="$(sed -n 's/^最后更新：\([0-9-]*\).*/\1/p' docs/project/status.md | head -1)"
[[ -n "$readme_date" && "$readme_date" == "$status_date" ]] ||
    fail "README.md and docs/project/status.md have different or missing update dates"

# Runtime documentation derives its counts from the committed authority rather
# than maintaining an independent total.
runtime_summary="$(grep '^runtime_total=' baselines/latest-runtime-baseline.txt | tail -1 || true)"
runtime_passed="$(sed -n 's/.*runtime_passed=\([0-9][0-9]*\).*/\1/p' <<<"$runtime_summary")"
runtime_skipped="$(sed -n 's/.*runtime_skipped=\([0-9][0-9]*\).*/\1/p' <<<"$runtime_summary")"
runtime_unverified="$(sed -n 's/.*runtime_unverified=\([0-9][0-9]*\).*/\1/p' <<<"$runtime_summary")"
if [[ -z "$runtime_passed" || -z "$runtime_skipped" || -z "$runtime_unverified" ]]; then
    fail "latest runtime baseline has no parseable summary"
else
    runtime_text="$runtime_passed PASS / $runtime_skipped SKIP / $runtime_unverified UNVERIFIED"
    require_text docs/project/status.md "$runtime_text"
    require_text docs/project/test-strategy.md "$runtime_text"
    require_text docs/planning/current-work.md "$runtime_text"
fi
require_text baselines/latest-runtime-baseline.txt 'runtime_vulkan_execution=PASS'
require_text baselines/latest-runtime-baseline.txt 'runtime_opencl_execution=PASS'
require_text baselines/latest-runtime-baseline.txt 'runtime_vaapi_decode=PASS'
require_text baselines/latest-runtime-baseline.txt 'bash tools/run-dmabuf-regression-test.sh'   # DMA-BUF 回归人工命令指向聚合入口（工具代码已提交后从干净提交重新生成的基线）
require_text baselines/latest-runtime-baseline.txt 'runtime_dmabuf_regression=PASS'   # DMA-BUF 真机 PASS 证据已封存（2026-08-26）

# The source deb identity is owned by the manifest and must be repeated exactly
# in the user-facing acquisition documents.
source_deb_sha="$(python3 - <<'PY'
import json
with open('binary-manifest.json', encoding='utf-8') as stream:
    print(json.load(stream)['source_deb_sha256'])
PY
)"
require_text docs/project/dependencies.md "$source_deb_sha"
require_text docs/user/new-device-install.md "$source_deb_sha"

# Imported source is not under one uniform license. The deterministic auditor
# owns per-path classification and manifest semantics; documentation repeats
# only the release decision and scope boundary.
require_text docs/project/source-license-audit.md '发布状态：BLOCKED'
require_text docs/project/source-license-audit.md 'MIT OR GPL-2.0-only'
require_text docs/project/source-license-audit.md 'BSD-3-Clause OR LGPL-2.1-only'
require_text docs/project/source-license-audit.md '70 个'
require_text docs/project/licensing.md 'license_release_gate=BLOCKED'

stale_current_state="$({
    rg -n '18 PASS.?/? ?9 SKIP.?/? ?8 UNVERIFIED|18/9/8|人工授权项待运行' \
        README.md drivers/README.md docs/project docs/user docs/planning/source-tree-migration.md \
        docs/planning/phase5-retirement-design.md tests/runtime/README.md 2>/dev/null || true
    rg -n 'VA-API H264\+HEVC 硬解已实机|VA-API H264\+HEVC 硬解等|当前运行包.*patched-23' \
        README.md drivers/README.md docs/project docs/user docs/planning/source-tree-migration.md \
        docs/planning/phase5-retirement-design.md tests/runtime/README.md 2>/dev/null || true
    rg -n '当前设备构建仍由旧流程|binary-manifest\.json.*当前文件尚不存在' \
        README.md drivers/README.md docs/project docs/user 2>/dev/null || true
} )"
if [[ -n "$stale_current_state" ]]; then
    printf '%s\n' "$stale_current_state" >&2
    fail "current documentation contains a stale architecture, capability, or runtime assertion"
fi

# Validate the column count of ordinary Markdown pipe tables. Pipes inside
# backtick code spans and escaped pipes do not split cells.
if ! python3 - <<'PY'
from pathlib import Path
import re
import sys

roots = [Path('README.md'), Path('drivers/README.md'), Path('docs'), Path('scripts'),
         Path('baselines'), Path('tests'), Path('collab')]
files = []
for root in roots:
    if root.is_dir():
        files.extend(root.rglob('*.md'))
    elif root.is_file():
        files.append(root)
    # an optional root that is absent (e.g. local-only collab/) is skipped

def cells(line):
    stripped = line.strip()
    if not stripped.startswith('|'):
        return None
    values, current = [], []
    escaped = False
    ticks = 0
    for char in stripped[1:]:
        if escaped:
            current.append(char)
            escaped = False
        elif char == '\\':
            current.append(char)
            escaped = True
        elif char == '`':
            ticks ^= 1
            current.append(char)
        elif char == '|' and not ticks:
            values.append(''.join(current).strip())
            current = []
        else:
            current.append(char)
    if current or not stripped.endswith('|'):
        values.append(''.join(current).strip())
    return values

def separator(row):
    return row and all(re.fullmatch(r':?-{3,}:?', value) for value in row)

errors = []
for path in sorted(set(files)):
    lines = path.read_text(encoding='utf-8').splitlines()
    index = 0
    while index + 1 < len(lines):
        header, divider = cells(lines[index]), cells(lines[index + 1])
        if header and separator(divider):
            expected = len(header)
            if len(divider) != expected:
                errors.append(f'{path}:{index + 2}: table divider has {len(divider)} cells, expected {expected}')
            row_index = index + 2
            while row_index < len(lines) and cells(lines[row_index]) is not None:
                actual = len(cells(lines[row_index]))
                if actual != expected:
                    errors.append(f'{path}:{row_index + 1}: table row has {actual} cells, expected {expected}')
                row_index += 1
            index = row_index
        else:
            index += 1

if errors:
    print('\n'.join(errors), file=sys.stderr)
    raise SystemExit(1)
PY
then
    fail "Markdown table structure validation failed"
fi

# Stable p21 evidence remains navigationally referenced as historical evidence.
require_text docs/patches/patched-21-release-candidate.md 'RUNTIME_VALIDATION: PASS_ON_CURRENT_DEVICE'
require_text docs/project/status.md "\`3.3.3.42-patched-21\` 已安装、重启"
require_text docs/patches/README.md 'p21 已在当前设备运行验收'
require_text docs/project/dependencies.md '当前设备已完成运行验收的候选'
require_text docs/project/architecture.md '4.0.0-i1` 是已完成基线验收'
require_text docs/project/architecture.md 'R06 的 `4.0.1-i3/i4`'
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

if rg -q 'baselines/latest-(desktop|ddx|current-xorg|post-reboot-hwgl)' docs/project/status.md; then
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
    docs/project/source-license-audit.md \
    docs/project/status.md \
    docs/project/compositor-management.md \
    docs/project/display-management.md \
    docs/project/glossary.md \
    docs/project/maintenance-policy.md \
    docs/project/multiagent-collab.md \
    docs/patches/patched-21-release-candidate.md \
    docs/incidents/patched-20-legacy-helper-payload.md \
    docs/planning/current-work.md \
    docs/planning/todo.md \
    docs/planning/history.md \
    docs/user/new-device-install.md \
    docs/user/picom-install.md \
    docs/user/verification.md \
    THIRD_PARTY_NOTICES.md \
    docs/project/licensing.md \
    docs/project/project-tools-allowlist.txt \
    docs/project/driver-source-allowlist.txt \
    LICENSES/GPL-3.0-or-later.txt \
    docs/user/local-extractor.md \
    docs/user/recovery.md; do
    [[ -f "$path" ]] || fail "required document is missing: $path"
done

if rg -n '^\s*- \[ \]' docs/planning/todo.md >/dev/null; then
    fail "completed-work timeline contains an active task; move it to docs/planning/current-work.md"
fi

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
