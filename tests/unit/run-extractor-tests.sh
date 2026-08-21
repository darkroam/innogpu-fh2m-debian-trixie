#!/bin/bash
# Unit tests: extract-vendor-binaries.sh against an isolated fixture deb/vendor tree.
# Covers: vendor file missing, hash mismatch, --check-only failure on missing,
# interrupted/partial recovery, idempotent re-run, source-deb SHA mismatch.
# Read-only on the repo; all fixtures live in a mktemp dir; no root/device/reboot.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EXTRACTOR="$ROOT/scripts/extract-vendor-binaries.sh"
VALIDATOR="$ROOT/tools/validate-binary-manifest.py"
runtime="$(mktemp -d "${TMPDIR:-/tmp}/inno-extractor-tests.XXXXXX")"
trap 'rm -rf "$runtime"' EXIT

tests=0; failures=0; skipped=0
pass() { tests=$((tests+1)); printf '%s=PASS\n' "$1"; }
fail() { tests=$((tests+1)); failures=$((failures+1)); printf '%s=FAIL reason=%s\n' "$1" "$2"; }

# ---- fixture deb: 1 payload file + 1 symlink ----
mkdir -p "$runtime/root/DEBIAN" "$runtime/root/usr/lib/x86_64-linux-gnu/innogpu-fh2m"
printf 'Package: inno-fixture\nVersion: 1.0\nArchitecture: all\nMaintainer: test <t@example.invalid>\nDescription: fixture\n' > "$runtime/root/DEBIAN/control"
printf 'FOO-CONTENT-12345' > "$runtime/root/usr/lib/x86_64-linux-gnu/innogpu-fh2m/libfoo.so"
# 符号链接载荷(GBM 风格): libbar.so -> ../innogpu-fh2m/libfoo.so
mkdir -p "$runtime/root/usr/lib/x86_64-linux-gnu/gbm"
ln -s ../innogpu-fh2m/libfoo.so "$runtime/root/usr/lib/x86_64-linux-gnu/gbm/libbar.so"
dpkg-deb --root-owner-group --build "$runtime/root" "$runtime/fixture.deb" >/dev/null 2>&1
DEB_SHA="$(sha256sum "$runtime/fixture.deb" | cut -d' ' -f1)"
FOO_SHA="$(printf 'FOO-CONTENT-12345' | sha256sum | cut -d' ' -f1)"
FOO_SIZE=$(stat -c %s "$runtime/root/usr/lib/x86_64-linux-gnu/innogpu-fh2m/libfoo.so")

write_manifest() { # <deb_sha> -> writes $runtime/manifest.json
    cat > "$runtime/manifest.json" <<JSON
{
  "format_version": 1,
  "source_package": "inno-fixture",
  "source_version": "1.0",
  "source_deb_sha256": "$1",
  "architecture": "amd64",
  "entries": [
    {
      "source_path": "usr/lib/x86_64-linux-gnu/innogpu-fh2m/libfoo.so",
      "vendor_path": "userspace/x86_64-linux-gnu/innogpu-fh2m/libfoo.so",
      "sha256": "$FOO_SHA",
      "size": $FOO_SIZE,
      "kind": "userspace-lib",
      "role": "test",
      "license": "vendor-binary"
    },
    {
      "source_path": "usr/lib/x86_64-linux-gnu/gbm/libbar.so",
      "vendor_path": "userspace/x86_64-linux-gnu/gbm/libbar.so",
      "link_target": "../innogpu-fh2m/libfoo.so",
      "kind": "userspace-lib",
      "role": "test",
      "license": "vendor-binary"
    }
  ]
}
JSON
}
write_manifest "$DEB_SHA"

run_extract() { # <args...> ; sets rc
    set +e
    MANIFEST_PATH="$runtime/manifest.json" VENDOR_ROOT="$runtime/vendor" \
        INNOGPU_DEEPIN_DEB="$runtime/fixture.deb" "$EXTRACTOR" "$@" > "$runtime/out.log" 2>&1
    rc=$?
    set -e
}

