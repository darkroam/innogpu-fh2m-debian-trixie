#!/bin/bash
# Check repository documentation links, compatibility entries, and local data.
# shellcheck disable=SC2016 # literal Markdown backticks and shell tokens are required text

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
# tools/internal/private-data-patterns.txt (loaded by tools/internal/validate-collab.py too);
# both consumers scan case-insensitively.
privacy_patterns="tools/internal/private-data-patterns.txt"
personal_refs="$(
    {
        # Repository privacy roots: --hidden --no-ignore also covers hidden
        # and ignored local files under these explicitly selected paths.
        rg -n --hidden --no-ignore --ignore-case --pcre2 -f "$privacy_patterns" \
            README.md docs tests config patches tools \
            --glob '!tools/check-docs.sh' \
            --glob '!tools/internal/validate-collab.py' \
            --glob '!tools/internal/private-data-patterns.txt' 2>/dev/null
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
# one-to-one correspondence. Implemented in tools/internal/validate-collab.py (exact id
# matching, so R01 never matches R010); persistent fixtures live in
# tests/unit/run-collab-structure-tests.sh. collab/ is a local-only directory
# (.gitignore): when it does not exist (fresh clone / CI) the tool passes.
if ! python3 tools/internal/validate-collab.py; then
    fail "collab/ round structure or INDEX registration is invalid"
fi

for removed_copy in \
    tools/xdisplay.sh \
    tools/displayselect \
    tests/xdisplay/run-stage2-tests.sh \
    tests/xdisplay/run-stage2-watch-tests.sh \
    tests/xdisplay/run-stage4-regression-tests.sh; do
    [[ ! -e "$removed_copy" ]] || fail "dotconfig-owned xdisplay copy returned: $removed_copy"
done

# Once R50 task 4 records a successful migration, legacy build/ must not
# reappear. Keep the ignore rule as defense-in-depth; this is the live-state gate.
if rg -l '^R50_TASK4_MIGRATION_PASS ' \
    .build/r50-system-cleanup-20261001-01/task4-migration/apply-*/result.txt \
    >/dev/null 2>&1; then
    [[ ! -e build ]] || fail "retired build/ directory returned after R50 task 4"
fi

stale_xdisplay_refs="$(
    rg -n \
        'tools/xdisplay\.sh|tools/displayselect|tests/xdisplay/run-stage[24](-watch|-regression)?-tests\.sh' \
        README.md docs tools tests baselines \
        --glob '!docs/archive/**' \
        --glob '!docs/history/history.md' \
        --glob '!docs/history/display-integration.md' \
        --glob '!docs/project/display-management.md' \
        --glob '!tools/check-docs.sh' 2>/dev/null || true
)"
if [[ -n "$stale_xdisplay_refs" ]]; then
    printf '%s\n' "$stale_xdisplay_refs" >&2
    fail "current documentation references a removed xdisplay implementation"
fi

# Payload manifest schema must stay valid (read-only; works without vendor/).
if ! python3 -c 'import json,sys; m=json.load(open("binary-manifest-fantgpu.json",encoding="utf-8")); e=m.get("entries"); s=m.get("source_deb_sha256"); locked="6f0daaf79fb6b2a547138c17628bb990dff0d0c684ee1c13775bebc2d28fd11b"; sys.exit(0 if isinstance(e,list) and e and s==locked else 1)'; then
    fail "binary-manifest-fantgpu.json schema validation failed"
fi
if ! python3 tools/internal/audit-licenses.py >/dev/null 2>&1; then
    fail "source/payload license inventory or policy is stale"
fi
# Migration boundary: drivers/ must never carry build artifacts or black-box objects.
if git ls-files drivers | grep -E '\.o_shipped$|\.o\.cmd$' >/dev/null; then
    fail "drivers/ tracks build artifacts or black-box objects (.o_shipped/.o.cmd)"
fi

[[ -r tools/internal/patch-gpupll-object.py ]] || fail "GPU PLL object patch tool is missing or unreadable"

