#!/bin/bash
# Unit tests for tools/r16-gate.py
# Patterned after tests/unit/run-p2-normalize-tests.sh:
#   isolated tmpfs, never touches build/ or repo state.
#   Each test builds a synthetic p2-manifest + canon-to-bc + per-file
#   evidence under TMPROOT and runs the gate via env var overrides
#   (R16_MANIFEST / R16_BC_MAP / R16_PER_FILE).
#
# Exit: 0=all pass, 1=any fail, 2=setup error
#
# Tests:
#   t01 — positive: synthetic 435-row evidence → ALL GATES PASS
#   t02 — negative: tampered canon-to-bc.tsv (BC label unknown) → FAIL
#   t03 — negative: tampered per-file-classification.tsv
#         (disposition=unknown) → FAIL
#   t04 — negative: tampered per-file-classification.tsv
#         (license not in whitelist) → FAIL
#   t05 — negative: per-file BC tag differs from canon-to-bc.tsv → FAIL
#   t06 — negative: BEHAVIORAL+drop fail-closed violation → FAIL
#   t07 — negative: missing manifest row column → FAIL
#   t08 — determinism: same evidence twice → byte-equal stdout sha256

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
GATE="$REPO_ROOT/tools/r16-gate.py"

if [ ! -f "$GATE" ]; then
    echo "FATAL: gate not found: $GATE" >&2
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

assert_stdout_contains() {
    local stdout_file="$1" pattern="$2" label="$3"
    if ! grep -q "$pattern" "$stdout_file"; then
        echo "FAIL: $label: stdout missing pattern '$pattern'"
        return 1
    fi
    return 0
}

# Build a synthetic p2 manifest with N differs rows + 3 explicit F-only
# (production invariant: 432 differs + 3 F-only = 435).
make_manifest() {
    local path="$1" n="$2"
    {
        echo -e "canon_path\td_rel\tf_rel\td_sha256\tf_sha256\tstatus"
        for i in $(seq 1 $n); do
            printf "srvkm/include/test_%03d.c\tsrvkm/include/test_%03d.c\tsrvkm/include/test_%03d.c\tsha%03d\tsha%03d\tdiffers\n" \
                "$i" "$i" "$i" "$i" "$i"
        done
        # 3 F-only (BC-22a/BC-22b production paths)
        echo -e "gpu/fant_stackprotector.c\t\tgpu/fant_stackprotector.c\t\tsha_sp_c\tF-only"
        echo -e "gpu/fant_stackprotector.h\t\tgpu/fant_stackprotector.h\t\tsha_sp_h\tF-only"
        echo -e "srvkm/include/common_ri_bridge.h\t\tsrvkm/include/common_ri_bridge.h\t\tsha_crb\tF-only"
    } > "$path"
}

# Build a synthetic canon-to-bc.tsv with N canonical paths all assigned BC-01
# (used by negative tests that tamper with classification/disposition/BC).
make_bc_map_uniform() {
    local path="$1" n="$2"
    {
        echo -e "canon_path\tbc_id"
        for i in $(seq 1 $n); do
            printf "srvkm/include/test_%03d.c\tBC-01\n" "$i"
        done
    } > "$path"
}

# Build a synthetic canon-to-bc.tsv with N canonical paths distributed across
# all 23 BCs: first 22 rows one-per-BC (BC-01..BC-21 + BC-22a + BC-22b),
# remaining N-22 rows in BC-01. Ensures Gate 3 (BC coverage) passes when N ≥ 22.
make_bc_map() {
    local path="$1" n="$2"
    local bcs=(BC-01 BC-02 BC-03 BC-04 BC-05 BC-06 BC-07 BC-08 BC-09 BC-10 \
               BC-11 BC-12 BC-13 BC-14 BC-15 BC-16 BC-17 BC-18 BC-19 BC-20 \
               BC-21 BC-22a BC-22b)
    {
        echo -e "canon_path\tbc_id"
        local i=1
        for bc in "${bcs[@]}"; do
            if [ "$i" -le "$n" ]; then
                printf "srvkm/include/test_%03d.c\t%s\n" "$i" "$bc"
                i=$((i + 1))
            fi
        done
        while [ "$i" -le "$n" ]; do
            printf "srvkm/include/test_%03d.c\tBC-01\n" "$i"
            i=$((i + 1))
        done
    } > "$path"
}

