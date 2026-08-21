#!/bin/bash
# Unit tests: binary-manifest schema/path/kind/duplicate validation.
# Uses tools/validate-binary-manifest.py against real + adversarial fixtures.
# Read-only; no root, no device, no reboot.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
VALIDATOR="$(dirname "${BASH_SOURCE[0]}")/../../tools/validate-binary-manifest.py"
tests=0; failures=0; skipped=0

pass() { tests=$((tests+1)); printf '%s=PASS\n' "$1"; }
fail() { tests=$((tests+1)); failures=$((failures+1)); printf '%s=FAIL reason=%s\n' "$1" "$2"; }

expect_reject() { # <name> <fixture> <reason>
    local name="$1" f="$2" reason="$3"
    if python3 "$VALIDATOR" "$f" >/dev/null 2>&1; then
        fail "$name" "fixture should be rejected: $reason"
    else
        pass "$name"
    fi
}

if python3 "$VALIDATOR" binary-manifest.json >/dev/null 2>&1; then
    pass "manifest_real_valid"
else
    fail "manifest_real_valid" "real binary-manifest.json should validate"
fi

expect_reject "manifest_reject_abs_path"    tests/fixtures/manifest-abs-path.json    "absolute vendor_path"
expect_reject "manifest_reject_traversal"   tests/fixtures/manifest-traversal.json   "path traversal .."
expect_reject "manifest_reject_bad_kind"    tests/fixtures/manifest-bad-kind.json    "unknown kind"
expect_reject "manifest_reject_dup_target"  tests/fixtures/manifest-dup-target.json  "duplicate vendor_path"
expect_reject "manifest_reject_missing_sha" tests/fixtures/manifest-missing-sha.json "file entry without sha256"
expect_reject "manifest_reject_link_escape" tests/fixtures/manifest-link-escape.json "symlink link_target escape"

if python3 "$VALIDATOR" /nonexistent-manifest.json >/dev/null 2>&1; then
    fail "manifest_missing_file" "missing manifest file should fail"
else
    pass "manifest_missing_file"
fi

printf 'tests_total=%d tests_passed=%d tests_failed=%d tests_skipped=%d\n' "$tests" "$((tests-failures))" "$failures" "$skipped"
[ "$failures" -eq 0 ]
