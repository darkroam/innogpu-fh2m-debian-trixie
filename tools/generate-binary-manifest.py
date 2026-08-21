#!/usr/bin/env python3
"""Generate binary-manifest.json from the pinned Deepin deb.

Deterministic build-time transform: validates the deb, extracts the payload
paths, records every file (sha256/size/kind/role/license) and symlink
(link_target) under vendor/<...> layout. Writes machine-readable JSON.
"""
import argparse, hashlib, json, os, subprocess, sys, tempfile

VENDOR_PREFIXES = [
    ("usr/src/innogpu-kernel-2.2/", "kernel/"),
    ("usr/lib/x86_64-linux-gnu/innogpu-fh2m/", "userspace/x86_64-linux-gnu/innogpu-fh2m/"),
    ("usr/lib/x86_64-linux-gnu/dri/", "userspace/x86_64-linux-gnu/dri/"),
    ("usr/lib/x86_64-linux-gnu/gbm/", "userspace/x86_64-linux-gnu/gbm/"),
    ("usr/lib/xorg/modules/drivers/", "userspace/xorg/modules/drivers/"),
    ("usr/share/glvnd/", "userspace/share/glvnd/"),
    ("etc/vulkan/", "userspace/etc/vulkan/"),
    ("etc/OpenCL/", "userspace/etc/OpenCL/"),
    ("opt/innogpu/", "opt/innogpu/"),
    ("lib/firmware/innogpu/", "firmware/"),
]

PAYLOAD_ROOTS = [
    "usr/src/innogpu-kernel-2.2",
    "usr/lib/x86_64-linux-gnu/innogpu-fh2m",
    "usr/lib/x86_64-linux-gnu/dri",
    "usr/lib/x86_64-linux-gnu/gbm",
    "usr/lib/xorg/modules/drivers",
    "usr/share/glvnd/egl_vendor.d",
    "etc/vulkan/icd.d",
    "etc/OpenCL/vendors",
    "opt/innogpu",
    "lib/firmware/innogpu",
]

def sha256_file(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()

def vendor_path(src):
    for prefix, vp in VENDOR_PREFIXES:
        if src.startswith(prefix):
            return vp + src[len(prefix):]
    raise ValueError("unmapped payload path: " + src)

def kind_of(src):
    if src.startswith("usr/src/innogpu-kernel-2.2/"):
        return "kernel-black-box"
    if src.startswith("lib/firmware/innogpu/"):
        return "firmware"
    if "/xorg/modules/drivers/" in src:
        return "ddx"
    if src.startswith("opt/innogpu/"):
        return "userspace-lib" if src.endswith(".so") or ".so." in src else "userspace-config"
    return "userspace-lib"

def role_of(src):
    base = os.path.basename(src)
    if ".o_shipped" in base:
        return base.replace(".o_shipped", "").upper() + " (precompiled, read-only)"
    if base.startswith("libVK_"):
        return "Vulkan ICD"
    if base.startswith("libINNOOCL"):
        return "OpenCL ICD"
    if base.startswith("libsrv_um"):
        return "services UMD"
    if base.startswith("libusc"):
        return "USC shader compiler"
    if base.startswith("libufwriter"):
        return "UF writer"
    if base.startswith("libGL") or base.startswith("libEGL") or base.startswith("libGLX") or base.startswith("libglapi"):
        return "GL/EGL stack"
    if base.startswith("libinno_codec") or base == "innogpu_drv_video.so":
        return "video codec"
    if base.startswith("libifbc"):
        return "IFBC frame-buffer compression"
    if base in ("inno_dri.so", "innogpu_dri.so", "kms_swrast_inno_dri.so", "swrast_inno_dri.so"):
        return "DRI driver"
    if base == "innogpu_gbm.so" or base.startswith("libinnogpu_gbm"):
        return "GBM backend"
    if base == "innogpu_drv.so":
        return "Xorg DDX"
    if base.startswith("fh2"):
        return "GPU firmware"
    if base.endswith(".json"):
        return "GLVND/ICD manifest"
    if base.endswith(".icd"):
        return "OpenCL ICD manifest"
    if base.endswith(".conf"):
        return "ALSA UCM config"
    return "userspace component"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("deb", nargs="?", default="debs/innogpu-fh2m_20250421190503-debug_amd64.deb")
    ap.add_argument("--out", default="binary-manifest.json")
    ap.add_argument("--expected-deb-sha256", default="b5a70e7854db6e199d208ff31296ff637f59b5731d31e8123f95c39009f6f5b2")
    args = ap.parse_args()

    deb_sha = sha256_file(args.deb)
    if deb_sha != args.expected_deb_sha256:
        print("manifest_source_deb_sha256=FAIL got=%s expected=%s" % (deb_sha, args.expected_deb_sha256))
        sys.exit(1)

    tmp = tempfile.mkdtemp(prefix="inno-manifest-", dir=".build")
    try:
        subprocess.run(["dpkg-deb", "-x", args.deb, tmp], check=True)
        entries = []
        seen_src = set()
        seen_vp = set()
        for root in PAYLOAD_ROOTS:
            absp = os.path.join(tmp, root)
            if not os.path.exists(absp):
                continue
            for dirpath, dirnames, filenames in os.walk(absp):
                dirnames.sort(); filenames.sort()
                for name in filenames:
                    full = os.path.join(dirpath, name)
                    rel = os.path.relpath(full, tmp)
                    # 内核根目录只收录黑盒 .o_shipped（源码在 drivers/，产物 .o.cmd 排除）
                    if rel.startswith("usr/src/innogpu-kernel-2.2/") and ".o_shipped" not in name:
                        continue
                    if rel in seen_src:
                        continue
                    seen_src.add(rel)
                    vp = vendor_path(rel)
                    if vp in seen_vp:
                        print("manifest_duplicate_target=FAIL %s" % vp)
                        sys.exit(1)
                    seen_vp.add(vp)
                    if os.path.islink(full):
                        entries.append({
                            "source_path": rel, "vendor_path": vp,
                            "link_target": os.readlink(full),
                            "kind": kind_of(rel), "role": role_of(rel), "license": "vendor-binary",
                        })
                    else:
                        st = os.stat(full)
                        entries.append({
                            "source_path": rel, "vendor_path": vp,
                            "sha256": sha256_file(full), "size": st.st_size,
                            "kind": kind_of(rel), "role": role_of(rel), "license": "vendor-binary",
                        })
        manifest = {
            "format_version": 1,
            "source_package": "innogpu-fh2m",
            "source_version": "20250421190503-debug",
            "source_deb_sha256": deb_sha,
            "architecture": "amd64",
            "entries": entries,
        }
        with open(args.out, "w") as f:
            json.dump(manifest, f, indent=2, sort_keys=True)
            f.write("\n")
        n_files = sum(1 for e in entries if "sha256" in e)
        n_links = len(entries) - n_files
        print("manifest_entries=%d files=%d symlinks=%d" % (len(entries), n_files, n_links))
        print("manifest_source_deb_sha256=PASS")
        print("manifest_written=%s" % args.out)
        # 自校验输出清单
        rc = subprocess.run([sys.executable, "tools/validate-binary-manifest.py", args.out]).returncode
        if rc != 0:
            print("manifest_self_validation=FAIL")
            sys.exit(1)
        print("manifest_self_validation=PASS")
    finally:
        subprocess.run(["rm", "-rf", tmp], check=False)

if __name__ == "__main__":
    main()
