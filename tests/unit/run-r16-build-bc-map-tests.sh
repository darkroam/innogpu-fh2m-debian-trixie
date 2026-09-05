#!/bin/bash
# Unit tests for tools/r16-build-bc-map.py
# Patterned after tests/unit/run-p2-normalize-tests.sh:
#   isolated tmpfs, never touches build/ or repo state.
#   Each test sets R16_MANIFEST and R16_BC_MAP_OUT to TMPROOT paths.
#
# Exit: 0=all pass, 1=any fail, 2=setup error
#
# Tests:
#   t01 — positive: 435 differs rows → rc=0, BC map written
#   t02 — negative: malformed manifest row (col < 6) → rc=1, no output
#   t03 — negative: unknown manifest status → rc=1, no output
#   t04 — negative: empty differs/F-only set → rc=1, no output
#   t05 — determinism: two runs → byte-equal output
#   t06 — negative: under production cardinality (434 differs) → rc=1, no output
#   t07 — negative: raw duplicate canonical path → rc=1, no output
#   t08 — negative: 435 differs + 0 F-only (wrong mix) → rc=1, no output
#   t09 — negative: 431 differs + 3 F-only (differs miscount) → rc=1, no output
#   t10 — fail-closed output discipline: pre-existing output hash preserved on failure

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOL="$REPO_ROOT/tools/r16-build-bc-map.py"

if [ ! -f "$TOOL" ]; then
    echo "FATAL: tool not found: $TOOL" >&2
    exit 2
fi

TMPROOT=""
cleanup() {
    if [ -n "$TMPROOT" ] && [ -d "$TMPROOT" ]; then
        rm -rf "$TMPROOT"
    fi
}
trap cleanup EXIT INT TERM HUP

TMPROOT="$(mktemp -d)"

total=0
passed=0
failed=0

assert_rc() {
    local expected="$1" actual="$2" label="$3"
    if [ "$actual" -ne "$expected" ]; then
        echo "FAIL: $label: expected rc=$expected, got rc=$actual"
        return 1
    fi
    return 0
}

assert_no_output_file() {
    local path="$1" label="$2"
    if [ -f "$path" ]; then
        echo "FAIL: $label: output file should not exist: $path"
        return 1
    fi
    return 0
}

assert_stderr_contains() {
    local stderr_file="$1" pattern="$2" label="$3"
    if ! grep -q "$pattern" "$stderr_file"; then
        echo "FAIL: $label: stderr missing pattern '$pattern'"
        return 1
    fi
    return 0
}

assert_file_hash_equals() {
    local path="$1" expected="$2" label="$3"
    if [ ! -f "$path" ]; then
        echo "FAIL: $label: file should exist: $path"
        return 1
    fi
    local actual
    actual="$(sha256sum "$path" | awk '{print $1}')"
    if [ "$actual" != "$expected" ]; then
        echo "FAIL: $label: hash mismatch (expected $expected, got $actual)"
        return 1
    fi
    return 0
}

# Build a synthetic p2 manifest with N differs rows + 3 explicit F-only
# (gpu/fant_stackprotector.c, gpu/fant_stackprotector.h, srvkm/include/common_ri_bridge.h).
# When N=432, total is 435; when N != 432, total != 435.
make_manifest() {
    local path="$1" n="$2"
    {
        echo -e "canon_path\td_rel\tf_rel\td_sha256\tf_sha256\tstatus"
        for i in $(seq 1 $n); do
            printf "srvkm/include/test_%03d.c\tsrvkm/include/test_%03d.c\tsrvkm/include/test_%03d.c\tsha%03d\tsha%03d\tdiffers\n" \
                "$i" "$i" "$i" "$i" "$i"
        done
        # 3 explicit F-only paths matching production (BC-22a/BC-22b)
        echo -e "gpu/fant_stackprotector.c\t\tgpu/fant_stackprotector.c\t\tsha_sp_c\tF-only"
        echo -e "gpu/fant_stackprotector.h\t\tgpu/fant_stackprotector.h\t\tsha_sp_h\tF-only"
        echo -e "srvkm/include/common_ri_bridge.h\t\tsrvkm/include/common_ri_bridge.h\t\tsha_crb\tF-only"
    } > "$path"
}

