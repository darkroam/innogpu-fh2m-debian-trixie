#!/bin/bash
# Unit tests for deterministic source/payload license classification.

set -u -o pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
AUDITOR="$ROOT/tools/audit-licenses.py"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/innogpu-license-tests.XXXXXX")"
WORK_REL=".build/license-audit-tests.$$"
WORK="$ROOT/$WORK_REL"
trap 'rm -rf "$TMP" "$WORK"' EXIT
mkdir -p "$WORK"

tests=0
failures=0
pass() { tests=$((tests + 1)); printf 'license_audit_t%02d=PASS # %s\n' "$tests" "$1"; }
fail() { tests=$((tests + 1)); failures=$((failures + 1)); printf 'license_audit_t%02d=FAIL reason=%s\n' "$tests" "$1"; }

expect() {
    local label=$1 rc=$2 output=$3 pattern=$4 expected_rc=${5:-0}
    if [[ "$rc" -eq "$expected_rc" ]] && grep -Fq "$pattern" "$output"; then
        pass "$label"
    else
        fail "$label:rc=$rc expected=$expected_rc missing=$pattern"
    fi
}

make_fixture() {
    local dir=$1 source_text=$2
    mkdir -p "$dir/drivers"
    printf '%s\n' "$source_text" > "$dir/drivers/source.c"
    printf '%s\n' 'Project documentation mentioning Strictly Confidential and Dual MIT/GPLv2.' > "$dir/drivers/README.md"
    cat > "$dir/manifest.json" <<'EOF'
{"entries":[{"source_path":"payload.bin","license":"vendor-binary"}]}
EOF
    cat > "$dir/policy.json" <<'EOF'
{
  "format_version": 1,
  "release_status": "BLOCKED",
  "source_root": "drivers",
  "source_origin": "fixture",
  "project_owned_paths": ["drivers/README.md"],
  "classifications": {"strict_confidential": [], "bsd_lgpl_dual": []},
  "dual_mit_gpl": {
    "declaration": "Dual MIT/GPLv2",
    "required_references": ["GPL-COPYING", "MIT-COPYING"],
    "normalized_spdx": "MIT OR GPL-2.0-only"
  },
  "required_license_texts": [],
  "modified_paths": {},
  "module_license_metadata": {},
  "known_declaration_conflicts": [],
  "manifest": {
    "path": "manifest.json",
    "unresolved_marker": "vendor-binary",
    "resolved_licenses": {}
  },
  "expected_summary": {
    "tracked_paths": 2,
    "project_documents": 1,
    "implementation_paths": 1,
    "dual_mit_gpl": 0,
    "strict_confidential": 0,
    "bsd_lgpl_dual": 0,
    "standard_spdx": 0,
    "unclassified": 1,
    "module_license_files": 0,
    "declaration_conflicts": 0,
    "manifest_entries": 1,
    "manifest_unresolved": 1
  },
  "release_blockers": ["fixture"]
}
EOF
    git -C "$dir" init -q
    git -C "$dir" add drivers
}

O="$TMP/output"

python3 "$AUDITOR" --root "$ROOT" > "$O" 2>&1
rc=$?
expect current_inventory_consistent "$rc" "$O" 'license_audit_overall=PASS'

python3 "$AUDITOR" --root "$ROOT" --require-releasable > "$O" 2>&1
rc=$?
expect blocked_release_gate "$rc" "$O" 'license_release_readiness=FAIL reason=release_gate_blocked' 2

python3 "$AUDITOR" --root "$ROOT" --inventory "$WORK_REL/one.tsv" --write-inventory > "$O" 2>&1
rc1=$?
python3 "$AUDITOR" --root "$ROOT" --inventory "$WORK_REL/two.tsv" --write-inventory >> "$O" 2>&1
rc2=$?
if [[ "$rc1" -eq 0 && "$rc2" -eq 0 ]] && cmp -s "$WORK/one.tsv" "$WORK/two.tsv"; then
    pass deterministic_inventory
else
    fail "deterministic_inventory:rc=$rc1/$rc2"
fi

cp "$ROOT/docs/project/source-license-inventory.tsv" "$WORK/stale.tsv"
printf '\n' >> "$WORK/stale.tsv"
python3 "$AUDITOR" --root "$ROOT" --inventory "$WORK_REL/stale.tsv" > "$O" 2>&1
rc=$?
expect stale_inventory_rejected "$rc" "$O" 'reason=inventory_stale' 1

sed 's#LICENSES/MIT.txt#LICENSES/MISSING.txt#' "$ROOT/license-audit-policy.json" > "$WORK/missing-text-policy.json"
python3 "$AUDITOR" --root "$ROOT" --policy "$WORK_REL/missing-text-policy.json" --write-inventory \
    --inventory "$WORK_REL/missing-text.tsv" > "$O" 2>&1
rc=$?
expect missing_license_text_rejected "$rc" "$O" 'reason=required_license_text_missing:LICENSES/MISSING.txt' 1

F="$TMP/confidential"
make_fixture "$F" '/* Strictly Confidential */'
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv --write-inventory > "$O" 2>&1
rc=$?
expect confidential_set_drift_rejected "$rc" "$O" 'reason=unallowlisted_confidential_file:drivers/source.c' 1

F="$TMP/partial-dual"
make_fixture "$F" '/* Dual MIT/GPLv2; GPL-COPYING */'
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv --write-inventory > "$O" 2>&1
rc=$?
expect partial_dual_header_rejected "$rc" "$O" 'reason=partial_dual_mit_gpl_declaration:drivers/source.c' 1

F="$TMP/missing-manifest-license"
make_fixture "$F" '/* no license declaration */'
sed -i 's/"license":"vendor-binary"/"license":""/' "$F/manifest.json"
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv --write-inventory > "$O" 2>&1
rc=$?
expect missing_manifest_license_rejected "$rc" "$O" 'reason=manifest_license_missing:0' 1

F="$TMP/unsupported-resolved"
make_fixture "$F" '/* no license declaration */'
sed -i 's/"vendor-binary"/"MIT"/' "$F/manifest.json"
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv --write-inventory > "$O" 2>&1
rc=$?
expect unsupported_resolved_license_rejected "$rc" "$O" \
    'reason=manifest_license_without_policy_evidence:payload.bin:MIT' 1

F="$TMP/project-doc-marker"
make_fixture "$F" '/* no license declaration */'
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv --write-inventory > "$O" 2>&1
rc=$?
expect project_doc_markers_excluded "$rc" "$O" 'license_project_documents=1'

F="$TMP/module-metadata-drift"
make_fixture "$F" 'MODULE_LICENSE("GPL");'
python3 "$AUDITOR" --root "$F" --policy policy.json --inventory inventory.tsv --write-inventory > "$O" 2>&1
rc=$?
expect module_license_metadata_drift_rejected "$rc" "$O" 'reason=module_license_metadata_set_changed' 1

printf 'tests_total=%d tests_passed=%d tests_failed=%d tests_skipped=0\n' \
    "$tests" "$((tests - failures))" "$failures"
[[ "$failures" -eq 0 ]]
