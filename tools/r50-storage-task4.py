#!/usr/bin/env python3
"""R50 task 4: inventory, physical-TTY approval, and exact build/ migration."""

import csv
import datetime as dt
import hashlib
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
ROUND_ROOT = ROOT / ".build/r50-system-cleanup-20261001-01"
PLAN = ROUND_ROOT / "task4-migration-plan.tsv"
EVIDENCE = ROUND_ROOT / "task4-migration"
PLAN_SHA = "ebf74915b3171bd4034bf55f0328515de70066e1016cd5a549490acd030c3559"
FIELDS = ("source", "target", "action", "type", "bytes", "file_count",
          "content_sha256", "metadata_sha256", "basis")


def fail(message):
    raise SystemExit(f"FAIL: {message}")


def sha_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sha_bytes(data):
    return hashlib.sha256(data).hexdigest()


def safe_relative(value, prefixes):
    path = Path(value)
    if path.is_absolute() or ".." in path.parts or not path.parts or path.parts[0] not in prefixes:
        fail(f"unsafe path: {value}")
    return ROOT / path


def plan_rows():
    if sha_file(PLAN) != PLAN_SHA:
        fail("migration plan identity drift")
    with PLAN.open(newline="") as stream:
        rows = list(csv.DictReader(stream, delimiter="\t"))
    if len(rows) != 45 or sum(r["action"] == "move" for r in rows) != 43:
        fail("migration plan cardinality drift")
    if sum(r["action"] == "remove-empty" for r in rows) != 2:
        fail("remove-empty cardinality drift")
    if len({r["source"] for r in rows}) != 45:
        fail("duplicate source in migration plan")
    targets = [r["target"] for r in rows if r["action"] == "move"]
    if len(set(targets)) != 43:
        fail("duplicate target in migration plan")
    return rows


def walk_rows(path):
    """Yield deterministic lstat rows without following symlinks."""
    def visit(current, relative):
        info = current.lstat()
        mode = info.st_mode
        if stat.S_ISREG(mode):
            kind, target, content = "f", "", sha_file(current)
        elif stat.S_ISDIR(mode):
            kind, target, content = "d", "", ""
        elif stat.S_ISLNK(mode):
            kind, target, content = "l", os.readlink(current), ""
        else:
            fail(f"unsupported file type: {current.relative_to(ROOT)}")
        yield (kind, relative, info, target, content)
        if kind == "d":
            for child in sorted(current.iterdir(), key=lambda item: os.fsencode(item.name)):
                child_rel = child.name if relative == "." else f"{relative}/{child.name}"
                yield from visit(child, child_rel)
    yield from visit(path, ".")


def object_identity(path):
    if path.is_symlink() or not path.exists():
        fail(f"source missing or top-level symlink: {path.relative_to(ROOT)}")
    content = hashlib.sha256()
    metadata = hashlib.sha256()
    total = files = 0
    root_kind = None
    direct_file_sha = None
    for kind, relative, info, target, file_sha in walk_rows(path):
        root_kind = root_kind or {"f": "file", "d": "directory"}.get(kind, "symlink")
        total += info.st_size
        if kind == "f":
            files += 1
            if relative == ".":
                direct_file_sha = file_sha
        content.update(f"{kind}\t{relative}\t{target}\t{file_sha}\n".encode())
        metadata.update(
            f"{kind}\t{relative}\t{stat.S_IMODE(info.st_mode):o}\t{info.st_uid}\t{info.st_gid}\t"
            f"{info.st_size}\t{info.st_mtime_ns}\t{target}\n".encode()
        )
    return {
        "type": root_kind,
        "bytes": str(total),
        "file_count": str(files),
        "content_sha256": direct_file_sha or content.hexdigest(),
        "metadata_sha256": metadata.hexdigest(),
    }


def validate_layout(rows):
    build_top = {entry.name for entry in (ROOT / "build").iterdir()}
    planned_top = {Path(r["source"]).parts[1] for r in rows}
    if build_top != planned_top:
        fail(f"build top-level coverage drift missing={sorted(build_top-planned_top)} extra={sorted(planned_top-build_top)}")
    for base in (ROOT / "build", ROOT / ".build", ROOT / "debs"):
        if base.stat().st_dev != (ROOT / "build").stat().st_dev:
            fail("migration roots are not on one filesystem")


