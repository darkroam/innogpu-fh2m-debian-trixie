#!/usr/bin/env python3
"""R50 task 5: inventory, approve, and remove retired r5dpm build artifacts."""

import csv
import datetime as dt
import hashlib
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
EVIDENCE = ROOT / ".build/r50-system-cleanup-20261001-01/task5-r5dpm-cleanup/artifacts"
RELEASES = ("6.12.101-r5dpm1", "6.12.101-r5dpm2")
KEEP = ("6.12.107+deb13-amd64", "6.12.101+deb13-amd64")
FIELDS = ("area", "path", "kind", "bytes", "file_count", "content_sha256",
          "metadata_sha256", "basis")


def fail(message):
    raise SystemExit(f"FAIL: {message}")


def sha_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def relative(path):
    path = path.resolve(strict=False)
    try:
        return path.relative_to(ROOT)
    except ValueError:
        fail(f"path outside repository: {path}")


def walk(path):
    root_dev = path.lstat().st_dev

    def visit(current, rel):
        info = current.lstat()
        if info.st_dev != root_dev:
            fail(f"cross-filesystem candidate: {relative(current)}")
        if stat.S_ISREG(info.st_mode):
            kind, target, digest = "f", "", sha_file(current)
        elif stat.S_ISDIR(info.st_mode):
            kind, target, digest = "d", "", ""
        elif stat.S_ISLNK(info.st_mode):
            kind, target, digest = "l", os.readlink(current), ""
        else:
            fail(f"unsupported candidate type: {relative(current)}")
        yield kind, rel, info, target, digest
        if kind == "d":
            for child in sorted(current.iterdir(), key=lambda item: os.fsencode(item.name)):
                child_rel = child.name if rel == "." else f"{rel}/{child.name}"
                yield from visit(child, child_rel)

    yield from visit(path, ".")


def identity(path):
    if path.is_symlink() or not path.exists():
        fail(f"candidate missing or top-level symlink: {relative(path)}")
    content = hashlib.sha256()
    metadata = hashlib.sha256()
    total = files = 0
    root_kind = direct_sha = None
    for kind, rel, info, target, digest in walk(path):
        root_kind = root_kind or {"f": "file", "d": "directory"}[kind]
        total += info.st_size
        if kind == "f":
            files += 1
            if rel == ".":
                direct_sha = digest
        content.update(f"{kind}\t{rel}\t{target}\t{digest}\n".encode())
        metadata.update(
            f"{kind}\t{rel}\t{stat.S_IMODE(info.st_mode):o}\t{info.st_uid}\t{info.st_gid}\t"
            f"{info.st_size}\t{info.st_mtime_ns}\t{target}\n".encode()
        )
    return {
        "kind": root_kind,
        "bytes": str(total),
        "file_count": str(files),
        "content_sha256": direct_sha or content.hexdigest(),
        "metadata_sha256": metadata.hexdigest(),
    }