# --- t01 positive: 432 differs + 3 F-only = 435 ---
test_bcmap_t01_positive() {
    echo "=== bcmap_t01_positive ==="
    local dir="$TMPROOT/t01"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local out="$dir/canon-to-bc.tsv"
    local err="$dir/err.txt"
    make_manifest "$manifest" 432

    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP_OUT="$out" \
        python3 "$TOOL" 2>"$err" || rc=$?

    assert_rc 0 "$rc" "t01_positive" || return 1
    if [ ! -f "$out" ]; then
        echo "FAIL: t01_positive: output file not written"
        return 1
    fi
    local lines
    lines=$(wc -l < "$out")
    if [ "$lines" -ne 436 ]; then
        echo "FAIL: t01_positive: expected 436 lines (1 header + 435), got $lines"
        return 1
    fi
    echo "PASS: bcmap_t01_positive (rc=0, 432 differs + 3 F-only = 435 BCs assigned)"
    return 0
}

# --- t02 negative: malformed manifest row (col < 6) ---
test_bcmap_t02_malformed_manifest() {
    echo "=== bcmap_t02_malformed_manifest ==="
    local dir="$TMPROOT/t02"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local out="$dir/canon-to-bc.tsv"
    local err="$dir/err.txt"
    {
        echo -e "canon_path\td_rel\tf_rel\td_sha256\tf_sha256\tstatus"
        echo -e "srvkm/include/test_001.c\tsrvkm/include/test_001.c\tsrvkm/include/test_001.c\tsha1\tsha1\tdiffers"
        # Row missing status column (only 5 cols)
        echo -e "srvkm/include/test_002.c\tsrvkm/include/test_002.c\tsrvkm/include/test_002.c\tsha2\tsha2"
    } > "$manifest"

    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP_OUT="$out" \
        python3 "$TOOL" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t02_malformed_manifest" || return 1
    assert_no_output_file "$out" "t02_malformed_manifest" || return 1
    assert_stderr_contains "$err" "manifest row has" "t02_malformed_manifest" || return 1
    echo "PASS: bcmap_t02_malformed_manifest (rc=1, no output)"
    return 0
}

# --- t03 negative: unknown manifest status ---
test_bcmap_t03_unknown_status() {
    echo "=== bcmap_t03_unknown_status ==="
    local dir="$TMPROOT/t03"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local out="$dir/canon-to-bc.tsv"
    local err="$dir/err.txt"
    {
        echo -e "canon_path\td_rel\tf_rel\td_sha256\tf_sha256\tstatus"
        echo -e "srvkm/include/test_001.c\tsrvkm/include/test_001.c\tsrvkm/include/test_001.c\tsha1\tsha1\tdiffers"
        echo -e "srvkm/include/test_002.c\tsrvkm/include/test_002.c\tsrvkm/include/test_002.c\tsha2\tsha2\tbogus-status"
    } > "$manifest"

    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP_OUT="$out" \
        python3 "$TOOL" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t03_unknown_status" || return 1
    assert_no_output_file "$out" "t03_unknown_status" || return 1
    assert_stderr_contains "$err" "unknown statuses" "t03_unknown_status" || return 1
    echo "PASS: bcmap_t03_unknown_status (rc=1, no output)"
    return 0
}

# --- t04 negative: empty differs/F-only set (manifest has only identical) ---
test_bcmap_t04_empty_differs() {
    echo "=== bcmap_t04_empty_differs ==="
    local dir="$TMPROOT/t04"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local out="$dir/canon-to-bc.tsv"
    local err="$dir/err.txt"
    {
        echo -e "canon_path\td_rel\tf_rel\td_sha256\tf_sha256\tstatus"
        echo -e "srvkm/include/test_001.c\tsrvkm/include/test_001.c\tsrvkm/include/test_001.c\tsha1\tsha1\tidentical"
        echo -e "srvkm/include/test_002.c\tsrvkm/include/test_002.c\tsrvkm/include/test_002.c\tsha2\tsha2\tidentical"
    } > "$manifest"

    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP_OUT="$out" \
        python3 "$TOOL" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t04_empty_differs" || return 1
    assert_no_output_file "$out" "t04_empty_differs" || return 1
    assert_stderr_contains "$err" "no differs" "t04_empty_differs" || return 1
    echo "PASS: bcmap_t04_empty_differs (rc=1, no output)"
    return 0
}