# Build a synthetic per-file-classification.tsv. Args:
#   $1 path, $2 n, $3 disposition, $4 classification, $5 bc_map_path
# Reads BC labels from the bc_map_path (so per-file BC matches canon-to-bc.tsv).
make_per_file() {
    local path="$1" n="$2" disp="$3" cls="$4" bc_map_path="$5"
    local -A bc_for_path=()
    while IFS=$'\t' read -r cp bc; do
        [[ "$cp" == "canon_path" ]] && continue
        bc_for_path[$cp]="$bc"
    done < "$bc_map_path"
    {
        echo -e "canon_path\tbc\tdisposition\tclassification\tlicense_d\tlicense_f\tfirst_diff_d\tfirst_diff_f"
        for i in $(seq 1 $n); do
            local cp="srvkm/include/test_$(printf '%03d' $i).c"
            local bc="${bc_for_path[$cp]:-BC-01}"
            printf "%s\t%s\t%s\t%s\tmit-or-gpl-2.0-only\tmit-or-gpl-2.0-only\t\t\n" \
                "$cp" "$bc" "$disp" "$cls"
        done
    } > "$path"
}

# Build a complete evidence triple (manifest + bc_map + per_file) with the
# production invariant: 432 differs + 3 F-only = 435. Used by positive/determinism
# tests that must pass Gate 0. Includes the 3 F-only paths mapped to BC-22a/22b.
make_full_evidence() {
    local dir="$1" disp="$2" cls="$3"
    local manifest="$dir/p2-manifest.tsv"
    local bc_map="$dir/canon-to-bc.tsv"
    local per_file="$dir/per-file-classification.tsv"

    # Manifest: 432 differs + 3 F-only
    make_manifest "$manifest" 432

    # BC map: 432 differs distributed across 23 BCs + 3 F-only (BC-22a/22a/22b)
    local bcs=(BC-01 BC-02 BC-03 BC-04 BC-05 BC-06 BC-07 BC-08 BC-09 BC-10 \
               BC-11 BC-12 BC-13 BC-14 BC-15 BC-16 BC-17 BC-18 BC-19 BC-20 \
               BC-21 BC-22a BC-22b)
    {
        echo -e "canon_path\tbc_id"
        local i=1
        for bc in "${bcs[@]}"; do
            printf "srvkm/include/test_%03d.c\t%s\n" "$i" "$bc"
            i=$((i + 1))
        done
        while [ "$i" -le 432 ]; do
            printf "srvkm/include/test_%03d.c\tBC-01\n" "$i"
            i=$((i + 1))
        done
        echo -e "gpu/fant_stackprotector.c\tBC-22a"
        echo -e "gpu/fant_stackprotector.h\tBC-22a"
        echo -e "srvkm/include/common_ri_bridge.h\tBC-22b"
    } > "$bc_map"

    # Per-file: matches manifest + bc_map
    {
        echo -e "canon_path\tbc\tdisposition\tclassification\tlicense_d\tlicense_f\tfirst_diff_d\tfirst_diff_f"
        local -A bc_for_path=()
        while IFS=$'\t' read -r cp bc; do
            [[ "$cp" == "canon_path" ]] && continue
            bc_for_path[$cp]="$bc"
        done < "$bc_map"
        for i in $(seq 1 432); do
            local cp="srvkm/include/test_$(printf '%03d' $i).c"
            local bc="${bc_for_path[$cp]:-BC-01}"
            printf "%s\t%s\t%s\t%s\tmit-or-gpl-2.0-only\tmit-or-gpl-2.0-only\t\t\n" \
                "$cp" "$bc" "$disp" "$cls"
        done
        echo -e "gpu/fant_stackprotector.c\tBC-22a\tdrop\tF-ONLY\tF-ONLY\tmit-or-gpl-2.0-only\t\t"
        echo -e "gpu/fant_stackprotector.h\tBC-22a\tdrop\tF-ONLY\tF-ONLY\tmit-or-gpl-2.0-only\t\t"
        echo -e "srvkm/include/common_ri_bridge.h\tBC-22b\tdrop\tF-ONLY\tF-ONLY\tmit-or-gpl-2.0-only\t\t"
    } > "$per_file"
}

