#!/usr/bin/env python3
"""R27 real-tool runner, called by run-fantgpu-maintainer-tests.py.

Preflight does not compile. Run requires a reviewed manifest hash.
No retry/resume: a failed formal window retains its roots for review.
"""
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
K = sorted(["6.12.101+deb13-amd64", "6.12.101-r5dpm1", "6.12.101-r5dpm2",
            "6.12.107+deb13-amd64", "6.12.63+deb13-amd64", "6.12.90+deb13.1-amd64",
            "6.12.95+deb13-amd64", "6.12.96+deb13-amd64"])
EPOCH = "1790035200"
BATCH = "20260922-offline-01"
BATCH_WORK = Path.home() / "tmp" / f"r5-phase2-f-i7-{BATCH}"
WORK = BATCH_WORK / "attempt-02"
N0_TEST_WORK = BATCH_WORK / "revision-n0-test-20260923-02"
EVIDENCE = ROOT / f".build/r26-f-i7-{BATCH}" / "attempt-02"
SOURCE = "usr/src/fantgpu-fh2m-kernel-2.2"
META = "docs/planning/evidence/o-stage/5.0.0-i6"
META_SHA = "a14250711f4b367cce0aed345da6e89c9921761b1b65aa3d87c5d67769d91321"
POLICY_SHA = "e362342a516c0507da4407b889d041a1df7e187f4cb8d05c8a039b1a307d2d4b"
BASE = "68981bb9583fe15a6f4c8c8047a99b926bafe411"
OLD_MODULES = [Path(f"/lib/modules/{k}/updates/dkms/fantgpu.ko.xz") for k in K[:3]]
REPO_INPUTS = ["scripts", "tools", "drivers", "binary-manifest-fantgpu.json",
               "vendor/fantgpu", META, "tests/unit"]
# Explicit ordinary tool closure; no /usr/local, host source, modules or keys.
TOOL_INPUTS = ["/usr/bin", "/usr/sbin", "/usr/lib64", "/usr/include",
               "/usr/lib/x86_64-linux-gnu", "/usr/lib/gcc", "/usr/lib/bfd-plugins",
               "/usr/lib/python3.13", "/usr/lib/python3", "/usr/lib/dkms", "/usr/lib/dracut",
               "/usr/lib/udev", "/usr/lib/klibc", "/usr/lib/modprobe.d", "/usr/lib/depmod.d",
               "/usr/lib/dpkg", "/usr/share/initramfs-tools", "/usr/share/dpkg",
               "/usr/share/perl5", "/usr/share/perl", "/usr/share/gcc", "/usr/share/misc"]
TOOL_INPUTS += sorted(map(str, Path("/usr/lib").glob("klibc-*.so")))
# Copy these exact ordinary dependencies, never the host systemd or /etc tree.
N0_INPUTS = ["/usr/lib/systemd/systemd", "/usr/lib/systemd/systemd-udevd",
             "/usr/lib/systemd/network", "/etc/ld.so.conf", "/etc/ld.so.conf.d"]
DENIED = "modprobe insmod rmmod systemctl service reboot shutdown halt poweroff update-grub grub-install update-secureboot-policy".split()
GUARD = '''#!/bin/sh
echo "FORBIDDEN: $0 $*" >> /evidence/forbidden.log
exit 97
'''
SIGNING_REQUEST = ["openssl", "req", "-config", "/etc/ssl/openssl.cnf", "-new", "-x509", "-nodes", "-days", "30",
                   "-subj", "/CN=R27 offline test only", "-newkey", "rsa:2048", "-addext", "subjectKeyIdentifier=hash",
                   "-addext", "extendedKeyUsage=codeSigning", "-keyout", "/var/lib/dkms/mok.key",
                   "-outform", "DER", "-out", "/var/lib/dkms/mok.pub"]
STRIP = '''#!/bin/bash
set -euo pipefail
[[ $# == 2 && $1 == -g && $2 == *.ko ]] || { echo strip-argv-UNKNOWN >&2; exit 98; }
k=$(modinfo -F vermagic "$2"); k=${k%% *}
grep -Fxq "$k" /fixture/kernels || exit 98
dest=/evidence/abi/$k
mkdir -p "$dest"
[[ ! -e $dest/unstripped.ko ]] || { echo repeated-strip >&2; exit 98; }
cp -- "$2" "$dest/unstripped.ko"
pahole -C dev_rsrc "$dest/unstripped.ko" > "$dest/dev_rsrc.pahole"
python3 /repo/tools/check-fantgpu-shipped-abi.py "$dest/dev_rsrc.pahole" > "$dest/check.txt"
printf '%s\\0' "$@" > "$dest/strip.argv"
exec /fixture/real-strip "$@"
'''


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def write(root, name, text, mode=0o644):
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    path.chmod(mode)
    return path


