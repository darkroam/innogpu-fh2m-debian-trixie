#!/bin/bash
# Fixture tests: --results-file strict parsing in run-capability-baseline.sh.
# Uses RUNTIME_PARSE_ONLY=1 (no device probes) + RUNTIME_BASELINE_DIR (no
# clobber of baselines/), so each case is fast and side-effect free.

set -u -o pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SCRIPT="$ROOT/tests/runtime/run-capability-baseline.sh"
runtime="$(mktemp -d "${TMPDIR:-/tmp}/inno-results-tests.XXXXXX")"
trap 'rm -rf "$runtime"' EXIT

passed=0; failed=0; t=0
pass() { passed=$((passed+1)); printf 'results_parser_t%02d=PASS # %s\n' "$passed" "$1"; }
fail() { failed=$((failed+1)); printf 'results_parser_t%02d=FAIL reason=%s\n' "$passed" "$2"; }

# run_capture <fixture> <out-file>  ; 纯解析模式运行脚本并落盘输出（快、不污染 baselines/）
run_capture() {
    RUNTIME_PARSE_ONLY=1 RUNTIME_BASELINE_DIR="$runtime" \
        timeout 20 bash "$SCRIPT" --allow-authorized-tests --results-file "$1" > "$2" 2>&1
    return $?
}

# 预构建 fixture
printf 'runtime_audio_playback=PASS reason=heard-tone\n' > "$runtime/valid.txt"
printf 'runtime_bogus_thing=PASS reason=x\n' > "$runtime/unknown-name.txt"
printf 'runtime_audio_playback=MAYBE reason=x\n' > "$runtime/bad-status.txt"
printf 'runtime_audio_playback=PASS reason=first\nruntime_audio_playback=FAIL reason=second\n' > "$runtime/duplicate.txt"
printf 'runtime_audio_playback=PASS reason=xruntime_display_topology=PASS\n' > "$runtime/glued.txt"
printf 'runtime_audio_playback=PASS reason=heard-tone' > "$runtime/no-newline.txt"   # 无尾换行
printf 'runtime_audio_playback=PASS\n' > "$runtime/no-reason.txt"
printf 'runtime_audio_playback=PASS reason=x\nruntime_audio_playback=UNVERIFIED reason=y\n' > "$runtime/dup-last.txt"
printf '# 说明注释：真实设备原始输出示例\n# vaapi_decode_node=ok /dev/dri/renderD128\nruntime_audio_playback=PASS reason=heard-tone\n' > "$runtime/comments.txt"

check() { # <label> <out-file> <grep-pat> <negate?>
    t=$((t+1))
    local label="$1" out="$2" pat="$3" neg="${4:-}"
    if [[ -n "$neg" ]]; then
        if ! grep -q "$pat" "$out"; then pass "$label"; else fail "$label" "unexpected match: $pat"; fi
    else
        if grep -q "$pat" "$out"; then pass "$label"; else fail "$label" "missing: $pat"; fi
    fi
}

O="$runtime/o.log"
run_capture "$runtime/valid.txt" "$O"; rc=$?
check valid_merge "$O" 'runtime_audio_playback=PASS reason=heard-tone'
# parse-only 模式：35 项种子全 SKIP + 1 PASS 合并 → overall=SKIP (rc=2)
[ "$rc" -eq 2 ] || fail valid_rc "rc=$rc (parse-only expect 2=SKIP)"

run_capture "$runtime/unknown-name.txt" "$O"
check unknown_name "$O" 'ignored unknown results entry'
check unknown_name_absent "$O" '^runtime_bogus_thing=' negate

run_capture "$runtime/bad-status.txt" "$O"
check bad_status "$O" 'ignored bad status'
check bad_status_stays "$O" 'runtime_audio_playback=SKIP'

run_capture "$runtime/duplicate.txt" "$O"
check duplicate_warn "$O" 'duplicate results entry, using last'
check duplicate_last "$O" 'runtime_audio_playback=FAIL reason=second'

run_capture "$runtime/glued.txt" "$O"
check glued_reject "$O" 'ignored glued'
check glued_stays "$O" 'runtime_audio_playback=SKIP'

run_capture "$runtime/no-newline.txt" "$O"
check no_newline_ok "$O" 'runtime_audio_playback=PASS reason=heard-tone'

run_capture "$runtime/no-reason.txt" "$O"
check no_reason_reject "$O" 'without evidence reason'
check no_reason_stays "$O" 'runtime_audio_playback=SKIP'

run_capture "$runtime/dup-last.txt" "$O"
check dup_last_warn "$O" 'duplicate results entry, using last'
check dup_last_value "$O" 'runtime_audio_playback=UNVERIFIED reason=y'

run_capture "$runtime/comments.txt" "$O"
check comments_skip "$O" 'runtime_audio_playback=PASS reason=heard-tone'
check comments_no_warn "$O" 'ignored malformed' negate
check comments_no_vaapi_leak "$O" 'vaapi_decode_node=' negate

RUNTIME_PARSE_ONLY=1 RUNTIME_BASELINE_DIR="$runtime" bash "$SCRIPT" --allow-authorized-tests --results-file "$runtime/missing-file.txt" >/dev/null 2>&1
rc=$?
t=$((t+1))
if [ "$rc" -eq 2 ]; then pass missing_file_rc2; else fail missing_file_rc2 "rc=$rc"; fi

RUNTIME_PARSE_ONLY=1 RUNTIME_BASELINE_DIR="$runtime" bash "$SCRIPT" --results-file "$runtime/valid.txt" >/dev/null 2>&1
rc=$?
t=$((t+1))
if [ "$rc" -eq 2 ]; then pass unauthorized_rc2; else fail unauthorized_rc2 "rc=$rc"; fi

printf 'tests_total=%d tests_passed=%d tests_failed=%d tests_skipped=0\n' "$t" "$passed" "$failed"
[ "$failed" -eq 0 ]
