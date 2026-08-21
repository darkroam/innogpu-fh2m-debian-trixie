#!/bin/bash
# Offline key-symbol comparison of the DKMS modules built from two debs.
#
# Builds the DKMS source of the candidate and the reference (patched-27)
# against the same kernel headers, then compares, per produced .ko:
#   - vermagic                        (modinfo)
#   - module dependencies             (modinfo depends)
#   - full defined symbol table       (nm --defined-only: type+name)
#   - full undefined/imported table   (nm --undefined-only: name)
#   - exported symbols                (.ksymtab_strings) when present
#   - modversions CRCs                (__versions section bytes) when present
#
# Kernel modules have no .dynsym, so nm -D is NOT used; the regular .symtab
# carries the ABI-relevant symbols. This is the Phase-3 "vermagic/关键符号逐项
# 一致" gate: vermagic alone is not sufficient.
# Read-only; builds in $ROOT/.build; no install, no reboot.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

CAND="${1:-$ROOT/build/innogpu-fh2m-trixie_4.0.0-i1.deb}"
REF="${2:-$ROOT/debs/innogpu-fh2m-trixie_3.3.3.42-patched-27.deb}"
KERNEL="${3:-${KERNELDIR_VER:-$(uname -r)}}"

[[ -f "$CAND" ]] || { echo "module_symbols=UNCOMPARABLE (missing candidate $CAND)"; exit 1; }
[[ -f "$REF" ]] || { echo "module_symbols=UNCOMPARABLE (missing reference $REF)"; exit 1; }
[[ -d "/lib/modules/$KERNEL/build" ]] || {
    echo "module_symbols=UNCOMPARABLE (kernel headers missing: /lib/modules/$KERNEL/build)"; exit 1; }

W="$(mktemp -d "$ROOT/build/symcmp.XXXXXX")"
trap 'rm -rf "$W"' EXIT

jobs=$(nproc); (( jobs > 8 )) && jobs=8

build_side() { # <side> <deb>
    local side="$1" deb="$2"
    mkdir -p "$W/$side"
    dpkg-deb -x "$deb" "$W/$side/root"
    if ! make -C "$W/$side/root/usr/src/innogpu-kernel-2.2" -j "$jobs" \
         KERNELDIR="/lib/modules/$KERNEL/build" > "$W/$side/build.log" 2>&1; then
        echo "module_symbols=UNCOMPARABLE (offline build failed side=$side)" >&2
        tail -8 "$W/$side/build.log" >&2
        return 1
    fi
    return 0
}

build_side cand "$CAND" || exit 1
build_side ref "$REF" || exit 1

# 模块集合一致性
( cd "$W/ref/root/usr/src/innogpu-kernel-2.2"  && find . -name '*.ko' | sort ) > "$W/ref.ko"
( cd "$W/cand/root/usr/src/innogpu-kernel-2.2" && find . -name '*.ko' | sort ) > "$W/cand.ko"
if ! diff -u "$W/ref.ko" "$W/cand.ko" > "$W/ko.diff"; then
    echo "module_symbols=FAIL (produced .ko set differs:)"
    cat "$W/ko.diff"
    exit 1
fi
[[ -s "$W/ref.ko" ]] || { echo "module_symbols=UNCOMPARABLE (no .ko produced by offline build)"; exit 1; }

