#!/bin/bash
# Main installer wrapper for this repository.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"

usage() {
    cat <<'USAGE'
Usage:
  sudo scripts/install.sh [--prereqs] [--patched8|--patched17]

Default:
  sudo scripts/install.sh --patched17

Options:
  --prereqs    Install Debian package prerequisites first.
  --patched8   Install the stable rollback package.
  --patched17  Install the conservative automated baseline and rollback package.

Hardware GL userspace is intentionally a second step after a patched-17 reboot:
  scripts/prepare-deepin-userspace-root.sh
  sudo scripts/run-local-ddx-vt-test.sh
  sudo scripts/install-deepin-desktop-hwgl-trial.sh
USAGE
}

install_prereqs=0
target=patched17

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prereqs) install_prereqs=1 ;;
        --patched8) target=patched8 ;;
        --patched17) target=patched17 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "ERROR: unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    echo "ERROR: run as root: sudo $0" >&2
    exit 1
fi

if [[ "$install_prereqs" == "1" ]]; then
    "$ROOT/scripts/install-prereqs-debian.sh"
fi

case "$target" in
    patched8) exec "$ROOT/scripts/install-patched8-and-check.sh" ;;
    patched17) exec "$ROOT/scripts/install-patched17-and-check.sh" ;;
esac