def validate_consumers():
    command = [
        "rg", "-n", r"(^|[^[:alnum:]_.-])build/(r16|p2-manifest|innogpu-fh2m|fantgpu-fh2m)|\$ROOT/build/",
        "scripts", "tools", "--glob", "!README.md", "--glob", "!r50-storage-task3.sh",
        "--glob", "!r50-storage-task4.py",
    ]
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, check=False)
    if result.returncode not in (0, 1):
        fail(f"consumer scan failed: {result.stderr.strip()}")
    if result.stdout:
        fail(f"active legacy build/ consumer remains:\n{result.stdout.rstrip()}")


def collect_inventory():
    rows = plan_rows()
    validate_layout(rows)
    validate_consumers()
    result = []
    for row in rows:
        source = safe_relative(row["source"], {"build"})
        if row["action"] == "move":
            target = safe_relative(row["target"], {".build", "debs"})
            if target.exists() or target.is_symlink():
                fail(f"target already exists: {row['target']}")
        elif row["action"] != "remove-empty" or row["target"] != "-":
            fail(f"invalid action: {row['action']}")
        identity = object_identity(source)
        result.append({**row, **identity})
    return result


def write_inventory(owner_uid=None, owner_gid=None):
    rows = collect_inventory()
    stamp = dt.datetime.now().astimezone().strftime("%Y%m%d-%H%M%S")
    output = EVIDENCE / f"inventory-{stamp}"
    output.mkdir(parents=True)
    manifest = output / "migration-manifest.tsv"
    with manifest.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    total = sum(int(r["bytes"]) for r in rows if r["action"] == "move")
    (output / "summary.txt").write_text(
        f"plan_sha256={PLAN_SHA}\nmanifest_sha256={sha_file(manifest)}\n"
        f"moves=43\nremove_empty=2\nmove_bytes={total}\nbuild_top_level_coverage=43/43\n"
        "same_filesystem=PASS\ntargets_absent=PASS\nactive_legacy_consumers=0\n",
        encoding="utf-8",
    )
    (output / "consumer-scan.txt").write_text(
        "PASS: no active script/tool defaults consume legacy build/ paths\n"
        "Exclusions: R50 task3 historical cleanup definitions and README/history prose.\n",
        encoding="utf-8",
    )
    (output / "migration-manifest.tsv.sha256").write_text(
        f"{sha_file(manifest)}  {manifest}\n", encoding="utf-8"
    )
    if owner_uid is not None:
        for path in [EVIDENCE, output, *output.iterdir()]:
            os.chown(path, owner_uid, owner_gid)
    return output


def inventory_as_user():
    if os.geteuid() == 0:
        fail("start as the normal user, not root")
    command = ["sudo", str(Path(__file__).resolve()), "--inventory-root", str(os.getuid()), str(os.getgid())]
    output = Path(subprocess.check_output(command, text=True).strip().splitlines()[-1])
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    pointer = EVIDENCE / "latest-inventory.txt"
    with tempfile.NamedTemporaryFile("w", dir=EVIDENCE, delete=False) as stream:
        stream.write(f"{output}\n")
        temporary = Path(stream.name)
    temporary.chmod(0o644)
    temporary.replace(pointer)
    return output


def show_inventory(output):
    print((output / "summary.txt").read_text(), end="")
    with (output / "migration-manifest.tsv").open(newline="") as stream:
        for row in csv.DictReader(stream, delimiter="\t"):
            print(f"[{row['action']}] {row['source']} -> {row['target']} ({int(row['bytes']) / 1e6:.2f} MB)")
    print(f"inventory={output}")


