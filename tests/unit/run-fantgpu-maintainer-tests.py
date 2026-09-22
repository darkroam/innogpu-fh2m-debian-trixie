#!/usr/bin/env python3
"""Run generated control scripts in a minimal, rootless bubblewrap filesystem.

No host /boot, /dev, /sys, DKMS or package database is exposed. The network
namespace is shared but unused. No compiler or package installer runs.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "scripts/generate-fantgpu-maintainer-scripts.sh"
KERNELS = sorted(["6.12.101+deb13-amd64", "6.12.107+deb13-amd64",
                  "6.12.101-r5dpm1", "6.12.101-r5dpm2"])
MODULE = "fantgpu-fh2m-kernel"
SOURCE = f"usr/src/{MODULE}-2.2"
REGISTRY = f"var/lib/dkms/{MODULE}/2.2"
BASE = "68981bb9583fe15a6f4c8c8047a99b926bafe411"
STUB = r'''#!/bin/bash
set -euo pipefail
cmd=${0##*/}
printf '%s %s\n' "$cmd" "$*" >> /fixture/calls
if [[ -f /fixture/fail && $(</fixture/fail) == "$cmd $*" ]]; then
    echo "injected failure: $cmd $*" >&2
    exit 23
fi
mode=
[[ ! -f /fixture/mode ]] || mode=$(</fixture/mode)
case "$cmd" in
uname)
    case "$1" in -r) cat /fixture/running ;; -m) echo x86_64 ;; *) exit 94 ;; esac ;;
dpkg-query) cat /fixture/packages ;;
dkms)
    action=$1; shift
    m= v= k=
    while (( $# )); do
        case "$1" in
            -m) m=$2; shift 2 ;; -v) v=$2; shift 2 ;; -k) k=$2; shift 2 ;;
            --force|--all) shift ;; *) exit 94 ;;
        esac
    done
    reg=/var/lib/dkms/$m/$v
    case "$action" in
    status)
        [[ "$mode" != status-garbage ]] || { echo broken; exit 0; }
        if [[ -L "$reg/source" ]]; then
            if [[ -n "$k" && -e "$reg/$k/installed" ]]; then
                echo "$m/$v, $k, x86_64: installed"
            else
                echo "$m/$v: added"
            fi
        fi ;;
    add)
        [[ "$mode" != add-noop ]] || exit 0
        mkdir -p "$reg"
        ln -s "/usr/src/$m-$v" "$reg/source" ;;
    build)
        [[ "$mode" != build-noop ]] || exit 0
        mkdir -p "$reg/$k/x86_64/module"
        printf 'fixture module for %s\n' "$k" > "$reg/$k/x86_64/module/fantgpu.ko" ;;
    install)
        mkdir -p "/lib/modules/$k/updates/dkms"
        cp "$reg/$k/x86_64/module/fantgpu.ko" "/lib/modules/$k/updates/dkms/fantgpu.ko"
        [[ "$mode" != stale-module ]] || echo stale >> "/lib/modules/$k/updates/dkms/fantgpu.ko"
        [[ "$mode" != install-no-status ]] || exit 0
        touch "$reg/$k/installed" ;;
    remove) [[ "$mode" != remove-noop ]] || exit 0; rm -rf "$reg" ;;
    *) exit 94 ;;
    esac ;;
modinfo)
    if [[ "$1" == -k ]]; then
        echo "/lib/modules/$2/updates/dkms/fantgpu.ko"
    elif [[ "$2" == name ]]; then
        [[ "$mode" != wrong-name ]] || { echo innogpu; exit 0; }
        echo fantgpu
    elif [[ "$2" == vermagic ]]; then
        [[ "$mode" != wrong-vermagic ]] || { echo WRONG; exit 0; }
        k=${3#/lib/modules/}; k=${k%%/*}; echo "$k SMP preempt mod_unload"
    else exit 94
    fi ;;
update-initramfs)
    [[ "$mode" != initramfs-noop ]] || exit 0
    printf 'fixture initramfs\n' > "/boot/initrd.img-$3" ;;
lsinitramfs)
    [[ "$mode" != initramfs-missing-module ]] || exit 0
    k=${1#/boot/initrd.img-}
    echo "usr/lib/modules/$k/updates/dkms/fantgpu.ko" ;;
depmod|ldconfig) : ;;
lspci) exit 1 ;;
make|gcc) echo 'FORBIDDEN: compiler invoked in fixture' >&2; exit 95 ;;
*) exit 94 ;;
esac
'''


def put(root, path, text):
    p = root / path
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text)
    return p


def mounts(root):
    # Bind only the two interpreters and their ordinary library dependencies.
    args = ["bwrap", "--unshare-user", "--unshare-pid", "--unshare-ipc",
            "--unshare-uts", "--die-with-parent", "--new-session", "--clearenv",
            "--bind", str(root), "/", "--chdir", "/"]
    binaries = ["/usr/bin/bash", "/usr/bin/busybox"]
    paths = set(binaries)
    for binary in binaries:
        out = subprocess.check_output(["ldd", binary], text=True)
        paths.update(re.findall(r"(?:=>\s+)?(/\S+)", out))
    for name in sorted(paths):
        dest = root / name.lstrip("/")
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.touch()
        args.extend(["--ro-bind", name, name])
    return args


def make_root(directory):
    directory.mkdir()
    for name in ["DEBIAN", "boot", "sys", "dev", "proc", "tmp", "fixture",
                 "var/lib/dkms", "usr/bin", "usr/sbin"]:
        (directory / name).mkdir(parents=True, exist_ok=True)
    (directory / "bin").symlink_to("usr/bin")
    (directory / "sbin").symlink_to("usr/sbin")
    # Regular fixture null file; no host character devices are bound.
    put(directory, "dev/null", "")
    applets = "cat mkdir ln cp rm touch find sort xargs sha256sum cmp readlink grep".split()
    for name in applets:
        (directory / "usr/bin" / name).symlink_to("busybox")
    dispatcher = put(directory, "usr/bin/fixture-command", STUB)
    dispatcher.chmod(0o755)
    for name in "uname dpkg-query dkms make gcc ldconfig depmod update-initramfs lsinitramfs modinfo lspci".split():
        (directory / "usr/bin" / name).symlink_to("fixture-command")
    for k in KERNELS:
        put(directory, f"boot/vmlinuz-{k}", "fixture kernel\n")
        put(directory, f"lib/modules/{k}/build/Makefile", "fixture headers\n")
        put(directory, f"lib/modules/{k}/build/include/config/kernel.release", k + "\n")
    put(directory, "fixture/running", KERNELS[0] + "\n")
    put(directory, "fixture/packages", "".join(f"installed\tlinux-image-{k}\n" for k in reversed(KERNELS)))
    put(directory, "usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so", "fixture userspace\n")
    put(directory, "etc/modprobe.d/fantgpu.conf", "options fantgpu firmware_en=1\n")
    put(directory, SOURCE + "/dkms.conf", 'PACKAGE_NAME="fantgpu-fh2m-kernel"\nPACKAGE_VERSION="2.2"\n')
    put(directory, SOURCE + "/Makefile", "fixture, never compiled\n")
    put(directory, SOURCE + "/driver.c", "fixture source, never compiled\n")
    subprocess.run(["bash", str(GENERATOR), str(directory), "5.0.0-i99+fixture"], check=True)
    return mounts(directory)


def run(root, args, script="postinst", action="configure"):
    guard = ('test ! -e /sys/class && test ! -e /dev/dri && '
             'test ! -e /boot/grub && test ! -e /proc/1 && '
             'exec /bin/bash "$1" "$2"')
    p = subprocess.run(args + ["--", "/bin/bash", "-c", guard, "fixture",
                               f"/DEBIAN/{script}", action], capture_output=True, text=True)
    # Full fixture stdout/stderr and command log remain for the batch transcript.
    calls = (root / "fixture/calls").read_text() if (root / "fixture/calls").exists() else ""
    print(f"--- {root.name} {script} {action}: rc={p.returncode} ---")
    print(p.stdout, end="")
    print(p.stderr, end="")
    print("calls:\n" + calls, end="")
    return p, calls


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--work-dir", required=True, type=Path)
    opts = ap.parse_args()
    work = opts.work_dir.resolve()
    if not str(work).startswith("/tmp/r5-phase2-") or work.exists():
        raise SystemExit("work-dir must be a new /tmp/r5-phase2-* directory")
    work.mkdir()
    baseline = subprocess.check_output(["git", "show", BASE + ":scripts/build-innogpu-driver.sh"], cwd=ROOT, text=True)
    old_prerm = re.search(r'cat > "\$P/DEBIAN/prerm" <<\'PEOF\'\n(.*?)\nPEOF', baseline, re.S).group(1) + "\n"
    cases = []

    def case(name, mutate=lambda r: None, success=False, preflight=False, extra=None):
        root = work / name
        args = make_root(root)
        before_options = (root / "etc/modprobe.d/fantgpu.conf").read_bytes()
        mutate(root)
        p, calls = run(root, args)
        assert (p.returncode == 0) == success, (name, p.returncode)
        if preflight:
            assert not re.search(r'^dkms (add|build|install|remove) ', calls, re.M), name
        assert (root / "etc/modprobe.d/fantgpu.conf").read_bytes() == before_options, name
        assert not re.search(r'^(make|gcc) ', calls, re.M), name
        if extra:
            extra(root, args, p, calls)
        cases.append(name)
        print("PASS " + name)
        return root, args

    def all_k(root, args, p, calls):
        assert p.stdout.count("result=PASS") == len(KERNELS)
        for k in KERNELS:
            assert calls.count(f"dkms build -m {MODULE} -v 2.2 -k {k} --force\n") == 1
            assert calls.count(f"dkms install -m {MODULE} -v 2.2 -k {k} --force\n") == 1
            assert f"update-initramfs -c -k {k}\n" in calls

    def membership(root):
        put(root, "fixture/packages", (root / "fixture/packages").read_text() +
            f"installed\tlinux-image-{KERNELS[-1]}-unsigned\ninstalled\tlinux-image-amd64\n")
        put(root, "lib/modules/9.9-headers-only/build/Makefile", "not a target\n")

    good, good_args = case("V3-V4-targets", membership, success=True, extra=all_k)
    scripts = {n: (good / "DEBIAN" / n).read_bytes() for n in ["postinst", "prerm", "postrm"]}
    subprocess.run(["bash", str(GENERATOR), str(good), "5.0.0-i99+fixture"], check=True)
    assert all((good / "DEBIAN" / n).read_bytes() == b for n, b in scripts.items())
    print("PASS V7-generator-determinism")
    cases.append("V7-generator-determinism")
    def image_alias(root):
        (root / "boot/vmlinuz.old").symlink_to(f"vmlinuz-{KERNELS[1]}")
    case("V3-image-alias", image_alias, success=True, extra=all_k)
    (good / "fixture/calls").unlink()
    p, calls = run(good, good_args)
    assert p.returncode == 0 and "dkms add " not in calls
    assert calls.count("dkms build ") == 4
    assert calls.count("update-initramfs -u ") == 4
    cases.append("V5-reconfigure")
    print("PASS V5-reconfigure")

    for part in [f"lib/modules/{KERNELS[-1]}/build", f"boot/vmlinuz-{KERNELS[-1]}",
                 f"lib/modules/{KERNELS[-1]}", "usr/bin/update-initramfs",
                 "usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so"]:
        def remove(root, path=part):
            p = root / path
            shutil.rmtree(p) if p.is_dir() else p.unlink()
        case("V3-missing-" + str(len(cases)), remove, preflight=True)
    case("V3-wrong-headers", lambda r: put(r, f"lib/modules/{KERNELS[-1]}/build/include/config/kernel.release", "wrong\n"), preflight=True)
    case("V3-bad-uname", lambda r: put(r, "fixture/running", "../escape\n"), preflight=True)
    case("V3-empty-uname", lambda r: put(r, "fixture/running", "\n"), preflight=True)
    case("V3-query-error", lambda r: put(r, "fixture/fail", "dpkg-query -W -f=${db:Status-Status}\\t${Package}\\n"), preflight=True)
    case("V3-malformed-enumeration", lambda r: put(r, "fixture/packages", "garbage\n"), preflight=True)
    case("V3-incomplete-image-package", lambda r: put(r, "fixture/packages", f"unpacked\tlinux-image-{KERNELS[0]}\n"), preflight=True)
    case("V3-source-changed", lambda r: put(r, SOURCE + "/driver.c", "different\n"), preflight=True)
    case("V3-status-error", lambda r: put(r, "fixture/fail", f"dkms status -m {MODULE} -v 2.2"), preflight=True)
    case("V3-status-garbage", lambda r: put(r, "fixture/mode", "status-garbage"), preflight=True)
    def wrong_source(root):
        reg = root / REGISTRY
        reg.mkdir(parents=True)
        (reg / "source").symlink_to("/usr/src/elsewhere")
    case("V3-registry-conflict", wrong_source, preflight=True)
    case("V3-dkms-override", lambda r: put(r, "etc/dkms/framework.conf", "modprobe_on_install=true\n"), preflight=True)
    case("V5-add-error", lambda r: put(r, "fixture/fail", f"dkms add -m {MODULE} -v 2.2"))
    for mode in ["add-noop", "build-noop", "install-no-status", "stale-module", "wrong-name", "wrong-vermagic", "initramfs-noop", "initramfs-missing-module"]:
        case("V5-" + mode, lambda r, m=mode: put(r, "fixture/mode", m))
    for command in [f"dkms build -m {MODULE} -v 2.2 -k {KERNELS[1]} --force",
                    f"dkms install -m {MODULE} -v 2.2 -k {KERNELS[1]} --force",
                    f"depmod -a {KERNELS[1]}", f"update-initramfs -c -k {KERNELS[1]}"]:
        def retry(root, args, p, calls):
            assert f"kernel={KERNELS[0]} result=PASS" in p.stdout
            assert f"kernel={KERNELS[1]} result=FAILED" in p.stdout
            assert p.stdout.count("result=NOT_RUN") == 2
            (root / "fixture/fail").unlink()
            (root / "fixture/calls").unlink()
            again, log = run(root, args)
            assert again.returncode == 0 and log.count("dkms build ") == 4
        case("V5-partial-" + str(len(cases)), lambda r, c=command: put(r, "fixture/fail", c), extra=retry)
    case("V5-ldconfig-error", lambda r: put(r, "fixture/fail", "ldconfig "))

    # A real historical prerm, extracted from its locked Git blob, is the
    # upgrade predecessor. It is not a copy of the new implementation.
    for old in [False, True]:
        root = work / ("V6-old-upgrade" if old else "V6-new-upgrade")
        args = make_root(root)
        p, _ = run(root, args)
        assert p.returncode == 0
        other = root / "var/lib/dkms/innogpu-kernel/2.2"
        other.mkdir(parents=True)
        (other / "source").symlink_to("/usr/src/innogpu-kernel-2.2")
        if old:
            put(root, "DEBIAN/prerm", old_prerm)
        p, calls = run(root, args, "prerm", "upgrade")
        assert p.returncode == 0 and other.exists() != old
        assert not (root / REGISTRY).exists()
        p, calls = run(root, args)
        assert p.returncode == 0
        cases.append(root.name)
        print("PASS " + root.name)

    for action in ["remove", "deconfigure", "remove-error", "remove-noop", "policy-override"]:
        root = work / ("V6-registered-" + action)
        args = make_root(root)
        p, _ = run(root, args)
        assert p.returncode == 0
        if action == "remove-error":
            put(root, "fixture/fail", f"dkms remove -m {MODULE} -v 2.2 --all")
        elif action == "remove-noop":
            put(root, "fixture/mode", "remove-noop")
        elif action == "policy-override":
            put(root, "etc/dkms/framework.conf", "install_tree=/unexpected\n")
        p, calls = run(root, args, "prerm", action if action in ["remove", "deconfigure"] else "remove")
        assert (p.returncode == 0) == (action in ["remove", "deconfigure"])
        assert not re.search(r'^dkms remove -m innogpu', calls, re.M)
        if action == "policy-override":
            assert (root / REGISTRY).exists()
        cases.append(root.name)
        print("PASS " + root.name)

    for script, actions in [("postinst", ["abort-upgrade", "abort-remove", "abort-deconfigure", "invalid"]),
                            ("prerm", ["remove", "deconfigure", "failed-upgrade", "invalid"]),
                            ("postrm", ["remove", "purge", "upgrade", "failed-upgrade", "abort-install", "abort-upgrade", "disappear", "invalid"])]:
        for action in actions:
            root = work / f"V6-{script}-{action}"
            args = make_root(root)
            p, calls = run(root, args, script, action)
            assert (p.returncode == 0) == (action != "invalid")
            if script == "postinst" or action in ["failed-upgrade", "invalid"] and script == "prerm":
                assert not calls
            cases.append(root.name)
            print("PASS " + root.name)

    current = (ROOT / "scripts/build-innogpu-driver.sh").read_text()
    assert 'bash "$ROOT/scripts/generate-fantgpu-maintainer-scripts.sh" "$P" "$VERSION"' in current
    start = 'cat > "$P/DEBIAN/postinst" <<EOF\n'
    end = 'chmod 0755 "$P/DEBIAN/postinst" "$P/DEBIAN/prerm" "$P/DEBIAN/postrm"'
    # The legacy O emission block is byte-identical, avoiding an O policy change.
    assert current[current.index(start):current.index(end)] == baseline[baseline.index(start):baseline.index(end)]
    cases.append("V7-production-wiring-and-O-preservation")
    print("PASS V7-production-wiring-and-O-preservation")
    p = subprocess.run(["bash", str(ROOT / "scripts/build-innogpu-driver.sh")],
                       env={**os.environ, "VERSION": "5.0.0-i6", "SOURCE_DATE_EPOCH": "1789516800",
                            "KERNELDIR": str(work), "STAGE_ROOT": str(work / "must-not-exist")},
                       capture_output=True, text=True)
    print(p.stdout, end=""); print(p.stderr, end="")
    assert p.returncode == 1 and "builder_maintainer_policy=FAIL" in p.stdout
    assert not (work / "must-not-exist").exists()
    cases.append("V7-frozen-version-rebuild-refused")
    print("PASS V7-frozen-version-rebuild-refused")
    print(f"RESULT: PASS_FANTGPU_MAINTAINER_FIXTURES cases={len(cases)} real_builds=0 real_installs=0")


if __name__ == "__main__":
    main()
