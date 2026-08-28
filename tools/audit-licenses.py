#!/usr/bin/env python3
"""Deterministic mechanical license audit for the three-layer model.

Layers
------
- original (GPL-3.0-or-later): the project's own framework, scripts, tools,
  tests, docs, config and auxiliary work (non-drivers tree + drivers/README.md).
- upstream_mit (MIT): content inherited from the Tim Hant fork
  (https://github.com/timhant/innogpu-fh2m-debian-trixie); its copyright and
  MIT text are preserved and never removed.
- drivers/ (per-file): imported vendor source; each file keeps the license
  declared in its own header. "Dual MIT/GPLv2" normalizes to
  `MIT OR GPL-2.0-only` (never GPLv3-only); BSD/LGPL dual stays as declared.
  `MODULE_LICENSE(...)` is module metadata, not a file license.
- local_payloads (not distributed): debs/, vendor/, build/, third_party/ and
  *.deb are never part of a public artifact.

Checks (all mechanical; the auditor is read-only against .git: only
`git ls-files`, `git ls-tree`, `git cat-file` and `git status` are used, never
`git write-tree` and never creating index.lock):

1. Required license texts exist with pinned SHA-256 (LICENSES/*).
2. Root LICENSE is GPL-3.0-or-later and carries an explicit original-layer
   scope marker; a scope that claims to cover drivers/ is rejected.
3. The upstream MIT notice is preserved: THIRD_PARTY_NOTICES.md and
   LICENSES/MIT.txt keep "Copyright (c) 2026 Tim Hant".
4. drivers/ per-file classification (explicit / strict-confidential /
   unclassified) matches the policy allowlists and expected counts.
5. Public artifact allowlists contain no confidential / NOASSERTION / vendor /
   deb content; every allowlist path is safe, unique, tracked, not a symlink,
   and bidirectionally complete against the mechanically derived set.
6. The authority doc (docs/project/licensing.md) carries the same release gate
   status as the policy.
7. Manifest license values are either the unresolved marker (`vendor-binary`,
   a provenance classification, never a license) or an exact allowed SPDX
   expression. Malformed expressions, `NOASSERTION`, and pending LicenseRef
   values are never releasable licenses.

status=CLEARED is never an authorization by itself: artifact clearance is only
reported after the whole mechanical pipeline above passes, and `--require-
releasable` additionally requires a clean working tree (working == index ==
commit) so policy, allowlists and file blobs are read from one commit.
"""

import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import re
import subprocess
import sys


INVENTORY_COLUMNS = (
    "path",
    "sha256",
    "content_class",
    "observed_declaration",
    "normalized_spdx",
    "license_references",
    "module_license_metadata",
    "copyright_notices",
)
ARTIFACT_NAMES = ("project-tools", "driver-source")
# Valid SPDX identifiers for license-text labels (a label is not a releasable
# expression; the pinned SHA-256 is the authoritative text check).
KNOWN_SPDX_IDENTIFIERS = {
    "MIT", "GPL-2.0-only", "GPL-2.0-or-later", "GPL-3.0-only", "GPL-3.0-or-later",
    "LGPL-2.1-only", "LGPL-2.1-or-later", "BSD-2-Clause", "BSD-3-Clause",
    "Apache-2.0", "MPL-2.0", "ISC", "CC0-1.0",
}
SPDX_LINE = re.compile(r"SPDX-License-Identifier:\s*([^\s*]+(?:\s+(?:AND|OR|WITH)\s+[^\s*]+)*)")
MODULE_LICENSE_RE = re.compile(r'MODULE_LICENSE\(\s*"([^"]+)"\s*\)')
GIT_BLOB_HASH_RE = re.compile(r"^[0-9a-f]{40}$|^[0-9a-f]{64}$")


class AuditError(Exception):
    pass


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def load_json(path, label):
    try:
        with path.open(encoding="utf-8") as stream:
            value = json.load(stream)
    except (OSError, json.JSONDecodeError) as exc:
        raise AuditError(f"{label}_unreadable:{exc}") from exc
    if not isinstance(value, dict):
        raise AuditError(f"{label}_not_object")
    return value