def confirm():
    if os.geteuid() == 0:
        fail("start as the normal user, not root")
    tty = os.ttyname(sys.stdin.fileno())
    if not tty.startswith("/dev/tty") or not tty.removeprefix("/dev/tty").isdigit():
        fail(f"physical TTY required; current={tty}")
    output = inventory_as_user()
    show_inventory(output)
    phrase = "MIGRATE build TO .build AND debs"
    answer = input(f'\nType exactly "{phrase}" to approve, or press Enter to stop: ')
    approved = answer == phrase
    manifest = output / "migration-manifest.tsv"
    receipt = output / "confirmation.tsv"
    receipt.write_text(
        "key\tvalue\n"
        f"created\t{dt.datetime.now().astimezone().isoformat()}\n"
        f"tty\t{tty}\nuid\t{os.getuid()}\ninventory\t{output}\n"
        f"plan_sha256\t{PLAN_SHA}\nmanifest_sha256\t{sha_file(manifest)}\n"
        f"approve_task4\t{'yes' if approved else 'no'}\n",
        encoding="utf-8",
    )
    (output / "confirmation.tsv.sha256").write_text(f"{sha_file(receipt)}  {receipt}\n", encoding="utf-8")
    print(f"CONFIRMATION_RECORDED approved={str(approved).lower()} no_migration=1 receipt={receipt}")


def receipt_values(receipt):
    with receipt.open(newline="") as stream:
        return {row["key"]: row["value"] for row in csv.DictReader(stream, delimiter="\t")}


def approved_rows(receipt):
    sidecar = receipt.with_name("confirmation.tsv.sha256")
    if not receipt.is_file() or not sidecar.is_file():
        fail("receipt or receipt hash missing")
    expected_receipt = sidecar.read_text().split()[0]
    if sha_file(receipt) != expected_receipt:
        fail("receipt hash mismatch")
    values = receipt_values(receipt)
    if values.get("approve_task4") != "yes" or values.get("plan_sha256") != PLAN_SHA:
        fail("receipt did not approve this plan")
    inventory = Path(values["inventory"])
    manifest = inventory / "migration-manifest.tsv"
    if sha_file(manifest) != values.get("manifest_sha256"):
        fail("bound inventory changed")
    return sidecar, manifest, list(csv.DictReader(manifest.open(newline=""), delimiter="\t"))


def move_one(source, target, row, execute):
    source_present = source.exists() or source.is_symlink()
    target_present = target.exists() or target.is_symlink()
    if source_present == target_present:
        fail(f"expected exactly one source/target: {row['source']} -> {row['target']}")
    current = source if source_present else target
    identity = object_identity(current)
    if identity["content_sha256"] != row["content_sha256"] or identity["metadata_sha256"] != row["metadata_sha256"]:
        fail(f"identity mismatch: {current}")
    if not source_present:
        return "already-moved"
    if not execute:
        return "ready"
    source.rename(target)
    if object_identity(target) != identity:
        fail(f"post-move identity mismatch: {target}")
    return "moved"


def apply(receipt):
    if os.geteuid() == 0:
        fail("apply must start as the normal user")
    approved_rows(receipt)
    for path in (ROOT / "debs/archive/legacy-build", ROOT / ".build/work/r16",
                 ROOT / ".build/evidence/r16", ROOT / ".build/evidence/legacy"):
        path.mkdir(parents=True, exist_ok=True)
    command = ["sudo", str(Path(__file__).resolve()), "--apply-root", str(receipt),
               str(os.getuid()), str(os.getgid())]
    subprocess.check_call(command)


def audit_root(receipt):
    if os.geteuid() != 0:
        fail("audit-root requires root")
    _, _, approved = approved_rows(receipt)
    ready = already = remove_ready = 0
    for row in approved:
        source = ROOT / row["source"]
        if row["action"] == "move":
            state = move_one(source, ROOT / row["target"], row, False)
            ready += state == "ready"
            already += state == "already-moved"
        elif source.exists():
            if not source.is_dir() or any(source.iterdir()):
                fail(f"remove-empty source not empty: {row['source']}")
            remove_ready += 1
    print(f"R50_TASK4_RESUME_AUDIT_PASS ready={ready} already_moved={already} "
          f"remove_empty_ready={remove_ready} plan_rows={len(approved)}")