# --- t01 positive: 432 differs + 3 F-only = 435, all PURE_RENAME+drop ---
test_r16_gate_t01_positive() {
    echo "=== r16_gate_t01_positive ==="
    local dir="$TMPROOT/t01"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local bc_map="$dir/canon-to-bc.tsv"
    local per_file="$dir/per-file-classification.tsv"
    make_full_evidence "$dir" "drop" "PURE_RENAME"

    local out="$dir/out.txt" err="$dir/err.txt"
    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP="$bc_map" R16_PER_FILE="$per_file" \
        python3 "$GATE" >"$out" 2>"$err" || rc=$?

    assert_rc 0 "$rc" "t01_positive" || return 1
    assert_stdout_contains "$out" "ALL GATES PASS" "t01_positive" || return 1
    echo "PASS: r16_gate_t01_positive (rc=0, ALL GATES PASS)"
    return 0
}

# --- t02 negative: unknown BC label in canon-to-bc.tsv ---
test_r16_gate_t02_unknown_bc() {
    echo "=== r16_gate_t02_unknown_bc ==="
    local dir="$TMPROOT/t02"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local bc_map="$dir/canon-to-bc.tsv"
    local per_file="$dir/per-file-classification.tsv"
    make_manifest "$manifest" 435
    # First 434 rows: BC-01; row 435: unknown BC-ZZ
    {
        echo -e "canon_path\tbc_id"
        for i in $(seq 1 434); do
            printf "srvkm/include/test_%03d.c\tBC-01\n" "$i"
        done
        echo -e "srvkm/include/test_435.c\tBC-ZZ"
    } > "$bc_map"
    make_per_file "$per_file" 435 "drop" "PURE_RENAME" "$bc_map"

    local out="$dir/out.txt" err="$dir/err.txt"
    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP="$bc_map" R16_PER_FILE="$per_file" \
        python3 "$GATE" >"$out" 2>"$err" || rc=$?

    # Either load_bc_map fatal-exits (rc=1, FATAL on stderr) or G3 fails.
    if [ "$rc" -eq 0 ]; then
        echo "FAIL: t02_unknown_bc: expected non-zero, got rc=0"
        return 1
    fi
    if ! grep -qE 'unknown BC label|extra BCs|FATAL' "$err"; then
        echo "FAIL: t02_unknown_bc: stderr missing 'unknown BC label' or 'extra BCs' or 'FATAL'"
        return 1
    fi
    echo "PASS: r16_gate_t02_unknown_bc (rc=$rc, unknown BC rejected)"
    return 0
}

# --- t03 negative: unknown disposition ---
test_r16_gate_t03_unknown_disposition() {
    echo "=== r16_gate_t03_unknown_disposition ==="
    local dir="$TMPROOT/t03"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local bc_map="$dir/canon-to-bc.tsv"
    local per_file="$dir/per-file-classification.tsv"
    make_manifest "$manifest" 435
    make_bc_map "$bc_map" 435
    # First 434 rows: drop; row 435: bogus 'unknown'
    {
        echo -e "canon_path\tbc\tdisposition\tclassification\tlicense_d\tlicense_f\tfirst_diff_d\tfirst_diff_f"
        for i in $(seq 1 434); do
            printf "srvkm/include/test_%03d.c\tBC-01\tdrop\tPURE_RENAME\tmit-or-gpl-2.0-only\tmit-or-gpl-2.0-only\t\t\n" "$i"
        done
        echo -e "srvkm/include/test_435.c\tBC-01\tunknown\tPURE_RENAME\tmit-or-gpl-2.0-only\tmit-or-gpl-2.0-only\t\t"
    } > "$per_file"

    local out="$dir/out.txt" err="$dir/err.txt"
    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP="$bc_map" R16_PER_FILE="$per_file" \
        python3 "$GATE" >"$out" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t03_unknown_disposition" || return 1
    assert_stdout_contains "$out" "invalid dispositions" "t03_unknown_disposition" || return 1
    echo "PASS: r16_gate_t03_unknown_disposition (rc=1, unknown disposition rejected)"
    return 0
}