def candidate_specs():
    runtime_roots = [
        ROOT / ".runtime-archive/runtime-5.0.0-i6/r5-dpm-watchdog-kernel",
        ROOT / ".runtime-archive/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-kernel",
    ]
    runtime_dirs = [
        runtime_roots[0] / "input",
        runtime_roots[1] / "input",
        runtime_roots[0] / "source-package",
        runtime_roots[1] / "source-package",
        runtime_roots[0] / "baseline-normalize",
        runtime_roots[1] / "baseline-normalize",
        runtime_roots[0] / "A-failed-missing-debhelper/build",
    ]
    specs = [(path, "runtime", "rebuildable extracted source/config or failed build staging")
             for path in runtime_dirs]
    for base in runtime_roots:
        for path in sorted(base.rglob("*.deb")):
            if not any(path == parent or path.is_relative_to(parent) for parent in runtime_dirs):
                specs.append((path, "runtime", "retired diagnostic kernel package copy; logs and hashes retained"))

    abi_bases = [
        ROOT / ".build/r26-f-i7-20260922-offline-01/attempt-04/A-logs/abi",
        ROOT / ".build/r26-f-i7-20260922-offline-01/attempt-09-i9-all-k/A-logs/abi",
        ROOT / ".build/r26-f-i7-20260922-offline-01/attempt-09-i9-all-k/B-logs/abi",
        ROOT / ".build/r26-f-i7-20260922-offline-01/attempt-10-i10-all-k/A-logs/abi",
        ROOT / ".build/r26-f-i7-20260922-offline-01/attempt-10-i10-all-k/B-logs/abi",
        ROOT / ".build/r35-i11-20260924-offline-01/attempt-11-i11-all-k/A-logs/abi",
        ROOT / ".build/r35-i11-20260924-offline-01/attempt-11-i11-all-k/B-logs/abi",
    ]
    binary_names = ("unstripped.ko", "dependency-files.tar", "installed-module",
                    "fantgpu.ko.xz", "fantgpu.mod", "fantgpu.mod.c")
    abi_dirs = []
    for base in abi_bases:
        for release in RELEASES:
            directory = base / release
            abi_dirs.append(directory)
            for name in binary_names:
                path = directory / name
                if path.exists():
                    specs.append((path, "build", "rebuildable A/B module or dependency artifact; ABI text/hash retained"))

    postverify = [
        ROOT / ".build/r28-install-evidence/install-i9-20260923-01/postverify",
        ROOT / ".build/r31-install-evidence/install-i10-20260924-01/postverify",
        ROOT / ".build/r37-install-evidence/install-i11-20260928-01/postverify",
        ROOT / ".build/r49-install-evidence/install-i12-20261001-01/postverify",
    ]
    unpack_dirs = []
    for base in postverify:
        for release in RELEASES:
            path = base / release / "initramfs-unpacked"
            unpack_dirs.append(path)
            specs.append((path, "build", "rebuildable initramfs extraction; postverify logs retained"))
    initial_initrds = [
        ROOT / ".build/r26-f-i7-20260922-offline-01/n0-revision-regression-02/initial-initrds/initrd.img-6.12.101-r5dpm2",
        ROOT / ".build/r26-f-i7-20260922-offline-01/window-02-control/N0/initial-initrds/initrd.img-6.12.101-r5dpm2",
    ]
    specs.extend((path, "build", "retired initrd copy; control logs retained") for path in initial_initrds)

    if len(runtime_dirs) != 7 or len(abi_dirs) != 14 or len(unpack_dirs) != 8:
        fail("static candidate cardinality drift")
    if sum(area == "runtime" for _, area, _ in specs) != 35:
        fail("runtime candidate cardinality drift")
    if sum(area == "build" for _, area, _ in specs) != 84:
        fail("build candidate cardinality drift")
    if len(specs) != 119 or len({path for path, _, _ in specs}) != 119:
        fail("candidate cardinality or uniqueness drift")
    paths = [path for path, _, _ in specs]
    for path in paths:
        if not path.exists() or path.is_symlink():
            fail(f"candidate absent or top-level symlink: {relative(path)}")
    for left in paths:
        for right in paths:
            if left != right and right.is_relative_to(left):
                fail(f"overlapping candidates: {relative(left)} and {relative(right)}")
    return sorted(specs, key=lambda item: os.fsencode(str(relative(item[0]))))


def validate_host():
    if Path("/proc/sys/kernel/osrelease").read_text().strip() != KEEP[0]:
        fail("running kernel is not retained 107")
    for release in RELEASES:
        if Path(f"/lib/modules/{release}").exists() or any(Path("/boot").glob(f"*-{release}")):
            fail(f"diagnostic kernel still installed: {release}")
    for release in KEEP:
        for prefix in ("vmlinuz", "initrd.img", "System.map", "config"):
            if not Path(f"/boot/{prefix}-{release}").is_file():
                fail(f"retained boot file missing: {prefix}-{release}")
        if not Path(f"/lib/modules/{release}").is_dir():
            fail(f"retained module tree missing: {release}")
    boot_releases = {path.name.removeprefix("vmlinuz-") for path in Path("/boot").glob("vmlinuz-*")}
    module_releases = {path.name for path in Path("/lib/modules").iterdir() if path.is_dir()}
    if boot_releases != set(KEEP) or module_releases != set(KEEP):
        fail("installed kernel set is not exactly retained 107/101")
    if "GRUB_DEFAULT=0" not in Path("/etc/default/grub").read_text().splitlines():
        fail("GRUB default changed")
    grub = Path("/boot/grub/grub.cfg").read_text(errors="replace")
    grub_releases = set(re.findall(r"vmlinuz-([^\s'\"]+)", grub))
    if grub_releases != set(KEEP) or Path("/boot/grub/custom.cfg").exists():
        fail("GRUB release set is not 107/101")


def collect_rows(specs):
    rows = []
    for path, area, basis in specs:
        rows.append({"area": area, "path": str(relative(path)), "basis": basis, **identity(path)})
    return rows


def under(path, parents):
    return any(path == parent or path.is_relative_to(parent) for parent in parents)


def retain_rows(specs):
    candidates = [path for path, _, _ in specs]
    # Derive roots from the approved paths, not their current existence: this
    # same retain manifest must still be reproducible after deletion.
    roots = {path.parent for path, _, _ in specs}
    rows = {}
    for base in sorted(roots, key=lambda path: os.fsencode(str(path))):
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if under(path, candidates) or path.is_dir():
                continue
            rel = str(relative(path))
            if path.is_symlink():
                rows[rel] = ("symlink", os.readlink(path))
            elif path.is_file():
                rows[rel] = ("file", sha_file(path))
            else:
                fail(f"unsupported retained object: {rel}")
    return [{"path": path, "kind": value[0], "identity": value[1]} for path, value in sorted(rows.items())]


