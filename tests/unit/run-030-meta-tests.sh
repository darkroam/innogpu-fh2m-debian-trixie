#!/usr/bin/env bash
# tests/unit/run-030-meta-tests.sh — 13 条 030-NNN meta.json 静态契约校验
#
# 职责（docs/planning/o-stage-integration-plan.md §四）：
#   1) 每条 030-*.meta.json 为合法 JSON、schema_version == "1.0"；
#   2) deepin_patch.sha256 == 对应来源 patch 磁盘 SHA-256；apply.patch_sha256
#      == 对应 030-NNN.patch 磁盘 SHA-256；
#   3) 链点哈希衔接：首条 before == F0 树 hash 7219d817…；每条 before ==
#      前一条 after；链尾 after == 937e3710…（O_stage 树 hash）；
#   4) status == "draft"、license/runtime 如实 pending（本批不冒充 PASS）。
#
# 只读 patches/；零写入。退出码：0=全过；1=任一校验失败。

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
LC_ALL=C
export LC_ALL

F0_TREE="7219d817c412fcf87a5341f1604e03bb24b7b6670cf51d0f2c8814e5fe72c0cb"
FINAL_TREE="937e37107f692712e6fba9b5eb93a48dd6bb034f138e5e51a2bb9c2beaa0d652"

CHAIN=(
  "030-001|patches/001-kernel-6.12-compat.patch"
  "030-002|patches/002-dp-fbdev-fallback-mode.patch"
  "030-006|patches/006-local-connector-acpi-map.patch"
  "030-009|patches/009-local-internal-edp-connector.patch"
  "030-007|patches/007-fbdev-io-mmap.patch"
  "030-023|patches/023-invisible-read-no-writeback.patch"
  "030-025|patches/025-dma-resv-usage-rw.patch"
  "030-024|patches/024-suspend-resume.patch"
  "030-026|patches/026-inactive-crtc-vblank-guard.patch"
  "030-027|patches/027-foreign-dmabuf-lifecycle.patch"
  "030-026-lifecycle|patches/026-suspend-resume-dvfs-lifecycle.patch"
  "030-028|patches/028-suspend-resume-hal-temp-monitor-delay.patch"
  "030-029|patches/029-suspend-resume-ddcci-panel.patch"
)

fail() { echo "FAIL: $*" >&2; FAILED=1; }
FAILED=0
prev_after="$F0_TREE"
idx=0

for entry in "${CHAIN[@]}"; do
    id="${entry%%|*}"
    src_patch="${entry#*|}"
    meta="$ROOT/patches/$id.meta.json"
    patch="$ROOT/patches/$id.patch"
    idx=$((idx + 1))

    [[ -f "$meta" ]] || { fail "$id: meta missing"; continue; }
    [[ -f "$patch" ]] || { fail "$id: 030 patch missing"; continue; }
    [[ -f "$ROOT/$src_patch" ]] || { fail "$id: source patch missing: $src_patch"; continue; }

    got="$(python3 - "$meta" "$patch" "$ROOT/$src_patch" <<'PY'
import hashlib, json, sys

def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as fh:
        for blk in iter(lambda: fh.read(1 << 20), b""):
            h.update(blk)
    return h.hexdigest()

meta_path, patch_path, src_path = sys.argv[1], sys.argv[2], sys.argv[3]
m = json.load(open(meta_path, encoding="utf-8"))
errors = []
if m.get("schema_version") != "1.0":
    errors.append("schema_version!=1.0")
if m.get("id") != meta_path.split("/")[-1].removesuffix(".meta.json"):
    errors.append("id mismatch")
if m["deepin_patch"]["sha256"] != sha(src_path):
    errors.append("deepin_patch.sha256 != source patch on disk")
if m["apply"]["patch_sha256"] != sha(patch_path):
    errors.append("apply.patch_sha256 != 030 patch on disk")
if m.get("status") != "draft":
    errors.append("status!=draft")
if m["verified"]["static"]["status"] != "pass":
    errors.append("verified.static!=pass")
if m["verified"]["runtime"]["status"] != "pending":
    errors.append("verified.runtime!=pending")
print(json.dumps({"errors": errors, "before": m["apply"]["before_tree_hash"],
                  "after": m["apply"]["after_tree_hash"]}))
PY
)"
    before="$(python3 -c "import json,sys; print(json.loads(sys.argv[1])['before'])" "$got")"
    after="$(python3 -c "import json,sys; print(json.loads(sys.argv[1])['after'])" "$got")"
    errs="$(python3 -c "import json,sys; print(';'.join(json.loads(sys.argv[1])['errors']))" "$got")"

    [[ -n "$errs" ]] && fail "$id: $errs"
    [[ "$before" == "$prev_after" ]] || fail "$id: chain base mismatch: before=$before expect=$prev_after"
    prev_after="$after"
done

[[ "$prev_after" == "$FINAL_TREE" ]] || fail "chain tail after=$prev_after != O_stage tree $FINAL_TREE"

if [[ "$FAILED" == 0 ]]; then
    echo "PASS: 13 meta files verified (chain $F0_TREE -> $FINAL_TREE)"
    exit 0
fi
echo "FAILED" >&2
exit 1
