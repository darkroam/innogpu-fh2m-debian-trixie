#!/bin/bash
# Generate the F package's control scripts without building or installing it.
set -euo pipefail
export LC_ALL=C
[[ $# == 2 && -d "$1/DEBIAN" && "$2" =~ ^5\.0\.0-i[0-9]+([.+~-][A-Za-z0-9.+~-]+)?$ ]] || {
    echo 'usage: generate-fantgpu-maintainer-scripts.sh PACKAGE_ROOT F_VERSION' >&2
    exit 2
}
package_root=$(realpath "$1")
version=$2

# Emit the same identity function into postinst; never source dkms.conf to
# discover identity (it is executable shell, not metadata).
source_digest() {
    (
        cd "$1" || return 1
        [[ -f dkms.conf && -f Makefile ]] || return 1
        [[ -z $(find . ! -type f ! -type d -print -quit) ]] || return 1
        find . -type f -print0 | sort -z | xargs -0 -r sha256sum | sha256sum
    )
}
dkms_policy_guard() {
    local conf line digest
    for conf in /etc/dkms /etc/dkms/framework.conf.d; do
        [[ ! -L "$conf" && ( ! -e "$conf" || -d "$conf" ) ]] || {
            echo "ERROR: DKMS configuration directory requires review: $conf" >&2
            return 1
        }
    done
    shopt -s nullglob
    for conf in /etc/dkms/framework.conf /etc/dkms/framework.conf.d/*.conf /etc/dkms/fantgpu-fh2m-kernel*.conf; do
        [[ ! -L "$conf" ]] || {
            echo "ERROR: DKMS configuration symlink requires review: $conf" >&2
            return 1
        }
        [[ ! -e "$conf" ]] || {
            [[ -f "$conf" ]] || return 1
            if [[ "$conf" == /etc/dkms/framework.conf.d/autoinstall_all_kernels.conf ]]; then
                digest=$(sha256sum "$conf") || return 1
                # Exactly the reviewed 30 bytes, including its final newline.
                [[ ${digest%% *} != e362342a516c0507da4407b889d041a1df7e187f4cb8d05c8a039b1a307d2d4b ]] || continue
            fi
            while IFS= read -r line || [[ -n "$line" ]]; do
                line=${line%%#*}
                [[ "$line" =~ ^[[:space:]]*$ ]] || {
                    echo "ERROR: DKMS override requires review: $conf" >&2
                    return 1
                }
            done < "$conf"
        }
    done
    return 0
}
identity=$(source_digest "$package_root/usr/src/fantgpu-fh2m-kernel-2.2")
identity=${identity%% *}
{
    printf '#!/bin/bash\nset -Eeuo pipefail\nexport LC_ALL=C\n'
    printf 'expected_source_sha=%q\npackage_version=%q\n' "$identity" "$version"
    declare -f source_digest
    declare -f dkms_policy_guard
    cat <<'POSTINST'
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export PATH
case "${1:-}" in
    configure) ;;
    abort-upgrade|abort-remove|abort-deconfigure) exit 0 ;;
    *) echo "ERROR: unknown postinst argument ${1:-}" >&2; exit 1 ;;
esac

module=fantgpu-fh2m-kernel
version=2.2
source_dir=/usr/src/$module-$version
registry=/var/lib/dkms/$module/$version
declare -A targets=() results=()
kernels=()
stage=preflight
finish() {
    local rc=$? k
    trap - EXIT
    for k in "${kernels[@]}"; do
        printf 'kernel=%s result=%s\n' "$k" "${results[$k]}"
    done
    printf 'postinst_rc=%s stage=%s package=%s\n' "$rc" "$stage" "$package_version"
    if (( rc != 0 )); then
        echo 'ERROR: package is not configured; do not reboot. Preserve this log and use the approved recovery plan.' >&2
    fi
    exit "$rc"
}
trap finish EXIT
die() { echo "ERROR: $*" >&2; exit 1; }
add_kernel() {
    [[ "$1" =~ ^[0-9][A-Za-z0-9._+~-]*$ ]] || die "invalid kernel version: $1"
    targets["$1"]=1
}
for cmd in uname dpkg-query dkms make gcc ldconfig depmod update-initramfs lsinitramfs modinfo readlink find sort xargs sha256sum cmp grep; do
    command -v "$cmd" >/dev/null || die "missing tool: $cmd"
done
# DKMS configuration can redirect trees or run hooks/load modules. Do not
# execute unreviewed local overrides; leave them untouched for review.
dkms_policy_guard
add_kernel "$(uname -r)"
arch=$(uname -m)
shopt -s nullglob
for image in /boot/vmlinuz-*; do
    [[ -e "$image" ]] || continue
    resolved=$(readlink -f "$image")
    [[ "$resolved" == /boot/vmlinuz-* ]] || die "kernel image link escapes /boot: $image"
    add_kernel "${resolved#/boot/vmlinuz-}"
done
package_list=$(dpkg-query -W -f='${db:Status-Status}\t${Package}\n')
while IFS=$'\t' read -r status package; do
    [[ -n "$status" && "$package" =~ ^[a-z0-9][a-z0-9+.-]+$ ]] || die 'malformed package enumeration'
    case "$status" in
        installed|not-installed|config-files|half-installed|unpacked|half-configured|triggers-awaited|triggers-pending) ;;
        *) die 'unknown package state' ;;
    esac
    [[ "$package" == linux-image-[0-9]* ]] || continue
    case "$status" in
        not-installed|config-files) continue ;;
        installed) ;;
        *) die "kernel image package not configured: $package" ;;
    esac
    kernel=${package#linux-image-}
    add_kernel "${kernel%-unsigned}"
done <<< "$package_list"
sorted=$(printf '%s\n' "${!targets[@]}" | sort)
mapfile -t kernels <<< "$sorted"
for k in "${kernels[@]}"; do results[$k]=NOT_RUN; done
missing=0
for k in "${kernels[@]}"; do
    for required in "/boot/vmlinuz-$k" "/lib/modules/$k/build/Makefile"; do
        if [[ ! -s "$required" ]]; then
            echo "ERROR: kernel=$k missing=$required" >&2
            missing=1
        fi
    done
    if [[ ! -f /lib/modules/$k/build/include/config/kernel.release ]] ||
       [[ $(</lib/modules/"$k"/build/include/config/kernel.release) != "$k" ]]; then
        echo "ERROR: kernel=$k mismatched headers" >&2
        missing=1
    fi
done
(( missing == 0 )) || die 'incomplete target kernels; no DKMS write attempted'
DRI_SO=/usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so
[[ -s "$DRI_SO" ]] || die 'coherent fh2m_dri.so is missing'
[[ $(</etc/modprobe.d/fantgpu.conf) == 'options fantgpu firmware_en=1' ]] || die 'unexpected packaged fantgpu options'
actual=$(source_digest "$source_dir")
[[ ${actual%% *} == "$expected_source_sha" ]] || die 'packaged DKMS source identity mismatch'
registration=$(dkms status -m "$module" -v "$version")
if [[ -e "$registry" || -L "$registry" || -n "$registration" ]]; then
    [[ -n "$registration" && -L "$registry/source" ]] || die 'inconsistent DKMS registration'
    [[ $(readlink -f "$registry/source") == "$source_dir" ]] || die 'registered source points elsewhere'
    while IFS= read -r row; do
        [[ "$row" == "$module/$version: added" ||
           "$row" == "$module/$version, "*": built" ||
           "$row" == "$module/$version, "*": installed" ]] || die 'unexpected DKMS status'
    done <<< "$registration"
else
    stage=add
    dkms add -m "$module" -v "$version"
    [[ -L "$registry/source" && $(readlink -f "$registry/source") == "$source_dir" ]] || die 'dkms add did not register packaged source'
fi

for k in "${kernels[@]}"; do
    results[$k]=FAILED
    stage=build:$k
    dkms build -m "$module" -v "$version" -k "$k" --force
    built=("$registry/$k/$arch/module/"fantgpu.ko*)
    [[ ${#built[@]} == 1 && -s "${built[0]}" ]] || die "DKMS build output missing or ambiguous: $k"
    stage=install:$k
    dkms install -m "$module" -v "$version" -k "$k" --force
    stage=depmod:$k
    depmod -a "$k"
    stage=verify:$k
    installed=$(dkms status -m "$module" -v "$version" -k "$k")
    [[ "$installed" == "$module/$version, $k, $arch: installed" ]] || die "DKMS installation not confirmed: $k"
    ko=$(modinfo -k "$k" -n fantgpu)
    [[ "$ko" == "/lib/modules/$k/updates/"* && -s "$ko" ]] || die "unexpected module path: $k"
    [[ $(modinfo -F name "$ko") == fantgpu ]] || die "module name mismatch: $k"
    [[ $(modinfo -F vermagic "$ko") == "$k "* ]] || die "module vermagic mismatch: $k"
    cmp -s "${built[0]}" "$ko" || die "installed module differs from DKMS build: $k"
    stage=initramfs:$k
    action=-c
    [[ ! -e /boot/initrd.img-$k ]] || action=-u
    update-initramfs "$action" -k "$k"
    [[ -s /boot/initrd.img-$k ]] || die "initramfs not created: $k"
    contents=$(lsinitramfs "/boot/initrd.img-$k")
    grep -Fxq "${ko#/}" <<< "$contents" ||
        grep -Fxq "usr/${ko#/}" <<< "$contents" || die "module absent from initramfs: $k"
    results[$k]=PASS
done
stage=source-recheck
actual=$(source_digest "$source_dir")
[[ ${actual%% *} == "$expected_source_sha" ]] || die 'DKMS source changed during configure'
stage=ldconfig
ldconfig
# This is a warning-only device hint, not permission to probe or load modules.
if ! command -v lspci >/dev/null 2>&1; then
    echo 'WARNING: lspci unavailable; 1ec8:9810 device gate not executed' >&2
elif ! lspci -n -d 1ec8:9810 | grep -q .; then
    echo 'WARNING: no 1ec8:9810 device present' >&2
fi
stage=complete
echo 'Installed coherent fantgpu (F) userspace payload; module autoload policy was not changed.'
POSTINST
} > "$package_root/DEBIAN/postinst"

{
    printf '#!/bin/bash\nset -euo pipefail\nexport LC_ALL=C\n'
    declare -f dkms_policy_guard
    cat <<'PRERM'
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export PATH
case "${1:-}" in
    remove|upgrade|deconfigure)
        dkms_policy_guard
        registration=$(dkms status -m fantgpu-fh2m-kernel -v 2.2)
        if [[ -n "$registration" ]]; then
            dkms remove -m fantgpu-fh2m-kernel -v 2.2 --all
            remaining=$(dkms status -m fantgpu-fh2m-kernel -v 2.2)
            [[ -z "$remaining" && ! -e /var/lib/dkms/fantgpu-fh2m-kernel/2.2 &&
               ! -L /var/lib/dkms/fantgpu-fh2m-kernel/2.2 ]] || {
                echo 'ERROR: DKMS registration remains after removal' >&2
                exit 1
            }
        elif [[ -e /var/lib/dkms/fantgpu-fh2m-kernel/2.2 || -L /var/lib/dkms/fantgpu-fh2m-kernel/2.2 ]]; then
            echo 'ERROR: inconsistent DKMS registration; refusing silent removal' >&2
            exit 1
        fi
        ;;
    failed-upgrade) ;;
    *) echo "ERROR: unknown prerm argument ${1:-}" >&2; exit 1 ;;
esac
PRERM
} > "$package_root/DEBIAN/prerm"
cat > "$package_root/DEBIAN/postrm" <<'POSTRM'
#!/bin/bash
set -e
case "${1:-}" in
    remove|purge|upgrade|failed-upgrade|abort-install|abort-upgrade|disappear) ldconfig ;;
    *) echo "postrm called with unknown argument ${1:-}" >&2; exit 1 ;;
esac
POSTRM
chmod 0755 "$package_root/DEBIAN/"{postinst,prerm,postrm}