# --- t04 negative: license not in whitelist ---
test_r16_gate_t04_unknown_license() {
    echo "=== r16_gate_t04_unknown_license ==="
    local dir="$TMPROOT/t04"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local bc_map="$dir/canon-to-bc.tsv"
    local per_file="$dir/per-file-classification.tsv"
    make_manifest "$manifest" 435
    make_bc_map "$bc_map" 435
    {
        echo -e "canon_path\tbc\tdisposition\tclassification\tlicense_d\tlicense_f\tfirst_diff_d\tfirst_diff_f"
        for i in $(seq 1 434); do
            printf "srvkm/include/test_%03d.c\tBC-01\tdrop\tPURE_RENAME\tmit-or-gpl-2.0-only\tmit-or-gpl-2.0-only\t\t\n" "$i"
        done
        echo -e "srvkm/include/test_435.c\tBC-01\tdrop\tPURE_RENAME\tGPL-3.0-bogus\tmit-or-gpl-2.0-only\t\t"
    } > "$per_file"

    local out="$dir/out.txt" err="$dir/err.txt"
    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP="$bc_map" R16_PER_FILE="$per_file" \
        python3 "$GATE" >"$out" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t04_unknown_license" || return 1
    assert_stdout_contains "$out" "invalid license_d" "t04_unknown_license" || return 1
    echo "PASS: r16_gate_t04_unknown_license (rc=1, whitelist violation rejected)"
    return 0
}

# --- t05 negative: per-file BC tag differs from canon-to-bc.tsv ---
test_r16_gate_t05_bc_mismatch() {
    echo "=== r16_gate_t05_bc_mismatch ==="
    local dir="$TMPROOT/t05"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local bc_map="$dir/canon-to-bc.tsv"
    local per_file="$dir/per-file-classification.tsv"
    make_manifest "$manifest" 435
    make_bc_map_uniform "$bc_map" 435
    # First 434 rows: BC-01 in both; row 435: BC-01 in map, BC-02 in per-file
    {
        echo -e "canon_path\tbc\tdisposition\tclassification\tlicense_d\tlicense_f\tfirst_diff_d\tfirst_diff_f"
        for i in $(seq 1 434); do
            printf "srvkm/include/test_%03d.c\tBC-01\tdrop\tPURE_RENAME\tmit-or-gpl-2.0-only\tmit-or-gpl-2.0-only\t\t\n" "$i"
        done
        echo -e "srvkm/include/test_435.c\tBC-02\tdrop\tPURE_RENAME\tmit-or-gpl-2.0-only\tmit-or-gpl-2.0-only\t\t"
    } > "$per_file"

    local out="$dir/out.txt" err="$dir/err.txt"
    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP="$bc_map" R16_PER_FILE="$per_file" \
        python3 "$GATE" >"$out" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t05_bc_mismatch" || return 1
    assert_stdout_contains "$out" "BC tags differ" "t05_bc_mismatch" || return 1
    echo "PASS: r16_gate_t05_bc_mismatch (rc=1, BC consistency violation rejected)"
    return 0
}