def write_tsv(path, fields, rows):
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def write_inventory(owner_uid, owner_gid):
    validate_host()
    specs = candidate_specs()
    rows = collect_rows(specs)
    retained = retain_rows(specs)
    stamp = dt.datetime.now().astimezone().strftime("%Y%m%d-%H%M%S")
    output = EVIDENCE / f"inventory-{stamp}"
    output.mkdir(parents=True)
    manifest = output / "deletion-manifest.tsv"
    retain = output / "retain-manifest.tsv"
    write_tsv(manifest, FIELDS, rows)
    write_tsv(retain, ("path", "kind", "identity"), retained)
    totals = {area: sum(int(row["bytes"]) for row in rows if row["area"] == area)
              for area in ("build", "runtime")}
    (output / "summary.txt").write_text(
        f"candidates={len(rows)}\nbuild_candidates=84\nruntime_candidates=35\n"
        f"build_bytes={totals['build']}\nruntime_bytes={totals['runtime']}\n"
        f"total_bytes={sum(totals.values())}\nretained_objects={len(retained)}\n"
        f"manifest_sha256={sha_file(manifest)}\nretain_sha256={sha_file(retain)}\n"
        "host_kernel_set=107,101\ngrub_default=0\n",
        encoding="utf-8",
    )
    for path in [EVIDENCE, output, *output.iterdir()]:
        os.chown(path, owner_uid, owner_gid)
    return output


def inventory_as_user():
    if os.geteuid() == 0:
        fail("start as the normal user, not root")
    command = ["sudo", str(Path(__file__).resolve()), "--inventory-root", str(os.getuid()), str(os.getgid())]
    output = Path(subprocess.check_output(command, text=True).strip().splitlines()[-1])
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", dir=EVIDENCE, delete=False) as stream:
        stream.write(f"{output}\n")
        temporary = Path(stream.name)
    temporary.chmod(0o644)
    temporary.replace(EVIDENCE / "latest-inventory.txt")
    return output


def show_inventory(output):
    print((output / "summary.txt").read_text(), end="")
    with (output / "deletion-manifest.tsv").open(newline="") as stream:
        for row in csv.DictReader(stream, delimiter="\t"):
            print(f"[{row['area']}] {row['path']} ({int(row['bytes']) / 1e6:.2f} MB) — {row['basis']}")
    print(f"inventory={output}")


def confirm():
    if os.geteuid() == 0:
        fail("start as the normal user, not root")
    tty = os.ttyname(sys.stdin.fileno())
    if not tty.startswith("/dev/tty") or not tty.removeprefix("/dev/tty").isdigit():
        fail(f"physical TTY required; current={tty}")
    output = inventory_as_user()
    show_inventory(output)
    phrase = "DELETE 119 RETIRED r5dpm ARTIFACTS"
    approved = input(f'\nType exactly "{phrase}" to approve, or press Enter to stop: ') == phrase
    manifest = output / "deletion-manifest.tsv"
    retain = output / "retain-manifest.tsv"
    receipt = output / "confirmation.tsv"
    receipt.write_text(
        "key\tvalue\n"
        f"created\t{dt.datetime.now().astimezone().isoformat()}\n"
        f"tty\t{tty}\nuid\t{os.getuid()}\ninventory\t{output}\n"
        f"script_sha256\t{sha_file(Path(__file__))}\n"
        f"manifest_sha256\t{sha_file(manifest)}\nretain_sha256\t{sha_file(retain)}\n"
        f"approve_task5\t{'yes' if approved else 'no'}\n",
        encoding="utf-8",
    )
    (output / "confirmation.tsv.sha256").write_text(f"{sha_file(receipt)}  {receipt}\n", encoding="utf-8")
    print(f"CONFIRMATION_RECORDED approved={str(approved).lower()} no_deletion=1 receipt={receipt}")


