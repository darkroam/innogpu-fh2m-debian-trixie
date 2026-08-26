#!/usr/bin/env python3
"""Validate binary-manifest.json schema and path safety.

Called by scripts/extract-vendor-binaries.sh before any extraction.
Rules:
- source_path / vendor_path: non-empty, unique, relative (no leading '/',
  no path component equal to '..'), no duplicate vendor_path
- link_target (symlinks only): non-empty, relative, no '..' components,
  not absolute
- kind in allowed set
- license: non-empty string (semantic rights evidence is checked separately by
  tools/audit-licenses.py; vendor-binary remains an unresolved provenance marker)
- file entries: sha256 (64 hex) + size present
- symlink entries: link_target present
- top-level: format_version, source_package, source_version,
  source_deb_sha256 (64 hex), architecture non-empty
"""
import json, re, sys

ALLOWED_KINDS = {"kernel-black-box", "userspace-lib", "ddx", "userspace-config", "firmware"}
SHA_RE = re.compile(r"^[0-9a-f]{64}$")

def rel_safe(p):
    return bool(p) and not p.startswith("/") and not any(c == ".." for c in p.split("/"))

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "binary-manifest.json"
    try:
        m = json.load(open(path))
    except Exception as e:
        print("manifest_schema=FAIL json_error=%s" % e); return 1
    problems = []

    for field in ("format_version", "source_package", "source_version", "source_deb_sha256", "architecture"):
        if not m.get(field):
            problems.append("top-level missing/empty: " + field)
    if not SHA_RE.match(m.get("source_deb_sha256", "")):
        problems.append("source_deb_sha256 not 64-hex")

    entries = m.get("entries")
    if not isinstance(entries, list) or not entries:
        problems.append("entries missing/empty")

    srcs, vps = set(), set()
    for i, e in enumerate(entries or []):
        tag = "entry[%d]" % i
        sp = e.get("source_path", ""); vp = e.get("vendor_path", "")
        kind = e.get("kind", ""); lt = e.get("link_target"); license_value = e.get("license")
        if not rel_safe(sp): problems.append("%s source_path unsafe/empty: %r" % (tag, sp))
        if not rel_safe(vp): problems.append("%s vendor_path unsafe/empty: %r" % (tag, vp))
        if sp in srcs: problems.append("%s duplicate source_path: %s" % (tag, sp))
        if vp in vps: problems.append("%s duplicate vendor_path: %s" % (tag, vp))
        srcs.add(sp); vps.add(vp)
        if kind not in ALLOWED_KINDS: problems.append("%s unknown kind: %s" % (tag, kind))
        if not isinstance(license_value, str) or not license_value.strip():
            problems.append("%s license missing/empty" % tag)
        is_link = "link_target" in e
        if is_link:
            if not lt: problems.append("%s link_target empty" % tag)
            elif lt.startswith("/"): problems.append("%s link_target absolute: %r" % (tag, lt))
            else:
                # 解析链接目标: 相对 vp 所在目录, 必须仍位于载荷根内(不允许越出)
                import posixpath
                resolved = posixpath.normpath(posixpath.join(posixpath.dirname(vp), lt))
                if resolved.startswith(".."): problems.append("%s link_target escapes payload root: %r -> %s" % (tag, lt, resolved))
            if "sha256" in e or "size" in e: problems.append("%s symlink must not carry sha256/size" % tag)
        else:
            if not SHA_RE.match(e.get("sha256", "")): problems.append("%s sha256 missing/not-64-hex" % tag)
            if not isinstance(e.get("size"), int) or e.get("size", 0) <= 0: problems.append("%s size invalid" % tag)
            if "link_target" in e and lt: problems.append("%s file must not carry link_target" % tag)

    if problems:
        for p in problems[:20]: print("manifest_schema=FAIL " + p)
        print("manifest_schema_overall=FAIL problems=%d" % len(problems))
        return 1
    print("manifest_schema=VALID entries=%d" % len(entries))
    print("manifest_schema_overall=PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
