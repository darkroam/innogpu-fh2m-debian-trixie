#!/usr/bin/env bash
# List or explicitly remove package-manager source .debs for kernels no longer installed.
set -euo pipefail
PATH=/usr/sbin:/usr/bin:/sbin:/bin:$PATH

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
CACHE="$ROOT/debs"
DELETE=0
while (($#)); do
    case $1 in
        --delete) DELETE=1; shift ;;
        -h|--help) printf '%s\n' 'Usage: scripts/prune-kernel-source-cache.sh [--delete]'; exit 0 ;;
        *) printf 'FAIL: unknown option: %s\n' "$1" >&2; exit 2 ;;
    esac
done
[[ -d $CACHE ]] || { printf 'source_deb_cache=empty\n'; exit 0; }

for deb in "$CACHE"/linux-source-*.deb; do
    [[ -f $deb ]] || continue
    version=$(dpkg-deb -f "$deb" Version 2>/dev/null || true)
    [[ -n $version ]] || continue
    installed=0
    while read -r _package installed_version; do
        if [[ $installed_version == "$version" ]]; then
            installed=1
            break
        fi
    done < <(dpkg-query -W -f='${binary:Package} ${Version}\n' 'linux-image-*' 2>/dev/null || true)
    if ((installed)); then
        printf 'KEEP source_deb=%s version=%s\n' "$(basename "$deb")" "$version"
        continue
    fi
    if ((DELETE)); then
        rm -f -- "$deb"
        printf 'DELETE source_deb=%s version=%s\n' "$(basename "$deb")" "$version"
    else
        printf 'CANDIDATE source_deb=%s version=%s\n' "$(basename "$deb")" "$version"
    fi
done
