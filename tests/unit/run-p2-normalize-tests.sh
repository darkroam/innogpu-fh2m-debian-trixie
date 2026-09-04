#!/bin/bash
# Unit tests for tools/p2-normalize-v3.py
# Fully isolated: uses mktemp + trap, never touches build/ or repo state.
# Exit: 0=all pass, 1=any fail, 2=usage/setup error

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOL="$REPO_ROOT/tools/p2-normalize-v3.py"

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
skipped=0

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

make_synthetic_tree() {
    local base="$1"
    local d_dir="$base/D"
    local f_dir="$base/F"
    mkdir -p "$d_dir/innogpu" "$d_dir/innovpu"
    mkdir -p "$f_dir/fantgpu" "$f_dir/fantvpu"

    echo "void shared(void) {}" > "$d_dir/innogpu/innogpu_core.c"
    echo "void dpvr(void) {}" > "$d_dir/innogpu/innogpu_pvr_spec.c"
    echo "void drgx(void) {}" > "$d_dir/innogpu/innogpu_rgx_core.c"
    echo "void dimg(void) {}" > "$d_dir/innogpu/innogpu_img_util.c"
    echo "void dvpu(void) {}" > "$d_dir/innovpu/innovpu_decode.c"

    echo "void shared(void) {}" > "$f_dir/fantgpu/fantgpu_core.c"
    echo "void fpvr(void) {}" > "$f_dir/fantgpu/fantgpu_pvr_spec.c"
    echo "void frgx(void) {}" > "$f_dir/fantgpu/fantgpu_rgx_core.c"
    echo "void fimg(void) {}" > "$f_dir/fantgpu/fantgpu_img_util.c"
    echo "void fon(void) {}" > "$f_dir/fantgpu/fantgpu_extra.c"
}

# --- Test 1: Deterministic normal generation with exact assertions ---
test_p2_norm_t01_deterministic() {
    echo "=== p2_norm_t01_deterministic ==="
    local tree="$TMPROOT/t01"
    make_synthetic_tree "$tree"
    local out1="$TMPROOT/t01-out1.tsv"
    local out2="$TMPROOT/t01-out2.tsv"

    P2_NORM_D_SRC="$tree/D" P2_NORM_F_SRC="$tree/F" python3 "$TOOL" "$out1"
    local rc1=$?
    P2_NORM_D_SRC="$tree/D" P2_NORM_F_SRC="$tree/F" python3 "$TOOL" "$out2"
    local rc2=$?

    if [ $rc1 -ne 0 ] || [ $rc2 -ne 0 ]; then
        echo "FAIL: expected rc=0, got rc1=$rc1 rc2=$rc2"
        return 1
    fi

    if ! cmp -s "$out1" "$out2"; then
        echo "FAIL: two runs produced different output"
        return 1
    fi

    local lines
    lines=$(wc -l < "$out1")
    if [ "$lines" -ne 7 ]; then
        echo "FAIL: expected 7 lines (1 header + 4 common + 1 D-only + 1 F-only), got $lines"
        return 1
    fi

    if ! head -1 "$out1" | grep -q '^canon_path	d_rel	f_rel	d_sha256	f_sha256	status$'; then
        echo "FAIL: header mismatch"
        return 1
    fi

    local common_count d_only_count f_only_count
    common_count=$(grep -c '	differs$\|	identical$' "$out1" || true)
    d_only_count=$(grep -c '	D-only$' "$out1" || true)
    f_only_count=$(grep -c '	F-only$' "$out1" || true)

    if [ "$common_count" -ne 4 ]; then
        echo "FAIL: expected 4 common pairs, got $common_count"
        return 1
    fi
    if [ "$d_only_count" -ne 1 ]; then
        echo "FAIL: expected 1 D-only, got $d_only_count"
        return 1
    fi
    if [ "$f_only_count" -ne 1 ]; then
        echo "FAIL: expected 1 F-only, got $f_only_count"
        return 1
    fi

    local expected_paths="gpu/gpu_core.c gpu/gpu_ft_spec.c gpu/gpu_ftx_core.c gpu/gpu_fant_util.c"
    for p in $expected_paths; do
        if ! grep -q "^${p}	" "$out1"; then
            echo "FAIL: expected canonical path '$p' not found"
            return 1
        fi
    done

    if ! grep -q '^vpu/vpu_decode.c	.*D-only$' "$out1"; then
        echo "FAIL: expected D-only vpu/vpu_decode.c not found"
        return 1
    fi

    if ! grep -q '^gpu/gpu_extra.c	.*F-only$' "$out1"; then
        echo "FAIL: expected F-only gpu/gpu_extra.c not found"
        return 1
    fi

    local identical_count differs_count
    identical_count=$(grep -c '	identical$' "$out1" || true)
    differs_count=$(grep -c '	differs$' "$out1" || true)

    if [ "$identical_count" -ne 1 ]; then
        echo "FAIL: expected identical=1, got $identical_count"
        return 1
    fi
    if [ "$differs_count" -ne 3 ]; then
        echo "FAIL: expected differs=3, got $differs_count"
        return 1
    fi

    local status
    status=$(grep '^gpu/gpu_core.c	' "$out1" | cut -f6)
    if [ "$status" != "identical" ]; then
        echo "FAIL: gpu/gpu_core.c expected identical, got '$status'"
        return 1
    fi

    for p in gpu/gpu_ft_spec.c gpu/gpu_ftx_core.c gpu/gpu_fant_util.c; do
        status=$(grep "^${p}	" "$out1" | cut -f6)
        if [ "$status" != "differs" ]; then
            echo "FAIL: $p expected differs, got '$status'"
            return 1
        fi
    done

    echo "PASS: p2_norm_t01_deterministic (rc=0, 7 lines, deterministic, exact paths/statuses)"
    return 0
}

