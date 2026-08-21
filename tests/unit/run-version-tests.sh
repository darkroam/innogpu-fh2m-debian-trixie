#!/bin/bash
# Unit tests: Debian version ordering contract for the release chain.
# Read-only; requires dpkg.

set -euo pipefail

tests=0; failures=0; skipped=0
pass() { tests=$((tests+1)); printf '%s=PASS\n' "$1"; }
fail() { tests=$((tests+1)); failures=$((failures+1)); printf '%s=FAIL reason=%s\n' "$1" "$2"; }

chk() { # <name> <a> <op> <b>
    local name="$1" a="$2" op="$3" b="$4"
    if dpkg --compare-versions "$a" "$op" "$b"; then
        pass "$name"
    else
        fail "$name" "$a $op $b"
    fi
}

chk "version_current_gt_rollback" "4.0.0-i1" gt "3.3.3.42-patched-27"
chk "version_rollback_lt_current" "3.3.3.42-patched-27" lt "4.0.0-i1"
chk "version_i1x_below_patched"   "1.0.0-i1" lt "3.3.3.42-patched-27"
chk "version_i2_gt_i1"            "4.0.0-i2" gt "4.0.0-i1"
chk "version_identity"            "4.0.0-i1" eq "4.0.0-i1"
chk "version_p27_gt_p26"          "3.3.3.42-patched-27" gt "3.3.3.42-patched-26"

printf 'tests_total=%d tests_passed=%d tests_failed=%d tests_skipped=%d\n' "$tests" "$((tests-failures))" "$failures" "$skipped"
[ "$failures" -eq 0 ]
