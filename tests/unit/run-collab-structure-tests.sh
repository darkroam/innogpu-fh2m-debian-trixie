#!/bin/bash
# Persistent fixtures for tools/validate-collab.py (multiagent-collab.md §五).
# Builds isolated collab/ trees in a temporary directory and asserts the
# validator's exit codes and violation messages. No device, no git, no network.

TOOL="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/tools/validate-collab.py"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
passed=0
failed=0

write_index() {
    local root=$1
    {
        printf '%s\n' '| 轮次 | 日期 | 主题 | 状态 |'
        printf '%s\n' '|------|------|------|------|'
        cat
    } > "$root/collab/INDEX.md"
}

make_round() {
    local root=$1 name=$2
    mkdir -p "$root/collab/$name"
    printf '%s\n' '# request' > "$root/collab/$name/request.md"
    printf '%s\n' '# report' > "$root/collab/$name/report.md"
}

expect() {
    local name=$1 root=$2 want=$3 needle=${4:-} out rc
    out="$(python3 "$TOOL" --root "$root" 2>&1)"
    rc=$?
    local number=$((passed + failed + 1))
    local ok=0
    if [[ $rc -eq $want ]]; then
        if [[ -n "$needle" ]]; then
            grep -Fq "$needle" <<<"$out" && ok=1
        else
            [[ -z "$out" ]] && ok=1
        fi
    fi
    if [[ $ok -eq 1 ]]; then
        echo "collab_t$number=PASS # $name"
        passed=$((passed + 1))
    else
        echo "collab_t$number=FAIL # $name (rc=$rc want=$want out=$out)"
        failed=$((failed + 1))
    fi
}

seed() {
    local root=$1
    mkdir -p "$root/collab"
    make_round "$root" R01-2026-08-31-topic-a
    printf '%s\n' '| R01 | 2026-08-31 | topic-a | 待开发 |' | write_index "$root"
}

# t1: minimal valid round tree.
root="$WORK/t1"
seed "$root"
expect "valid_minimal_round" "$root" 0

# t2: R01 and R010 side by side, both registered - exact id matching; the
# old awk regex match ($2 ~ id) failed here by printing two statuses for R01.
root="$WORK/t2"
seed "$root"
make_round "$root" R010-2026-08-31-topic-b
printf '%s\n' '| R010 | 2026-08-31 | topic-b | 待开发 |' >> "$root/collab/INDEX.md"
expect "exact_id_match_R01_vs_R010" "$root" 0

# t3: R010 directory exists but is not registered.
root="$WORK/t3"
seed "$root"
make_round "$root" R010-2026-08-31-topic-b
expect "unregistered_round_detected" "$root" 1 "R010"

# t4: duplicate INDEX rows for the same round.
root="$WORK/t4"
seed "$root"
printf '%s\n' '| R01 | 2026-08-31 | topic-a | 待开发 |' >> "$root/collab/INDEX.md"
expect "duplicate_index_row_rejected" "$root" 1 "重复登记"

# t5: INDEX row without a corresponding directory.
root="$WORK/t5"
seed "$root"
printf '%s\n' '| R99 | 2026-08-31 | ghost | 待开发 |' >> "$root/collab/INDEX.md"
expect "orphan_index_row_rejected" "$root" 1 "无对应目录"

# t6: directory without an INDEX row.
root="$WORK/t6"
seed "$root"
make_round "$root" R02-2026-08-31-topic-c
expect "orphan_round_rejected" "$root" 1 "未登记"

# t7: directory date disagrees with the INDEX row date.
root="$WORK/t7"
seed "$root"
sed -i 's/2026-08-31/2026-08-30/' "$root/collab/INDEX.md"
expect "date_mismatch_rejected" "$root" 1 "日期不一致"

# t8: invalid round directory name.
root="$WORK/t8"
seed "$root"
mkdir -p "$root/collab/notes"
printf '%s\n' '# x' > "$root/collab/notes/note.md"
expect "invalid_directory_name_rejected" "$root" 1 "命名无效"

# t9: stray file at the collab/ root.
root="$WORK/t9"
seed "$root"
printf '%s\n' '# stray' > "$root/collab/stray.md"
expect "stray_root_file_rejected" "$root" 1 "只允许 INDEX.md"

# t10: symlink at the collab/ root.
root="$WORK/t10"
seed "$root"
ln -s INDEX.md "$root/collab/link.md"
expect "root_symlink_rejected" "$root" 1 "符号链接"

# t11: symlink inside a round directory.
root="$WORK/t11"
seed "$root"
ln -s request.md "$root/collab/R01-2026-08-31-topic-a/extra.md"
expect "round_symlink_rejected" "$root" 1 "符号链接"

# t12: nested directory inside a round directory.
root="$WORK/t12"
seed "$root"
mkdir -p "$root/collab/R01-2026-08-31-topic-a/sub"
printf '%s\n' '# x' > "$root/collab/R01-2026-08-31-topic-a/sub/x.md"
expect "nested_directory_rejected" "$root" 1 "嵌套目录"