def apply_root(receipt, owner_uid, owner_gid):
    if os.geteuid() != 0:
        fail("apply-root requires root")
    sidecar, manifest, approved = approved_rows(receipt)

    stamp = dt.datetime.now().astimezone().strftime("%Y%m%d-%H%M%S")
    run = EVIDENCE / f"apply-{stamp}"
    run.mkdir(parents=True)
    shutil.copy2(receipt, run / receipt.name)
    shutil.copy2(sidecar, run / sidecar.name)
    shutil.copy2(manifest, run / manifest.name)
    log = (run / "operations.tsv").open("w", buffering=1)
    log.write("action\tsource\ttarget\tcontent_sha256\tmetadata_sha256\n")
    try:
        for row in approved:
            source = ROOT / row["source"]
            if row["action"] == "move":
                target = ROOT / row["target"]
                action = move_one(source, target, row, True)
                log.write(f"{action}\t{row['source']}\t{row['target']}\t{row['content_sha256']}\t{row['metadata_sha256']}\n")
            else:
                if source.exists():
                    source.rmdir()
                    action = "removed-empty"
                else:
                    action = "already-removed"
                log.write(f"{action}\t{row['source']}\t-\t{row['content_sha256']}\t{row['metadata_sha256']}\n")
        build = ROOT / "build"
        if build.exists():
            build.rmdir()
            action = "removed-empty"
        else:
            action = "already-removed"
        log.write(f"{action}\tbuild\t-\t-\t-\n")
        (run / "result.txt").write_text(f"R50_TASK4_MIGRATION_PASS evidence={run}\n", encoding="utf-8")
    finally:
        log.close()
        for path in [run, *run.iterdir()]:
            os.chown(path, owner_uid, owner_gid)
    print(f"R50_TASK4_MIGRATION_PASS evidence={run}")


def self_test():
    with tempfile.TemporaryDirectory() as temporary:
        source = Path(temporary) / "source"
        target = Path(temporary) / "target"
        source.mkdir()
        (source / "a").write_bytes(b"alpha\n")
        (source / "link").symlink_to("a")
        before = object_identity(source)
        source.rename(target)
        after = object_identity(target)
        if before != after:
            fail("identity changed across rename")
    with tempfile.TemporaryDirectory(dir=ROOT) as temporary:
        source = Path(temporary) / "source"
        target = Path(temporary) / "target"
        source.mkdir()
        (source / "a").write_bytes(b"resume\n")
        row = {"source": str(source), "target": str(target), **object_identity(source)}
        if move_one(source, target, row, True) != "moved":
            fail("resume self-test initial move")
        if move_one(source, target, row, True) != "already-moved":
            fail("resume self-test already-moved")
    if len(plan_rows()) != 45:
        fail("plan self-test")
    p2 = (ROOT / "tools/internal/p2-normalize-v3.py").read_text(encoding="utf-8")
    gate = (ROOT / "tools/internal/r16-gate.py").read_text(encoding="utf-8")
    if "else '.build/work/r16/p2-manifest.tsv'" not in p2:
        fail("P2 generator default is not work")
    if 'os.environ.get("R16_MANIFEST", ".build/evidence/r16/p2-manifest.tsv")' not in gate:
        fail("R16 gate default is not accepted evidence")
    accepted = [r for r in plan_rows() if r["source"] == "build/p2-manifest.tsv"]
    if len(accepted) != 1 or accepted[0]["target"] != ".build/evidence/r16/p2-manifest.tsv":
        fail("accepted P2 migration target drift")
    print("R50_TASK4_SELF_TEST_PASS plan_rows=45 rename_identity=PASS p2_roles=PASS")


def usage():
    fail(f"usage: {Path(__file__).name} --inventory | --self-test | --confirm | --apply RECEIPT")


if __name__ == "__main__":
    try:
        if sys.argv[1:] == ["--self-test"]:
            self_test()
        elif sys.argv[1:] == ["--inventory"]:
            output = inventory_as_user()
            show_inventory(output)
        elif len(sys.argv) == 4 and sys.argv[1] == "--inventory-root":
            if os.geteuid() != 0:
                fail("inventory-root requires root")
            print(write_inventory(int(sys.argv[2]), int(sys.argv[3])))
        elif sys.argv[1:] == ["--confirm"]:
            confirm()
        elif len(sys.argv) == 3 and sys.argv[1] == "--apply":
            apply(Path(sys.argv[2]).resolve())
        elif len(sys.argv) == 5 and sys.argv[1] == "--apply-root":
            apply_root(Path(sys.argv[2]).resolve(), int(sys.argv[3]), int(sys.argv[4]))
        elif len(sys.argv) == 3 and sys.argv[1] == "--audit-root":
            audit_root(Path(sys.argv[2]).resolve())
        else:
            usage()
    except (OSError, KeyError, ValueError, subprocess.CalledProcessError) as error:
        fail(str(error))