def identity(paths):
    """Lock mounted regular inputs and symlinks, not just version labels."""
    rows = []

    def visit(path):
        if path.name == "__pycache__":
            return
        label = str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path)
        if path.is_symlink():
            rows.append([label, "link", os.readlink(path)])
        elif path.is_dir():
            rows.append([label, "dir", path.stat().st_mode & 0o777])
            for child in sorted(path.iterdir()):
                visit(child)
        elif path.is_file():
            if path.suffix in [".key", ".pem", ".p12", ".pfx"]:
                raise RuntimeError(f"unreviewed key-like input: {label}")
            rows.append([label, "file", path.stat().st_mode & 0o777, sha(path)])
        else:
            raise RuntimeError(f"nonordinary/missing input: {label}")
    for path in sorted(set(map(Path, paths))):
        visit(path)
    return rows


def inputs():
    assert sha(ROOT / META / "5.0.0-i6.meta.json") == META_SHA, "source meta drift"
    current = subprocess.check_output(["uname", "-r"], text=True).strip()
    packages = subprocess.check_output(["dpkg-query", "-W", "-f=${db:Status-Status}\t${Package}\n"], text=True)
    actual = {current}
    actual.update(p.name.removeprefix("vmlinuz-") for p in Path("/boot").glob("vmlinuz-*") if p.is_file())
    for row in packages.splitlines():
        status, package = row.split("\t")
        if re.fullmatch(r"linux-image-[0-9][a-z0-9+.-]*", package):
            if status in ["not-installed", "config-files"]:
                continue
            assert status == "installed", "incomplete image package"
            actual.add(package.removeprefix("linux-image-").removesuffix("-unsigned"))
    assert sorted(actual) == K and current == K[0], "K/current kernel drift"
    paths = [ROOT / p for p in REPO_INPUTS]
    paths += [Path(p) for p in TOOL_INPUTS if Path(p).exists()]
    assert sorted(map(str, Path("/etc").glob("ld.so.conf*"))) == sorted(N0_INPUTS[-2:]), "ld.so input set changed"
    for name in N0_INPUTS:
        path = Path(name)
        assert path.exists(), f"required N0 dependency missing: {name}"
        if path.is_symlink():
            assert name == "/usr/lib/systemd/systemd-udevd" and path.resolve() == Path("/usr/bin/udevadm"), "N0 link target UNKNOWN"
        paths.append(path)
    paths.append(Path("/usr/lib/systemd/systemd-udevd").resolve())
    headers = sorted(Path("/usr/src").glob("linux-headers-*")) + sorted(Path("/usr/src").glob("linux-kbuild-*"))
    headers = sorted(set(headers + [p.resolve() for p in headers]))
    paths += headers
    paths += [Path("/etc/os-release").resolve(), Path("/etc/debian_version"), Path("/etc/ssl/openssl.cnf")]
    for folder in [Path("/etc/dkms"), Path("/etc/dkms/framework.conf.d")]:
        assert not folder.is_symlink() and folder.is_dir(), "DKMS config directory redirect"
    paths += [Path("/etc/dkms/framework.conf")]
    paths += sorted(Path("/etc/dkms/framework.conf.d").glob("*.conf"))
    paths += sorted(Path("/etc/dkms").glob("fantgpu-fh2m-kernel*.conf"))
    paths += OLD_MODULES
    assert sha(Path("/etc/dkms/framework.conf.d/autoinstall_all_kernels.conf")) == POLICY_SHA
    for k in K:
        build = Path(f"/lib/modules/{k}/build").resolve()
        assert build in headers, "headers outside locked /usr/src roots"
        assert (build / "include/config/kernel.release").read_text().strip() == k
        for name in ["Makefile", "Module.symvers", "scripts/sign-file"]:
            assert (build / name).is_file(), (k, name)
        paths += [Path(f"/boot/vmlinuz-{k}"), Path(f"/boot/config-{k}"), Path(f"/lib/modules/{k}/build")]
        # Never reuse host updates/DKMS caches or initrds.
        paths += [Path(f"/lib/modules/{k}/kernel")]
        paths += sorted(Path(f"/lib/modules/{k}").glob("modules.*"))
    return {"head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
            "kernels": K, "version": "5.0.0-i7", "epoch": EPOCH,
            "cpus": sorted(os.sched_getaffinity(0))[:8],
            "headers": list(map(str, headers)), "files": identity(paths)}


