#!/bin/bash
# Apply or revert the lid-closed external-display snippet. Identical content is a no-op.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TEMPLATE=$ROOT/tools/gentoo-lid-external.conf
DEST=${GENTOO_LID_CONF:-/etc/X11/xorg.conf.d/90-lid-external.conf}

same() { [[ -f $DEST ]] && cmp -s "$TEMPLATE" "$DEST"; }

case ${1:-} in
    --apply)
        if same; then
            printf 'lid_conf=SKIP identical\n'
            exit 0
        fi
        if [[ -e $DEST ]]; then
            printf 'lid_conf=FAIL differs\n' >&2
            exit 1
        fi
        install -m 0644 "$TEMPLATE" "$DEST"
        printf 'lid_conf=APPLIED\n'
        ;;
    --revert)
        if [[ ! -e $DEST ]]; then
            printf 'lid_conf=ABSENT\n'
            exit 0
        fi
        if ! same; then
            printf 'lid_conf=FAIL differs\n' >&2
            exit 1
        fi
        rm -f -- "$DEST"
        printf 'lid_conf=REVERTED\n'
        ;;
    *)
        printf 'use --apply or --revert\n' >&2
        exit 1
        ;;
esac