n_mod=0; fail=0
while IFS= read -r rel; do
    [[ -n "$rel" ]] || continue
    n_mod=$((n_mod+1))
    m=$(basename "$rel")
    r="$W/ref/root/usr/src/innogpu-kernel-2.2/$rel"
    c="$W/cand/root/usr/src/innogpu-kernel-2.2/$rel"

    # 常规 .symtab 符号表(内核模块无 .dynsym)
    nm --defined-only "$r" | awk '{print $2, $3}' | sort > "$W/r.def"
    nm --defined-only "$c" | awk '{print $2, $3}' | sort > "$W/c.def"
    nm --undefined-only "$r" | awk '{print $NF}' | sort > "$W/r.und"
    nm --undefined-only "$c" | awk '{print $NF}' | sort > "$W/c.und"
    # 全局(强)定义符号 = 大写类型字母
    grep -E '^[A-Z] ' "$W/r.def" > "$W/r.glob" || true
    grep -E '^[A-Z] ' "$W/c.def" > "$W/c.glob" || true
    # 导出符号(.ksymtab_strings)与 modversions CRC(__versions)
    if readelf -S "$r" 2>/dev/null | grep -q '.ksymtab_strings'; then
        readelf -p .ksymtab_strings "$r" | awk '/^ *[/{print $2}' | sort > "$W/r.exp"
    else
        : > "$W/r.exp"
    fi
    if readelf -S "$c" 2>/dev/null | grep -q '.ksymtab_strings'; then
        readelf -p .ksymtab_strings "$c" | awk '/^ *[/{print $2}' | sort > "$W/c.exp"
    else
        : > "$W/c.exp"
    fi
    if readelf -S "$r" 2>/dev/null | grep -q '__versions'; then
        readelf -x __versions "$r" | sha256sum | cut -d' ' -f1 > "$W/r.crc"
    else
        : > "$W/r.crc"
    fi
    if readelf -S "$c" 2>/dev/null | grep -q '__versions'; then
        readelf -x __versions "$c" | sha256sum | cut -d' ' -f1 > "$W/c.crc"
    else
        : > "$W/c.crc"
    fi

    rv=$(/usr/sbin/modinfo -F vermagic "$r" 2>/dev/null || true)
    cv=$(/usr/sbin/modinfo -F vermagic "$c" 2>/dev/null || true)
    rd=$(/usr/sbin/modinfo -F depends "$r" 2>/dev/null || true)
    cd_=$(/usr/sbin/modinfo -F depends "$c" 2>/dev/null || true)

    ok=1
    [[ "$rv" == "$cv" ]] || { echo "  $m vermagic DIFF ref=$rv cand=$cv"; ok=0; }
    [[ "$rd" == "$cd_" ]] || { echo "  $m depends DIFF ref=$rd cand=$cd_"; ok=0; }
    diff -q "$W/r.def" "$W/c.def" >/dev/null || {
        echo "  $m defined_symbols DIFF (ref $(wc -l < "$W/r.def") vs cand $(wc -l < "$W/c.def"))"; ok=0; }
    diff -q "$W/r.und" "$W/c.und" >/dev/null || {
        echo "  $m undefined_symbols DIFF (ref $(wc -l < "$W/r.und") vs cand $(wc -l < "$W/c.und"))"; ok=0; }
    diff -q "$W/r.glob" "$W/c.glob" >/dev/null || {
        echo "  $m global_symbols DIFF (ref $(wc -l < "$W/r.glob") vs cand $(wc -l < "$W/c.glob"))"; ok=0; }
    diff -q "$W/r.exp" "$W/c.exp" >/dev/null || {
        echo "  $m exported_symbols DIFF (ref $(wc -l < "$W/r.exp") vs cand $(wc -l < "$W/c.exp"))"; ok=0; }
    if [[ -s "$W/r.crc" || -s "$W/c.crc" ]]; then
        diff -q "$W/r.crc" "$W/c.crc" >/dev/null || {
            echo "  $m modversions_crc DIFF (__versions section bytes)"; ok=0; }
    fi
    if [[ $ok -eq 1 ]]; then
        exp_note="no exported symbols"
        [[ -s "$W/r.exp" ]] && exp_note="$(wc -l < "$W/r.exp") exported"
        crc_note=""
        [[ -s "$W/r.crc" ]] && crc_note=", __versions CRCs identical"
        echo "  $m symbols=PASS ($(wc -l < "$W/r.def") defined, $(wc -l < "$W/r.und") imported, $exp_note$crc_note, vermagic+depends identical)"
    else
        fail=$((fail+1))
    fi
done < "$W/ref.ko"

if [[ $fail -eq 0 ]]; then
    echo "module_symbols=PASS ($n_mod modules: defined/imported/global/exported symbol tables, vermagic, depends identical)"
else
    echo "module_symbols=FAIL ($fail/$n_mod modules differ; see DIFF lines above)"
    exit 1
fi
