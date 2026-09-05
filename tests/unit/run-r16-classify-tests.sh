#!/bin/bash
# Unit tests for tools/r16-classify.py
# Patterned after tests/unit/run-r16-build-bc-map-tests.sh:
#   isolated tmpfs, never touches build/ or repo state.
#   Each test sets R16_MANIFEST / R16_D_SRC / R16_F_SRC / R16_D_LICENSE /
#   R16_F_LICENSE / R16_OUT_DIR to TMPROOT paths and uses synthetic
#   per-file content to exercise each branch.
#
# Exit: 0=all pass, 1=any fail, 2=setup error
#
# Tests:
#   t01 — positive: 3 differs (1 PURE_RENAME, 1 BEHAVIORAL, 1 MISSING-source → abort)
#   t02 — negative: malformed manifest row → rc=1, no output
#   t03 — negative: unknown manifest status → rc=1, no output
#   t04 — negative: empty differs/F-only set → rc=1, no output
#   t05 — determinism: two runs → byte-equal per-file output
#   t06 — negative: differs row whose F content is missing → rc=1, abort
#   t07 — negative: 434 differs + 3 F-only (under production cardinality) → rc=1, no output
#   t08 — negative: raw duplicate canonical path → rc=1, no output
#   t09 — fail-closed output discipline: pre-existing per-file + per-bc output
#         hashes preserved when tool aborts
#   t10 — positive: per-file-classification.tsv exactly 8 fields, per-bc-summary.tsv
#         exactly 9 fields (header + every data row); exact header literal lock

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TOOL="$REPO_ROOT/tools/r16-classify.py"

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

