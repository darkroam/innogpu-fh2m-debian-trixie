#!/usr/bin/env python3
"""Finalize a failed suspend round after evidence and rollback verification."""

from __future__ import annotations

import argparse
import os
import re
import stat
import sys
from pathlib import Path


SAFE_NAME = re.compile(r"^[a-z0-9][a-z0-9._-]*$")
REQUIRED_MARKERS = (
    "failure-evidence-captured",
    "rollback-verified",
    "reboot-verified",
)


def fail(message: str) -> None:
    print(f"failure_finalize=FAIL reason={message}", file=sys.stderr)
    raise SystemExit(1)


def read_regular_at(dir_fd: int, name: str, description: str) -> str:
    flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
    try:
        fd = os.open(name, flags, dir_fd=dir_fd)
    except OSError as exc:
        fail(f"{description}_open_failed path={name} error={exc}")
    try:
        if not stat.S_ISREG(os.fstat(fd).st_mode):
            fail(f"{description}_must_be_regular_file path={name}")
        with os.fdopen(os.dup(fd), encoding="utf-8") as stream:
            return stream.read()
    finally:
        os.close(fd)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Remove one active pointer only after failed-round evidence, rollback, "
            "and reboot markers have all been verified."
        )
    )
    parser.add_argument("--state-root", required=True, type=Path)
    parser.add_argument("--active-name", required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = args.state_root
    if not root.is_absolute():
        fail("state_root_must_be_absolute")
    if root == Path("/") or root.resolve(strict=False) != root:
        fail("state_root_must_be_canonical_and_not_root")
    if root.is_symlink() or not root.is_dir():
        fail("state_root_must_be_real_directory")
    if not SAFE_NAME.fullmatch(args.active_name) or not args.active_name.startswith("active-"):
        fail("invalid_active_name")

    root_fd = os.open(root, os.O_RDONLY | os.O_DIRECTORY | getattr(os, "O_NOFOLLOW", 0))
    try:
        lines = read_regular_at(root_fd, args.active_name, "active_pointer").splitlines()
        if len(lines) != 1 or not SAFE_NAME.fullmatch(lines[0]):
            fail("invalid_active_pointer_content")

        round_id = lines[0]
        try:
            evidence_fd = os.open(
                round_id,
                os.O_RDONLY | os.O_DIRECTORY | getattr(os, "O_NOFOLLOW", 0),
                dir_fd=root_fd,
            )
        except OSError as exc:
            fail(f"evidence_directory_open_failed path={root / round_id} error={exc}")

        try:
            expected = f"round_id={round_id}\nstatus=PASS\n"
            for name in REQUIRED_MARKERS:
                value = read_regular_at(evidence_fd, name, name.replace("-", "_"))
                if value != expected:
                    fail(f"invalid_marker path={root / round_id / name}")

            try:
                finalized_fd = os.open(
                    "failure-finalized",
                    os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_NOFOLLOW", 0),
                    0o600,
                    dir_fd=evidence_fd,
                )
            except OSError as exc:
                fail(f"finalized_marker_create_failed path={root / round_id / 'failure-finalized'} error={exc}")
            try:
                os.write(
                    finalized_fd,
                    f"round_id={round_id}\nstatus=FINALIZED\n".encode("utf-8"),
                )
                os.fsync(finalized_fd)
            finally:
                os.close(finalized_fd)
            os.fsync(evidence_fd)

            try:
                os.unlink(args.active_name, dir_fd=root_fd)
            except OSError as exc:
                os.unlink("failure-finalized", dir_fd=evidence_fd)
                os.fsync(evidence_fd)
                fail(f"active_pointer_remove_failed error={exc}")
            os.fsync(root_fd)
        finally:
            os.close(evidence_fd)
    finally:
        os.close(root_fd)

    print(f"failure_finalize=PASS round_id={round_id} evidence={root / round_id}")
    print("failure_evidence_preserved=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