def setup(root, kernel_inputs=False):
    root.mkdir(mode=0o700)
    for name in ["boot", "sys", "dev", "proc", "tmp", "run", "evidence", "fixture", "n0-unpacked", "unpacked",
                 "etc/dkms/framework.conf.d", "etc/initramfs-tools/hooks", "etc/initramfs-tools/scripts",
                 "etc/initramfs-tools/conf.d", "etc/modprobe.d", "usr/bin", "usr/sbin", "usr/src",
                 "usr/lib/modules", "usr/lib/firmware", "var/lib/dkms", "var/lib/dpkg", "var/lib/initramfs-tools"]:
        (root / name).mkdir(parents=True, exist_ok=True)
    for name, target in {"bin": "usr/bin", "sbin": "usr/sbin", "lib": "usr/lib", "lib64": "usr/lib64"}.items():
        (root / name).symlink_to(target)
    for path in [*TOOL_INPUTS, *N0_INPUTS]:
        host = Path(path)
        if not host.exists():
            assert path not in N0_INPUTS, f"required N0 dependency missing: {path}"
            continue
        dest = root / path.lstrip("/")
        if host.is_dir():
            dest.mkdir(parents=True, exist_ok=True)
            subprocess.run(["cp", "-a", "--reflink=auto", str(host) + "/.", str(dest)], check=True)
        else:
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(host, dest, follow_symlinks=False)
    for name in ["null", "zero"]:
        write(root, "dev/" + name, "")  # Ordinary private files, no host devices.
    write(root, "etc/passwd", "root:x:0:0:root:/root:/bin/sh\n")
    write(root, "etc/group", "root:x:0:\n")
    shutil.copyfile(Path("/etc/os-release").resolve(), root / "etc/os-release")
    shutil.copyfile("/etc/debian_version", root / "etc/debian_version")
    write(root, "etc/ssl/openssl.cnf", Path("/etc/ssl/openssl.cnf").read_text())
    for conf in [Path("/etc/dkms/framework.conf"), *Path("/etc/dkms/framework.conf.d").glob("*.conf"),
                 *Path("/etc/dkms").glob("fantgpu-fh2m-kernel*.conf")]:
        assert not conf.is_symlink() and conf.is_file(), "DKMS config type UNKNOWN"
        write(root, str(conf).lstrip("/"), conf.read_text())
    write(root, "etc/initramfs-tools/initramfs.conf", "MODULES=list\nBUSYBOX=y\nKEYMAP=n\nCOMPRESS=gzip\nDEVICE=\nNFSROOT=auto\nRUNSIZE=10%\nFSTYPE=auto\n")
    write(root, "etc/initramfs-tools/update-initramfs.conf", "update_initramfs=yes\nbackup_initramfs=no\n")
    write(root, "etc/initramfs-tools/modules", "")
    write(root, "etc/initramfs-tools/conf.d/resume", "RESUME=none\n")
    write(root, "fixture/kernels", "\n".join(K) + "\n")
    status = "".join(f"Package: linux-image-{k}\nStatus: install ok installed\nArchitecture: amd64\nVersion: 1\nDescription: isolated image record\n\n" for k in K)
    write(root, "var/lib/dpkg/status", status)
    for name in DENIED:
        for prefix in ["usr/bin/", "usr/sbin/"]:
            (root / (prefix + name)).unlink(missing_ok=True)
            write(root, prefix + name, GUARD, 0o755)
    (root / "usr/bin/strip").unlink(missing_ok=True)
    write(root, "usr/bin/strip", STRIP, 0o755)
    # The production script has an explicit unavailable-lspci warning path.
    for prefix in ["usr/bin/", "usr/sbin/"]:
        (root / (prefix + "lspci")).unlink(missing_ok=True)
    if kernel_inputs:
        for k in K:
            dest = root / f"usr/lib/modules/{k}"
            dest.mkdir()
            shutil.copytree(f"/lib/modules/{k}/kernel", dest / "kernel", symlinks=True)
            assert not (dest / "kernel/kylin").exists(), "unreviewed Kylin input present"
            for path in Path(f"/lib/modules/{k}").glob("modules.*"):
                shutil.copyfile(path, dest / path.name)
            (dest / "build").symlink_to(Path(f"/lib/modules/{k}/build").resolve())
            for name in ["vmlinuz", "config"]:
                shutil.copyfile(f"/boot/{name}-{k}", root / f"boot/{name}-{k}")