def approved(receipt, expected_uid):
    sidecar = receipt.with_name("confirmation.tsv.sha256")
    if not receipt.is_file() or not sidecar.is_file() or sha_file(receipt) != sidecar.read_text().split()[0]:
        fail("receipt or receipt hash invalid")
    with receipt.open(newline="") as stream:
        receipt_rows = list(csv.DictReader(stream, delimiter="\t"))
    keys = {row["key"] for row in receipt_rows}
    expected_keys = {"created", "tty", "uid", "inventory", "script_sha256",
                     "manifest_sha256", "retain_sha256", "approve_task5"}
    if len(receipt_rows) != len(expected_keys) or keys != expected_keys:
        fail("receipt schema or key uniqueness invalid")
    values = {row["key"]: row["value"] for row in receipt_rows}
    tty = values["tty"]
    inventory = Path(values["inventory"]).resolve()
    if (not tty.startswith("/dev/tty") or not tty.removeprefix("/dev/tty").isdigit()
            or values["uid"] != str(expected_uid)):
        fail("receipt physical TTY or UID invalid")
    if receipt.resolve() != inventory / "confirmation.tsv" or not inventory.is_relative_to(EVIDENCE.resolve()):
        fail("receipt inventory path invalid")
    if values.get("approve_task5") != "yes" or values.get("script_sha256") != sha_file(Path(__file__)):
        fail("receipt did not approve this script")
    manifest = inventory / "deletion-manifest.tsv"
    retain = inventory / "retain-manifest.tsv"
    if sha_file(manifest) != values.get("manifest_sha256") or sha_file(retain) != values.get("retain_sha256"):
        fail("bound inventory changed")
    with manifest.open(newline="") as stream:
        rows = list(csv.DictReader(stream, delimiter="\t"))
    with retain.open(newline="") as stream:
        retained = list(csv.DictReader(stream, delimiter="\t"))
    return sidecar, manifest, retain, rows, retained


def apply(receipt):
    if os.geteuid() == 0:
        fail("apply must start as the normal user")
    approved(receipt, os.getuid())
    subprocess.check_call(["sudo", str(Path(__file__).resolve()), "--apply-root", str(receipt),
                           str(os.getuid()), str(os.getgid())])


def apply_root(receipt, owner_uid, owner_gid):
    if os.geteuid() != 0:
        fail("apply-root requires root")
    validate_host()
    sidecar, manifest, retain, approved_rows, approved_retain = approved(receipt, owner_uid)
    specs = candidate_specs()
    if collect_rows(specs) != approved_rows or retain_rows(specs) != approved_retain:
        fail("candidate or retained evidence drift after confirmation")
    stamp = dt.datetime.now().astimezone().strftime("%Y%m%d-%H%M%S")
    run = EVIDENCE / f"cleanup-{stamp}"
    run.mkdir(parents=True)
    for source in (receipt, sidecar, manifest, retain):
        shutil.copy2(source, run / source.name)
    hashes = (run / "deletion-files.sha256").open("w")
    for path, _, _ in specs:
        for kind, rel, _, _, digest in walk(path):
            if kind == "f":
                shown = path if rel == "." else path / rel
                hashes.write(f"{digest}  {shown}\n")
    hashes.close()
    log = (run / "deletion.tsv").open("w", buffering=1)
    log.write("status\tpath\n")
    try:
        for path, _, _ in specs:
            if path.is_dir():
                shutil.rmtree(path)
            else:
                path.unlink()
            log.write(f"deleted\t{relative(path)}\n")
        if any(path.exists() or path.is_symlink() for path, _, _ in specs):
            fail("candidate remains after cleanup")
        if retain_rows(specs) != approved_retain:
            fail("retained evidence changed during cleanup")
        validate_host()
        (run / "result.txt").write_text(
            f"R50_TASK5_ARTIFACT_CLEANUP_PASS candidates=119 evidence={run}\n", encoding="utf-8"
        )
    finally:
        log.close()
        for path in [run, *run.iterdir()]:
            os.chown(path, owner_uid, owner_gid)
    print((run / "result.txt").read_text(), end="")


def self_test():
    with tempfile.TemporaryDirectory(dir=ROOT) as temporary:
        base = Path(temporary)
        path = base / "candidate"
        path.mkdir()
        (path / "data").write_bytes(b"r5dpm\n")
        kept = base / "keep.log"
        kept.write_bytes(b"retain\n")
        specs = [(path, "build", "self-test")]
        before = identity(path)
        if before["file_count"] != "1" or len(before["content_sha256"]) != 64:
            fail("identity self-test")
        retained = retain_rows(specs)
        shutil.rmtree(path)
        if path.exists() or retain_rows(specs) != retained or retained != [{
                "path": str(relative(kept)), "kind": "file", "identity": sha_file(kept)}]:
            fail("deletion or post-delete retain self-test")
    print("R50_TASK5_SELF_TEST_PASS receipt_binding=PASS exact_delete=PASS post_delete_retain=PASS")


def usage():
    fail(f"usage: {Path(__file__).name} --self-test | --inventory | --confirm | --apply RECEIPT")


if __name__ == "__main__":
    try:
        if sys.argv[1:] == ["--self-test"]:
            self_test()
        elif sys.argv[1:] == ["--inventory"]:
            show_inventory(inventory_as_user())
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
        else:
            usage()
    except (OSError, KeyError, ValueError, subprocess.CalledProcessError) as error:
        fail(str(error))
