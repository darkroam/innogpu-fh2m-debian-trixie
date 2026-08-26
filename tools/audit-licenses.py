#!/usr/bin/env python3
"""Deterministically audit imported source and payload license metadata.

This tool verifies observable declarations and repository policy. A successful
mechanical audit does not make the repository releasable: release readiness is
reported separately from audit consistency.
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
    "source_origin",
    "modification_status",
    "patch_ids",
    "redistribution_review_state",
    "copyright_notices",
)
SPDX_LINE = re.compile(r"SPDX-License-Identifier:\s*([^\s*]+(?:\s+(?:AND|OR|WITH)\s+[^\s*]+)*)")
MODULE_LICENSE_RE = re.compile(r'MODULE_LICENSE\(\s*"([^"]+)"\s*\)')


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


def safe_repo_path(value, label):
    if not isinstance(value, str) or not value or Path(value).is_absolute() or ".." in Path(value).parts:
        raise AuditError(f"unsafe_{label}:{value!r}")
    return value


def list_tracked(root, source_root):
    try:
        proc = subprocess.run(
            ["git", "-C", str(root), "ls-files", "-z", "--", source_root],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        raise AuditError("git_ls_files_failed") from exc
    paths = [item.decode("utf-8") for item in proc.stdout.split(b"\0") if item]
    if not paths:
        raise AuditError("source_tree_empty")
    return sorted(paths)


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


def classify(path, data, policy, problems):
    text = data.decode("utf-8", errors="replace")
    project_paths = set(policy["project_owned_paths"])
    classes = policy["classifications"]
    confidential_paths = set(classes["strict_confidential"])
    bsd_paths = set(classes["bsd_lgpl_dual"])
    dual = policy["dual_mit_gpl"]
    declaration = dual["declaration"]
    references = dual["required_references"]

    if path in project_paths:
        return {
            "content_class": "project-document",
            "observed_declaration": "project-owned; root LICENSE scope",
            "normalized_spdx": "MIT",
            "license_references": "LICENSE",
            "redistribution_review_state": "project-license",
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
            "redistribution_review_state": "blocked-confidential-rights",
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
            "redistribution_review_state": "declared-rights-review-pending",
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
            "normalized_spdx": dual["normalized_spdx"],
            "license_references": ",".join(references),
            "redistribution_review_state": "declared-rights-review-pending",
        }

    spdx = SPDX_LINE.search(text)
    if spdx:
        expression = " ".join(spdx.group(1).split())
        return {
            "content_class": "standard-spdx",
            "observed_declaration": f"SPDX-License-Identifier: {expression}",
            "normalized_spdx": expression,
            "license_references": "-",
            "redistribution_review_state": "declared-rights-review-pending",
        }

    return {
        "content_class": "unclassified",
        "observed_declaration": "no mechanically recognized declaration",
        "normalized_spdx": "NOASSERTION",
        "license_references": "-",
        "redistribution_review_state": "blocked-unclassified",
    }


def validate_policy(root, policy):
    problems = []
    if policy.get("format_version") != 1:
        problems.append("unsupported_policy_format")
    if policy.get("release_status") not in {"BLOCKED", "CLEARED"}:
        problems.append("invalid_release_status")
    source_root = safe_repo_path(policy.get("source_root"), "source_root")
    for key in ("project_owned_paths", "required_license_texts", "release_blockers"):
        if not isinstance(policy.get(key), list):
            problems.append(f"policy_{key}_not_list")
    if not isinstance(policy.get("classifications"), dict):
        problems.append("policy_classifications_not_object")
    if not isinstance(policy.get("modified_paths"), dict):
        problems.append("policy_modified_paths_not_object")
    if not isinstance(policy.get("module_license_metadata"), dict):
        problems.append("policy_module_license_metadata_not_object")
    if not isinstance(policy.get("known_declaration_conflicts"), list):
        problems.append("policy_known_declaration_conflicts_not_list")
    if not isinstance(policy.get("expected_summary"), dict):
        problems.append("policy_expected_summary_not_object")

    listed = []
    for value in policy.get("project_owned_paths", []):
        listed.append(safe_repo_path(value, "project_owned_path"))
    for values in policy.get("classifications", {}).values():
        if not isinstance(values, list):
            problems.append("classification_allowlist_not_list")
            continue
        for value in values:
            listed.append(safe_repo_path(value, "classified_path"))
    if len(listed) != len(set(listed)):
        problems.append("classified_path_overlap")
    for value, patch_ids in policy.get("modified_paths", {}).items():
        safe_repo_path(value, "modified_path")
        if not isinstance(patch_ids, list) or not patch_ids or not all(
            isinstance(item, str) and re.fullmatch(r"patch-[0-9]{3}", item) for item in patch_ids
        ):
            problems.append(f"invalid_patch_ids:{value}")
    for value, metadata in policy.get("module_license_metadata", {}).items():
        safe_repo_path(value, "module_license_path")
        if not isinstance(metadata, str) or not metadata:
            problems.append(f"invalid_module_license_metadata:{value}")
    for value in policy.get("known_declaration_conflicts", []):
        safe_repo_path(value, "declaration_conflict_path")
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
        if not isinstance(item.get("spdx"), str) or not item["spdx"]:
            problems.append(f"required_license_text_spdx_missing:{rel}")
    return source_root, problems


def validate_manifest(root, policy):
    problems = []
    config = policy.get("manifest")
    if not isinstance(config, dict):
        return {}, ["manifest_policy_not_object"]
    path = root / safe_repo_path(config.get("path"), "manifest_path")
    manifest = load_json(path, "manifest")
    entries = manifest.get("entries")
    if not isinstance(entries, list):
        return {}, ["manifest_entries_not_list"]
    unresolved_marker = config.get("unresolved_marker")
    resolved = config.get("resolved_licenses")
    if not isinstance(unresolved_marker, str) or not unresolved_marker:
        problems.append("manifest_unresolved_marker_invalid")
    if not isinstance(resolved, dict):
        problems.append("manifest_resolved_licenses_not_object")
        resolved = {}

    unresolved = 0
    seen_sources = set()
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
        else:
            evidence = resolved.get(license_value)
            if not isinstance(evidence, dict):
                problems.append(f"manifest_license_without_policy_evidence:{source_path}:{license_value}")
                continue
            permitted = evidence.get("source_paths")
            evidence_path = evidence.get("evidence")
            if not isinstance(permitted, list) or source_path not in permitted:
                problems.append(f"manifest_license_path_not_evidenced:{source_path}:{license_value}")
            if not isinstance(evidence_path, str) or not (root / safe_repo_path(evidence_path, "evidence_path")).is_file():
                problems.append(f"manifest_license_evidence_missing:{source_path}:{license_value}")
        if source_path in seen_sources:
            problems.append(f"manifest_duplicate_source_path:{source_path}")
        seen_sources.add(source_path)
    return {"manifest_entries": len(entries), "manifest_unresolved": unresolved}, problems


def build_inventory(root, policy, tracked, problems):
    rows = []
    modified = policy["modified_paths"]
    project_paths = set(policy["project_owned_paths"])
    conflict_paths = set(policy["known_declaration_conflicts"])
    source_origin = policy["source_origin"]
    for path in tracked:
        target = root / path
        try:
            data = target.read_bytes()
        except OSError:
            problems.append(f"tracked_source_missing:{path}")
            continue
        details = classify(path, data, policy, problems)
        patch_ids = modified.get(path, [])
        metadata = module_license_metadata(data.decode("utf-8", errors="replace"))
        if path in conflict_paths:
            details["redistribution_review_state"] = "blocked-declaration-conflict"
        rows.append(
            {
                "path": path,
                "sha256": sha256_bytes(data),
                **details,
                "module_license_metadata": metadata,
                "source_origin": "project" if path in project_paths else source_origin,
                "modification_status": (
                    "project-owned" if path in project_paths else "project-modified" if patch_ids else "imported-unmodified"
                ),
                "patch_ids": ",".join(patch_ids) if patch_ids else "-",
                "copyright_notices": copyright_notices(data.decode("utf-8", errors="replace")),
            }
        )
    return rows


def inventory_text(rows):
    stream = io.StringIO(newline="")
    writer = csv.DictWriter(stream, fieldnames=INVENTORY_COLUMNS, delimiter="\t", lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)
    return stream.getvalue()


def summarize(rows, manifest_summary):
    class_to_key = {
        "project-document": "project_documents",
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
    summary["declaration_conflicts"] = sum(
        row["redistribution_review_state"] == "blocked-declaration-conflict" for row in rows
    )
    summary.update(manifest_summary)
    return summary


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--policy", default="license-audit-policy.json")
    parser.add_argument("--manifest")
    parser.add_argument("--inventory", default="docs/project/source-license-inventory.tsv")
    parser.add_argument("--write-inventory", action="store_true")
    parser.add_argument("--require-releasable", action="store_true")
    args = parser.parse_args(argv)

    root = Path(args.root).resolve()
    problems = []
    try:
        policy = load_json(root / args.policy, "policy")
        if args.manifest:
            policy.setdefault("manifest", {})["path"] = args.manifest
        source_root, policy_problems = validate_policy(root, policy)
        problems.extend(policy_problems)
        tracked = list_tracked(root, source_root)
        rows = build_inventory(root, policy, tracked, problems)
        manifest_summary, manifest_problems = validate_manifest(root, policy)
        problems.extend(manifest_problems)
        summary = summarize(rows, manifest_summary)
        expected = policy.get("expected_summary", {})
        for key, wanted in expected.items():
            if not isinstance(wanted, int) or wanted < 0:
                problems.append(f"invalid_expected_summary:{key}")
            elif summary.get(key) != wanted:
                problems.append(f"summary_mismatch:{key}:actual={summary.get(key)}:expected={wanted}")

        tracked_set = set(tracked)
        for path in policy.get("modified_paths", {}):
            if path not in tracked_set:
                problems.append(f"modified_path_not_tracked:{path}")
        for path in policy.get("project_owned_paths", []):
            if path not in tracked_set:
                problems.append(f"project_path_not_tracked:{path}")
        for values in policy.get("classifications", {}).values():
            for path in values:
                if path not in tracked_set:
                    problems.append(f"classified_path_not_tracked:{path}")
        observed_metadata = {
            row["path"]: row["module_license_metadata"]
            for row in rows
            if row["module_license_metadata"] != "-"
        }
        if observed_metadata != policy.get("module_license_metadata", {}):
            problems.append("module_license_metadata_set_changed")
        for path in policy.get("known_declaration_conflicts", []):
            if path not in tracked_set:
                problems.append(f"declaration_conflict_path_not_tracked:{path}")
            if path not in observed_metadata:
                problems.append(f"declaration_conflict_metadata_missing:{path}")

        rendered = inventory_text(rows)
        inventory_path = root / safe_repo_path(args.inventory, "inventory_path")
        if args.write_inventory and not problems:
            inventory_path.parent.mkdir(parents=True, exist_ok=True)
            inventory_path.write_text(rendered, encoding="utf-8", newline="")
        elif not args.write_inventory:
            try:
                committed = inventory_path.read_text(encoding="utf-8")
            except OSError:
                problems.append("inventory_missing")
            else:
                if committed != rendered:
                    problems.append("inventory_stale")

        if policy.get("release_status") == "CLEARED":
            if policy.get("release_blockers"):
                problems.append("cleared_policy_has_release_blockers")
            if (
                summary.get("strict_confidential")
                or summary.get("unclassified")
                or summary.get("declaration_conflicts")
                or summary.get("manifest_unresolved")
            ):
                problems.append("cleared_policy_has_unresolved_content")
    except (AuditError, KeyError, TypeError, ValueError) as exc:
        problems.append(str(exc))
        summary = {}
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
    print(f"license_release_gate={policy['release_status']}")
    if args.require_releasable and policy["release_status"] != "CLEARED":
        print("license_release_readiness=FAIL reason=release_gate_blocked")
        return 2
    print("license_release_readiness=PASS" if args.require_releasable else "license_release_readiness=NOT_REQUESTED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