# --- t05 determinism: two runs → byte-equal output ---
test_bcmap_t05_determinism() {
    echo "=== bcmap_t05_determinism ==="
    local dir="$TMPROOT/t05"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local out1="$dir/canon-to-bc-1.tsv" out2="$dir/canon-to-bc-2.tsv"
    local err1="$dir/err1.txt" err2="$dir/err2.txt"
    make_manifest "$manifest" 432

    local rc1=0 rc2=0
    R16_MANIFEST="$manifest" R16_BC_MAP_OUT="$out1" \
        python3 "$TOOL" 2>"$err1" || rc1=$?
    R16_MANIFEST="$manifest" R16_BC_MAP_OUT="$out2" \
        python3 "$TOOL" 2>"$err2" || rc2=$?

    assert_rc 0 "$rc1" "t05_determinism_run1" || return 1
    assert_rc 0 "$rc2" "t05_determinism_run2" || return 1
    if ! cmp -s "$out1" "$out2"; then
        echo "FAIL: t05_determinism: two runs produced different BC map"
        return 1
    fi
    echo "PASS: bcmap_t05_determinism (two runs byte-equal)"
    return 0
}

# --- t06 negative: under production cardinality (434 differs + 3 F-only = 437) ---
test_bcmap_t06_under_production() {
    echo "=== bcmap_t06_under_production ==="
    local dir="$TMPROOT/t06"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local out="$dir/canon-to-bc.tsv"
    local err="$dir/err.txt"
    make_manifest "$manifest" 434  # 434 differs + 3 F-only = 437 (over)

    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP_OUT="$out" \
        python3 "$TOOL" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t06_under_production" || return 1
    assert_no_output_file "$out" "t06_under_production" || return 1
    assert_stderr_contains "$err" "differs=434" "t06_under_production" || return 1
    echo "PASS: bcmap_t06_under_production (rc=1, no output, differs!=432 rejected)"
    return 0
}

# --- t07 negative: duplicate canonical path in manifest ---
test_bcmap_t07_duplicate_path() {
    echo "=== bcmap_t07_duplicate_path ==="
    local dir="$TMPROOT/t07"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local out="$dir/canon-to-bc.tsv"
    local err="$dir/err.txt"
    # 434 unique + 1 duplicate = 435 raw rows but only 434 distinct paths
    {
        echo -e "canon_path\td_rel\tf_rel\td_sha256\tf_sha256\tstatus"
        for i in $(seq 1 433); do
            printf "srvkm/include/test_%03d.c\tsrvkm/include/test_%03d.c\tsrvkm/include/test_%03d.c\tsha%d\tsha%d\tdiffers\n" "$i" "$i" "$i" "$i" "$i"
        done
        # Row 434 and 435 are duplicates
        printf "srvkm/include/test_434.c\tsrvkm/include/test_434.c\tsrvkm/include/test_434.c\tsha434\tsha434\tdiffers\n"
        printf "srvkm/include/test_434.c\tsrvkm/include/test_434.c\tsrvkm/include/test_434.c\tsha434b\tsha434b\tdiffers\n"
    } > "$manifest"

    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP_OUT="$out" \
        python3 "$TOOL" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t07_duplicate_path" || return 1
    assert_no_output_file "$out" "t07_duplicate_path" || return 1
    assert_stderr_contains "$err" "duplicate canonical paths" "t07_duplicate_path" || return 1
    echo "PASS: bcmap_t07_duplicate_path (rc=1, no output)"
    return 0
}

# --- t08 negative: 435 differs + 0 F-only (sums to 435 but wrong mix) ---
# codex 11th round finding: old G0 only checked total 435. After tightening,
# G0 must require EXACTLY 432 differs + 3 F-only = 435.
test_bcmap_t08_wrong_mix() {
    echo "=== bcmap_t08_wrong_mix ==="
    local dir="$TMPROOT/t08"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local out="$dir/canon-to-bc.tsv"
    local err="$dir/err.txt"
    {
        echo -e "canon_path\td_rel\tf_rel\td_sha256\tf_sha256\tstatus"
        for i in $(seq 1 435); do
            printf "srvkm/include/test_%03d.c\tsrvkm/include/test_%03d.c\tsrvkm/include/test_%03d.c\tsha%03d\tsha%03d\tdiffers\n" \
                "$i" "$i" "$i" "$i" "$i"
        done
        # No F-only rows; total still 435 but mix is wrong.
    } > "$manifest"

    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP_OUT="$out" \
        python3 "$TOOL" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t08_wrong_mix" || return 1
    assert_no_output_file "$out" "t08_wrong_mix" || return 1
    assert_stderr_contains "$err" "differs=435" "t08_wrong_mix" || return 1
    echo "PASS: bcmap_t08_wrong_mix (rc=1, 435 differs rejected despite 435 total)"
    return 0
}