# --- t06 negative: BEHAVIORAL+drop fail-closed violation ---
test_r16_gate_t06_fail_closed_violation() {
    echo "=== r16_gate_t06_fail_closed_violation ==="
    local dir="$TMPROOT/t06"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local bc_map="$dir/canon-to-bc.tsv"
    local per_file="$dir/per-file-classification.tsv"
    make_manifest "$manifest" 435
    make_bc_map_uniform "$bc_map" 435
    # Row 435: BEHAVIORAL + drop → fail-closed violation
    {
        echo -e "canon_path\tbc\tdisposition\tclassification\tlicense_d\tlicense_f\tfirst_diff_d\tfirst_diff_f"
        for i in $(seq 1 434); do
            printf "srvkm/include/test_%03d.c\tBC-01\tdrop\tPURE_RENAME\tmit-or-gpl-2.0-only\tmit-or-gpl-2.0-only\t\t\n" "$i"
        done
        echo -e "srvkm/include/test_435.c\tBC-01\tdrop\tBEHAVIORAL\tmit-or-gpl-2.0-only\tmit-or-gpl-2.0-only\tfoo\tbar"
    } > "$per_file"

    local out="$dir/out.txt" err="$dir/err.txt"
    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP="$bc_map" R16_PER_FILE="$per_file" \
        python3 "$GATE" >"$out" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t06_fail_closed" || return 1
    assert_stdout_contains "$out" "fail-closed violations" "t06_fail_closed" || return 1
    echo "PASS: r16_gate_t06_fail_closed_violation (rc=1, BEHAVIORAL+drop rejected)"
    return 0
}

# --- t07 negative: manifest row missing column ---
test_r16_gate_t07_malformed_manifest() {
    echo "=== r16_gate_t07_malformed_manifest ==="
    local dir="$TMPROOT/t07"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local bc_map="$dir/canon-to-bc.tsv"
    local per_file="$dir/per-file-classification.tsv"
    {
        echo -e "canon_path\td_rel\tf_rel\td_sha256\tf_sha256\tstatus"
        echo -e "srvkm/include/test_001.c\tsrvkm/include/test_001.c\tsrvkm/include/test_001.c\tsha1\tsha1\tdiffers"
        # Row missing status column
        echo -e "srvkm/include/test_002.c\tsrvkm/include/test_002.c\tsrvkm/include/test_002.c\tsha2\tsha2"
    } > "$manifest"
    make_bc_map "$bc_map" 2
    make_per_file "$per_file" 2 "drop" "PURE_RENAME" "$bc_map"

    local out="$dir/out.txt" err="$dir/err.txt"
    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP="$bc_map" R16_PER_FILE="$per_file" \
        python3 "$GATE" >"$out" 2>"$err" || rc=$?

    if [ "$rc" -eq 0 ]; then
        echo "FAIL: t07_malformed_manifest: expected non-zero, got rc=0"
        return 1
    fi
    if ! grep -qE 'manifest row has|FATAL' "$err"; then
        echo "FAIL: t07_malformed_manifest: stderr missing 'manifest row has' or 'FATAL'"
        return 1
    fi
    echo "PASS: r16_gate_t07_malformed_manifest (rc=$rc, malformed row rejected)"
    return 0
}

# --- t08 determinism: two runs of the same evidence → byte-equal stdout ---
test_r16_gate_t08_determinism() {
    echo "=== r16_gate_t08_determinism ==="
    local dir="$TMPROOT/t08"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local bc_map="$dir/canon-to-bc.tsv"
    local per_file="$dir/per-file-classification.tsv"
    make_full_evidence "$dir" "drop" "PURE_RENAME"

    local out1="$dir/out1.txt" out2="$dir/out2.txt"
    R16_MANIFEST="$manifest" R16_BC_MAP="$bc_map" R16_PER_FILE="$per_file" \
        python3 "$GATE" >"$out1" 2>/dev/null
    R16_MANIFEST="$manifest" R16_BC_MAP="$bc_map" R16_PER_FILE="$per_file" \
        python3 "$GATE" >"$out2" 2>/dev/null

    if ! cmp -s "$out1" "$out2"; then
        echo "FAIL: t08_determinism: two runs produced different stdout"
        return 1
    fi
    echo "PASS: r16_gate_t08_determinism (two runs byte-equal)"
    return 0
}

