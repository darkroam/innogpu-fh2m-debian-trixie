#!/bin/bash
# Phase-3 oracle comparison: new-architecture candidate vs patched-27 (old builder).
# Compares control fields, file lists, payload hashes, maintainer scripts and
# module symbols. Read-only; no install, no reboot.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

CAND="${1:-$ROOT/build/innogpu-fh2m-trixie_4.0.0-i1.deb}"
REF="${2:-$ROOT/debs/innogpu-fh2m-trixie_3.3.3.42-patched-27.deb}"
KERNEL="${3:-${KERNELDIR_VER:-$(uname -r)}}"
[[ -f "$CAND" ]] || { echo "oracle_candidate=FAIL missing $CAND"; exit 1; }
[[ -f "$REF" ]] || { echo "oracle_reference=FAIL missing $REF"; exit 1; }

W="$(mktemp -d "$ROOT/build/oracle.XXXXXX")"
trap 'rm -rf "$W"' EXIT
mkdir -p "$W/ref" "$W/cand"
dpkg-deb -x "$REF" "$W/ref/"
dpkg-deb -x "$CAND" "$W/cand/"
mkdir -p "$W/ref/DEBIAN" "$W/cand/DEBIAN"
dpkg-deb --ctrl-tarfile "$REF" | tar -x -C "$W/ref/DEBIAN" 2>/dev/null
dpkg-deb --ctrl-tarfile "$CAND" | tar -x -C "$W/cand/DEBIAN" 2>/dev/null

fail=0
report() {
    echo "$1=$2"
    [[ "$2" == "PASS" ]] || fail=$((fail+1))
}

rc="$(dpkg-deb -f "$REF" | grep -vE '^(Version|Description|Installed-Size):|^ ' | sha256sum || true)"
cc="$(dpkg-deb -f "$CAND" | grep -vE '^(Version|Description|Installed-Size):|^ ' | sha256sum || true)"
[[ "$rc" == "$cc" ]] && report control_fields PASS || report control_fields FAIL

# .o.cmd 为内核构建产物(构建元数据), 不属发布边界; 新旧包统一按构建产物排除。
ARTIFACT_RE='modules.order|Module.symvers|\.mod$|\.o$|\.ko$|\.o\.cmd$'
rf="$(cd "$W/ref" && find . -path ./DEBIAN -prune -o -type f -print | grep -vE "$ARTIFACT_RE" | sort | sha256sum || true)"
cf="$(cd "$W/cand" && find . -path ./DEBIAN -prune -o -type f -print | grep -vE "$ARTIFACT_RE" | sort | sha256sum || true)"
[[ "$rf" == "$cf" ]] && report file_list PASS || report file_list FAIL

pd="$(diff -rq "$W/ref" "$W/cand" 2>/dev/null | grep -vE "DEBIAN|usr/src/innogpu-kernel|$ARTIFACT_RE" | head -20 || true)"
[[ -z "$pd" ]] && report payload_hashes PASS || { report payload_hashes FAIL; echo "$pd"; }

sd="$(diff -rq "$W/ref/usr/src/innogpu-kernel-2.2" "$W/cand/usr/src/innogpu-kernel-2.2" 2>/dev/null | grep -vE 'o_shipped|.o.cmd' | head -20 || true)"
[[ -z "$sd" ]] && report dkms_source_parity PASS || { report dkms_source_parity FAIL; echo "$sd"; }

bh="$(for f in "$W/ref/usr/src/innogpu-kernel-2.2"/*/*.o_shipped; do
    b=$(basename "$f"); m=$(basename "$(dirname "$f")")
    r=$(sha256sum "$f" | cut -d' ' -f1)
    c=$(sha256sum "$W/cand/usr/src/innogpu-kernel-2.2/$m/$b" 2>/dev/null | cut -d' ' -f1)
    [ "$r" == "$c" ] || echo "$m/$b"
done)"
[[ -z "$bh" ]] && report blackbox_hashes PASS || { report blackbox_hashes FAIL; echo "$bh"; }

norm() { sed -E 's/3\.3\.3\.42-patched-[0-9]+|patched-[0-9]+|4\.0\.0-i[0-9]+/VERSION/g' "$1" | sha256sum; }
ms_ok=1
for s in postinst prerm postrm; do
    [[ "$(norm "$W/ref/DEBIAN/$s")" == "$(norm "$W/cand/DEBIAN/$s")" ]] || ms_ok=0
done
[[ "$ms_ok" -eq 1 ]] && report maintainer_scripts PASS || report maintainer_scripts FAIL

if dpkg --compare-versions "$(dpkg-deb -f "$CAND" Version)" gt "$(dpkg-deb -f "$REF" Version)"; then
    report version_ordering PASS
else
    report version_ordering FAIL
fi

echo "build_artifacts=EXCLUDED (.o.cmd is build metadata, outside release boundary; p27 inherits 4 from Deepin payload, new package ships none)"
if [[ -f scripts/compare-module-symbols.sh ]]; then
    ms_line="$(bash scripts/compare-module-symbols.sh "$CAND" "$REF" "$KERNEL" 2>&1 | grep '^module_symbols=' | tail -1 || echo 'module_symbols=UNCOMPARABLE')"
    echo "$ms_line"
    case "$ms_line" in
        module_symbols=PASS*) ;;
        *) fail=$((fail+1)) ;;
    esac
else
    echo "module_symbols=UNCOMPARABLE (scripts/compare-module-symbols.sh missing)"
    fail=$((fail+1))
fi
echo "oracle_overall=$([ $fail -eq 0 ] && echo PASS || echo FAIL)"
[ "$fail" -eq 0 ]
