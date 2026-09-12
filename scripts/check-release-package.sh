#!/bin/bash
# Read-only release gate for coherent packages built from the current source tree.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
DEB="${1:-}"

if [[ -z "$DEB" || $# -ne 1 ]]; then
    echo "Usage: scripts/check-release-package.sh DEB" >&2
    exit 2
fi
[[ -f "$DEB" ]] || { echo "ERROR: package not found: $DEB" >&2; exit 1; }
command -v dpkg-deb >/dev/null 2>&1 || { echo "ERROR: dpkg-deb is required" >&2; exit 1; }
command -v tar >/dev/null 2>&1 || { echo "ERROR: tar is required" >&2; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "ERROR: python3 is required" >&2; exit 1; }

package=$(dpkg-deb -f "$DEB" Package)
version=$(dpkg-deb -f "$DEB" Version)
architecture=$(dpkg-deb -f "$DEB" Architecture)
installed_size=$(dpkg-deb -f "$DEB" Installed-Size)
# C1-①：包名白名单增加 fantgpu 血统，且**包名与版本血统强制配对**
# （codex 初审 P1-4：禁止 innogpu↔5.0.0-iN / fantgpu↔patched·4.x 笛卡尔积）。
case "$package" in
    innogpu-fh2m-trixie)
        if ! { [[ "$version" =~ ^3\.3\.3\.42-patched-([0-9]+)$ ]] && (( 10#${BASH_REMATCH[1]} > 20 )); } &&
           ! [[ "$version" =~ ^4\.0\.(0|[1-9][0-9]*)-i([1-9][0-9]*)$ ]]; then
            echo "ERROR: innogpu-fh2m-trixie only accepts patched-N (N>20) or 4.0.x-iN versions: $version" >&2
            exit 1
        fi
        ;;
    fantgpu-fh2m-trixie)
        [[ "$version" =~ ^5\.0\.0-i([1-9][0-9]*)$ ]] || {
            echo "ERROR: fantgpu-fh2m-trixie only accepts 5.0.0-iN versions: $version" >&2
            exit 1
        }
        ;;
    *)
        echo "ERROR: unexpected package: $package" >&2
        exit 1
        ;;
esac
[[ "$architecture" == "amd64" ]] || {
    echo "ERROR: unexpected architecture: $architecture" >&2
    exit 1
}
[[ "$installed_size" =~ ^[1-9][0-9]*$ ]] || {
    echo "ERROR: missing or invalid Installed-Size: $installed_size" >&2
    exit 1
}

runtime=$(mktemp -d "${TMPDIR:-/tmp}/innogpu-package-audit.XXXXXX")
trap 'rm -rf "$runtime"' EXIT HUP INT TERM
dpkg-deb --fsys-tarfile "$DEB" | tar -tf - |
    sed -e 's#^\./##' -e 's#/$##' > "$runtime/files"

# 带类型清单（type<TAB>path[<TAB>link-target]；codex 初审 P1-2：required 断言
# 必须验证对象类型与链接有效性，不得只做路径字符串存在性检查）。
LC_ALL=C dpkg-deb --fsys-tarfile "$DEB" | LC_ALL=C tar -tvf - |
    sed -n -e 's#^\(.\).* \./\(.*\) -> \(.*\)$#\1\t\2\t\3#p' \
           -e 's#^\(.\).* \./\(.*\)/$#\1\t\2#p' \
           -e 's#^\(.\).* \./\(.*\)$#\1\t\2#p' > "$runtime/typed"

if grep -Eq '\.(orig|rej)$' "$runtime/files"; then
    echo "ERROR: patch backup or reject file is present in package" >&2
    exit 1
fi

# C1-②：required/forbidden 载荷断言按血统（fantgpu 按 F 内核期望路径，
# 与 validation-plan §三 C1-② 定稿口径一致；O 血统保持历史清单）。
helper_cmp_mode="innogpu"
if [[ "$package" == "fantgpu-fh2m-trixie" ]]; then
    helper_cmp_mode="fantgpu"
    required=(
        lib/firmware/fantgpu/fh2m/fh2m.fw
        lib/firmware/fantgpu/fh2m/fh2m.sh
        lib/firmware/fantgpu/fh2m/fh2c.fw
        lib/firmware/fantgpu/fh2m/fh2c.sh
        usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so
        usr/lib/x86_64-linux-gnu/dri/fh2m_drv_video.so
        usr/lib/x86_64-linux-gnu/gbm/fh2m_gbm.so
        usr/lib/xorg/modules/drivers/fh2m_drv.so
        usr/share/glvnd/egl_vendor.d/00_fh2m.json
        etc/vulkan/icd.d/fh2m_conf.json
        etc/OpenCL/vendors/FANT_fh2m.icd
        usr/share/drirc.d/01-fh2m_drv.conf
        etc/modprobe.d/blacklist-fh2m.conf
        usr/share/X11/xorg.conf.d/10-fh2m.conf
        etc/ld.so.conf.d/0-fantgpu-hwgl.conf
        # codex 初审 P1-1 + 复审 P1：权威清单（validation-plan §三 C3 第二层
        # :257-260）私有库的**完整运行时链接链**（配置/ICD 引用名 + 各 symlink
        # 成员 + 真身 ELF），x86_64 运行集；任一链成员缺失/悬空均 fail。
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libfh2m_gbm.so
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libfh2m_gbm.so.1
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libfh2m_gbm.so.1.0.0
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libgbm.so
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libgbm.so.1
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libgbm.so.1.0.0
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libVK_FANT_fh2m.so
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libVK_FANT_fh2m.so.1
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libEGL_fh2m.so
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libEGL_fh2m.so.0
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libEGL_fh2m.so.1
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libEGL_fh2m_l.so
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libEGL_fh2m_l.so.0
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libEGL_fh2m_l.so.0.0.0
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libFTOCL_fh2m.so
        usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libFTOCL_fh2m.so.1
        "usr/share/$package/restore-dp1-mode-x11.sh"
        "usr/share/$package/xdisplay-session.sh"
        "usr/share/$package/install-xdisplay-user.sh"
    )
    forbidden=(
        usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so
        usr/lib/x86_64-linux-gnu/dri/innogpu_drv_video.so
        usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so
        usr/lib/xorg/modules/drivers/innogpu_drv.so
        usr/share/glvnd/egl_vendor.d/00_inno.json
        usr/share/innogpu-fh2m-trixie
        usr/bin/innogpu-repair-dri-nodes
        usr/sbin/innogpu-repair-dri-nodes
    )
else
    required=(
        lib/firmware/innogpu/fh2m.fw
        lib/firmware/innogpu/fh2m.sh
        lib/firmware/innogpu/fh2c.fw
        lib/firmware/innogpu/fh2c.sh
        usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so
        usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so
        usr/lib/x86_64-linux-gnu/innogpu-fh2m/libgbm.so.1.0.0
        usr/lib/x86_64-linux-gnu/innogpu-fh2m/libglapi_inno.so.0.0.0
        usr/lib/xorg/modules/drivers/innogpu_drv.so
        usr/share/glvnd/egl_vendor.d/00_inno.json
        "usr/share/$package/restore-dp1-mode-x11.sh"
        "usr/share/$package/xdisplay-session.sh"
        "usr/share/$package/install-xdisplay-user.sh"
    )
    forbidden=(
        usr/share/innogpu-fh2m-trixie/xdisplay.sh
        usr/share/innogpu-fh2m-trixie/displayselect
        usr/share/innogpu-fh2m-trixie/install-kylin-userspace.sh
        usr/share/innogpu-fh2m-trixie/install-experimental-hwgl.sh
        usr/share/innogpu-fh2m-trixie/patch-skip-first-gpupll.sh
        usr/bin/innogpu-install-kylin-userspace
        usr/bin/innogpu-install-experimental-hwgl
        usr/bin/innogpu-skip-first-gpupll
        usr/sbin/innogpu-install-kylin-userspace
        usr/sbin/innogpu-install-experimental-hwgl
        usr/sbin/innogpu-skip-first-gpupll
    )
fi
printf '%s\n' "${required[@]}" > "$runtime/required.list"
printf '%s\n' "${forbidden[@]}" > "$runtime/forbidden.list"
# codex 复审#2 P1：F 血统内容断言——ELF 真身必须带 \x7fELF magic（空文件/
# 非 ELF 冒充必须 fail），配置文件必须实际引用对应库（token 与真品一致）。
# 两清单内的条目**强制为普通文件**（复审#3 P1：有效 symlink 同样拒绝）。
# 注意 gbm/fh2m_gbm.so 在真品中是 symlink（M2 shim → fantgpu-fh2m/
# libfh2m_gbm.so），不入 elfs 清单——其内容断言由链末真身
# libfh2m_gbm.so.1.0.0 承载。O 血统保持历史口径（无内容断言）。
if [[ "$package" == "fantgpu-fh2m-trixie" ]]; then
    cat > "$runtime/elfs.list" <<'EOF'
usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so
usr/lib/x86_64-linux-gnu/dri/fh2m_drv_video.so
usr/lib/xorg/modules/drivers/fh2m_drv.so
usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libfh2m_gbm.so.1.0.0
usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libgbm.so.1.0.0
usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libVK_FANT_fh2m.so.1
usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libEGL_fh2m.so.1
usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libEGL_fh2m_l.so.0.0.0
usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libFTOCL_fh2m.so.1
EOF
    printf '%s\n' \
        'etc/vulkan/icd.d/fh2m_conf.json	libVK_FANT_fh2m.so' \
        'usr/share/glvnd/egl_vendor.d/00_fh2m.json	libEGL_fh2m.so.0' \
        'etc/OpenCL/vendors/FANT_fh2m.icd	libFTOCL_fh2m.so' \
        > "$runtime/cfgrefs.tsv"
    # codex 复审#4 P1：required symlink 的期望 target 映射（与真品逐字节一致，
    # 从 build-A typed 清单核对）——target 错误或链成员被换成普通文件均 fail。
    printf '%s\n' \
        'usr/lib/x86_64-linux-gnu/gbm/fh2m_gbm.so	../fantgpu-fh2m/libfh2m_gbm.so' \
        'usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libfh2m_gbm.so	libfh2m_gbm.so.1' \
        'usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libfh2m_gbm.so.1	libfh2m_gbm.so.1.0.0' \
        'usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libgbm.so	libgbm.so.1' \
        'usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libgbm.so.1	libgbm.so.1.0.0' \
        'usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libVK_FANT_fh2m.so	libVK_FANT_fh2m.so.1' \
        'usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libEGL_fh2m.so	libEGL_fh2m.so.1' \
        'usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libEGL_fh2m.so.0	libEGL_fh2m.so' \
        'usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libEGL_fh2m_l.so	libEGL_fh2m_l.so.0' \
        'usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libEGL_fh2m_l.so.0	libEGL_fh2m_l.so.0.0.0' \
        'usr/lib/x86_64-linux-gnu/fantgpu-fh2m/libFTOCL_fh2m.so	libFTOCL_fh2m.so.1' \
        > "$runtime/links.tsv"
else
    : > "$runtime/elfs.list"
    : > "$runtime/cfgrefs.tsv"
    : > "$runtime/links.tsv"
fi
python3 - "$DEB" "$runtime/typed" "$runtime/required.list" "$runtime/forbidden.list" \
    "$runtime/elfs.list" "$runtime/cfgrefs.tsv" "$runtime/links.tsv" <<'PYEOF'
import sys
import subprocess
import tarfile

deb, typed_path, required_path, forbidden_path, elfs_path, cfgrefs_path, links_path = \
    sys.argv[1:8]
entries = {}
with open(typed_path, encoding="utf-8", errors="surrogateescape") as f:
    for line in f:
        parts = line.rstrip("\n").split("\t")
        entries[parts[1]] = (parts[0], parts[2] if len(parts) > 2 else None)

elf_paths = set()
with open(elfs_path, encoding="utf-8", errors="surrogateescape") as f:
    elf_paths = {line.rstrip("\n") for line in f if line.strip()}
cfgrefs = {}
with open(cfgrefs_path, encoding="utf-8", errors="surrogateescape") as f:
    for line in f:
        parts = line.rstrip("\n").split("\t")
        if len(parts) >= 2 and parts[0].strip():
            cfgrefs[parts[0]] = [t for t in parts[1:] if t]
link_expect = {}
with open(links_path, encoding="utf-8", errors="surrogateescape") as f:
    for line in f:
        parts = line.rstrip("\n").split("\t")
        if len(parts) >= 2 and parts[0].strip():
            link_expect[parts[0]] = parts[1]

content = {}
if elf_paths or cfgrefs:
    proc = subprocess.Popen(
        ["dpkg-deb", "--fsys-tarfile", deb], stdout=subprocess.PIPE)
    with tarfile.open(fileobj=proc.stdout, mode="r|") as tf:
        for m in tf:
            name = m.name[2:] if m.name.startswith("./") else m.name
            if name in elf_paths and m.isfile():
                content[name] = tf.extractfile(m).read(4)
            elif name in cfgrefs and m.isfile():
                content[name] = tf.extractfile(m).read()
    proc.wait()
    if proc.returncode != 0:
        sys.exit(1)


def resolve(p, hops=0):
    # 链接链解析：包内相对/绝对目标，最终必须落在常规文件上；防环。
    if hops > 8:
        return ("loop", None)
    if p not in entries:
        return ("dangling", None)
    t, target = entries[p]
    if t == "l":
        if target is None:
            return ("dangling", None)
        import posixpath
        nxt = target.lstrip("/") if target.startswith("/") else \
            posixpath.normpath(posixpath.join(posixpath.dirname(p), target))
        return resolve(nxt, hops + 1)
    return (t, p)


errors = []
with open(required_path, encoding="utf-8", errors="surrogateescape") as f:
    for p in (line.rstrip("\n") for line in f if line.strip()):
        if p not in entries:
            errors.append("ERROR: required release file is missing: %s" % p)
            continue
        t, target = entries[p]
        if t == "d":
            errors.append("ERROR: required release file is a directory: %s" % p)
            continue
        if t == "l":
            # codex 复审#3 P1：ELF 真身与配置本体必须为普通文件——有效
            # symlink 同样拒绝（否则内容断言可被链接指向任意包内文件绕过）。
            if p in elf_paths or p in cfgrefs:
                errors.append(
                    "ERROR: required release file must be a regular file, "
                    "not a symlink: %s" % p)
                continue
            # codex 复审#4 P1：链成员 target 必须与真品链定义逐字节一致——
            # 「指向包内其他合法 ELF 但 target 错误」同样拒绝。
            if p in link_expect and target != link_expect[p]:
                errors.append(
                    "ERROR: required release file symlink target mismatch: "
                    "%s -> %s (expected: %s)"
                    % (p, target, link_expect[p]))
                continue
            rt, _ = resolve(p)
            if rt != "-":
                errors.append(
                    "ERROR: required release file is a dangling symlink: %s -> %s"
                    % (p, target))
            continue
        if t != "-":
            errors.append(
                "ERROR: required release file has unexpected type (%s): %s" % (t, p))
            continue
        if p in link_expect:
            errors.append(
                "ERROR: required release file must be a symlink, found "
                "regular file: %s" % p)
        if p in elf_paths and content.get(p) != b"\x7fELF":
            errors.append("ERROR: required release file is not an ELF object: %s" % p)
        if p in cfgrefs:
            data = content.get(p, b"")
            missing = [tok for tok in cfgrefs[p] if tok.encode() not in data]
            if missing:
                errors.append(
                    "ERROR: required release file content does not reference "
                    "expected library: %s (missing: %s)" % (p, ",".join(missing)))
with open(forbidden_path, encoding="utf-8", errors="surrogateescape") as f:
    for p in (line.rstrip("\n") for line in f if line.strip()):
        if p in entries:
            errors.append("ERROR: forbidden release file is present: %s" % p)
for e in errors:
    print(e, file=sys.stderr)
sys.exit(1 if errors else 0)
PYEOF

install -d "$runtime/helpers"
for helper in restore-dp1-mode-x11.sh xdisplay-session.sh install-xdisplay-user.sh; do
    dpkg-deb --fsys-tarfile "$DEB" |
        tar -xOf - "./usr/share/$package/$helper" > "$runtime/helpers/$helper"
    if [[ "$helper_cmp_mode" == "fantgpu" ]]; then
        # F 包内 helper 是 transform-fantgpu-helper.sh 的输出（builder 同
        # 管线），cmp 必须对照变换后字节而非 scripts/ 原始字节。
        bash "$ROOT/tools/transform-fantgpu-helper.sh" < "$ROOT/scripts/$helper" \
            > "$runtime/helpers/$helper.expected"
        cmp -s "$runtime/helpers/$helper.expected" "$runtime/helpers/$helper" || {
            echo "ERROR: packaged integration helper differs from transformed source: $helper" >&2
            exit 1
        }
    else
        cmp -s "$ROOT/scripts/$helper" "$runtime/helpers/$helper" || {
            echo "ERROR: packaged integration helper differs from current source: $helper" >&2
            exit 1
        }
    fi
done

echo "RESULT: PASS_RELEASE_PACKAGE_BOUNDARIES package=$package version=$version architecture=$architecture"