# --- t09 negative: MISSING classification must FAIL ---
test_r16_gate_t09_missing_classification() {
    echo "=== r16_gate_t09_missing_classification ==="
    local dir="$TMPROOT/t09"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local bc_map="$dir/canon-to-bc.tsv"
    local per_file="$dir/per-file-classification.tsv"
    make_manifest "$manifest" 435
    make_bc_map_uniform "$bc_map" 435
    # Row 435: MISSING + defer → must fail (MISSING must not appear)
    {
        echo -e "canon_path\tbc\tdisposition\tclassification\tlicense_d\tlicense_f\tfirst_diff_d\tfirst_diff_f"
        for i in $(seq 1 434); do
            printf "srvkm/include/test_%03d.c\tBC-01\tdrop\tPURE_RENAME\tmit-or-gpl-2.0-only\tmit-or-gpl-2.0-only\t\t\n" "$i"
        done
        echo -e "srvkm/include/test_435.c\tBC-01\tdefer\tMISSING\tmit-or-gpl-2.0-only\tmit-or-gpl-2.0-only\tfoo\tbar"
    } > "$per_file"

    local out="$dir/out.txt" err="$dir/err.txt"
    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP="$bc_map" R16_PER_FILE="$per_file" \
        python3 "$GATE" >"$out" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t09_missing_classification" || return 1
    assert_stdout_contains "$out" "invalid classifications" "t09_missing_classification" || return 1
    echo "PASS: r16_gate_t09_missing_classification (rc=1, MISSING rejected)"
    return 0
}

# --- t10 negative: under production cardinality (434 differs) ---
test_r16_gate_t10_under_production() {
    echo "=== r16_gate_t10_under_production ==="
    local dir="$TMPROOT/t10"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local bc_map="$dir/canon-to-bc.tsv"
    local per_file="$dir/per-file-classification.tsv"
    # 434 differs only — 1 short of 435
    make_manifest "$manifest" 434
    {
        echo -e "canon_path\tbc"
        for i in $(seq 1 434); do
            printf "srvkm/include/test_%03d.c\tBC-01\n" "$i"
        done
    } > "$bc_map"
    {
        echo -e "canon_path\tbc\tdisposition\tclassification\tlicense_d\tlicense_f\tfirst_diff_d\tfirst_diff_f"
        for i in $(seq 1 434); do
            printf "srvkm/include/test_%03d.c\tBC-01\tdrop\tPURE_RENAME\tmit-or-gpl-2.0-only\tmit-or-gpl-2.0-only\t\t\n" "$i"
        done
    } > "$per_file"

    local out="$dir/out.txt" err="$dir/err.txt"
    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP="$bc_map" R16_PER_FILE="$per_file" \
        python3 "$GATE" >"$out" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t10_under_production" || return 1
    assert_stdout_contains "$out" "G0" "t10_under_production" || return 1
    echo "PASS: r16_gate_t10_under_production (rc=1, G0 cardinality invariant enforced)"
    return 0
}