# Current-state guards: changing the installed package or R14 conclusion
# requires updating the summary and patch analysis together.
require_text README.md '当前驱动包和正式交付版本为 `4.0.2-i3`'
require_text README.md '| 最近失败候选 | `4.0.2-i1`：patch-024 + patch-026'
require_text README.md 'patch-028 + patch-029'
require_text docs/patches/025-suspend-resume-display.md 'patch-025 仍为 **UNVERIFIED**'
require_text docs/patches/028-suspend-resume-hal-temp-monitor-delay.md 'R14 又完成 D1-D3 接电无外屏'
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
runtime_summary="$(grep '^runtime_total=' baselines/runtime/latest-runtime-baseline.txt | tail -1 || true)"
runtime_passed="$(sed -n 's/.*runtime_passed=\([0-9][0-9]*\).*/\1/p' <<<"$runtime_summary")"
runtime_skipped="$(sed -n 's/.*runtime_skipped=\([0-9][0-9]*\).*/\1/p' <<<"$runtime_summary")"
runtime_unverified="$(sed -n 's/.*runtime_unverified=\([0-9][0-9]*\).*/\1/p' <<<"$runtime_summary")"
if [[ -z "$runtime_passed" || -z "$runtime_skipped" || -z "$runtime_unverified" ]]; then
    fail "latest runtime baseline has no parseable summary"
else
    runtime_text="$runtime_passed PASS / $runtime_skipped SKIP / $runtime_unverified UNVERIFIED"
    require_text docs/project/status.md "$runtime_text"
    require_text docs/project/test-strategy.md "$runtime_text"
    require_text docs/state/current-work.md "$runtime_text"
fi
require_text baselines/runtime/latest-runtime-baseline.txt 'runtime_vulkan_execution=PASS'
require_text baselines/runtime/latest-runtime-baseline.txt 'runtime_opencl_execution=PASS'
require_text baselines/runtime/latest-runtime-baseline.txt 'runtime_vaapi_decode=PASS'
require_text baselines/runtime/latest-runtime-baseline.txt 'bash tools/run-dmabuf-regression-test.sh'   # DMA-BUF 回归人工命令指向聚合入口（工具代码已提交后从干净提交重新生成的基线）
require_text baselines/runtime/latest-runtime-baseline.txt 'runtime_dmabuf_regression=PASS'   # DMA-BUF 真机 PASS 证据已封存（2026-08-26）

# The source deb identity is owned by the manifest and must be repeated exactly
# in the user-facing acquisition documents.
source_deb_sha="$(python3 - <<'PY'
import json
with open('binary-manifest-fantgpu.json', encoding='utf-8') as stream:
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
        README.md drivers/README.md docs/project docs/user docs/design/source-tree-migration.md \
        docs/design/phase5-retirement-design.md tests/runtime/README.md 2>/dev/null || true
    rg -n 'VA-API H264\+HEVC 硬解已实机|VA-API H264\+HEVC 硬解等|当前运行包.*patched-23' \
        README.md drivers/README.md docs/project docs/user docs/design/source-tree-migration.md \
        docs/design/phase5-retirement-design.md tests/runtime/README.md 2>/dev/null || true
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