# --- Test 2: Missing D root ---
test_p2_norm_t02_missing_d_root() {
    echo "=== p2_norm_t02_missing_d_root ==="
    local tree="$TMPROOT/t02"
    mkdir -p "$tree/F"
    local out="$TMPROOT/t02-out.tsv"
    local err="$TMPROOT/t02-err.txt"

    local rc=0
    P2_NORM_D_SRC="$tree/nonexistent" P2_NORM_F_SRC="$tree/F" \
        python3 "$TOOL" "$out" 2>"$err" || rc=$?

    assert_rc 3 $rc "missing_d_root" || return 1
    assert_no_output_file "$out" "missing_d_root" || return 1
    assert_stderr_contains "$err" "not found" "missing_d_root" || return 1

    echo "PASS: p2_norm_t02_missing_d_root (rc=3, no output)"
    return 0
}

# --- Test 3: Missing F root ---
test_p2_norm_t03_missing_f_root() {
    echo "=== p2_norm_t03_missing_f_root ==="
    local tree="$TMPROOT/t03"
    mkdir -p "$tree/D"
    local out="$TMPROOT/t03-out.tsv"
    local err="$TMPROOT/t03-err.txt"

    local rc=0
    P2_NORM_D_SRC="$tree/D" P2_NORM_F_SRC="$tree/nonexistent" \
        python3 "$TOOL" "$out" 2>"$err" || rc=$?

    assert_rc 3 $rc "missing_f_root" || return 1
    assert_no_output_file "$out" "missing_f_root" || return 1
    assert_stderr_contains "$err" "not found" "missing_f_root" || return 1

    echo "PASS: p2_norm_t03_missing_f_root (rc=3, no output)"
    return 0
}

# --- Test 4: Empty scan (dirs exist but no .c/.h) ---
test_p2_norm_t04_empty_scan() {
    echo "=== p2_norm_t04_empty_scan ==="
    local tree="$TMPROOT/t04"
    mkdir -p "$tree/D" "$tree/F"
    local out="$TMPROOT/t04-out.tsv"
    local err="$TMPROOT/t04-err.txt"

    local rc=0
    P2_NORM_D_SRC="$tree/D" P2_NORM_F_SRC="$tree/F" \
        python3 "$TOOL" "$out" 2>"$err" || rc=$?

    assert_rc 4 $rc "empty_scan" || return 1
    assert_no_output_file "$out" "empty_scan" || return 1
    assert_stderr_contains "$err" "no .c/.h" "empty_scan" || return 1

    echo "PASS: p2_norm_t04_empty_scan (rc=4, no output)"
    return 0
}

# --- Test 5: Canonical path conflict ---
test_p2_norm_t05_conflict() {
    echo "=== p2_norm_t05_conflict ==="
    local tree="$TMPROOT/t05"
    mkdir -p "$tree/D/innogpu" "$tree/D/fantgpu"
    mkdir -p "$tree/F/fantgpu"

    echo "void a(void) {}" > "$tree/D/innogpu/innogpu_spec.c"
    echo "void b(void) {}" > "$tree/D/fantgpu/fantgpu_spec.c"
    echo "void c(void) {}" > "$tree/F/fantgpu/fantgpu_other.c"

    local out="$TMPROOT/t05-out.tsv"
    local err="$TMPROOT/t05-err.txt"

    local rc=0
    P2_NORM_D_SRC="$tree/D" P2_NORM_F_SRC="$tree/F" \
        python3 "$TOOL" "$out" 2>"$err" || rc=$?

    assert_rc 1 $rc "conflict" || return 1
    assert_no_output_file "$out" "conflict" || return 1
    assert_stderr_contains "$err" "CONFLICT" "conflict" || return 1

    echo "PASS: p2_norm_t05_conflict (rc=1, no output)"
    return 0
}

# --- Test 6: Extra arguments ---
test_p2_norm_t06_extra_args() {
    echo "=== p2_norm_t06_extra_args ==="
    local out="$TMPROOT/t06-out.tsv"
    local err="$TMPROOT/t06-err.txt"

    local rc=0
    python3 "$TOOL" "$out" extra1 extra2 2>"$err" || rc=$?

    assert_rc 2 $rc "extra_args" || return 1
    assert_no_output_file "$out" "extra_args" || return 1
    assert_stderr_contains "$err" "Usage" "extra_args" || return 1

    echo "PASS: p2_norm_t06_extra_args (rc=2, no output)"
    return 0
}

# --- Run all tests ---
cd "$REPO_ROOT"
for test_fn in test_p2_norm_t01_deterministic \
               test_p2_norm_t02_missing_d_root \
               test_p2_norm_t03_missing_f_root \
               test_p2_norm_t04_empty_scan \
               test_p2_norm_t05_conflict \
               test_p2_norm_t06_extra_args; do
    total=$((total + 1))
    if $test_fn; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi
done

echo ""
echo "tests_total=$total tests_passed=$passed tests_failed=$failed tests_skipped=$skipped"

if [ "$failed" -gt 0 ]; then
    exit 1
fi
exit 0