# --- t09 negative: 431 differs + 3 F-only = 434 (differs != 432) ---
test_bcmap_t09_differs_miscount() {
    echo "=== bcmap_t09_differs_miscount ==="
    local dir="$TMPROOT/t09"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local out="$dir/canon-to-bc.tsv"
    local err="$dir/err.txt"
    {
        echo -e "canon_path\td_rel\tf_rel\td_sha256\tf_sha256\tstatus"
        for i in $(seq 1 431); do
            printf "srvkm/include/test_%03d.c\tsrvkm/include/test_%03d.c\tsrvkm/include/test_%03d.c\tsha%03d\tsha%03d\tdiffers\n" \
                "$i" "$i" "$i" "$i" "$i"
        done
        # 3 known F-only rows; total 434 but differs != 432.
        echo -e "gpu/fant_stackprotector.c\t\tgpu/fant_stackprotector.c\t\tsha_sp_c\tF-only"
        echo -e "gpu/fant_stackprotector.h\t\tgpu/fant_stackprotector.h\t\tsha_sp_h\tF-only"
        echo -e "srvkm/include/common_ri_bridge.h\t\tsrvkm/include/common_ri_bridge.h\t\tsha_crb\tF-only"
    } > "$manifest"

    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP_OUT="$out" \
        python3 "$TOOL" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t09_differs_miscount" || return 1
    assert_no_output_file "$out" "t09_differs_miscount" || return 1
    assert_stderr_contains "$err" "differs=431" "t09_differs_miscount" || return 1
    echo "PASS: bcmap_t09_differs_miscount (rc=1, differs=431 rejected)"
    return 0
}

# --- t10 negative: fail-closed output discipline ---
# Pre-create a sentinel canon-to-bc.tsv with a known hash; run the tool with
# a failing input (malformed manifest row); the tool must NOT modify the
# pre-existing file. This proves "aborts without overwriting output" holds
# even when an output file already exists at the destination path, not just
# when the file is absent.
test_bcmap_t10_output_preservation() {
    echo "=== bcmap_t10_output_preservation ==="
    local dir="$TMPROOT/t10"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local out="$dir/canon-to-bc.tsv"
    local err="$dir/err.txt"

    # Pre-create the output file with sentinel content + record its hash.
    printf 'sentinel\tcontent\tmust\tbe\tpreserved\nold_run_id=%s\n' \
        "$$-$(date +%s)" > "$out"
    local pre_hash
    pre_hash="$(sha256sum "$out" | awk '{print $1}')"

    # Construct a failing manifest (row with < 6 columns → tool must abort).
    {
        echo -e "canon_path\td_rel\tf_rel\td_sha256\tf_sha256\tstatus"
        echo -e "srvkm/include/test_001.c\tsrvkm/include/test_001.c\tsrvkm/include/test_001.c\tsha1\tsha1\tdiffers"
        echo -e "srvkm/include/test_002.c\tsrvkm/include/test_002.c\tsrvkm/include/test_002.c\tsha2\tsha2"
    } > "$manifest"

    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP_OUT="$out" \
        python3 "$TOOL" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t10_output_preservation" || return 1
    assert_stderr_contains "$err" "manifest row has" "t10_output_preservation" || return 1
    assert_file_hash_equals "$out" "$pre_hash" "t10_output_preservation" || return 1
    echo "PASS: bcmap_t10_output_preservation (rc=1, pre-existing output hash unchanged)"
    return 0
}

# --- Run all tests ---
cd "$REPO_ROOT"
for test_fn in test_bcmap_t01_positive \
               test_bcmap_t02_malformed_manifest \
               test_bcmap_t03_unknown_status \
               test_bcmap_t04_empty_differs \
               test_bcmap_t05_determinism \
               test_bcmap_t06_under_production \
               test_bcmap_t07_duplicate_path \
               test_bcmap_t08_wrong_mix \
               test_bcmap_t09_differs_miscount \
               test_bcmap_t10_output_preservation; do
    total=$((total + 1))
    if $test_fn; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi
done

echo ""
echo "tests_total=$total tests_passed=$passed tests_failed=$failed"

if [ "$failed" -gt 0 ]; then
    exit 1
fi
exit 0