# E1: vendor 空 + --check-only -> 必须 FAIL
run_extract --check-only
if [ "$rc" -ne 0 ] && grep -q 'vendor_check_only_result=FAIL' "$runtime/out.log"; then
    pass "extractor_missing_check_only"
else
    fail "extractor_missing_check_only" "rc=$rc log=$(tail -1 "$runtime/out.log")"
fi

# E2: 完整提取 -> PASS，文件与链接就位且哈希正确
run_extract
if [ "$rc" -eq 0 ] && [ -f "$runtime/vendor/userspace/x86_64-linux-gnu/innogpu-fh2m/libfoo.so" ] \
   && [ "$(sha256sum "$runtime/vendor/userspace/x86_64-linux-gnu/innogpu-fh2m/libfoo.so" | cut -d' ' -f1)" == "$FOO_SHA" ] \
   && [ -L "$runtime/vendor/userspace/x86_64-linux-gnu/gbm/libbar.so" ]; then
    pass "extractor_extract_ok"
else
    fail "extractor_extract_ok" "rc=$rc $($([ -f "$runtime/vendor/userspace/x86_64-linux-gnu/innogpu-fh2m/libfoo.so" ] && echo file-ok || echo file-missing))"
fi

# E3: 幂等重跑 -> skipped>=1, rebuilt=0, PASS
run_extract
if [ "$rc" -eq 0 ] && grep -q 'vendor_extraction=PASS' "$runtime/out.log" && grep -q 'rebuilt=0' "$runtime/out.log"; then
    pass "extractor_idempotent"
else
    fail "extractor_idempotent" "rc=$rc log=$(tail -1 "$runtime/out.log")"
fi

# E4: 提取后 --check-only -> PASS (skipped=2 failures=0)
run_extract --check-only
if [ "$rc" -eq 0 ] && grep -q 'vendor_check_only_result=PASS' "$runtime/out.log"; then
    pass "extractor_check_only_ok"
else
    fail "extractor_check_only_ok" "rc=$rc log=$(tail -1 "$runtime/out.log")"
fi

# E5: 篡改 vendor 文件哈希 -> --check-only 必须 FAIL
printf 'TAMPERED' > "$runtime/vendor/userspace/x86_64-linux-gnu/innogpu-fh2m/libfoo.so"
run_extract --check-only
if [ "$rc" -ne 0 ] && grep -q 'vendor_check_only_result=FAIL' "$runtime/out.log"; then
    pass "extractor_hash_mismatch_check_only"
else
    fail "extractor_hash_mismatch_check_only" "rc=$rc log=$(tail -1 "$runtime/out.log")"
fi

# E6: 中断/残留文件恢复 -> 部分文件被重建为正确哈希
run_extract
if [ "$rc" -eq 0 ] && [ "$(sha256sum "$runtime/vendor/userspace/x86_64-linux-gnu/innogpu-fh2m/libfoo.so" | cut -d' ' -f1)" == "$FOO_SHA" ]; then
    pass "extractor_partial_recovery"
else
    fail "extractor_partial_recovery" "rc=$rc hash=$($(sha256sum "$runtime/vendor/userspace/x86_64-linux-gnu/innogpu-fh2m/libfoo.so" 2>/dev/null || echo missing))"
fi

# E7: 源 deb SHA 不匹配 -> FAIL
write_manifest "$(printf 'x' | sha256sum | cut -d' ' -f1)"
run_extract --check-only
if [ "$rc" -ne 0 ] && grep -q 'vendor_source_deb_sha256=FAIL' "$runtime/out.log"; then
    pass "extractor_source_deb_sha_mismatch"
else
    fail "extractor_source_deb_sha_mismatch" "rc=$rc log=$(tail -1 "$runtime/out.log")"
fi

printf 'tests_total=%d tests_passed=%d tests_failed=%d tests_skipped=%d\n' "$tests" "$((tests-failures))" "$failures" "$skipped"
[ "$failures" -eq 0 ]
