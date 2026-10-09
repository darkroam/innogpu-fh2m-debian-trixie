#!/usr/bin/env python3
"""Deterministic release archive builder from artifact allowlists (HEAD-bound).

Supported artifacts (the only ones this tool can actually build):
- project-tools  — original GPL-3.0-or-later layer plus necessary upstream MIT
                   content (with the MIT notice); no drivers/vendor/debs/build.
- driver-source  — drivers/ paths that carry an explicit, redistributable
                   license declaration only; confidential and no-license paths
                   are excluded by the allowlist; the bundle is NOT a complete
                   driver and its README says so.

Every allowlist entry is read from the blob store of HEAD (the working tree
must be clean, so working == index == commit); the working tree is never read
for content. Release mode additionally reads the policy and the allowlist
file itself from HEAD. Symlink entries (git mode 120000) are rejected. Modes
are normalized from git (100644 -> 0644, 100755 -> 0755). The output is
written via a temp file with 0644 plus atomic rename, paths inside the
repository are refused, and the temp file is removed on failure.

Release builds run the full auditor clearance pipeline (import audit_licenses,
artifact_is_cleared) against the same policy: manually flipping status to
CLEARED cannot bypass the license/notice/allowlist validation. --draft is for
structural testing only and reports archive_draft=OK, never release-PASS
semantics.
"""

import argparse
import gzip
import hashlib
import io
import json
import os
import re
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path


def load_policy(root, policy_rel):
    path = root / policy_rel
    try:
        with path.open(encoding="utf-8") as stream:
            return json.load(stream)
    except (OSError, json.JSONDecodeError) as exc:
        raise SystemExit(f"archive_build=FAIL reason=policy_unreadable:{exc}")