# t13: non-markdown file inside a round directory.
root="$WORK/t13"
seed "$root"
printf '%s\n' 'binary' > "$root/collab/R01-2026-08-31-topic-a/data.bin"
expect "non_markdown_file_rejected" "$root" 1 "只允许 Markdown"

# t14: report.md missing.
root="$WORK/t14"
seed "$root"
rm "$root/collab/R01-2026-08-31-topic-a/report.md"
expect "missing_report_rejected" "$root" 1 "缺少 report.md"

# t15: request.md missing.
root="$WORK/t15"
seed "$root"
rm "$root/collab/R01-2026-08-31-topic-a/request.md"
expect "missing_request_rejected" "$root" 1 "缺少 request.md"

# t16: status outside the authoritative whitelist.
root="$WORK/t16"
seed "$root"
sed -i 's/待开发/已完成/' "$root/collab/INDEX.md"
expect "invalid_status_rejected" "$root" 1 "状态不在白名单"

# t17: INDEX header is not 轮次/日期/主题/状态.
root="$WORK/t17"
seed "$root"
sed -i 's/轮次 | 日期 | 主题 | 状态/编号 | 日期 | 主题 | 状态/' "$root/collab/INDEX.md"
expect "wrong_header_rejected" "$root" 1 "表头必须为"

# t18: collab/INDEX.md missing entirely.
root="$WORK/t18"
seed "$root"
rm "$root/collab/INDEX.md"
expect "missing_index_rejected" "$root" 1 "INDEX.md 不存在"

# t19: the collab/ directory itself is absent (fresh clone / CI) - valid.
root="$WORK/t19"
expect "missing_collab_dir_is_valid" "$root" 0

# t20: collab/ itself must not redirect validation to an external directory.
root="$WORK/t20"
external="$WORK/t20-external"
seed "$external"
mkdir -p "$root"
ln -s "$external/collab" "$root/collab"
expect "collab_root_symlink_rejected" "$root" 1 "根目录不允许是符号链接"

# t21: a broken collab/ symlink is not equivalent to an absent local directory.
root="$WORK/t21"
mkdir -p "$root"
ln -s "$root/missing" "$root/collab"
expect "broken_collab_root_symlink_rejected" "$root" 1 "根目录不允许是符号链接"

# t22: INDEX topic must exactly match the topic encoded in the directory name.
root="$WORK/t22"
seed "$root"
sed -i 's/topic-a/topic-b/' "$root/collab/INDEX.md"
expect "topic_mismatch_rejected" "$root" 1 "主题不一致"

# t23: hidden Markdown receives the same privacy checks as visible records.
root="$WORK/t23"
seed "$root"
printf '/%s/%s/work\n' home private-user > "$root/collab/R01-2026-08-31-topic-a/.private.md"
expect "hidden_markdown_private_data_rejected" "$root" 1 "包含敏感信息标记"

# t24: common service-token forms are rejected, not only local path markers.
root="$WORK/t24"
seed "$root"
printf '%s%s\n' 'sk-proj-abcdefghij' 'klmnopqrstuvwxyz123456' > "$root/collab/R01-2026-08-31-topic-a/report.md"
expect "service_token_rejected" "$root" 1 "包含敏感信息标记"

# t25: INDEX.md is scanned too, and token matching is case-insensitive.
root="$WORK/t25"
seed "$root"
printf '\n%s %s%s\n' bearer abcdefghijklm nopqrstuvwxyz123456 >> "$root/collab/INDEX.md"
expect "index_lowercase_bearer_rejected" "$root" 1 "collab/INDEX.md"

# t26: the check-docs.sh ripgrep layer consumes the same shared pattern file
# and is case-insensitive as well (locks both layers to one source).
root="$WORK/t26"
seed "$root"
printf '%s %s%s\n' bearer abcdefghijklm nopqrstuvwxyz123456 > "$root/collab/R01-2026-08-31-topic-a/.note.md"
number=$((passed + failed + 1))
if rg -n --hidden --no-ignore --ignore-case --pcre2 -f "$(dirname "$TOOL")/private-data-patterns.txt" "$root/collab" --glob '*.md' 2>/dev/null | grep -Fq 'abcdefghijklmnopqrstuvwxyz123456'; then
    echo "collab_t$number=PASS # rg_shared_patterns_case_insensitive"
    passed=$((passed + 1))
else
    echo "collab_t$number=FAIL # rg_shared_patterns_case_insensitive"
    failed=$((failed + 1))
fi

tests_total=$((passed + failed))
tests_skipped=0
echo "tests_total=$tests_total tests_passed=$passed tests_failed=$failed tests_skipped=$tests_skipped"
if [[ $failed -ne 0 ]]; then
    exit 1
fi
exit 0
