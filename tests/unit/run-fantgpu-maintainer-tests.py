#!/usr/bin/env python3
"""Run generated control scripts in a minimal, rootless bubblewrap filesystem.

No host /boot, /dev, /sys, DKMS or package database is exposed. A private
network namespace is mandatory. Default mode never runs a compiler/installer.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
from types import SimpleNamespace
from unittest.mock import patch

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


def mounts(root, binaries=("/usr/bin/bash", "/usr/bin/busybox")):
    # Bind only the requested executables and their ordinary library dependencies.
    args = ["bwrap", "--unshare-user", "--unshare-pid", "--unshare-ipc",
            "--unshare-uts", "--unshare-net", "--die-with-parent", "--new-session", "--clearenv",
            "--bind", str(root), "/", "--chdir", "/"]
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
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--native-preflight", action="store_true")
    mode.add_argument("--native-n0-test", action="store_true")
    mode.add_argument("--native-build-test", action="store_true")
    mode.add_argument("--native-run", action="store_true")
    ap.add_argument("--manifest", type=Path)
    ap.add_argument("--reviewed-sha256")
    opts = ap.parse_args()
    work = opts.work_dir.absolute()
    if work.resolve() != work:
        raise SystemExit("work-dir must not traverse symlinks or parent aliases")
    if not __debug__:
        raise SystemExit("assertions must be enabled; do not use python -O")
    if opts.native_preflight or opts.native_n0_test or opts.native_build_test or opts.native_run:
        import fantgpu_native
        expected = (work if opts.native_build_test else
                    fantgpu_native.N0_TEST_WORK / "preflight" if opts.native_n0_test else
                    fantgpu_native.WORK / "preflight" if opts.native_preflight else fantgpu_native.WORK)
        if work != expected or (opts.native_preflight and work.exists()):
            raise SystemExit("native work-dir must match the approved batch/preflight path")
        if opts.native_build_test:
            if opts.manifest or opts.reviewed_sha256:
                raise SystemExit("review identity options require --native-run")
            fantgpu_native.build_test(work)
        elif opts.native_preflight or opts.native_n0_test:
            if opts.manifest or opts.reviewed_sha256:
                raise SystemExit("review identity options require --native-run")
            fantgpu_native.preflight(work, test_only=opts.native_n0_test)
        else:
            if not opts.manifest or not re.fullmatch(r"[0-9a-f]{64}", opts.reviewed_sha256 or ""):
                raise SystemExit("native-run requires reviewed manifest and full SHA-256")
            fantgpu_native.run(work, opts.manifest, opts.reviewed_sha256)
        return
    if opts.manifest or opts.reviewed_sha256:
        raise SystemExit("review identity options require --native-run")
    from fantgpu_native import WORK
    approved_synthetic = WORK / "synthetic"
    if (not str(work).startswith("/tmp/r5-phase2-") and work != approved_synthetic) or work.exists():
        raise SystemExit("work-dir must be fresh and inside an approved fixture location")
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
    # Both callers use the production function, including parent-directory checks.
    fixed = "etc/dkms/framework.conf.d/autoinstall_all_kernels.conf"
    accepted = 'autoinstall_all_kernels="yes"\n'
    policies = {
        "exact": (fixed, accepted),
        "value": (fixed, accepted.replace("yes", "no")),
        "newline": (fixed, accepted.rstrip()),
        "extra": (fixed, accepted + "modprobe_on_install=true\n"),
        "injection": (fixed, accepted + "x=$(touch /fixture/policy-executed)\n"),
        "other-path": ("etc/dkms/framework.conf.d/other.conf", accepted),
        "file-link": (fixed, accepted),
        "directory-link": (fixed, accepted),
        "dangling": (fixed, accepted),
    }
    for policy, (path, contents) in policies.items():
        for script, action in [("postinst", "configure"), ("prerm", "upgrade")]:
            name = f"policy-{policy}-{script}"
            root = work / name
            args = make_root(root)
            config = put(root, path, contents)
            if policy in ["file-link", "dangling"]:
                config.unlink()
                put(root, "fixture/approved.conf", accepted)
                config.symlink_to("/fixture/approved.conf" if policy == "file-link" else "/absent")
            elif policy == "directory-link":
                config.parent.rename(root / "fixture/configs")
                config.parent.symlink_to("/fixture/configs")
            p, calls = run(root, args, script, action)
            assert (p.returncode == 0) == (policy == "exact"), name
            assert not (root / "fixture/policy-executed").exists(), name
            if policy != "exact":
                assert not re.search(r'^dkms (add|build|install|remove) ', calls, re.M), name
            cases.append(name)
            print("PASS " + name)
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
    assert p.returncode == 1 and "staging_ostage_generation=FAIL" in p.stdout
    assert not (work / "must-not-exist").exists()
    cases.append("V7-frozen-version-rebuild-refused")
    print("PASS V7-frozen-version-rebuild-refused")
    spec = importlib.util.spec_from_file_location("abi", ROOT / "tools/check-fantgpu-shipped-abi.py")
    abi = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(abi)
    layout = "struct dev_rsrc {\n" + "".join(
        f" int {field}; /* {offset} 8 */\n" for field, offset in abi.OFFSETS.items()) + "/* size: 140536, members: 115 */\n};\n"
    abi.check(layout)
    for broken in ["", layout + layout, layout.replace("115", "116"),
                   layout.replace("140536", "140544"),
                   *[layout.replace(str(offset), str(offset + 8)) for offset in abi.OFFSETS.values()]]:
        try:
            abi.check(broken)
        except ValueError:
            continue
        raise AssertionError("ABI drift accepted")
    cases.append("ABI-complete-ambiguous-and-every-fixed-offset")
    print("PASS " + cases[-1])
    import fantgpu_native as native
    package = work / "package-links"
    put(package, "usr/share/helper", "fixture\n")
    (package / "usr/bin").mkdir()
    link = package / "usr/bin/helper"
    link.symlink_to("../share/helper")
    native.check_package_links(package)
    outside = put(work, "outside-package", "must not be accepted\n")
    for target in [str(outside), "../../../outside-package", "missing", "helper"]:
        link.unlink()
        link.symlink_to(target)
        try:
            native.check_package_links(package)
        except (AssertionError, OSError, RuntimeError):
            pass
        else:
            raise AssertionError("unsafe package link accepted: " + target)
    cases.append("native-package-relative-links-and-escape-rejection")
    link.unlink()
    link.symlink_to("../share/helper")
    target_root = work / "package-destination"
    put(target_root, "usr/share/helper", "old content\n")
    (target_root / "usr/bin").mkdir()
    (target_root / "usr/bin/helper").symlink_to("../share/helper")
    (target_root / "usr/lib").mkdir()
    (target_root / "lib").symlink_to("usr/lib")
    put(package, "lib/firmware/test", "firmware fixture\n")
    native.copy_package(package, target_root)
    assert (target_root / "usr/bin/helper").read_text() == "fixture\n"
    assert (target_root / "usr/lib/firmware/test").read_text() == "firmware fixture\n"
    assert (target_root / "lib").is_symlink()
    (target_root / "lib").unlink()
    (target_root / "lib").symlink_to(work)
    try:
        native.copy_package(package, target_root)
    except AssertionError:
        pass
    else:
        raise AssertionError("escaped package destination accepted")
    cases.append("native-package-replaces-links-and-preserves-usrmerge")
    root = work / "tool-chain"
    binary = put(root, "bin/real-tool", "#!/bin/sh\nexit 0\n")
    binary.chmod(0o755)
    alias = root / "bin/tool"
    middle = root / "bin/alternative"
    alias.symlink_to("alternative")
    middle.symlink_to("real-tool")
    with patch.object(native, "TOOL_INPUTS", [str(root / "bin")]), \
         patch.object(native, "TOOL_COMMANDS", ["tool"]), \
         patch.object(native.shutil, "which", return_value=str(alias)):
        assert native.tool_link_inputs() == sorted([alias, middle, binary])
        before = native.identity(native.tool_link_inputs())
        for target in ["missing", "tool", "/sys/forbidden", "../../outside"]:
            middle.unlink()
            middle.symlink_to(target)
            try:
                native.tool_link_inputs()
            except AssertionError:
                pass
            else:
                raise AssertionError("unsafe/broken tool chain accepted: " + target)
        middle.unlink()
        middle.symlink_to("real-tool")
        binary.write_text("#!/bin/sh\nexit 1\n")
        assert native.identity(native.tool_link_inputs()) != before
    cases.append("native-tool-chain-lock-and-reject")
    print("PASS " + cases[-1])
    root = work / "real-awk-chain"
    root.mkdir()
    with patch.object(native, "TOOL_COMMANDS", ["awk"]):
        chain = native.tool_link_inputs()
    executable = Path("/usr/bin/awk").resolve(strict=True)
    args = mounts(root, binaries=("/usr/bin/bash", str(executable)))
    for link in chain:
        if link.is_symlink() and not link.is_relative_to("/etc/alternatives"):
            dest = root / link.relative_to("/")
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.symlink_to(os.readlink(link))
    argv = args + ["--", "/usr/bin/bash", "-ec", "printf 'chain PASS\\n' | /usr/bin/awk '{print $2}'"]
    negative = subprocess.run(argv, capture_output=True, text=True)
    assert negative.returncode == 127, negative.stderr
    for link in chain:
        if link.is_relative_to("/etc/alternatives"):
            dest = root / link.relative_to("/")
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(link, dest, follow_symlinks=False)
    positive = subprocess.run(argv, capture_output=True, text=True)
    assert positive.returncode == 0 and positive.stdout == "PASS\n", positive.stderr
    put(root, "regression.log", "negative_rc=127\n" + negative.stderr + "positive_rc=0\n" + positive.stdout)
    cases.append("native-real-awk-broken-and-repaired-chain")
    print("PASS " + cases[-1])
    devices = native.pseudo_devices()
    assert devices == {"/dev/null": [1, 3], "/dev/zero": [1, 5]}
    for mode, rdev in [(native.stat.S_IFREG, 0), (native.stat.S_IFCHR, os.makedev(8, 0))]:
        with patch.object(Path, "lstat", return_value=SimpleNamespace(st_mode=mode, st_rdev=rdev)):
            try:
                native.pseudo_devices()
            except AssertionError:
                pass
            else:
                raise AssertionError("unsafe device identity accepted")
    for name in devices:
        put(root, name.lstrip("/"), "")
        args += ["--dev-bind", name, name]
    subprocess.run(args + ["--", "/usr/bin/bash", "-ec",
                   "test -c /dev/null && test -c /dev/zero; printf discarded > /dev/null; "
                   "test -z \"$(read -r -n 1 byte < /dev/null; printf '%s' \"$byte\")\""], check=True)
    cases.append("native-software-devices-identity-and-null-eof")
    print("PASS " + cases[-1])
    root = work / "native-guard"
    args = make_root(root)
    (root / "evidence").mkdir()
    for name in native.DENIED:
        guard = put(root, "usr/bin/" + name, native.GUARD)
        guard.chmod(0o755)
        p = subprocess.run(args + ["--", "/bin/bash", str("/usr/bin/" + name)], capture_output=True)
        assert p.returncode == 97, name
    records = (root / "evidence/forbidden.log").read_text().splitlines()
    assert len(records) == len(native.DENIED)
    subprocess.run(["bash", "-n"], input=native.STRIP, text=True, check=True)
    assert native.K == ["6.12.101+deb13-amd64"]
    assert native.HOST_K == sorted(set(native.HOST_K)) and len(native.HOST_K) == 8
    assert set(native.K) < set(native.HOST_K)
    cases.append("native-denied-commands-and-strip-syntax")
    root = work / "native-ignored-strip-status"
    args = make_root(root)
    (root / "evidence").mkdir()
    put(root, "fixture/kernels", KERNELS[0] + "\n")
    ko = f"/lib/modules/{KERNELS[0]}/updates/fantgpu.ko"
    put(root, ko.lstrip("/"), "fixture module\n")
    put(root, "fixture/strip", native.STRIP)
    put(root, "usr/bin/pahole", "#!/bin/bash\nexit 23\n").chmod(0o755)
    ledger = []
    start = time.monotonic()
    with patch.object(native, "mounts", return_value=args):
        try:
            native.command(root, {}, ["/bin/bash", "-c",
                f"export PATH=/usr/bin; /bin/bash /fixture/strip -g {ko} || true; "
                "touch /evidence/continued"], ledger, start + 10)
        except AssertionError:
            pass
        else:
            raise AssertionError("ignored strip failure was accepted")
    assert (root / "evidence/abi-failed").read_text().strip() == "strip_prepare_rc=23"
    assert ledger[-1]["abort"] == "abi-failed" and ledger[-1]["rc"] != 0
    assert time.monotonic() - start < 5 and not (root / "evidence/continued").exists()
    cases.append("native-abi-failure-stops-ignoring-caller")
    print("PASS " + cases[-1])
    root = work / "native-dependency-capture"
    root.mkdir()
    put(root, "dev/null", "")
    for name, content in [("fantgpu/kernel_autocfg.h", "#define A 1\n"),
                          ("fantgpu.mod", "a.o\n"), ("fantgpu.mod.c", "fixture\n"),
                          (".a.o.cmd", "dependency fixture\n")]:
        put(root, "build/" + name, content)
    (root / "evidence").mkdir()
    args = mounts(root, tuple("/usr/bin/" + name for name in
                  ["bash", "dirname", "cp", "find", "sort", "xargs", "sha256sum", "tar"]))
    start_capture = native.STRIP.index('build=$(dirname')
    capture = native.STRIP[start_capture:native.STRIP.index("\nprintf ", start_capture)]
    p = subprocess.run(args + ["--", "/usr/bin/bash", "-ec",
        'export PATH=/usr/bin; set -- -g /build/fantgpu.ko; dest=/evidence; ' + capture], capture_output=True)
    assert p.returncode == 0, p.stderr
    assert not (root / "dev/fd").exists()
    members = subprocess.check_output(["tar", "-tf", str(root / "evidence/dependency-files.tar")], text=True)
    assert members == ".a.o.cmd\n"
    assert (root / "evidence/kernel_autocfg.h").read_text() == "#define A 1\n"
    cases.append("native-dependency-capture-without-dev-fd")
    print("PASS " + cases[-1])
    root = work / "n0-receipt"
    common = root / "preflight/common"
    manifest = put(root, "preflight/inputs.json", "{}\n")
    for name in ["var/lib/dkms/mok.pub", "evidence/commands.json", *[f"boot/initrd.img-{k}" for k in native.K[::2]]]:
        put(common, name, "fixture artifact\n")
    key = put(common, "var/lib/dkms/mok.key", "fixture placeholder, not a private key\n")
    key.chmod(0o600)
    good_state = {"status": "PASS", "purpose": "formal", "deadline_unix": time.time() + 600,
                  "input_sha256": native.sha(manifest), "artifacts": native.n0_artifacts(common)}
    receipt = put(root, "preflight/n0.json", json.dumps(good_state))
    with patch.object(native, "WORK", root):
        assert native.verified_n0(root, manifest, native.sha(manifest)) == good_state
        for name, change in [("failed", {"status": "FAILED_OR_UNVERIFIED"}), ("test-only", {"purpose": "regression"}),
                             ("expired", {"deadline_unix": 0}), ("input-mix", {"input_sha256": "0" * 64}),
                             ("artifact-mix", {"artifacts": {}}), ("missing", None)]:
            if change is None:
                receipt.unlink()
            else:
                receipt.write_text(json.dumps({**good_state, **change}))
            # A bad N0 must fail before input rehash or any N1 subprocess.
            with patch.object(native, "inputs", side_effect=RuntimeError("N1 gate bypassed")):
                try:
                    native.run(root, manifest, native.sha(manifest))
                except (AssertionError, FileNotFoundError):
                    pass
                else:
                    raise AssertionError("invalid N0 accepted: " + name)
            assert not (root / "A").exists() and not (root / "synthetic").exists()
            cases.append("native-n0-reject-" + name)
            print("PASS " + cases[-1])
    key.unlink()
    fresh = work / "fresh-attempt"
    with patch.object(native, "WORK", fresh), patch.object(native, "EVIDENCE", work / "fresh-evidence"):
        native.claim_preflight(fresh / "preflight")
        for candidate in [fresh / "preflight", root / "preflight"]:
            try:
                native.claim_preflight(candidate)
            except AssertionError:
                pass
            else:
                raise AssertionError("old attempt reused")
    alias = work / "alias"
    alias.symlink_to(root, target_is_directory=True)
    with patch.object(native, "WORK", alias):
        try:
            native.claim_preflight(alias / "preflight")
        except AssertionError:
            pass
        else:
            raise AssertionError("redirected attempt accepted")
    cases.append("native-n0-fresh-old-and-symlink-roots")
    print("PASS " + cases[-1])
    failed = work / "n0-interrupted"
    def interrupted_prepare(root, lock, deadline):
        put(root, "evidence/partial.log", "injected initrd failure\n")
        raise RuntimeError("injected initrd failure")
    with patch.object(native, "WORK", failed), patch.object(native, "EVIDENCE", work / "unused-evidence"), \
         patch.object(native, "inputs", return_value={}), \
         patch.object(native.shutil, "disk_usage", return_value=SimpleNamespace(free=2**40)), \
         patch.object(native, "prepare_n0", side_effect=interrupted_prepare):
        try:
            native.preflight(failed / "preflight")
        except RuntimeError as error:
            assert str(error) == "injected initrd failure"
        else:
            raise AssertionError("partial N0 accepted")
    assert not (failed / "preflight/n0.json").exists()
    assert json.loads((failed / "preflight/result.json").read_text())["N0"] == "FAILED_OR_UNVERIFIED"
    cases.append("native-n0-partial-failure-never-green")
    print("PASS " + cases[-1])
    for variant in ["default-path-missing", "explicit-config-missing", "explicit-config"]:
        root = work / ("native-openssl-" + variant)
        root.mkdir(mode=0o700)
        (root / "var/lib/dkms").mkdir(parents=True)
        put(root, "dev/null", "")
        if variant != "explicit-config-missing":
            put(root, "etc/ssl/openssl.cnf", Path("/etc/ssl/openssl.cnf").read_text())
        assert not (root / "usr/lib/ssl/openssl.cnf").exists()
        args = mounts(root, ("/usr/bin/openssl",)) + ["--setenv", "PATH", "/usr/bin", "--setenv", "LC_ALL", "C", "--"]
        argv = native.SIGNING_REQUEST.copy()
        if variant == "default-path-missing":
            index = argv.index("-config")
            del argv[index:index + 2]
        key = root / "var/lib/dkms/mok.key"
        cert = root / "var/lib/dkms/mok.pub"
        try:
            p = subprocess.run(args + argv, capture_output=True, text=True, timeout=30)
            put(root, "req.log", p.stdout + p.stderr)
            print(f"--- {root.name}: rc={p.returncode} ---")
            assert (p.returncode == 0) == (variant == "explicit-config"), p.stderr
            if p.returncode:
                assert "openssl.cnf" in p.stderr and "No such file or directory" in p.stderr
                assert not key.exists() and not cert.exists()
            else:
                assert key.stat().st_mode & 0o777 == 0o600
                p = subprocess.run(args + ["openssl", "x509", "-inform", "DER", "-in", "/var/lib/dkms/mok.pub",
                                          "-noout", "-subject", "-serial", "-ext", "subjectKeyIdentifier,extendedKeyUsage"],
                                   capture_output=True, text=True, timeout=30)
                put(root, "certificate.log", p.stdout + p.stderr)
                assert p.returncode == 0 and "CN=R27 offline test only" in p.stdout.replace(" = ", "=")
                assert re.search(r"(?:[0-9A-F]{2}:){19}[0-9A-F]{2}", p.stdout)
                assert "Code Signing" in p.stdout
                serial = re.search(r"^serial=([0-9A-F]+)$", p.stdout, re.M).group(1)
                assert native.certificate_sig_key(p.stdout).replace(":", "") == serial.lower()
                assert native.certificate_sig_key("serial=01AB\nSubject Key Identifier: 11:22\n") == "01:ab"
                for bad in ["", "serial=A\n", "serial=ZZ\n", "serial=01\nserial=02\n"]:
                    try:
                        native.certificate_sig_key(bad)
                    except AssertionError:
                        pass
                    else:
                        raise AssertionError("invalid certificate serial accepted")
        finally:
            key.unlink(missing_ok=True)  # No private-key reads, logs or retained fixture keys.
        cases.append(root.name)
        print("PASS " + cases[-1])
    print(f"RESULT: PASS_FANTGPU_MAINTAINER_FIXTURES cases={len(cases)} real_builds=0 real_installs=0")


if __name__ == "__main__":
    main()