def mounts(root, lock):
    args = ["bwrap", "--unshare-user", "--uid", "0", "--gid", "0", "--unshare-pid",
            "--unshare-ipc", "--unshare-uts", "--unshare-net", "--die-with-parent",
            "--new-session", "--clearenv", "--cap-drop", "ALL", "--bind", str(root), "/", "--proc", "/proc", "--chdir", "/repo"]

    for path in [*TOOL_INPUTS, "/usr/lib/systemd", "/etc/ld.so.conf", "/etc/ld.so.conf.d"]:
        if (root / path.lstrip("/")).exists():
            args.extend(["--ro-bind", str(root / path.lstrip("/")), path])
    for path in lock["headers"]:
        args.extend(["--ro-bind", path, path])
    for path in REPO_INPUTS:
        args.extend(["--ro-bind", str(ROOT / path), "/repo/" + path])
    args.extend(["--ro-bind", str(Path("/usr/bin/strip").resolve()), "/fixture/real-strip"])
    env = {"PATH": "/usr/sbin:/usr/bin:/sbin:/bin", "LC_ALL": "C", "TZ": "UTC",
           "SOURCE_DATE_EPOCH": EPOCH, "PYTHONDONTWRITEBYTECODE": "1", "TMPDIR": "/tmp",
           "KBUILD_BUILD_TIMESTAMP": "Tue Sep 22 00:00:00 UTC 2026",
           "KBUILD_BUILD_USER": "r27", "KBUILD_BUILD_HOST": "offline", "KBUILD_BUILD_VERSION": "1",
           "OMP_NUM_THREADS": str(len(lock["cpus"]))}
    for key, value in env.items():
        args.extend(["--setenv", key, value])
    return ["taskset", "-c", ",".join(map(str, lock["cpus"]))] + args