assert_stdout_contains() {
    local stdout_file="$1" pattern="$2" label="$3"
    if ! grep -q "$pattern" "$stdout_file"; then
        echo "FAIL: $label: stdout missing pattern '$pattern'"
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

# Build a 435-row manifest + matching D/F source/license trees. 432 differs
# (PURE_RENAME or BEHAVIORAL, alternating) + 3 F-only (matching production
# BC-22a/BC-22b) = 435 paths. The 3 F-only use exact production canon paths.
make_classify_fixture() {
    local dir="$1" d_src="$2" f_src="$3" manifest="$4" d_lic="$5" f_lic="$6"
    mkdir -p "$d_src/srvkm/include" "$d_src/gpu" \
             "$f_src/srvkm/include" "$f_src/gpu"
    : > "$manifest"
    : > "$d_lic"
    : > "$f_lic"
    echo -e "canon_path\td_rel\tf_rel\td_sha256\tf_sha256\tstatus" >> "$manifest"
    local i
    for i in $(seq 1 432); do
        local cls
        if [ $((i % 2)) -eq 0 ]; then
            cls="BEHAVIORAL"
        else
            cls="PURE_RENAME"
        fi
        printf "void d_%d(void) { return %d; }\n" "$i" "$i" \
            > "$d_src/srvkm/include/d_${i}.h"
        if [ "$cls" = "PURE_RENAME" ]; then
            printf "void f_%d(void) { return %d; }\n" "$i" "$i" \
                > "$f_src/srvkm/include/f_${i}.h"
        else
            printf "void f_%d(void) { return %d; }\n" "$i" "$((i+1000))" \
                > "$f_src/srvkm/include/f_${i}.h"
        fi
        printf "srvkm/include/d_%d.h\tsrvkm/include/d_%d.h\tsrvkm/include/f_%d.h\tsha%d\tsha%d\tdiffers\n" \
            "$i" "$i" "$i" "$i" "$i" >> "$manifest"
        printf "mit-or-gpl-2.0-only\tsrvkm/include/d_%d.h\n" "$i" >> "$d_lic"
        printf "mit-or-gpl-2.0-only\tsrvkm/include/f_%d.h\n" "$i" >> "$f_lic"
    done
    # 3 explicit F-only paths matching production (BC-22a/BC-22b)
    for f in "gpu/fant_stackprotector.c" "gpu/fant_stackprotector.h" \
             "srvkm/include/common_ri_bridge.h"; do
        base=$(basename "$f")
        printf "void %s(void) {}\n" "$base" > "$f_src/$f"
        printf "%s\t\t%s\t\tsha_%s\tF-only\n" "$f" "$f" "$base" >> "$manifest"
        printf "mit-or-gpl-2.0-only\t%s\n" "$f" >> "$f_lic"
    done
}

# --- t01 positive: 432 differs + 3 F-only = 435 (production cardinality) ---
test_classify_t01_positive() {
    echo "=== classify_t01_positive ==="
    local dir="$TMPROOT/t01"
    mkdir -p "$dir"
    local d_src="$dir/D"
    local f_src="$dir/F"
    local manifest="$dir/p2-manifest.tsv"
    local d_lic="$dir/d.lic.txt"
    local f_lic="$dir/f.lic.txt"
    local out_dir="$dir/out"
    local err="$dir/err.txt"
    make_classify_fixture "$dir" "$d_src" "$f_src" "$manifest" "$d_lic" "$f_lic"

    local rc=0
    R16_MANIFEST="$manifest" R16_D_SRC="$d_src" R16_F_SRC="$f_src" \
        R16_D_LICENSE="$d_lic" R16_F_LICENSE="$f_lic" R16_OUT_DIR="$out_dir" \
        python3 "$TOOL" 2>"$err" || rc=$?

    assert_rc 0 "$rc" "t01_positive" || return 1
    if [ ! -f "$out_dir/per-file-classification.tsv" ]; then
        echo "FAIL: t01_positive: per-file output not written"
        return 1
    fi
    if [ ! -f "$out_dir/per-bc-summary.tsv" ]; then
        echo "FAIL: t01_positive: per-bc summary not written"
        return 1
    fi
    local rows
    rows=$(($(wc -l < "$out_dir/per-file-classification.tsv") - 1))
    if [ "$rows" -ne 435 ]; then
        echo "FAIL: t01_positive: per-file row count = $rows, expected 435"
        return 1
    fi
    echo "PASS: classify_t01_positive (rc=0, per-file + per-bc written, 435 rows)"
    return 0
}

# --- t02 negative: malformed manifest row ---
test_classify_t02_malformed_manifest() {
    echo "=== classify_t02_malformed_manifest ==="
    local dir="$TMPROOT/t02"
    mkdir -p "$dir"
    local d_src="$dir/D" f_src="$dir/F"
    mkdir -p "$d_src" "$f_src"
    local manifest="$dir/p2-manifest.tsv"
    local d_lic="$dir/d.lic.txt" f_lic="$dir/f.lic.txt"
    local out_dir="$dir/out"
    local err="$dir/err.txt"
    cat > "$manifest" <<EOF
canon_path	d_rel	f_rel	d_sha256	f_sha256	status
srvkm/include/foo.h	srvkm/include/innogpu.h	srvkm/include/fantgpu.h	sha1	sha1
EOF
    : > "$d_lic"; : > "$f_lic"

    local rc=0
    R16_MANIFEST="$manifest" R16_D_SRC="$d_src" R16_F_SRC="$f_src" \
        R16_D_LICENSE="$d_lic" R16_F_LICENSE="$f_lic" R16_OUT_DIR="$out_dir" \
        python3 "$TOOL" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t02_malformed_manifest" || return 1
    assert_no_output_file "$out_dir/per-file-classification.tsv" "t02_malformed_manifest" || return 1
    assert_stderr_contains "$err" "manifest row has" "t02_malformed_manifest" || return 1
    echo "PASS: classify_t02_malformed_manifest (rc=1, no output)"
    return 0
}

# --- t03 negative: unknown manifest status ---
test_classify_t03_unknown_status() {
    echo "=== classify_t03_unknown_status ==="
    local dir="$TMPROOT/t03"
    mkdir -p "$dir"
    local d_src="$dir/D" f_src="$dir/F"
    mkdir -p "$d_src" "$f_src"
    local manifest="$dir/p2-manifest.tsv"
    local d_lic="$dir/d.lic.txt" f_lic="$dir/f.lic.txt"
    local out_dir="$dir/out"
    local err="$dir/err.txt"
    cat > "$manifest" <<EOF
canon_path	d_rel	f_rel	d_sha256	f_sha256	status
srvkm/include/foo.h	srvkm/include/innogpu.h	srvkm/include/fantgpu.h	sha1	sha1	differs
srvkm/include/bar.h	srvkm/include/innopci.h	srvkm/include/fantpci.h	sha2	sha2	bogus-status
EOF
    : > "$d_lic"; : > "$f_lic"

    local rc=0
    R16_MANIFEST="$manifest" R16_D_SRC="$d_src" R16_F_SRC="$f_src" \
        R16_D_LICENSE="$d_lic" R16_F_LICENSE="$f_lic" R16_OUT_DIR="$out_dir" \
        python3 "$TOOL" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t03_unknown_status" || return 1
    assert_no_output_file "$out_dir/per-file-classification.tsv" "t03_unknown_status" || return 1
    assert_stderr_contains "$err" "unknown statuses" "t03_unknown_status" || return 1
    echo "PASS: classify_t03_unknown_status (rc=1, no output)"
    return 0
}

# --- t04 negative: empty differs/F-only ---
test_classify_t04_empty_differs() {
    echo "=== classify_t04_empty_differs ==="
    local dir="$TMPROOT/t04"
    mkdir -p "$dir"
    local d_src="$dir/D" f_src="$dir/F"
    mkdir -p "$d_src" "$f_src"
    local manifest="$dir/p2-manifest.tsv"
    local d_lic="$dir/d.lic.txt" f_lic="$dir/f.lic.txt"
    local out_dir="$dir/out"
    local err="$dir/err.txt"
    cat > "$manifest" <<EOF
canon_path	d_rel	f_rel	d_sha256	f_sha256	status
srvkm/include/foo.h	srvkm/include/innogpu.h	srvkm/include/fantgpu.h	sha1	sha1	identical
EOF
    : > "$d_lic"; : > "$f_lic"

    local rc=0
    R16_MANIFEST="$manifest" R16_D_SRC="$d_src" R16_F_SRC="$f_src" \
        R16_D_LICENSE="$d_lic" R16_F_LICENSE="$f_lic" R16_OUT_DIR="$out_dir" \
        python3 "$TOOL" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t04_empty_differs" || return 1
    assert_no_output_file "$out_dir/per-file-classification.tsv" "t04_empty_differs" || return 1
    assert_stderr_contains "$err" "no differs" "t04_empty_differs" || return 1
    echo "PASS: classify_t04_empty_differs (rc=1, no output)"
    return 0
}

# --- t05 determinism: two runs → byte-equal output ---
test_classify_t05_determinism() {
    echo "=== classify_t05_determinism ==="
    local dir="$TMPROOT/t05"
    mkdir -p "$dir"
    local d_src="$dir/D"
    local f_src="$dir/F"
    local manifest="$dir/p2-manifest.tsv"
    local d_lic="$dir/d.lic.txt"
    local f_lic="$dir/f.lic.txt"
    make_classify_fixture "$dir" "$d_src" "$f_src" "$manifest" "$d_lic" "$f_lic"

    local out1="$dir/out1" out2="$dir/out2" err1="$dir/err1.txt" err2="$dir/err2.txt"
    local rc1=0 rc2=0
    R16_MANIFEST="$manifest" R16_D_SRC="$d_src" R16_F_SRC="$f_src" \
        R16_D_LICENSE="$d_lic" R16_F_LICENSE="$f_lic" R16_OUT_DIR="$out1" \
        python3 "$TOOL" 2>"$err1" || rc1=$?
    R16_MANIFEST="$manifest" R16_D_SRC="$d_src" R16_F_SRC="$f_src" \
        R16_D_LICENSE="$d_lic" R16_F_LICENSE="$f_lic" R16_OUT_DIR="$out2" \
        python3 "$TOOL" 2>"$err2" || rc2=$?

    assert_rc 0 "$rc1" "t05_determinism_run1" || return 1
    assert_rc 0 "$rc2" "t05_determinism_run2" || return 1
    if ! cmp -s "$out1/per-file-classification.tsv" "$out2/per-file-classification.tsv"; then
        echo "FAIL: t05_determinism: two runs produced different per-file output"
        return 1
    fi
    echo "PASS: classify_t05_determinism (two runs byte-equal)"
    return 0
}

# --- t06 negative: differs row whose F content is missing ---
test_classify_t06_missing_f_content() {
    echo "=== classify_t06_missing_f_content ==="
    local dir="$TMPROOT/t06"
    mkdir -p "$dir"
    local d_src="$dir/D"
    local f_src="$dir/F"
    mkdir -p "$d_src/srvkm/include" "$f_src/srvkm/include"
    local manifest="$dir/p2-manifest.tsv"
    local d_lic="$dir/d.lic.txt"
    local f_lic="$dir/f.lic.txt"
    # D has the file, F does NOT (file missing on disk)
    echo "void foo(void) {}" > "$d_src/srvkm/include/innogpu.h"
    # f_src/srvkm/include/fantgpu.h does not exist
    cat > "$manifest" <<EOF
canon_path	d_rel	f_rel	d_sha256	f_sha256	status
srvkm/include/foo.h	srvkm/include/innogpu.h	srvkm/include/fantgpu.h	sha1	sha1	differs
EOF
    : > "$d_lic"; : > "$f_lic"
    local out_dir="$dir/out" err="$dir/err.txt"

    local rc=0
    R16_MANIFEST="$manifest" R16_D_SRC="$d_src" R16_F_SRC="$f_src" \
        R16_D_LICENSE="$d_lic" R16_F_LICENSE="$f_lic" R16_OUT_DIR="$out_dir" \
        python3 "$TOOL" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t06_missing_f_content" || return 1
    assert_no_output_file "$out_dir/per-file-classification.tsv" "t06_missing_f_content" || return 1
    assert_stderr_contains "$err" "cannot read content" "t06_missing_f_content" || return 1
    echo "PASS: classify_t06_missing_f_content (rc=1, no output)"
    return 0
}

# --- t07 negative: under production cardinality (434 differs) ---
test_classify_t07_under_production() {
    echo "=== classify_t07_under_production ==="
    local dir="$TMPROOT/t07"
    mkdir -p "$dir"
    local d_src="$dir/D" f_src="$dir/F"
    local manifest="$dir/p2-manifest.tsv"
    local d_lic="$dir/d.lic.txt" f_lic="$dir/f.lic.txt"
    # 434 differs rows: 1 short of 435. Each row must have real content
    # so the MISSING check doesn't trip first.
    {
        echo -e "canon_path\td_rel\tf_rel\td_sha256\tf_sha256\tstatus"
        for i in $(seq 1 434); do
            mkdir -p "$d_src/dir_$i" "$f_src/dir_$i"
            printf "void inno_%d(void) { return %d; }\n" "$i" "$i" > "$d_src/dir_$i/inno.c"
            printf "void fant_%d(void) { return %d; }\n" "$i" "$i" > "$f_src/dir_$i/fant.c"
            printf "dir_%d/file.c\tdir_%d/inno.c\tdir_%d/fant.c\tsha%d\tsha%d\tdiffers\n" "$i" "$i" "$i" "$i" "$i"
        done
    } > "$manifest"
    : > "$d_lic"; : > "$f_lic"
    local out_dir="$dir/out" err="$dir/err.txt"

    local rc=0
    R16_MANIFEST="$manifest" R16_D_SRC="$d_src" R16_F_SRC="$f_src" \
        R16_D_LICENSE="$d_lic" R16_F_LICENSE="$f_lic" R16_OUT_DIR="$out_dir" \
        python3 "$TOOL" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t07_under_production" || return 1
    assert_no_output_file "$out_dir/per-file-classification.tsv" "t07_under_production" || return 1
    assert_stderr_contains "$err" "differs=434" "t07_under_production" || return 1
    echo "PASS: classify_t07_under_production (rc=1, no output, differs!=432 rejected)"
    return 0
}

# --- t08 negative: duplicate canonical path in manifest ---
test_classify_t08_duplicate_path() {
    echo "=== classify_t08_duplicate_path ==="
    local dir="$TMPROOT/t08"
    mkdir -p "$dir"
    local d_src="$dir/D" f_src="$dir/F"
    local manifest="$dir/p2-manifest.tsv"
    local d_lic="$dir/d.lic.txt" f_lic="$dir/f.lic.txt"
    # 434 unique + 1 duplicate = 435 raw rows but only 434 distinct paths
    {
        echo -e "canon_path\td_rel\tf_rel\td_sha256\tf_sha256\tstatus"
        for i in $(seq 1 433); do
            mkdir -p "$d_src/dir_$i" "$f_src/dir_$i"
            printf "void inno_%d(void) { return %d; }\n" "$i" "$i" > "$d_src/dir_$i/inno.c"
            printf "void fant_%d(void) { return %d; }\n" "$i" "$i" > "$f_src/dir_$i/fant.c"
            printf "dir_%d/file.c\tdir_%d/inno.c\tdir_%d/fant.c\tsha%d\tsha%d\tdiffers\n" "$i" "$i" "$i" "$i" "$i"
        done
        mkdir -p "$d_src/dir_434" "$f_src/dir_434"
        printf "void inno_434(void) { return 434; }\n" > "$d_src/dir_434/inno.c"
        printf "void fant_434(void) { return 434; }\n" > "$f_src/dir_434/fant.c"
        printf "dir_434/file.c\tdir_434/inno.c\tdir_434/fant.c\tsha434\tsha434\tdiffers\n"
        printf "dir_434/file.c\tdir_434/inno.c\tdir_434/fant.c\tsha434b\tsha434b\tdiffers\n"
    } > "$manifest"
    : > "$d_lic"; : > "$f_lic"
    local out_dir="$dir/out" err="$dir/err.txt"

    local rc=0
    R16_MANIFEST="$manifest" R16_D_SRC="$d_src" R16_F_SRC="$f_src" \
        R16_D_LICENSE="$d_lic" R16_F_LICENSE="$f_lic" R16_OUT_DIR="$out_dir" \
        python3 "$TOOL" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t08_duplicate_path" || return 1
    assert_no_output_file "$out_dir/per-file-classification.tsv" "t08_duplicate_path" || return 1
    assert_stderr_contains "$err" "duplicate canonical paths" "t08_duplicate_path" || return 1
    echo "PASS: classify_t08_duplicate_path (rc=1, no output)"
    return 0
}

# --- t09 negative: fail-closed output discipline ---
# Pre-create sentinel per-file-classification.tsv AND per-bc-summary.tsv with
# known hashes; run the tool with a failing input (malformed manifest row);
# the tool must NOT modify either pre-existing file. This proves the
# "abort without overwriting output" discipline holds even when both output
# files already exist at the destination paths, not just when they are
# absent.
test_classify_t09_output_preservation() {
    echo "=== classify_t09_output_preservation ==="
    local dir="$TMPROOT/t09"
    mkdir -p "$dir"
    local d_src="$dir/D" f_src="$dir/F"
    local manifest="$dir/p2-manifest.tsv"
    local d_lic="$dir/d.lic.txt" f_lic="$dir/f.lic.txt"
    local out_dir="$dir/out"
    mkdir -p "$out_dir"
    local err="$dir/err.txt"

    # Pre-create BOTH output files at the destination with sentinel content.
    printf 'sentinel\tper-file\tmust\tbe\tpreserved\nold_run_id=%s\n' \
        "$$-$(date +%s)" > "$out_dir/per-file-classification.tsv"
    printf 'sentinel\tper-bc\tmust\tbe\tpreserved\nold_run_id=%s\n' \
        "$$-$(date +%s)" > "$out_dir/per-bc-summary.tsv"
    local pre_pf_hash pre_bc_hash
    pre_pf_hash="$(sha256sum "$out_dir/per-file-classification.tsv" | awk '{print $1}')"
    pre_bc_hash="$(sha256sum "$out_dir/per-bc-summary.tsv" | awk '{print $1}')"

    # Construct a failing manifest (row with < 6 columns → tool must abort).
    : > "$d_lic"; : > "$f_lic"
    {
        echo -e "canon_path\td_rel\tf_rel\td_sha256\tf_sha256\tstatus"
        echo -e "srvkm/include/test_001.c\tsrvkm/include/test_001.c\tsrvkm/include/test_001.c\tsha1\tsha1\tdiffers"
        echo -e "srvkm/include/test_002.c\tsrvkm/include/test_002.c\tsrvkm/include/test_002.c\tsha2\tsha2"
    } > "$manifest"

    local rc=0
    R16_MANIFEST="$manifest" R16_D_SRC="$d_src" R16_F_SRC="$f_src" \
        R16_D_LICENSE="$d_lic" R16_F_LICENSE="$f_lic" R16_OUT_DIR="$out_dir" \
        python3 "$TOOL" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t09_output_preservation" || return 1
    assert_stderr_contains "$err" "manifest row has" "t09_output_preservation" || return 1
    assert_file_hash_equals "$out_dir/per-file-classification.tsv" "$pre_pf_hash" "t09_output_preservation" || return 1
    assert_file_hash_equals "$out_dir/per-bc-summary.tsv" "$pre_bc_hash" "t09_output_preservation" || return 1
    echo "PASS: classify_t09_output_preservation (rc=1, both pre-existing output hashes unchanged)"
    return 0
}

# --- t10 positive: per-bc-summary.tsv has exactly 9 tab-separated fields ---
# (header + every data row); per-file-classification.tsv has exactly 8 fields.
# This locks down the schema so any future drift in the docstring or
# implementation that changes column count will fail loudly here rather
# than at downstream consumers.
test_classify_t10_schema_field_count() {
    echo "=== classify_t10_schema_field_count ==="
    local dir="$TMPROOT/t10"
    mkdir -p "$dir"
    local d_src="$dir/D"
    local f_src="$dir/F"
    local manifest="$dir/p2-manifest.tsv"
    local d_lic="$dir/d.lic.txt"
    local f_lic="$dir/f.lic.txt"
    local out_dir="$dir/out"
    local err="$dir/err.txt"
    make_classify_fixture "$dir" "$d_src" "$f_src" "$manifest" "$d_lic" "$f_lic"

    local rc=0
    R16_MANIFEST="$manifest" R16_D_SRC="$d_src" R16_F_SRC="$f_src" \
        R16_D_LICENSE="$d_lic" R16_F_LICENSE="$f_lic" R16_OUT_DIR="$out_dir" \
        python3 "$TOOL" 2>"$err" || rc=$?

    assert_rc 0 "$rc" "t10_schema_field_count" || return 1

    local pf="$out_dir/per-file-classification.tsv"
    local bc="$out_dir/per-bc-summary.tsv"
    if [ ! -f "$pf" ]; then
        echo "FAIL: t10_schema_field_count: per-file output missing"
        return 1
    fi
    if [ ! -f "$bc" ]; then
        echo "FAIL: t10_schema_field_count: per-bc output missing"
        return 1
    fi

    # per-file-classification.tsv: header + every row must have exactly 8 fields
    local bad_pf
    bad_pf="$(awk -F'\t' 'NF != 8 { print NR ":" NF ":" $0 }' "$pf")"
    if [ -n "$bad_pf" ]; then
        echo "FAIL: t10_schema_field_count: per-file rows with NF!=8:"
        echo "$bad_pf"
        return 1
    fi

    # per-bc-summary.tsv: header + every row must have exactly 9 fields
    local bad_bc
    bad_bc="$(awk -F'\t' 'NF != 9 { print NR ":" NF ":" $0 }' "$bc")"
    if [ -n "$bad_bc" ]; then
        echo "FAIL: t10_schema_field_count: per-bc rows with NF!=9:"
        echo "$bad_bc"
        return 1
    fi

    # Header literal check (catches reorder/rename even when NF=9)
    local pf_header bc_header
    pf_header="$(head -n1 "$pf")"
    bc_header="$(head -n1 "$bc")"
    local expected_pf expected_bc
    expected_pf="$(printf 'canon_path\tbc\tdisposition\tclassification\tlicense_d\tlicense_f\tfirst_diff_d\tfirst_diff_f')"
    expected_bc="$(printf 'bc\tdrop\tdefer\tselected\tf_only\tidentical\tpure_rename\tbehavioral\ttotal')"
    if [ "$pf_header" != "$expected_pf" ]; then
        echo "FAIL: t10_schema_field_count: per-file header drift"
        echo "  expected: $expected_pf"
        echo "  actual:   $pf_header"
        return 1
    fi
    if [ "$bc_header" != "$expected_bc" ]; then
        echo "FAIL: t10_schema_field_count: per-bc header drift"
        echo "  expected: $expected_bc"
        echo "  actual:   $bc_header"
        return 1
    fi

    echo "PASS: classify_t10_schema_field_count (rc=0, per-file=8 fields, per-bc=9 fields, headers exact)"
    return 0
}

# --- Run all tests ---
cd "$REPO_ROOT"
for test_fn in test_classify_t01_positive \
               test_classify_t02_malformed_manifest \
               test_classify_t03_unknown_status \
               test_classify_t04_empty_differs \
               test_classify_t05_determinism \
               test_classify_t06_missing_f_content \
               test_classify_t07_under_production \
               test_classify_t08_duplicate_path \
               test_classify_t09_output_preservation \
               test_classify_t10_schema_field_count; do
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