#!/usr/bin/env bash
set -euo pipefail

[[ $# -eq 7 ]] || {
    echo "Usage: $0 SOURCE_TREE SNAPSHOT MANIFEST BUILDER_TREE DKMS_TREE MODULE DEB" >&2
    exit 2
}

SOURCE_TREE=$1
SNAPSHOT=$2
MANIFEST=$3
BUILDER_TREE=$4
DKMS_TREE=$5
MODULE=$6
DEB=$7
PATTERN='fantgpu_pm_probe|R5_I4_DIAGNOSTIC|probe=pm_probe|entered_step|completed_step'
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

for path in "$SOURCE_TREE" "$SNAPSHOT" "$MANIFEST" "$BUILDER_TREE" \
            "$DKMS_TREE" "$MODULE" "$DEB"; do
    [[ -e "$path" ]] || { echo "pm_probe_removal=FAIL missing=$path"; exit 1; }
done

scan_tree() {
    local label=$1 path=$2
    if rg -a -l -m1 "$PATTERN" "$path" >/dev/null; then
        echo "pm_probe_removal_$label=FAIL diagnostic_token_present"
        return 1
    fi
    echo "pm_probe_removal_$label=PASS"
}

scan_module() {
    local label=$1 path=$2 object strings_file
    object=$path
    strings_file="$TMP/$label.strings"
    if [[ "$path" == *.xz ]]; then
        object="$TMP/$label.ko"
        xz -dc "$path" > "$object"
    fi
    strings "$object" > "$strings_file"
    if rg -q "$PATTERN" "$strings_file"; then
        echo "pm_probe_removal_$label=FAIL diagnostic_symbol_or_string_present"
        return 1
    fi
    echo "pm_probe_removal_$label=PASS"
}

scan_tree source "$SOURCE_TREE"
mkdir -p "$TMP/snapshot"
tar --use-compress-program=zstd -xf "$SNAPSHOT" -C "$TMP/snapshot"
scan_tree snapshot "$TMP/snapshot"
scan_tree manifest "$MANIFEST"
scan_tree builder "$BUILDER_TREE"
scan_tree dkms "$DKMS_TREE"
scan_module module "$MODULE"

mkdir -p "$TMP/deb"
dpkg-deb -x "$DEB" "$TMP/deb"
scan_tree deb_payload "$TMP/deb"
while IFS= read -r packaged_module; do
    scan_module deb_module "$packaged_module"
done < <(find "$TMP/deb" -type f \( -name '*.ko' -o -name '*.ko.xz' \) -print)

echo "pm_probe_removal_overall=PASS"