def git(root, *args):
    try:
        proc = subprocess.run(
            ["git", "-C", str(root), *args],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        raise AuditError(f"git_failed:{args[0]}:{exc}") from exc
    return proc.stdout


def safe_repo_path(value, label):
    if not isinstance(value, str) or not value or Path(value).is_absolute() or ".." in Path(value).parts:
        raise AuditError(f"unsafe_{label}:{value!r}")
    return value


def list_tracked(root, source_root=None):
    args = ["ls-files", "-z"]
    if source_root is not None:
        args += ["--", source_root]
    paths = [item.decode("utf-8") for item in git(root, *args).split(b"\0") if item]
    return sorted(paths)


def index_entries(root):
    """Return {path: (mode, blob_oid)} from the index.

    Read-only: `git ls-files --stage -z` never writes to .git and works in
    read-only checkouts.
    """
    entries = {}
    for item in git(root, "ls-files", "--stage", "-z").decode("utf-8", "replace").split("\0"):
        if not item:
            continue
        meta, _, path = item.partition("\t")
        parts = meta.split(" ")
        if len(parts) == 3:
            entries[path] = (parts[0], parts[1])
    return entries


def working_tree_clean(root):
    return git(root, "status", "--porcelain").decode("utf-8", "replace").strip() == ""


def copyright_notices(text):
    values = []
    for raw in text.splitlines():
        lowered = raw.lower()
        if "copyright" not in lowered:
            continue
        if "above copyright notice" in lowered or "copyright holders be liable" in lowered:
            continue
        cleaned = re.sub(r"^[\s/*#@-]+", "", raw).strip()
        if not cleaned:
            continue
        if cleaned not in values:
            values.append(cleaned)
        if len(values) == 4:
            break
    return " | ".join(values) if values else "-"


def module_license_metadata(text):
    values = []
    for value in MODULE_LICENSE_RE.findall(text):
        if value not in values:
            values.append(value)
    return ",".join(values) if values else "-"


def allowed_spdx(policy):
    values = policy.get("allowed_spdx_expressions")
    if not isinstance(values, list) or not values or not all(
        isinstance(item, str) and item for item in values
    ):
        raise AuditError("policy_allowed_spdx_invalid")
    return set(values)


def classify(path, data, policy, allowed, problems):
    """Classify one drivers/ path. Returns row details dict.

    Observation labels (NOASSERTION, LicenseRef-Strictly-Confidential) are
    never licenses; they only record that no releasable license was found.
    """
    text = data.decode("utf-8", errors="replace")
    project_paths = set(policy.get("project_owned_paths", []))
    classes = policy.get("classifications", {})
    confidential_paths = set(classes.get("strict_confidential", []))
    bsd_paths = set(classes.get("bsd_lgpl_dual", []))
    dual = policy.get("dual_mit_gpl", {})
    declaration = dual.get("declaration", "Dual MIT/GPLv2")
    references = dual.get("required_references", [])

    if path in project_paths:
        return {
            "content_class": "project-doc",
            "observed_declaration": "project-owned; root LICENSE scope (original layer)",
            "normalized_spdx": "GPL-3.0-or-later",
            "license_references": "LICENSE",
        }

    has_confidential = "Strictly Confidential" in text
    if path in confidential_paths:
        if not has_confidential:
            problems.append(f"confidential_marker_missing:{path}")
        return {
            "content_class": "strict-confidential",
            "observed_declaration": "Strictly Confidential",
            "normalized_spdx": "LicenseRef-Strictly-Confidential",
            "license_references": "-",
        }
    if has_confidential:
        problems.append(f"unallowlisted_confidential_file:{path}")

    if path in bsd_paths:
        required = (
            "distributed under BSD 3 clause and LGPL2.1 (dual license)",
            "SPDX License Identifier: BSD-3-Clause",
            "SPDX License Identifier: LGPL-2.1-only",
        )
        missing = [marker for marker in required if marker not in text]
        if missing:
            problems.append(f"bsd_lgpl_declaration_incomplete:{path}")
        return {
            "content_class": "bsd-lgpl-dual",
            "observed_declaration": "BSD 3 clause and LGPL2.1 (dual license)",
            "normalized_spdx": "BSD-3-Clause OR LGPL-2.1-only",
            "license_references": "BSD-3-Clause.txt,LGPL-2.1-only.txt",
        }
    if "distributed under BSD 3 clause and LGPL2.1" in text:
        problems.append(f"unallowlisted_bsd_lgpl_file:{path}")

    marker_state = [declaration in text] + [reference in text for reference in references]
    if any(marker_state):
        if not all(marker_state):
            problems.append(f"partial_dual_mit_gpl_declaration:{path}")
        return {
            "content_class": "dual-mit-gpl",
            "observed_declaration": declaration,
            "normalized_spdx": dual.get("normalized_spdx", "MIT OR GPL-2.0-only"),
            "license_references": ",".join(references),
        }

    spdx = SPDX_LINE.search(text)
    if spdx:
        expression = " ".join(spdx.group(1).split())
        if expression not in allowed:
            problems.append(f"spdx_expression_not_allowed:{path}:{expression}")
        return {
            "content_class": "standard-spdx",
            "observed_declaration": f"SPDX-License-Identifier: {expression}",
            "normalized_spdx": expression,
            "license_references": "-",
        }

    return {
        "content_class": "unclassified",
        "observed_declaration": "no mechanically recognized declaration",
        "normalized_spdx": "NOASSERTION",
        "license_references": "-",
    }


def validate_policy(policy, problems):
    if policy.get("format_version") != 2:
        problems.append("unsupported_policy_format")
    if policy.get("release_status") not in {"BLOCKED", "CLEARED"}:
        problems.append("invalid_release_status")
    for key in ("project_owned_paths", "required_license_texts", "module_license_metadata",
                "known_declaration_conflicts", "allowed_spdx_expressions"):
        if not isinstance(policy.get(key), (list, dict)) or policy.get(key) is None:
            problems.append(f"policy_{key}_invalid")
    for key in ("classifications", "dual_mit_gpl", "root_license", "upstream_mit_notice",
                "mit_license_text", "license_authority_doc", "artifacts", "manifest"):
        if not isinstance(policy.get(key), dict):
            problems.append(f"policy_{key}_missing")
    root_license = policy.get("root_license", {})
    if not isinstance(root_license.get("path"), str) or not root_license.get("path"):
        problems.append("root_license_path_missing")
    if not isinstance(root_license.get("scope_marker"), str) or not root_license["scope_marker"]:
        problems.append("root_license_scope_marker_missing")
    for key in ("upstream_mit_notice", "mit_license_text"):
        cfg = policy.get(key, {})
        if not isinstance(cfg.get("path"), str) or not cfg.get("path"):
            problems.append(f"{key}_path_missing")
        required = cfg.get("required_text")
        if not isinstance(required, list) or not required or not all(
            isinstance(item, str) and item for item in required
        ):
            problems.append(f"{key}_required_text_invalid")
    authority = policy.get("license_authority_doc", {})
    if not isinstance(authority.get("path"), str) or not authority.get("path"):
        problems.append("license_authority_doc_path_missing")
    if not isinstance(authority.get("status_marker_prefix"), str) or not authority["status_marker_prefix"]:
        problems.append("license_authority_doc_marker_invalid")
    try:
        allowed_spdx(policy)
    except AuditError as exc:
        problems.append(str(exc))
    layers = policy.get("layers", {})
    if not isinstance(layers, dict):
        problems.append("policy_layers_missing")
    else:
        allowed = set()
        try:
            allowed = allowed_spdx(policy)
        except AuditError:
            pass
        for name, layer in layers.items():
            if not isinstance(layer, dict):
                problems.append(f"layer_invalid:{name}")
                continue
            expr = layer.get("license")
            if name == "original" and expr not in allowed:
                problems.append(f"original_layer_license_invalid:{expr}")
            if name == "drivers" and expr == "GPL-3.0-or-later":
                problems.append("drivers_layer_must_not_be_gpl3")
            if name == "local_payloads" and expr not in ("not-distributed", "excluded"):
                problems.append(f"local_payloads_license_invalid:{expr}")

    listed = []
    for value in policy.get("project_owned_paths", []):
        try:
            listed.append(safe_repo_path(value, "project_owned_path"))
        except AuditError as exc:
            problems.append(str(exc))
    classes = policy.get("classifications", {})
    for values in classes.values():
        if not isinstance(values, list):
            problems.append("classification_allowlist_not_list")
            continue
        for value in values:
            try:
                listed.append(safe_repo_path(value, "classified_path"))
            except AuditError as exc:
                problems.append(str(exc))
    if len(listed) != len(set(listed)):
        problems.append("classified_path_overlap")
    for value in policy.get("module_license_metadata", {}):
        try:
            safe_repo_path(value, "module_license_path")
        except AuditError as exc:
            problems.append(str(exc))
    for value in policy.get("known_declaration_conflicts", []):
        try:
            safe_repo_path(value, "declaration_conflict_path")
        except AuditError as exc:
            problems.append(str(exc))
    non_drivers = policy.get("non_drivers_licenses")
    if not isinstance(non_drivers, dict) or not isinstance(non_drivers.get("paths"), dict):
        problems.append("policy_non_drivers_licenses_invalid")
    gate = policy.get("notice_gate")
    if not isinstance(gate, dict) or not isinstance(gate.get("entries"), list):
        problems.append("policy_notice_gate_invalid")


def _resolve_non_drivers_license(rel, original_roots, original, mapped):
    """Fail-closed per-path resolution for project-tools paths.

    Explicit path mapping wins; otherwise the path must live under an
    approved original root. Any other path is unknown and rejected — there is
    no global default license.
    """
    if rel in mapped:
        return mapped[rel]
    for prefix in original_roots:
        if rel.startswith(prefix):
            return original
    return None


def validate_non_drivers_licenses(root, policy, allowlist_paths, problems):
    """Fail-closed per-path license classification + path-bound NOTICE gate.

    Every project-tools allowlist path resolves to a license: an explicit
    entry in policy non_drivers_licenses.paths, or an approved original root
    (non_drivers_licenses.original_roots). Any path that matches neither is
    rejected (non_drivers_unclassified); there is no implicit default license,
    so adding a third-party file in an unknown location (e.g. external/) can
    never pass after a mere allowlist regeneration.

    The NOTICE gate binds markers to concrete path groups: every allowlist
    path whose license is not the exempt original expression and which is not
    a standard license text under LICENSES/ must belong to a notice_gate
    entry, and every entry whose paths appear in the allowlist must have all
    its markers present in THIRD_PARTY_NOTICES.md.
    """
    cfg = policy.get("non_drivers_licenses", {})
    if not isinstance(cfg, dict):
        problems.append("policy_non_drivers_licenses_missing")
        return
    try:
        allowed = allowed_spdx(policy)
    except AuditError as exc:
        problems.append(str(exc))
        return
    original = cfg.get("original", "GPL-3.0-or-later")
    if original not in allowed:
        problems.append(f"non_drivers_original_license_invalid:{original}")
    roots = cfg.get("original_roots")
    if not isinstance(roots, list) or not roots or not all(
        isinstance(item, str) and item.endswith("/") for item in roots
    ):
        problems.append("policy_non_drivers_original_roots_invalid")
        roots = []
    paths = cfg.get("paths")
    if not isinstance(paths, dict):
        problems.append("policy_non_drivers_paths_invalid")
        return
    mapped = {}
    for rel, expr in paths.items():
        try:
            rel = safe_repo_path(rel, "non_drivers_path")
        except AuditError as exc:
            problems.append(str(exc))
            continue
        if not isinstance(expr, str) or expr not in allowed:
            problems.append(f"non_drivers_license_invalid:{rel}:{expr}")
            continue
        if not (root / rel).is_file():
            problems.append(f"non_drivers_path_missing:{rel}")
            continue
        mapped[rel] = expr
    resolved = {}
    for rel in allowlist_paths:
        expr = _resolve_non_drivers_license(rel, roots, original, mapped)
        if expr is None:
            problems.append(f"non_drivers_unclassified:{rel}")
            continue
        resolved[rel] = expr

    gate = policy.get("notice_gate", {})
    notice_rel = gate.get("path")
    if not isinstance(notice_rel, str) or not notice_rel:
        problems.append("notice_gate_path_missing")
        return
    try:
        notice_text = (root / safe_repo_path(notice_rel, "notice_gate_path")).read_text(encoding="utf-8")
    except OSError:
        problems.append(f"notice_gate_missing:{notice_rel}")
        return
    exempt = gate.get("exempt_expression", "GPL-3.0-or-later")
    standard_root = gate.get("standard_text_root", "LICENSES/")
    entries = gate.get("entries")
    if not isinstance(entries, list):
        problems.append("notice_gate_entries_invalid")
        return
    covered = set()
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            problems.append(f"notice_gate_entry_invalid:{index}")
            continue
        entry_paths = entry.get("paths")
        markers = entry.get("markers")
        if not isinstance(entry_paths, list) or not entry_paths or not all(
            isinstance(item, str) and item for item in entry_paths
        ):
            problems.append(f"notice_gate_entry_paths_invalid:{index}")
            continue
        if not isinstance(markers, list) or not markers or not all(
            isinstance(item, str) and item for item in markers
        ):
            problems.append(f"notice_gate_entry_markers_invalid:{index}")
            continue
        key = entry_paths[0]
        if any(p in allowlist_paths for p in entry_paths):
            for marker in markers:
                if marker not in notice_text:
                    problems.append(f"notice_gate_marker_missing:{key}:{marker}")
        covered.update(entry_paths)
    # every non-exempt, non-standard-text allowlist path needs a gate entry
    for rel, expr in resolved.items():
        if expr == exempt or rel.startswith(standard_root):
            continue
        if rel not in covered:
            problems.append(f"notice_gate_entry_missing:{rel}")


def validate_license_texts(root, policy, problems):
    for item in policy.get("required_license_texts", []):
        if not isinstance(item, dict):
            problems.append("license_text_entry_not_object")
            continue
        rel = safe_repo_path(item.get("path"), "license_text_path")
        target = root / rel
        try:
            actual = sha256_bytes(target.read_bytes())
        except OSError:
            problems.append(f"required_license_text_missing:{rel}")
            continue
        if actual != item.get("sha256"):
            problems.append(f"required_license_text_hash_mismatch:{rel}")
        if not isinstance(item.get("spdx"), str) or item["spdx"] not in KNOWN_SPDX_IDENTIFIERS:
            problems.append(f"required_license_text_spdx_invalid:{rel}")


def validate_root_license(root, policy, problems):
    cfg = policy.get("root_license", {})
    rel = safe_repo_path(cfg.get("path"), "root_license_path")
    target = root / rel
    try:
        text = target.read_text(encoding="utf-8")
    except OSError:
        problems.append(f"root_license_missing:{rel}")
        return
    if cfg.get("scope_marker") not in text:
        problems.append(f"root_license_scope_invalid:{rel}")
    for marker in cfg.get("required_markers", []):
        if marker not in text:
            problems.append(f"root_license_marker_missing:{rel}:{marker}")


def validate_upstream_mit(root, policy, problems):
    for key in ("upstream_mit_notice", "mit_license_text"):
        cfg = policy.get(key, {})
        rel = safe_repo_path(cfg.get("path"), f"{key}_path")
        target = root / rel
        try:
            text = target.read_text(encoding="utf-8")
        except OSError:
            problems.append(f"{key}_missing:{rel}")
            continue
        for marker in cfg.get("required_text", []):
            if marker not in text:
                problems.append(f"{key}_marker_missing:{rel}:{marker}")


def validate_manifest(root, policy, problems):
    cfg = policy.get("manifest", {})
    path = root / safe_repo_path(cfg.get("path"), "manifest_path")
    manifest = load_json(path, "manifest")
    entries = manifest.get("entries")
    if not isinstance(entries, list):
        problems.append("manifest_entries_not_list")
        return {"manifest_entries": 0, "manifest_unresolved": 0}
    unresolved_marker = cfg.get("unresolved_marker", "vendor-binary")
    allowed = allowed_spdx(policy) | {unresolved_marker}
    unresolved = 0
    seen = set()
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            problems.append(f"manifest_entry_not_object:{index}")
            continue
        source_path = entry.get("source_path")
        license_value = entry.get("license")
        if not isinstance(license_value, str) or not license_value.strip():
            problems.append(f"manifest_license_missing:{index}")
            continue
        if license_value == unresolved_marker:
            unresolved += 1
        elif license_value not in allowed:
            # vendor-binary / NOASSERTION / malformed expressions are never
            # acceptable as a manifest license resolution.
            problems.append(f"manifest_license_invalid:{source_path}:{license_value}")
        if not isinstance(source_path, str) or not source_path:
            problems.append(f"manifest_source_path_missing:{index}")
        elif source_path in seen:
            problems.append(f"manifest_duplicate_source_path:{source_path}")
        seen.add(source_path)
    return {"manifest_entries": len(entries), "manifest_unresolved": unresolved}


def validate_authority_doc(root, policy, problems):
    cfg = policy.get("license_authority_doc", {})
    rel = safe_repo_path(cfg.get("path"), "authority_doc_path")
    target = root / rel
    try:
        text = target.read_text(encoding="utf-8")
    except OSError:
        problems.append(f"authority_doc_missing:{rel}")
        return
    prefix = cfg.get("status_marker_prefix", "license_release_gate=")
    expected = f"{prefix}{policy.get('release_status')}"
    if text.count(expected) != 1:
        problems.append(
            f"authority_doc_status_mismatch:{rel}:expected={expected}:count={text.count(expected)}"
        )


def build_inventory(root, policy, tracked, problems):
    allowed = allowed_spdx(policy)
    project_paths = set(policy.get("project_owned_paths", []))
    conflict_paths = set(policy.get("known_declaration_conflicts", []))
    rows = []
    for path in tracked:
        target = root / path
        try:
            data = target.read_bytes()
        except OSError:
            problems.append(f"tracked_source_missing:{path}")
            continue
        details = classify(path, data, policy, allowed, problems)
        if path in conflict_paths:
            details["observed_declaration"] += " (MODULE_LICENSE conflict)"
        rows.append(
            {
                "path": path,
                "sha256": sha256_bytes(data),
                **details,
                "module_license_metadata": module_license_metadata(data.decode("utf-8", errors="replace")),
                "copyright_notices": copyright_notices(data.decode("utf-8", errors="replace")),
            }
        )
    for path in project_paths:
        if path not in set(tracked):
            problems.append(f"project_path_not_tracked:{path}")
    for values in policy.get("classifications", {}).values():
        for path in values:
            if path not in set(tracked):
                problems.append(f"classified_path_not_tracked:{path}")
    return rows


def summarize(rows, manifest_summary, conflict_paths):
    class_to_key = {
        "project-doc": "project_documents",
        "dual-mit-gpl": "dual_mit_gpl",
        "strict-confidential": "strict_confidential",
        "bsd-lgpl-dual": "bsd_lgpl_dual",
        "standard-spdx": "standard_spdx",
        "unclassified": "unclassified",
    }
    summary = {value: 0 for value in class_to_key.values()}
    for row in rows:
        summary[class_to_key[row["content_class"]]] += 1
    summary["tracked_paths"] = len(rows)
    summary["implementation_paths"] = len(rows) - summary["project_documents"]
    summary["module_license_files"] = sum(row["module_license_metadata"] != "-" for row in rows)
    summary["declaration_conflicts"] = sum(row["path"] in conflict_paths for row in rows)
    summary.update(manifest_summary)
    return summary


def inventory_text(rows):
    stream = io.StringIO(newline="")
    writer = csv.DictWriter(stream, fieldnames=INVENTORY_COLUMNS, delimiter="\t", lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)
    return stream.getvalue()


def read_allowlist(root, rel, problems):
    path = root / safe_repo_path(rel, "allowlist_path")
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError:
        problems.append(f"allowlist_unreadable:{rel}")
        return None
    listed = [line.strip() for line in lines if line.strip() and not line.lstrip().startswith("#")]
    if not listed:
        problems.append(f"allowlist_empty:{rel}")
        return None
    return listed


def validate_allowlist(root, policy, name, inventory_rows, all_tracked, index, problems):
    cfg = policy.get("artifacts", {}).get(name, {})
    rel = cfg.get("allowlist_file")
    if not isinstance(rel, str) or not rel:
        problems.append(f"{name}_allowlist_path_missing")
        return
    listed = read_allowlist(root, rel, problems)
    if listed is None:
        return
    tracked_set = set(all_tracked)
    seen = set()
    for rel_path in listed:
        try:
            rel_path = safe_repo_path(rel_path, "allowlist_path")
        except AuditError as exc:
            problems.append(str(exc))
            continue
        if rel_path in seen:
            problems.append(f"allowlist_duplicate:{name}:{rel_path}")
        seen.add(rel_path)
        entry = index.get(rel_path)
        if entry is None:
            problems.append(f"allowlist_untracked:{name}:{rel_path}")
            continue
        mode = entry[0]
        if mode == "120000":
            problems.append(f"allowlist_symlink:{name}:{rel_path}")

    if name == "project-tools":
        patterns = []
        for pattern in cfg.get("denylist", []):
            try:
                patterns.append(re.compile(pattern))
            except re.error:
                problems.append(f"denylist_invalid:{name}:{pattern}")
        for rel_path in seen:
            if any(rx.search(rel_path) for rx in patterns):
                problems.append(f"allowlist_leak:{name}:{rel_path}")
        self_rel = safe_repo_path(rel, "allowlist_path")
        expected = {
            p for p in all_tracked
            if not p.startswith("drivers/")
            and not p.startswith("patches/")
            and not p.startswith("debs/")
        }
        expected.discard(self_rel)
        if seen != expected:
            missing = sorted(expected - seen)
            extra = sorted(seen - expected)
            problems.append(
                "allowlist_incomplete:{}:missing={}:extra={}".format(
                    name, ",".join(missing[:5]) or "-", ",".join(extra[:5]) or "-"
                )
            )
        validate_non_drivers_licenses(root, policy, sorted(seen), problems)
    elif name == "driver-source":
        expected = {
            row["path"]
            for row in inventory_rows
            if row["content_class"] in ("dual-mit-gpl", "bsd-lgpl-dual", "project-doc")
        }
        if seen != expected:
            missing = sorted(expected - seen)
            extra = sorted(seen - expected)
            problems.append(
                "allowlist_incomplete:{}:missing={}:extra={}".format(
                    name, ",".join(missing[:5]) or "-", ",".join(extra[:5]) or "-"
                )
            )
        for rel_path in seen:
            row = next((r for r in inventory_rows if r["path"] == rel_path), None)
            if row is None or row["content_class"] in ("strict-confidential", "unclassified"):
                problems.append(f"driver_source_restricted_leak:{rel_path}")


def validate_artifacts(root, policy, inventory_rows, all_tracked, index, problems):
    artifacts = policy.get("artifacts", {})
    if not isinstance(artifacts, dict):
        problems.append("policy_artifacts_missing")
        return
    if set(artifacts.keys()) != set(ARTIFACT_NAMES):
        extra = sorted(set(artifacts.keys()) - set(ARTIFACT_NAMES))
        missing = sorted(set(ARTIFACT_NAMES) - set(artifacts.keys()))
        problems.append(
            "artifacts_key_mismatch:extra={}:missing={}".format(
                ",".join(extra) or "-", ",".join(missing) or "-"
            )
        )
    for name in ARTIFACT_NAMES:
        config = artifacts.get(name)
        if not isinstance(config, dict):
            problems.append(f"artifact_config_missing:{name}")
            continue
        status = config.get("status")
        if status not in {"BLOCKED", "CLEARED"}:
            problems.append(f"artifact_status_invalid:{name}:{status}")
        if not isinstance(config.get("reason"), str) or not config["reason"]:
            problems.append(f"artifact_reason_missing:{name}")
        validate_allowlist(root, policy, name, inventory_rows, all_tracked, index, problems)


def run_full_audit(root, policy, inventory_rel, write_inventory, check_inventory=True):
    """Complete mechanical audit pipeline; the single authority for the CLI
    and the release archive builder (artifact_is_cleared)."""
    problems = []
    summary = {}
    try:
        validate_policy(policy, problems)
        validate_license_texts(root, policy, problems)
        validate_root_license(root, policy, problems)
        validate_upstream_mit(root, policy, problems)
        manifest_summary = validate_manifest(root, policy, problems)
        validate_authority_doc(root, policy, problems)
        tracked = list_tracked(root, policy.get("source_root", "drivers"))
        all_tracked = list_tracked(root)
        rows = build_inventory(root, policy, tracked, problems)
        index = index_entries(root)
        validate_artifacts(root, policy, rows, all_tracked, index, problems)
        summary = summarize(rows, manifest_summary, set(policy.get("known_declaration_conflicts", [])))
        expected = policy.get("expected_summary", {})
        for key, wanted in expected.items():
            if not isinstance(wanted, int) or wanted < 0:
                problems.append(f"invalid_expected_summary:{key}")
            elif summary.get(key) != wanted:
                problems.append(f"summary_mismatch:{key}:actual={summary.get(key)}:expected={wanted}")
        observed_metadata = {
            row["path"]: row["module_license_metadata"]
            for row in rows if row["module_license_metadata"] != "-"
        }
        if observed_metadata != policy.get("module_license_metadata", {}):
            problems.append("module_license_metadata_set_changed")
        conflict_paths = set(policy.get("known_declaration_conflicts", []))
        observed_conflicts = {row["path"] for row in rows if "MODULE_LICENSE conflict" in row["observed_declaration"]}
        if observed_conflicts != conflict_paths:
            problems.append("declaration_conflict_set_changed")
        rendered = inventory_text(rows)
        inventory_path = root / safe_repo_path(inventory_rel, "inventory_path")
        if write_inventory and not problems:
            inventory_path.parent.mkdir(parents=True, exist_ok=True)
            inventory_path.write_text(rendered, encoding="utf-8", newline="")
        elif not write_inventory and check_inventory:
            try:
                committed = inventory_path.read_text(encoding="utf-8")
            except OSError:
                problems.append("inventory_missing")
            else:
                if committed != rendered:
                    problems.append("inventory_stale")
    except (AuditError, KeyError, TypeError, ValueError) as exc:
        problems.append(str(exc))
        summary = {}
    return problems, summary


def write_allowlists(root, policy, inventory_rows, all_tracked, problems):
    """Write both artifact allowlists deterministically from the audit state."""
    for name in ARTIFACT_NAMES:
        cfg = policy.get("artifacts", {}).get(name, {})
        rel = cfg.get("allowlist_file")
        if not isinstance(rel, str) or not rel:
            problems.append(f"{name}_allowlist_path_missing")
            continue
        path = root / safe_repo_path(rel, "allowlist_path")
        if name == "project-tools":
            self_rel = safe_repo_path(rel, "allowlist_path")
            paths = sorted(
                p for p in all_tracked
                if not p.startswith("drivers/")
                and not p.startswith("patches/")
                and not p.startswith("debs/")
                and p != self_rel
            )
            header = (
                "# project-tools 候选制品允许清单（按权利边界生成）：原创层 GPL-3.0-or-later + "
                "上游继承层 MIT + components/ 第三方派生分类；"
                "不含 patches/、debs/、drivers/、vendor/、build/、third_party/"
            )
        else:
            paths = sorted(
                row["path"]
                for row in inventory_rows
                if row["content_class"] in ("dual-mit-gpl", "bsd-lgpl-dual", "project-doc")
            )
            header = "# driver-source 允许清单：drivers/ 中具有明确许可声明的路径（confidential 与无许可路径已排除）"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(header + "\n" + "\n".join(paths) + "\n", encoding="utf-8")


def artifact_is_cleared(root, policy, artifact_name):
    """Clearance via the exact same pipeline as the CLI.

    The committed-inventory comparison is enforced by CLI/CI runs
    (--require-releasable is only meaningful on a clean commit); the builder
    checks everything else against the same policy/allowlists/classification.
    """
    problems, _ = run_full_audit(
        root, policy, "docs/project/source-license-inventory.tsv", False, check_inventory=False
    )
    if problems:
        return False, problems
    status = policy.get("artifacts", {}).get(artifact_name, {}).get("status")
    return status == "CLEARED", []


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--policy", default="license-audit-policy.json")
    parser.add_argument("--inventory", default="docs/project/source-license-inventory.tsv")
    parser.add_argument("--write-inventory", action="store_true")
    parser.add_argument("--write-allowlists", action="store_true")
    parser.add_argument("--require-releasable", action="store_true")
    parser.add_argument("--artifact", choices=list(ARTIFACT_NAMES), default="project-tools")
    args = parser.parse_args(argv)

    root = Path(args.root).resolve()
    problems = []
    summary = {}
    try:
        policy = load_json(root / args.policy, "policy")
        if args.write_allowlists:
            # Generate allowlists from the audit state first, then validate
            # them in the full pipeline below (avoids missing-file failures).
            tracked = list_tracked(root)
            source_tracked = list_tracked(root, policy.get("source_root", "drivers"))
            all_rows = build_inventory(root, policy, source_tracked, [])
            write_allowlists(root, policy, all_rows, tracked, problems)
        problems, summary = run_full_audit(root, policy, args.inventory, args.write_inventory)
    except (AuditError, KeyError, TypeError, ValueError) as exc:
        problems.append(str(exc))
        policy = {"release_status": "UNKNOWN"}

    if problems:
        for problem in problems:
            print(f"license_audit=FAIL reason={problem}")
        print(f"license_audit_overall=FAIL problems={len(problems)}")
        return 1

    for key in (
        "tracked_paths",
        "project_documents",
        "implementation_paths",
        "dual_mit_gpl",
        "strict_confidential",
        "bsd_lgpl_dual",
        "standard_spdx",
        "unclassified",
        "module_license_files",
        "declaration_conflicts",
        "manifest_entries",
        "manifest_unresolved",
    ):
        print(f"license_{key}={summary.get(key, 0)}")
    print("license_audit_overall=PASS")
    print(f"license_release_gate={policy.get('release_status')}")
    for name in ARTIFACT_NAMES:
        status = policy.get("artifacts", {}).get(name, {}).get("status", "UNKNOWN")
        print(f"license_artifact_{name}_gate={status}")
    if args.require_releasable:
        status = policy.get("artifacts", {}).get(args.artifact, {}).get("status", "UNKNOWN")
        if status != "CLEARED":
            print(f"license_release_readiness=FAIL reason=artifact_{args.artifact}_gate_blocked")
            return 2
        if not working_tree_clean(root):
            print("license_release_readiness=FAIL reason=release_tree_not_clean")
            return 2
        print("license_release_readiness=PASS")
        return 0
    print("license_release_readiness=NOT_REQUESTED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
