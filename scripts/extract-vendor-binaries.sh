#!/bin/bash
# Idempotent vendor extraction from the pinned Deepin deb per binary-manifest.json.
# - verifies the source deb SHA-256
# - extracts each entry to vendor/<vendor_path> only when missing or hash-mismatched
# - recreates symlinks
# - atomic temp+rename, no partial vendor files
# - --check-only: read-only report
# - machine-readable PASS/FAIL summary

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

CHECK_ONLY=0
if [[ "${1:-}" == "--check-only" ]]; then CHECK_ONLY=1; fi

MANIFEST="$ROOT/binary-manifest.json"
DEB="${INNOGPU_DEEPIN_DEB:-$ROOT/debs/innogpu-fh2m_20250421190503-debug_amd64.deb}"
[[ -f "$MANIFEST" ]] || { echo "vendor_extraction=PASS_MANIFEST_MISSING"; echo "vendor_manifest_missing=$MANIFEST"; exit 1; }
[[ -f "$DEB" ]] || { echo "vendor_extraction=FAIL_MISSING_SOURCE_DEB"; echo "vendor_source_deb_missing=$DEB"; exit 1; }

# manifest 解析
EXPECTED_DEB_SHA=$(python3 -c "import json;print(json.load(open('$MANIFEST'))['source_deb_sha256'])")
ACTUAL_DEB_SHA=$(sha256sum "$DEB" | cut -d' ' -f1)
if [[ "$ACTUAL_DEB_SHA" != "$EXPECTED_DEB_SHA" ]]; then
    echo "vendor_source_deb_sha256=FAIL got=$ACTUAL_DEB_SHA expected=$EXPECTED_DEB_SHA"
    exit 1
fi
echo "vendor_source_deb_sha256=PASS"

# manifest schema 与路径安全校验（先于任何写入）
if ! python3 tools/validate-binary-manifest.py >/dev/null; then
    echo "vendor_manifest_schema=FAIL"
    python3 tools/validate-binary-manifest.py | head -5
    exit 1
fi
echo "vendor_manifest_schema=PASS"

# 合法 kind 集合
ALLOWED_KINDS="kernel-black-box userspace-lib ddx userspace-config firmware"

TMP_ROOT="$(mktemp -d "$ROOT/.build/vendor-extract.XXXXXX")"
trap 'rm -rf "$TMP_ROOT"' EXIT
dpkg-deb -x "$DEB" "$TMP_ROOT/root"

fail=0; ok=0; skipped=0; rebuilt=0
check_path() {
    local vp="$1"
    # 拒绝绝对路径与任何路径组件为 .. 的遍历
    if [[ "$vp" == /* || "$vp" =~ (^|/)\.\.(/|$) ]]; then
        echo "vendor_path_traversal=FAIL $vp"; return 1
    fi
    return 0
}

while IFS= read -r entry; do
    src=$(echo "$entry" | python3 -c "import json,sys;e=json.loads(sys.stdin.read());print(e['source_path'])")
    vp=$(echo "$entry" | python3 -c "import json,sys;e=json.loads(sys.stdin.read());print(e['vendor_path'])")
    kind=$(echo "$entry" | python3 -c "import json,sys;e=json.loads(sys.stdin.read());print(e['kind'])")
    is_link=$(echo "$entry" | python3 -c "import json,sys;e=json.loads(sys.stdin.read());print('link_target' in e)")
    link_target=""
    if [[ "$is_link" == "True" ]]; then
        link_target=$(echo "$entry" | python3 -c "import json,sys;e=json.loads(sys.stdin.read());print(e.get('link_target',''))")
    fi
    exp_sha=$(echo "$entry" | python3 -c "import json,sys;e=json.loads(sys.stdin.read());print(e.get('sha256',''))")
    exp_size=$(echo "$entry" | python3 -c "import json,sys;e=json.loads(sys.stdin.read());print(e.get('size',0))")

    # kind 校验
    [[ " $ALLOWED_KINDS " == *" "$kind" "* ]] || { echo "vendor_unknown_kind=FAIL $kind ($vp)"; fail=$((fail+1)); continue; }
    # 路径安全: source_path 与 vendor_path 均须为载荷根内相对路径
    check_path "$src" || { echo "vendor_source_path_unsafe=FAIL $src"; fail=$((fail+1)); continue; }
    check_path "$vp" || { fail=$((fail+1)); continue; }

    dest="$ROOT/vendor/$vp"
    if [[ "$is_link" == "True" ]]; then
        if [[ -L "$dest" && "$(readlink "$dest")" == "$link_target" ]]; then
            skipped=$((skipped+1)); continue
        fi
        if [[ "$CHECK_ONLY" -eq 1 ]]; then echo "vendor_check_only=MISSING_OR_MISMATCH $vp"; fail=$((fail+1)); continue; fi
        mkdir -p "$(dirname "$dest")"
        tmp_link="$TMP_ROOT/link.tmp.$RANDOM"
        ln -s "$link_target" "$tmp_link"
        mv -T "$tmp_link" "$dest" && rebuilt=$((rebuilt+1))
        continue
    fi

    # 文件: 存在、是普通文件(非符号链接)且哈希一致 -> 跳过
    if [[ -f "$dest" && ! -L "$dest" ]]; then
        cur=$(sha256sum "$dest" | cut -d' ' -f1)
        if [[ "$cur" == "$exp_sha" ]]; then skipped=$((skipped+1)); continue; fi
    fi
    if [[ "$CHECK_ONLY" -eq 1 ]]; then echo "vendor_check_only=MISSING_OR_MISMATCH $vp"; fail=$((fail+1)); continue; fi

    src_file="$TMP_ROOT/root/$src"
    # 包含性: 解析后必须仍位于解包根目录内
    src_real="$(realpath -m "$src_file")"
    root_real="$(realpath -m "$TMP_ROOT/root")"
    [[ "$src_real" == "$root_real"/* ]] || { echo "vendor_source_escape=FAIL $src -> $src_real"; fail=$((fail+1)); continue; }
    [[ -f "$src_file" ]] || { echo "vendor_source_missing=FAIL $src"; fail=$((fail+1)); continue; }
    got=$(sha256sum "$src_file" | cut -d' ' -f1)
    got_size=$(stat -c %s "$src_file")
    if [[ "$got" != "$exp_sha" || "$got_size" != "$exp_size" ]]; then
        echo "vendor_hash_mismatch=FAIL $vp got=$got expected=$exp_sha"
        fail=$((fail+1)); continue
    fi
    mkdir -p "$(dirname "$dest")"
    tmp_file="$TMP_ROOT/out.tmp.$RANDOM"
    cp "$src_file" "$tmp_file"
    mv "$tmp_file" "$dest" && rebuilt=$((rebuilt+1))
done < <(python3 -c "import json;m=json.load(open('$MANIFEST'));[print(json.dumps(e)) for e in m['entries']]")

if [[ "$CHECK_ONLY" -eq 1 ]]; then
    echo "vendor_extraction=CHECK_ONLY skipped=$skipped failures=$fail"
    if [[ "$fail" -eq 0 ]]; then echo "vendor_check_only_result=PASS"; else echo "vendor_check_only_result=FAIL"; fi
else
    echo "vendor_extraction=PASS skipped=$skipped rebuilt=$rebuilt"
fi
[[ "$fail" -eq 0 ]] || { echo "vendor_extraction_overall=FAIL failures=$fail"; exit 1; }
echo "vendor_extraction_overall=PASS"