# --- t11 negative: duplicate canonical path in manifest ---
test_r16_gate_t11_duplicate_manifest_path() {
    echo "=== r16_gate_t11_duplicate_manifest_path ==="
    local dir="$TMPROOT/t11"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local bc_map="$dir/canon-to-bc.tsv"
    local per_file="$dir/per-file-classification.tsv"
    # 434 unique + 1 duplicate = 435 raw rows
    {
        echo -e "canon_path\td_rel\tf_rel\td_sha256\tf_sha256\tstatus"
        for i in $(seq 1 433); do
            printf "srvkm/include/test_%03d.c\tsrvkm/include/test_%03d.c\tsrvkm/include/test_%03d.c\tsha%d\tsha%d\tdiffers\n" "$i" "$i" "$i" "$i" "$i"
        done
        printf "srvkm/include/test_434.c\tsrvkm/include/test_434.c\tsrvkm/include/test_434.c\tsha434\tsha434\tdiffers\n"
        printf "srvkm/include/test_434.c\tsrvkm/include/test_434.c\tsrvkm/include/test_434.c\tsha434b\tsha434b\tdiffers\n"
    } > "$manifest"
    {
        echo -e "canon_path\tbc"
        for i in $(seq 1 434); do
            printf "srvkm/include/test_%03d.c\tBC-01\n" "$i"
        done
    } > "$bc_map"
    {
        echo -e "canon_path\tbc\tdisposition\tclassification\tlicense_d\tlicense_f\tfirst_diff_d\tfirst_diff_f"
        for i in $(seq 1 434); do
            printf "srvkm/include/test_%03d.c\tBC-01\tdrop\tPURE_RENAME\tmit-or-gpl-2.0-only\tmit-or-gpl-2.0-only\t\t\n" "$i"
        done
    } > "$per_file"

    local out="$dir/out.txt" err="$dir/err.txt"
    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP="$bc_map" R16_PER_FILE="$per_file" \
        python3 "$GATE" >"$out" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t11_duplicate_manifest_path" || return 1
    assert_stdout_contains "$out" "G0" "t11_duplicate_manifest_path" || return 1
    assert_stdout_contains "$out" "duplicate canonical paths" "t11_duplicate_manifest_path" || return 1
    echo "PASS: r16_gate_t11_duplicate_manifest_path (rc=1, G0 duplicate manifest rejected)"
    return 0
}

# --- t12 negative: 435 differs + 0 F-only (sums to 435 but wrong mix) ---
# codex 11th round finding: old G0 only checked total 435. After tightening,
# G0 must require EXACTLY 432 differs + 3 F-only = 435.
test_r16_gate_t12_wrong_mix() {
    echo "=== r16_gate_t12_wrong_mix ==="
    local dir="$TMPROOT/t12"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local bc_map="$dir/canon-to-bc.tsv"
    local per_file="$dir/per-file-classification.tsv"
    # 435 differs, 0 F-only — total still 435 but mix is wrong.
    {
        echo -e "canon_path\td_rel\tf_rel\td_sha256\tf_sha256\tstatus"
        for i in $(seq 1 435); do
            printf "srvkm/include/test_%03d.c\tsrvkm/include/test_%03d.c\tsrvkm/include/test_%03d.c\tsha%03d\tsha%03d\tdiffers\n" "$i" "$i" "$i" "$i" "$i"
        done
    } > "$manifest"
    {
        echo -e "canon_path\tbc_id"
        for i in $(seq 1 435); do
            printf "srvkm/include/test_%03d.c\tBC-01\n" "$i"
        done
    } > "$bc_map"
    {
        echo -e "canon_path\tbc\tdisposition\tclassification\tlicense_d\tlicense_f\tfirst_diff_d\tfirst_diff_f"
        for i in $(seq 1 435); do
            printf "srvkm/include/test_%03d.c\tBC-01\tdrop\tPURE_RENAME\tmit-or-gpl-2.0-only\tmit-or-gpl-2.0-only\t\t\n" "$i"
        done
    } > "$per_file"

    local out="$dir/out.txt" err="$dir/err.txt"
    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP="$bc_map" R16_PER_FILE="$per_file" \
        python3 "$GATE" >"$out" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t12_wrong_mix" || return 1
    assert_stdout_contains "$out" "F-only=0" "t12_wrong_mix" || return 1
    echo "PASS: r16_gate_t12_wrong_mix (rc=1, F-only=0 rejected despite 435 total)"
    return 0
}

