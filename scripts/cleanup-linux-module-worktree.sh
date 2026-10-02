#!/usr/bin/env bash
# Remove only the expanded source/work tree after review; preserve deb and results.
set -euo pipefail
PATH=/usr/sbin:/usr/bin:/sbin:/bin:$PATH

EVIDENCE_ROOT=""
while (($#)); do
    case $1 in
        --evidence) EVIDENCE_ROOT=${2:?missing value}; shift 2 ;;
        -h|--help) printf '%s\n' 'Usage: scripts/cleanup-linux-module-worktree.sh --evidence DIR'; exit 0 ;;
        *) printf 'FAIL: unknown option: %s\n' "$1" >&2; exit 2 ;;
    esac
done
[[ -n $EVIDENCE_ROOT ]] || exit 2
[[ -d $EVIDENCE_ROOT ]] || { printf 'FAIL: evidence directory not found\n' >&2; exit 1; }
rm -rf -- "$EVIDENCE_ROOT/source" "$EVIDENCE_ROOT/kernel-build"
printf 'WORKTREE_CLEANED evidence=%s source_deb_and_results_preserved=1\n' "$EVIDENCE_ROOT"