def command(root, lock, argv, ledger, deadline, extra=()):
    start = datetime.now(timezone.utc).isoformat()
    log = root / "evidence" / f"command-{len(ledger):03}.log"
    remaining = deadline - time.monotonic()
    assert remaining > 0, "6h window expired"
    with log.open("wb") as stream:
        p = subprocess.Popen(mounts(root, lock) + list(extra) + ["--"] + argv,
                             stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            rc = p.wait(timeout=remaining)
        except subprocess.TimeoutExpired:
            os.killpg(p.pid, signal.SIGKILL)
            p.wait()
            rc = 124
    row = {"argv": argv, "start": start, "end": datetime.now(timezone.utc).isoformat(),
           "rc": rc, "log": str(log.relative_to(root)), "sha256": sha(log)}
    ledger.append(row)
    write(root, "evidence/commands.json", json.dumps(ledger, indent=2) + "\n")
    assert rc == 0, row
    assert not (root / "evidence/forbidden.log").exists(), "forbidden command invoked"
    return log.read_text(errors="replace")


def claim_preflight(work, test_only=False):
    expected = N0_TEST_WORK if test_only else WORK
    assert work == expected / "preflight" and work.resolve() == work, "unapproved/redirected N0 path"
    assert not expected.exists() and not expected.is_symlink(), "N0 attempt already exists; no retry/resume"
    assert EVIDENCE.resolve() == EVIDENCE and not EVIDENCE.exists(), "formal evidence must be fresh"
    expected.mkdir(mode=0o700)
    work.mkdir(mode=0o700)


def n0_artifacts(root):
    names = ["var/lib/dkms/mok.pub", "evidence/commands.json"]
    names += [f"boot/initrd.img-{k}" for k in K[::2]]
    return {name: sha(root / name) for name in names}


def prepare_n0(root, lock, deadline):
    """One preparation shared by the formal preflight and its isolated regression."""
    setup(root, True)
    ledger = []
    text = (ROOT / "scripts/generate-fantgpu-maintainer-scripts.sh").read_text()
    guard = text[text.index("dkms_policy_guard() {"):text.index("\nidentity=$(source_digest")]
    write(root, "fixture/probe.sh", "#!/bin/bash\nset -euo pipefail\n" + guard + '''
dkms_policy_guard
[[ $(id -u) == 0 && $(nproc) -le 8 ]]
[[ ! -e /sys/class && ! -e /dev/dri && ! -e /boot/grub && ! -e /usr/src/fantgpu-fh2m-kernel-2.2 ]]
[[ -z $(find /dev -type b -o -type c) ]]
[[ ! -e /sys/kernel/btf/vmlinux && ! -e /sys/firmware/efi/efivars ]]
[[ -z $(ls -A /var/lib/dkms) ]]
! command -v lspci
dkms --version
make --version
gcc --version
modinfo --version
pahole --version
python3 --version
test -x /usr/lib/systemd/systemd
test -x /usr/lib/systemd/systemd-udevd
test -d /usr/lib/systemd/network
test -f /etc/ld.so.conf
test -d /etc/ld.so.conf.d
printf 'isolation_probe=PASS driver_builds=0 installs=0\\n'
''')
    command(root, lock, ["bash", "/fixture/probe.sh"], ledger, deadline)
    command(root, lock, SIGNING_REQUEST, ledger, deadline)
    key = root / "var/lib/dkms/mok.key"
    assert not key.is_symlink() and key.is_file() and key.stat().st_mode & 0o777 == 0o600
    cert = command(root, lock, ["openssl", "x509", "-inform", "DER", "-in", "/var/lib/dkms/mok.pub",
                   "-noout", "-subject", "-ext", "subjectKeyIdentifier,extendedKeyUsage"], ledger, deadline)
    assert "CN=R27 offline test only" in cert.replace(" = ", "=") and "Code Signing" in cert
    key_id = re.search(r"([0-9A-F]{2}:){19}[0-9A-F]{2}", cert).group(0).lower()
    # Check each sign-file call without compiling or installing a driver.
    for k in K:
        command(root, lock, ["bash", "-ec", f'''
hash=$(sed -n 's/^CONFIG_MODULE_SIG_HASH="\\(.*\\)"/\\1/p' /boot/config-{k})
test -n "$hash"
printf 'N0 sign-file tool fixture\\n' > /tmp/sign-{k}
/lib/modules/{k}/build/scripts/sign-file "$hash" /var/lib/dkms/mok.key /var/lib/dkms/mok.pub /tmp/sign-{k}
test "$(stat -c %s /tmp/sign-{k})" -gt 26
rm /tmp/sign-{k}
'''], ledger, deadline)
    # Fixed initial update/create split, reused unchanged by both A and B.
    for k in K[::2]:
        command(root, lock, ["update-initramfs", "-c", "-k", k], ledger, deadline)
        command(root, lock, ["bash", "-ec", f'''
unmkinitramfs /boot/initrd.img-{k} /n0-unpacked/{k}
tool=$(find /n0-unpacked/{k} -type f -path '*/usr/bin/udevadm')
test "$(printf '%s\\n' "$tool" | wc -l)" = 1
cmp /usr/bin/udevadm "$tool"
conf=$(find /n0-unpacked/{k} -type f -path '*/etc/ld.so.conf')
test "$(printf '%s\\n' "$conf" | wc -l)" = 1
cmp /etc/ld.so.conf "$conf"
test -z "$(find /n0-unpacked/{k} -name 'fantgpu.ko*')"
'''], ledger, deadline)
    return key_id


def preflight(work, test_only=False):
    claim_preflight(work, test_only)
    start = datetime.now(timezone.utc).isoformat()
    deadline_unix = time.time() + 6 * 3600
    deadline = time.monotonic() + 6 * 3600
    success = False
    root = work / "common"
    try:
        lock = inputs()
        path = write(work, "inputs.json", json.dumps(lock, indent=2) + "\n")
        space = {"tmp_free": shutil.disk_usage(work).free, "repo_free": shutil.disk_usage(ROOT).free,
                 "tmp_required": 30 * 1024**3, "repo_required": 10 * 1024**3}
        write(work, "capacity.json", json.dumps(space, indent=2) + "\n")
        same_disk = work.stat().st_dev == ROOT.stat().st_dev
        assert space["tmp_free"] >= space["tmp_required"] + (space["repo_required"] if same_disk else 0), "N0 capacity BLOCKED"
        assert space["repo_free"] >= space["repo_required"], "N0 evidence capacity BLOCKED"
        key_id = prepare_n0(root, lock, deadline)
        assert inputs() == lock, "input drift during N0"
        assert time.monotonic() < deadline, "6h N0+N1-N5 window expired"
        write(work, "n0.json", json.dumps({"status": "PASS", "purpose": "regression" if test_only else "formal",
              "start": start, "deadline_unix": deadline_unix, "input_sha256": sha(path),
              "key_id": key_id, "artifacts": n0_artifacts(root)}, indent=2) + "\n")
        success = True
    finally:
        if test_only:
            (root / "var/lib/dkms/mok.key").unlink(missing_ok=True)
        write(work, "result.json", json.dumps({"N0": "PASS" if success else "FAILED_OR_UNVERIFIED",
              "purpose": "regression" if test_only else "formal", "start": start,
              "end": datetime.now(timezone.utc).isoformat(), "N1_N5": "NOT_RUN"}, indent=2) + "\n")
    print(f"native_n0=PASS purpose={'regression' if test_only else 'formal'} N1_N5=NOT_RUN")


def verified_n0(work, manifest, reviewed_sha):
    assert work == WORK and work.resolve() == work, "unapproved/redirected batch path"
    assert sorted(p.name for p in work.iterdir()) == ["preflight"], "fresh attempt must contain only N0"
    assert manifest == work / "preflight/inputs.json" and sha(manifest) == reviewed_sha, "N0 manifest mismatch"
    state = json.loads((work / "preflight/n0.json").read_text())
    assert state["status"] == "PASS" and state["purpose"] == "formal", "complete formal N0 required"
    assert state["input_sha256"] == reviewed_sha, "N0 input identity mismatch"
    assert state["deadline_unix"] > time.time(), "N0 window expired"
    common = work / "preflight/common"
    assert state["artifacts"] == n0_artifacts(common), "N0 artifacts drifted"
    key = common / "var/lib/dkms/mok.key"
    assert not key.is_symlink() and key.is_file() and key.stat().st_mode & 0o777 == 0o600, "N0 key absent/unsafe"
    return state


def run(work, manifest, reviewed_sha):
    n0 = verified_n0(work, manifest, reviewed_sha)
    lock = json.loads(manifest.read_text())
    assert inputs() == lock, "reviewed inputs drifted"
    evidence = EVIDENCE
    assert not evidence.exists() and not evidence.is_symlink(), "existing formal evidence"
    assert evidence.resolve() == evidence, "evidence parent redirect"
    assert shutil.disk_usage(work.parent).free >= 30 * 1024**3, "need 30 GiB temporary space"
    assert shutil.disk_usage(ROOT).free >= 10 * 1024**3, "need 10 GiB evidence space"
    evidence.mkdir(mode=0o700, parents=True)
    shutil.copyfile(manifest, evidence / "inputs.json")
    shutil.copyfile(work / "preflight/n0.json", evidence / "n0.json")
    common = work / "preflight/common"
    key_id = n0["key_id"]
    shutil.copyfile(common / "var/lib/dkms/mok.pub", evidence / "test-certificate.der")
    initial = {k: n0["artifacts"][f"boot/initrd.img-{k}"] for k in K[::2]}
    write(evidence, "initial-initramfs.json", json.dumps(initial, indent=2) + "\n")
    deadline = time.monotonic() + n0["deadline_unix"] - time.time()
    start = n0["start"]
    success = False
    try:
        for label, argv in [
            ("synthetic", ["python3", "-B", str(ROOT / "tests/unit/run-fantgpu-maintainer-tests.py"),
                           "--work-dir", str(work / "synthetic")]),
            ("builder-gates", ["bash", "tests/unit/run-builder-fantgpu-gates-tests.sh"]),
            ("package-gates", ["bash", "tests/unit/run-check-release-package-tests.sh"])]:
            with (evidence / (label + ".log")).open("wb") as stream:
                subprocess.run(argv, cwd=ROOT, check=True, stdout=stream, stderr=subprocess.STDOUT,
                               timeout=max(1, deadline - time.monotonic()))
        old = subprocess.check_output(["git", "show", BASE + ":scripts/build-innogpu-driver.sh"], cwd=ROOT, text=True)
        old = re.search(r'cat > "\$P/DEBIAN/prerm" <<\'PEOF\'\n(.*?)\nPEOF', old, re.S).group(1) + "\n"
        for side in ["A", "B"]:
            root = work / side
            setup(root, True)
            ledger = []
            for k in K[::2]:
                shutil.copyfile(common / f"boot/initrd.img-{k}", root / f"boot/initrd.img-{k}")
            signing = []
            for name in ["mok.key", "mok.pub"]:
                signing += ["--ro-bind", str(common / "var/lib/dkms" / name), "/var/lib/dkms/" + name]
            write(root, "fixture/build.sh", '''#!/bin/bash
set -euo pipefail
export VERSION=5.0.0-i7 SOURCE_DATE_EPOCH=1790035200
export KERNELDIR_VER=6.12.101+deb13-amd64
export KERNELDIR=/lib/modules/$KERNELDIR_VER/build
export STAGE_ROOT=/candidate BUILD_LOG=/evidence/builder.log OUT_DEB=/candidate/i7.deb
bash /repo/scripts/build-innogpu-driver.sh
''')
            command(root, lock, ["bash", "/fixture/build.sh"], ledger, deadline)
            command(root, lock, ["dpkg-deb", "-R", "/candidate/i7.deb", "/package"], ledger, deadline)
            # Never follow extracted symlinks while copying into the private root.
            # The package boundary gate already ran; also reject root escapes here.
            for link in (root / "package").rglob("*"):
                if link.is_symlink():
                    target = os.readlink(link)
                    assert not Path(target).is_absolute() and ".." not in Path(target).parts, "package link needs review"
            shutil.copytree(root / "package", root, dirs_exist_ok=True, symlinks=True)
            for name in DENIED:
                for prefix in ["usr/bin/", "usr/sbin/"]:
                    assert (root / (prefix + name)).read_text() == GUARD
            assert (root / "usr/bin/strip").read_text() == STRIP
            write(root, "fixture/old-prerm", old)
            write(root, "usr/src/innogpu-kernel-2.2/dkms.conf", (ROOT / "drivers/dkms.conf").read_text())
            command(root, lock, ["bash", "-ec", "dkms add -m innogpu-kernel -v 2.2; dkms add -m fantgpu-fh2m-kernel -v 2.2"], ledger, deadline, signing)
            # Copy the locked pre-existing i6 module bytes, never A/B outputs.
            # Real DKMS install establishes installed state without rebuilding i6.
            for path in OLD_MODULES:
                k = path.parts[3]
                dest = root / f"var/lib/dkms/fantgpu-fh2m-kernel/2.2/{k}/x86_64/module"
                dest.mkdir(parents=True)
                shutil.copyfile(path, dest / path.name)
            old_installs = "\n".join(f"dkms install -m fantgpu-fh2m-kernel -v 2.2 -k {k} --force" for k in K[:3])
            command(root, lock, ["bash", "-ec", old_installs + '''
dkms status
bash /fixture/old-prerm upgrade
test ! -e /var/lib/dkms/innogpu-kernel/2.2
test ! -e /var/lib/dkms/fantgpu-fh2m-kernel/2.2
dkms add -m innogpu-kernel -v 2.2
'''], ledger, deadline, signing)
            for k in K[:3]:
                assert not list((root / f"usr/lib/modules/{k}/updates").rglob("fantgpu.ko*")), "old module removal incomplete"
            for k in K:
                for module in ["fantgpu", "innogpu"]:
                    sentinel = root / f"usr/lib/modules/{k}/kernel/kylin/{module}.ko"
                    sentinel.parent.mkdir(parents=True, exist_ok=True)
                    # The hook only renames paths; deliberately non-module sentinels
                    # cannot be mistaken for a successfully built driver.
                    sentinel.write_bytes(b"R27 rename-only sentinel: " + module.encode() + b"\n")
            write(root, "etc/initramfs-tools/modules", "fantgpu\n")
            log = command(root, lock, ["bash", "/DEBIAN/postinst", "configure"], ledger, deadline, signing)
            assert all(f"kernel={k} result=PASS" in log for k in K)
            assert not re.search(r"failed to sign|won't be signed|unsigned module", log, re.I), "signature warning"
            out = evidence / side
            out.mkdir()
            shutil.copyfile(root / "candidate/i7.deb", out / "i7.deb")
            for k in K:
                for module in ["fantgpu", "innogpu"]:
                    sentinel = root / f"usr/lib/modules/{k}/kernel/kylin/{module}"
                    assert not sentinel.with_suffix(".ko").exists()
                    assert sentinel.with_suffix(".bak").read_bytes() == b"R27 rename-only sentinel: " + module.encode() + b"\n"
                assert (root / f"evidence/abi/{k}/check.txt").read_text().strip() == "shipped_abi=PASS size=140536 members=115 offset=140528"
                sign_hash = re.findall(r'^CONFIG_MODULE_SIG_HASH="([a-z0-9]+)"$', (root / f"boot/config-{k}").read_text(), re.M)
                assert len(sign_hash) == 1, "signature algorithm UNKNOWN"
                command(root, lock, ["bash", "-ec", f'''
ko=$(modinfo -k {k} -n fantgpu)
test "$(modinfo -F name "$ko")" = fantgpu
test "$(modinfo -F signer "$ko")" = 'R27 offline test only'
test "$(modinfo -F sig_key "$ko" | tr 'A-F' 'a-f')" = '{key_id}'
test "$(modinfo -F sig_hashalgo "$ko")" = '{sign_hash[0]}'
modinfo "$ko" > /evidence/abi/{k}/modinfo.txt
unmkinitramfs /boot/initrd.img-{k} /unpacked/{k}
found=$(find /unpacked/{k} -type f -name 'fantgpu.ko*')
test "$(printf '%s\\n' "$found" | wc -l)" = 1
cmp "$ko" "$found"
cp "$ko" /evidence/abi/{k}/installed-module
cp /var/lib/dkms/fantgpu-fh2m-kernel/2.2/{k}/x86_64/module/fantgpu.ko* /evidence/abi/{k}/
printf '%s\\n' "$ko" > /evidence/abi/{k}/installed-path.txt
sha256sum "$ko" "$found"
'''], ledger, deadline, signing)
                shutil.copyfile(root / f"boot/initrd.img-{k}", out / f"initrd.img-{k}")
            command(root, lock, ["bash", "/DEBIAN/prerm", "upgrade"], ledger, deadline, signing)
            assert (root / "var/lib/dkms/innogpu-kernel/2.2/source").is_symlink(), "new prerm removed O registration"
            for k in K:
                for module in ["fantgpu", "innogpu"]:
                    sentinel = root / f"usr/lib/modules/{k}/kernel/kylin/{module}"
                    assert sentinel.with_suffix(".ko").read_bytes() == b"R27 rename-only sentinel: " + module.encode() + b"\n"
                    assert not sentinel.with_suffix(".bak").exists()
            shutil.copytree(root / "evidence", out / "evidence")
        subprocess.run(["cmp", str(evidence / "A/i7.deb"), str(evidence / "B/i7.deb")], check=True)
        different_initrds = []
        for k in K:
            for name in ["installed-module", "unstripped.ko"]:
                subprocess.run(["cmp", str(evidence / f"A/evidence/abi/{k}/{name}"),
                                str(evidence / f"B/evidence/abi/{k}/{name}")], check=True)
            if sha(evidence / f"A/initrd.img-{k}") != sha(evidence / f"B/initrd.img-{k}"):
                different_initrds.append(k)
        assert inputs() == lock, "inputs drifted during formal window"
        assert time.monotonic() <= deadline, "6h window expired"
        write(evidence, "comparison.json", json.dumps({"deb_cmp": 0, "module_cmp": 0,
              "initramfs_differences_require_review": different_initrds}, indent=2) + "\n")
        write(evidence, "candidate-provenance.json", json.dumps({
            "candidate_version": "5.0.0-i7", "source_date_epoch": EPOCH,
            "source_materialization_version": "5.0.0-i6", "source_meta_sha256": META_SHA,
            "snapshot_sha256": sha(ROOT / META / "o-stage-snapshot.tar.zst"),
            "o_stage_tree_hash": "5f6a5347c7e217ba3f7c5b71fdcad520148e0bc71231ab95fb023655865d11da",
            "reviewed_inputs_sha256": reviewed_sha, "input_commit": lock["head"], "kernels": K,
            "certificate_sha256": sha(evidence / "test-certificate.der"),
            "deb_sha256": {s: sha(evidence / s / "i7.deb") for s in ["A", "B"]},
            "old_prerm_blob": BASE + ":scripts/build-innogpu-driver.sh",
            "predecessor_F": "three locked i6 modules, real isolated DKMS installed state",
            "predecessor_O": "added registration only; not installed O-module removal",
            "host_trust": "UNVERIFIED; offline test certificate only"}, indent=2) + "\n")
        assert not different_initrds, "initramfs differs: retain roots for per-file diff and qoder/dsh ruling"
        for label, argv in [
            ("check-docs", ["bash", "scripts/check-docs.sh"]),
            ("audit-licenses", ["python3", "tools/audit-licenses.py"]),
            ("validate-collab", ["python3", "tools/validate-collab.py"]),
            ("r16-gate", ["python3", "tools/r16-gate.py"]),
            ("diff-check", ["git", "diff", "--check"])]:
            with (evidence / (label + ".log")).open("wb") as stream:
                subprocess.run(argv, cwd=ROOT, check=True, stdout=stream, stderr=subprocess.STDOUT,
                               timeout=max(1, deadline - time.monotonic()))
        success = True
    finally:
        # Preserve failures too; never copy mok.key or hash its contents.
        for side, root in [("common", common), ("A", work / "A"), ("B", work / "B")]:
            if (root / "evidence").exists():
                shutil.copytree(root / "evidence", evidence / f"{side}-logs", dirs_exist_ok=True)
        write(evidence, "result.json", json.dumps({"start": start,
              "end": datetime.now(timezone.utc).isoformat(), "offline_result": "PASS" if success else "FAILED_OR_UNVERIFIED",
              "host_install": "NOT_RUN", "R5": "FAIL", "pm_test": "NOT_RUN"}, indent=2) + "\n")
        files = sorted(p for p in evidence.rglob("*") if p.is_file())
        write(evidence, "files.sha256", "".join(f"{sha(p)}  {p.relative_to(evidence)}\n" for p in files))
    print("native_offline=PASS host_install=NOT_RUN R5=FAIL")