# --- t13 negative: unknown manifest status (e.g. "bogus-status") ---
test_r16_gate_t13_unknown_status() {
    echo "=== r16_gate_t13_unknown_status ==="
    local dir="$TMPROOT/t13"
    mkdir -p "$dir"
    local manifest="$dir/p2-manifest.tsv"
    local bc_map="$dir/canon-to-bc.tsv"
    local per_file="$dir/per-file-classification.tsv"
    # 432 differs (with one bogus-status) + 3 F-only = 435 raw rows but
    # 1 status is unknown.
    {
        echo -e "canon_path\td_rel\tf_rel\td_sha256\tf_sha256\tstatus"
        for i in $(seq 1 431); do
            printf "srvkm/include/test_%03d.c\tsrvkm/include/test_%03d.c\tsrvkm/include/test_%03d.c\tsha%03d\tsha%03d\tdiffers\n" "$i" "$i" "$i" "$i" "$i"
        done
        # Row 432: unknown status
        printf "srvkm/include/test_432.c\tsrvkm/include/test_432.c\tsrvkm/include/test_432.c\tsha432\tsha432\tbogus-status\n"
        # 3 F-only paths
        echo -e "gpu/fant_stackprotector.c\t\tgpu/fant_stackprotector.c\t\tsha_sp_c\tF-only"
        echo -e "gpu/fant_stackprotector.h\t\tgpu/fant_stackprotector.h\t\tsha_sp_h\tF-only"
        echo -e "srvkm/include/common_ri_bridge.h\t\tsrvkm/include/common_ri_bridge.h\t\tsha_crb\tF-only"
    } > "$manifest"
    {
        echo -e "canon_path\tbc_id"
        for i in $(seq 1 431); do
            printf "srvkm/include/test_%03d.c\tBC-01\n" "$i"
        done
        printf "srvkm/include/test_432.c\tBC-01\n"
        echo -e "gpu/fant_stackprotector.c\tBC-22a"
        echo -e "gpu/fant_stackprotector.h\tBC-22a"
        echo -e "srvkm/include/common_ri_bridge.h\tBC-22b"
    } > "$bc_map"
    {
        echo -e "canon_path\tbc\tdisposition\tclassification\tlicense_d\tlicense_f\tfirst_diff_d\tfirst_diff_f"
        for i in $(seq 1 432); do
            printf "srvkm/include/test_%03d.c\tBC-01\tdrop\tPURE_RENAME\tmit-or-gpl-2.0-only\tmit-or-gpl-2.0-only\t\t\n" "$i"
        done
        echo -e "gpu/fant_stackprotector.c\tBC-22a\tdrop\tF-ONLY\tF-ONLY\tmit-or-gpl-2.0-only\t\t"
        echo -e "gpu/fant_stackprotector.h\tBC-22a\tdrop\tF-ONLY\tF-ONLY\tmit-or-gpl-2.0-only\t\t"
        echo -e "srvkm/include/common_ri_bridge.h\tBC-22b\tdrop\tF-ONLY\tF-ONLY\tmit-or-gpl-2.0-only\t\t"
    } > "$per_file"

    local out="$dir/out.txt" err="$dir/err.txt"
    local rc=0
    R16_MANIFEST="$manifest" R16_BC_MAP="$bc_map" R16_PER_FILE="$per_file" \
        python3 "$GATE" >"$out" 2>"$err" || rc=$?

    assert_rc 1 "$rc" "t13_unknown_status" || return 1
    assert_stdout_contains "$out" "unknown manifest status" "t13_unknown_status" || return 1
    echo "PASS: r16_gate_t13_unknown_status (rc=1, unknown status rejected)"
    return 0
}

# --- Run all tests ---
cd "$REPO_ROOT"
for test_fn in test_r16_gate_t01_positive \
               test_r16_gate_t02_unknown_bc \
               test_r16_gate_t03_unknown_disposition \
               test_r16_gate_t04_unknown_license \
               test_r16_gate_t05_bc_mismatch \
               test_r16_gate_t06_fail_closed_violation \
               test_r16_gate_t07_malformed_manifest \
               test_r16_gate_t08_determinism \
               test_r16_gate_t09_missing_classification \
               test_r16_gate_t10_under_production \
               test_r16_gate_t11_duplicate_manifest_path \
               test_r16_gate_t12_wrong_mix \
               test_r16_gate_t13_unknown_status; do
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