def load_policy_from_head(root, policy_rel):
    raw = git(root, "show", f"HEAD:{policy_rel}")
    try:
        return json.loads(raw.decode("utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise SystemExit(f"archive_build=FAIL reason=policy_unreadable_from_head:{exc}")


def git(root, *args):
    try:
        proc = subprocess.run(
            ["git", "-C", str(root), *args],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        raise SystemExit(f"archive_build=FAIL reason=git_failed:{args[0]}:{exc}") from exc
    return proc.stdout


def working_tree_clean(root):
    status = git(root, "status", "--porcelain").decode("utf-8", "replace")
    return status.strip() == ""


def load_head_tree(root):
    raw = git(root, "ls-tree", "-r", "-z", "HEAD").decode("utf-8", "replace")
    tree = {}
    for item in raw.split("\x00"):
        if not item:
            continue
        meta, _, path = item.partition("	")
        parts = meta.split(" ")
        if len(parts) != 3:
            continue
        tree[path] = (parts[0], parts[2])
    return tree


def read_allowlist(root, rel, from_head):
    """Read the allowlist for an artifact.

    Release mode reads the allowlist file from the HEAD blob store
    (the clean-tree requirement makes it equal to the working tree);
    draft mode reads the working-tree copy for structural testing.
    """
    if from_head:
        try:
            raw = git(root, "show", f"HEAD:{rel}").decode("utf-8", "replace")
        except SystemExit:
            raise SystemExit(f"archive_build=FAIL reason=allowlist_unreadable_from_head:{rel}")
    else:
        try:
            raw = (root / rel).read_text(encoding="utf-8")
        except OSError:
            raise SystemExit(f"archive_build=FAIL reason=allowlist_unreadable:{rel}")
    listed = [line.strip() for line in raw.splitlines() if line.strip() and not line.lstrip().startswith("#")]
    if not listed:
        raise SystemExit(f"archive_build=FAIL reason=allowlist_empty:{rel}")
    return listed


def build_archive(root, artifact, policy, epoch, draft, from_head):
    status = artifact.get("status")
    if status not in {"BLOCKED", "CLEARED"}:
        raise SystemExit(f"archive_build=FAIL reason=artifact_status_invalid:{status}")
    if not working_tree_clean(root):
        raise SystemExit("archive_build=FAIL reason=working_tree_dirty")

    if not draft:
        # same clearance authority as the auditor CLI: full validation pipeline.
        # Under the clean-tree requirement policy/allowlist/blobs read from
        # HEAD are identical to the working tree, so the auditor's checks are
        # exactly the release checks.
        try:
            import importlib.util
            auditor_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "audit-licenses.py")
            spec = importlib.util.spec_from_file_location("audit_licenses", auditor_path)
            audit_licenses = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(audit_licenses)
        except Exception:
            raise SystemExit("archive_build=FAIL reason=auditor_unavailable")
        cleared, problems = audit_licenses.artifact_is_cleared(root, policy, artifact.get("_name", "project-tools"))
        if problems:
            raise SystemExit("archive_build=FAIL reason=audit_problems:" + ";".join(problems[:3]))
        if not cleared:
            raise SystemExit(f"archive_build=FAIL reason=artifact_not_cleared:{status}")

    allowlist_rel = artifact.get("allowlist_file")
    if not isinstance(allowlist_rel, str) or not allowlist_rel:
        raise SystemExit("archive_build=FAIL reason=allowlist_file_missing")
    allowlist = read_allowlist(root, allowlist_rel, from_head)
    if len(allowlist) != len(set(allowlist)):
        raise SystemExit("archive_build=FAIL reason=allowlist_duplicate")

    denylist = artifact.get("denylist")
    patterns = []
    if isinstance(denylist, list) and denylist:
        for pattern in denylist:
            try:
                patterns.append(re.compile(pattern))
            except re.error as exc:
                raise SystemExit(f"archive_build=FAIL reason=denylist_invalid:{pattern}:{exc}")

    tree = load_head_tree(root)
    entries = []
    for rel in allowlist:
        if rel not in tree:
            raise SystemExit(f"archive_build=FAIL reason=allowlist_absent_in_commit:{rel}")
        mode, sha = tree[rel]
        if mode == "120000":
            raise SystemExit(f"archive_build=FAIL reason=allowlist_symlink:{rel}")
        if mode not in ("100644", "100755"):
            raise SystemExit(f"archive_build=FAIL reason=allowlist_mode_invalid:{rel}:{mode}")
        if any(rx.search(rel) for rx in patterns):
            raise SystemExit(f"archive_build=FAIL reason=allowlist_leak:{rel}")
        entries.append((rel, mode, git(root, "cat-file", "blob", sha)))

    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode="w", format=tarfile.PAX_FORMAT) as tar:
        for rel, mode, data in sorted(entries, key=lambda item: item[0]):
            info = tarfile.TarInfo(rel)
            info.size = len(data)
            info.mtime = epoch
            info.mode = 0o755 if mode == "100755" else 0o644
            info.uid = 0
            info.gid = 0
            info.uname = "root"
            info.gname = "root"
            tar.addfile(info, io.BytesIO(data))
    compressed = io.BytesIO()
    with gzip.GzipFile(fileobj=compressed, mode="wb", compresslevel=9, mtime=0, filename="") as gz:
        gz.write(raw.getvalue())
    content = compressed.getvalue()
    digest = hashlib.sha256(content).hexdigest()
    with tarfile.open(fileobj=io.BytesIO(content), mode="r:gz") as tar:
        names = tar.getnames()
    if sorted(names) != sorted(allowlist):
        raise SystemExit("archive_build=FAIL reason=entry_mismatch")
    return content, digest, names


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--policy", default="license-audit-policy.json")
    parser.add_argument("--artifact", default="project-tools",
                        choices=("project-tools", "driver-source"))
    parser.add_argument("--out", required=True)
    parser.add_argument("--epoch", type=int, default=None)
    parser.add_argument("--draft", action="store_true",
                        help="structural test of BLOCKED artifacts; reports archive_draft=OK")
    args = parser.parse_args(argv)

    root = Path(args.root).resolve()
    from_head = not args.draft
    if from_head:
        policy = load_policy_from_head(root, args.policy)
    else:
        policy = load_policy(root, args.policy)
    artifact = policy.get("artifacts", {}).get(args.artifact)
    if not isinstance(artifact, dict):
        raise SystemExit(f"archive_build=FAIL reason=artifact_config_missing:{args.artifact}")
    artifact = dict(artifact)
    artifact["_name"] = args.artifact
    epoch = args.epoch
    if epoch is None:
        epoch = int(os.environ.get("SOURCE_DATE_EPOCH", policy.get("release_archive_epoch", 1787342400)))

    content, digest, names = build_archive(root, artifact, policy, epoch, args.draft, from_head)

    out = Path(args.out).resolve()
    if out == root or root in out.parents:
        raise SystemExit(f"archive_build=FAIL reason=output_inside_repo:{out}")
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp = tempfile.NamedTemporaryFile(
        dir=str(out.parent), prefix=".archive-", suffix=".tmp", delete=False
    )
    try:
        tmp.write(content)
        tmp.flush()
        os.fsync(tmp.fileno())
    finally:
        tmp.close()
    try:
        os.chmod(tmp.name, 0o644)
        os.replace(tmp.name, str(out))
    except OSError:
        try:
            os.unlink(tmp.name)
        except OSError:
            pass
        raise SystemExit("archive_build=FAIL reason=output_write_failed")

    if args.draft:
        print(f"archive_draft=OK entries={len(names)} sha256={digest}")
        return 0
    print(f"archive_build=PASS entries={len(names)} sha256={digest}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