roots = [Path('README.md'), Path('drivers/README.md'), Path('docs'), Path('tools'),
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
require_text docs/project/architecture.md 'R06 `4.0.1-i3/i4`'
require_text docs/project/architecture.md '`4.0.2-i3` 与固定 epoch `1788796800`'
require_text docs/project/status.md '`4.0.2-i3` 已安装并重启'
require_text docs/patches/029-suspend-resume-ddcci-panel.md '6/6 deep'
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

if rg -q 'baselines/(desktop-hwgl/)?latest-(desktop|ddx|current-xorg|post-reboot-hwgl)' docs/project/status.md; then
    fail "current status must not cite unversioned historical baseline files as p21 evidence"
fi

while IFS= read -r script; do
    name=${script##*/}
    grep -Fq "\`$name\`" tools/README.md || fail "script is not registered in tools/README.md: $script"
done < <(find tools -maxdepth 1 -type f -name '*.sh' | sort)

while IFS= read -r tool; do
    name=${tool##*/}
    grep -Fq "\`$name\`" tools/internal/README.md || fail "tool is not registered in tools/internal/README.md: $tool"
done < <(find tools/internal -maxdepth 1 -type f ! -name 'README.md' | sort)

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
    docs/state/current-work.md \
    docs/history/todo.md \
    docs/history/history.md \
    docs/history/display-integration.md \
    docs/archive/phase4-device-validation.md \
    docs/archive/release-review-2026-08-20.md \
    docs/planning/phase4-device-validation.md \
    docs/planning/release-review-2026-08-20.md \
    docs/design/source-tree-migration.md \
    docs/design/phase5-retirement-design.md \
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

if rg -n '^\s*- \[ \]' docs/history/todo.md >/dev/null; then
    fail "completed-work timeline contains an active task; move it to docs/state/current-work.md"
fi

for path in \
    docs/patches/README.md \
    docs/incidents/README.md \
    tools/README.md \
    tools/internal/README.md \
    tests/README.md \
    tests/package/run-boundary-tests.sh \
    tools/debian/check-release-package.sh \
    debs/README.md; do
    [[ -f "$path" ]] || fail "required index is missing: $path"
done


# shortcut: checks cited paths and meta_kernel, not that the build script applies those ids. Upgrade when a row's builder list is machine-readable.
if ! matrix_err=$(python3 - << 'PY'
import hashlib, json, re, sys
from pathlib import Path

errors = []
path = Path("docs/patches/system-kernel-matrix.json")
try:
    doc = json.loads(path.read_text(encoding="utf-8"))
except (OSError, json.JSONDecodeError) as exc:
    print("system-kernel matrix: unreadable: %s" % exc)
    sys.exit(1)
if doc.get("schema") != "system-kernel-matrix/2":
    errors.append("schema must be system-kernel-matrix/2")
if doc.get("meta_applicability_field") is not False:
    errors.append("do not pretend meta applicability exists")
for key in ("current", "in_progress", "history"):
    if not isinstance(doc.get(key), list):
        errors.append(key + " must be a list")
raw = path.read_text(encoding="utf-8")
if "patches/m-*" in raw or "patches/g-*" in raw:
    errors.append("wildcard patch list is forbidden")

patch_id = re.compile(r"^(m|g)-[0-9]{3}(-lifecycle)?-[a-z0-9-]+$")
linux_names = (
    "001-hygon-148c-xhci-reset-on-resume",
    "002-kaitian-x7h-battery-notification-delay",
    "003-pinctrl-amd-hygon-gpio",
    "004-snd-hda-hygon-14c9",
)
states = {"applied", "not-in-install-path", "do-not-reuse"}

def sha256(rel):
    return hashlib.sha256(Path(rel).read_bytes()).hexdigest()

def check_meta(item, row_kernel, where):
    ident = item.get("id")
    if not patch_id.fullmatch(str(ident)):
        return
    patch = Path("patches") / (ident + ".patch")
    meta_path = Path("patches") / (ident + ".meta.json")
    if not patch.is_file() or not meta_path.is_file():
        errors.append("%s missing patch/meta: %s" % (where, ident))
        return
    meta = json.loads(meta_path.read_text(encoding="utf-8"))
    if meta.get("id") != ident:
        errors.append("%s meta id != %s" % (where, ident))
    recorded = meta.get("verified", {}).get("clean_build", {}).get("kernel")
    cited = item.get("meta_kernel")
    if cited is not None and cited != recorded:
        errors.append("%s meta_kernel != verified.clean_build.kernel: %s" % (where, ident))
    applicability = meta.get("applicability")
    if applicability is not None and row_kernel not in applicability:
        errors.append("%s kernel not in meta applicability: %s" % (where, ident))

def check_paths(values, where):
    if not isinstance(values, list) or not values:
        errors.append(where + " must be a non-empty path list")
        return
    for rel in values:
        if not isinstance(rel, str) or not Path(rel).exists():
            errors.append("missing path %s" % rel)

def builder_ids(row):
    return tuple(item.get("id") for item in row.get("m_base", []) if item.get("in_i12_builder") is True)

seen = {}
for table in ("current", "in_progress"):
    for index, row in enumerate(doc.get(table) or []):
        key = (row.get("system"), row.get("kernel"))
        if not key[0] or not key[1]:
            errors.append("%s row %d missing system or kernel" % (table, index))
            continue
        if key in seen:
            errors.append("duplicate system+kernel %s %s" % key)
        seen[key] = table

for index, row in enumerate(doc.get("current") or []):
    where = "current[%d]" % index
    if row.get("acceptance") != "accepted":
        errors.append(where + " acceptance must be accepted")
    if not isinstance(row.get("m_base"), list) or not isinstance(row.get("g_set"), list):
        errors.append(where + " m_base and g_set must be lists")
        continue
    for item in row["m_base"]:
        if not isinstance(item, dict):
            errors.append(where + " m_base entry must be an object")
            continue
        check_meta(item, row.get("kernel"), where)
    for ident in row["g_set"]:
        check_meta({"id": ident}, row.get("kernel"), where)
    linux = row.get("linux_patches")
    if not isinstance(linux, dict) or set(linux) != set(linux_names):
        errors.append(where + " linux_patches must name 001-004 exactly")
    else:
        for name, cell in linux.items():
            rel = "components/linux/%s.patch" % name
            if not isinstance(cell, dict) or cell.get("state") not in states:
                errors.append(where + " bad linux state " + name)
            elif not Path(rel).is_file() or cell.get("sha256") != sha256(rel):
                errors.append(where + " linux sha mismatch " + name)
    check_paths(row.get("install"), where + " install")
    check_paths(row.get("verify"), where + " verify")
    check_paths(row.get("rollback"), where + " rollback")
    for command in row.get("commands") or []:
        if not isinstance(command, str) or "/" in command:
            errors.append(where + " command is not an external command")
    doc_path = row.get("install_doc")
    if not isinstance(doc_path, str) or not Path(doc_path).is_file():
        errors.append(where + " install_doc missing")
    identity = row.get("build_identity") or {}
    if not identity.get("tree_hash") or not identity.get("srcversion"):
        errors.append(where + " build identity incomplete")

by_in = {}
for row in doc.get("current") or []:
    revision = (row.get("version_revision") or {}).get("iN")
    by_in.setdefault(revision, set()).add(builder_ids(row))
for revision, chains in by_in.items():
    if len(chains) != 1:
        errors.append("same iN has different in-builder m chain: %s" % revision)

for index, row in enumerate(doc.get("in_progress") or []):
    where = "in_progress[%d]" % index
    if row.get("acceptance") == "accepted":
        errors.append(where + " accepted row belongs in current")
    point = row.get("breakpoint")
    if not row.get("stage") or not row.get("next") or not isinstance(point, dict):
        errors.append(where + " needs stage, breakpoint, next")
        continue
    if not point.get("ko_status") or not isinstance(point.get("patches_applied"), list) or not isinstance(point.get("pending"), list):
        errors.append(where + " breakpoint incomplete")
    for ident in point.get("patches_applied") or []:
        check_meta({"id": ident}, row.get("kernel"), where)

for index, row in enumerate(doc.get("history") or []):
    if not row.get("system") or not row.get("kernel") or not row.get("reason"):
        errors.append("history[%d] needs system, kernel, reason" % index)

if errors:
    print("system-kernel matrix: " + "; ".join(errors))
    sys.exit(1)
PY
); then
    fail "system-kernel matrix gate failed: ${matrix_err}"
fi

if ! naming_err=$(python3 - << 'PY'
import json, re, sys
from pathlib import Path
root = Path("patches")
name_re = re.compile(r"^(m|g)-[0-9]{3}(-lifecycle)?-[a-z0-9-]+$")
errors = []
old = sorted(item.name for item in list(root.glob("030-*.patch")) + list(root.glob("030-*.meta.json")))
if old:
    errors.append("old 030 patch names remain: " + " ".join(old))
stems = {}
for item in list(root.glob("m-*.patch")) + list(root.glob("g-*.patch")) + list(root.glob("m-*.meta.json")) + list(root.glob("g-*.meta.json")):
    stem = item.name[:-6] if item.name.endswith(".patch") else item.name[:-10]
    kind = "patch" if item.name.endswith(".patch") else "meta.json"
    stems.setdefault(stem, set()).add(kind)
    if not name_re.fullmatch(stem):
        errors.append("patch name does not match naming.md: " + item.name)
for stem, kinds in sorted(stems.items()):
    if kinds != {"patch", "meta.json"}:
        errors.append("patch/meta pair incomplete: " + stem)
        continue
    meta = json.loads((root / (stem + ".meta.json")).read_text())
    if meta.get("id") != stem:
        errors.append("meta id != stem: " + stem + " id=" + str(meta.get("id")))
if errors:
    print("; ".join(errors))
    sys.exit(1)
PY
); then
    fail "patch naming gate failed: ${naming_err}"
fi

if (( failures > 0 )); then
    printf 'RESULT: FAIL_DOCS (%d issue(s))\n' "$failures" >&2
    exit 1
fi

echo "RESULT: PASS_DOCS"
