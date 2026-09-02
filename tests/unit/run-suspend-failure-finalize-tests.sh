#!/bin/bash

set -u -o pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TOOL="$ROOT/tools/finalize-suspend-resume-failure.py"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/innogpu-failure-finalize-tests.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

tests=0
failures=0
pass() { tests=$((tests + 1)); printf 'suspend_finalize_t%02d=PASS # %s\n' "$tests" "$1"; }
fail() { tests=$((tests + 1)); failures=$((failures + 1)); printf 'suspend_finalize_t%02d=FAIL reason=%s\n' "$tests" "$1"; }

make_state() {
    local root=$1 round=${2:-round-d0}
    mkdir -p "$root/$round"
    printf '%s\n' "$round" > "$root/active-d0"
    for marker in failure-evidence-captured rollback-verified reboot-verified; do
        printf 'round_id=%s\nstatus=PASS\n' "$round" > "$root/$round/$marker"
    done
    printf 'immutable evidence\n' > "$root/$round/journal.txt"
    printf 'unrelated state\n' > "$root/active-other"
}

state="$TMP/success"
make_state "$state"
before=$(sha256sum "$state/round-d0/journal.txt")
if output=$(python3 "$TOOL" --state-root "$state" --active-name active-d0 2>&1) &&
   [[ ! -e "$state/active-d0" ]] && [[ -f "$state/active-other" ]] &&
   [[ -f "$state/round-d0/failure-finalized" ]] &&
   [[ "$before" == "$(sha256sum "$state/round-d0/journal.txt")" ]] &&
   grep -Fq 'failure_finalize=PASS round_id=round-d0' <<<"$output" &&
   grep -Fq 'failure_evidence_preserved=PASS' <<<"$output"; then
    pass verified_failure_is_finalized_without_removing_evidence
else
    fail verified_failure_is_finalized_without_removing_evidence
fi

for missing in failure-evidence-captured rollback-verified reboot-verified; do
    state="$TMP/missing-$missing"
    make_state "$state"
    rm "$state/round-d0/$missing"
    if ! python3 "$TOOL" --state-root "$state" --active-name active-d0 >/dev/null 2>&1 &&
       [[ -f "$state/active-d0" ]] && [[ ! -e "$state/round-d0/failure-finalized" ]]; then
        pass "missing_${missing}_blocks_finalize"
    else
        fail "missing_${missing}_blocks_finalize"
    fi
done

state="$TMP/bad-marker"
make_state "$state"
printf 'round_id=other\nstatus=PASS\n' > "$state/round-d0/rollback-verified"
if ! python3 "$TOOL" --state-root "$state" --active-name active-d0 >/dev/null 2>&1 &&
   [[ -f "$state/active-d0" ]]; then
    pass mismatched_marker_blocks_finalize
else
    fail mismatched_marker_blocks_finalize
fi

state="$TMP/active-symlink"
make_state "$state"
mv "$state/active-d0" "$state/real-active"
ln -s real-active "$state/active-d0"
if ! python3 "$TOOL" --state-root "$state" --active-name active-d0 >/dev/null 2>&1 &&
   [[ -L "$state/active-d0" ]]; then
    pass active_symlink_is_rejected
else
    fail active_symlink_is_rejected
fi

state="$TMP/evidence-symlink"
mkdir -p "$state/outside"
printf 'round-d0\n' > "$state/active-d0"
ln -s outside "$state/round-d0"
if ! python3 "$TOOL" --state-root "$state" --active-name active-d0 >/dev/null 2>&1 &&
   [[ -f "$state/active-d0" ]]; then
    pass evidence_symlink_is_rejected
else
    fail evidence_symlink_is_rejected
fi

state="$TMP/invalid-active"
make_state "$state"
printf '../outside\n' > "$state/active-d0"
if ! python3 "$TOOL" --state-root "$state" --active-name active-d0 >/dev/null 2>&1 &&
   [[ -f "$state/active-d0" ]]; then
    pass path_traversal_in_active_pointer_is_rejected
else
    fail path_traversal_in_active_pointer_is_rejected
fi

if ! python3 "$TOOL" --state-root relative/state --active-name active-d0 >/dev/null 2>&1; then
    pass relative_state_root_is_rejected
else
    fail relative_state_root_is_rejected
fi

ln -s "$TMP/success" "$TMP/state-root-link"
if ! python3 "$TOOL" --state-root "$TMP/state-root-link" --active-name active-d0 >/dev/null 2>&1; then
    pass symlinked_state_root_is_rejected
else
    fail symlinked_state_root_is_rejected
fi

state="$TMP/existing-final"
make_state "$state"
printf 'preexisting\n' > "$state/round-d0/failure-finalized"
if ! python3 "$TOOL" --state-root "$state" --active-name active-d0 >/dev/null 2>&1 &&
   [[ -f "$state/active-d0" ]] && grep -Fxq preexisting "$state/round-d0/failure-finalized"; then
    pass existing_finalize_marker_is_not_overwritten
else
    fail existing_finalize_marker_is_not_overwritten
fi

if PYTHONPYCACHEPREFIX="$TMP/pycache" python3 -m py_compile "$TOOL" && bash -n "$0"; then
    pass syntax_checks
else
    fail syntax_checks
fi

printf 'tests_total=%d tests_passed=%d tests_failed=%d tests_skipped=0\n' \
    "$tests" "$((tests - failures))" "$failures"
[[ "$failures" -eq 0 ]]
