#!/bin/bash
# Apply the Innogpu GLX compatibility patch to a pinned Picom checkout.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
BASE_COMMIT=6d676824c457a933c52e3e92c5a1856466f90545
PATCH_FILE="$ROOT/components/picom/001-probe-explicit-uniform-location.patch"
SOURCE_DIR="${PICOM_SOURCE:-$HOME/src/picom}"
BUILD_DIR="${PICOM_BUILD_DIR:-}"
PREFIX="${PICOM_PREFIX:-/usr/local}"
install_binary=0

usage() {
    cat <<'USAGE'
Usage: scripts/build-patched-picom.sh [options]

Options:
  --source DIR      Pinned Picom source checkout (default: $HOME/src/picom)
  --build-dir DIR   Build directory (default: SOURCE/build-innogpu)
  --install         Back up and install the binary under /usr/local/bin
  -h, --help        Show this help
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --source) SOURCE_DIR=${2:?missing value for --source}; shift ;;
        --build-dir) BUILD_DIR=${2:?missing value for --build-dir}; shift ;;
        --install) install_binary=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "ERROR: unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if [[ ${EUID:-$(id -u)} -eq 0 ]]; then
    echo "ERROR: build as a normal user; --install will request sudo separately" >&2
    exit 1
fi

BUILD_DIR="${BUILD_DIR:-$SOURCE_DIR/build-innogpu}"
[[ -d "$SOURCE_DIR" ]] || { echo "ERROR: missing Picom source: $SOURCE_DIR" >&2; exit 1; }
[[ -r "$PATCH_FILE" ]] || { echo "ERROR: missing patch: $PATCH_FILE" >&2; exit 1; }

for command_name in git meson ninja; do
    command -v "$command_name" >/dev/null 2>&1 || {
        echo "ERROR: missing command: $command_name" >&2
        exit 1
    }
done

if [[ "$(git -C "$SOURCE_DIR" rev-parse --is-inside-work-tree 2>/dev/null || true)" != "true" ]]; then
    echo "ERROR: not a Picom Git checkout: $SOURCE_DIR" >&2
    exit 1
fi

current_commit="$(git -C "$SOURCE_DIR" rev-parse HEAD)"
if [[ "$current_commit" != "$BASE_COMMIT" ]]; then
    echo "ERROR: Picom HEAD is $current_commit; expected $BASE_COMMIT" >&2
    exit 1
fi

unexpected_changes="$(
    git -C "$SOURCE_DIR" status --short --untracked-files=no |
        awk '$NF != "src/backend/gl/gl_common.c" { print }'
)"
if [[ -n "$unexpected_changes" ]]; then
    printf '%s\n' "$unexpected_changes" >&2
    echo "ERROR: Picom checkout has unrelated tracked changes" >&2
    exit 1
fi

if git -C "$SOURCE_DIR" apply --unidiff-zero --check "$PATCH_FILE" 2>/dev/null; then
    git -C "$SOURCE_DIR" apply --unidiff-zero "$PATCH_FILE"
    echo "Applied Innogpu Picom GLX patch."
elif git -C "$SOURCE_DIR" apply --unidiff-zero --reverse --check "$PATCH_FILE" 2>/dev/null; then
    echo "Innogpu Picom GLX patch is already applied."
else
    echo "ERROR: patch is neither cleanly applicable nor exactly applied" >&2
    exit 1
fi

if [[ -f "$BUILD_DIR/build.ninja" ]]; then
    meson setup --reconfigure --buildtype=release --prefix="$PREFIX" \
        -Dwith_docs=false "$BUILD_DIR" "$SOURCE_DIR"
else
    meson setup --buildtype=release --prefix="$PREFIX" -Dwith_docs=false \
        "$BUILD_DIR" "$SOURCE_DIR"
fi
ninja -C "$BUILD_DIR"

built_binary="$BUILD_DIR/src/picom"
[[ -x "$built_binary" ]] || { echo "ERROR: build did not create $built_binary" >&2; exit 1; }
sha256sum "$built_binary"

if [[ "$install_binary" == "1" ]]; then
    target="$PREFIX/bin/picom"
    backup="$target.before-innogpu"
    sudo install -d -m 0755 "$PREFIX/bin"
    if sudo test -e "$target" && ! sudo test -e "$backup"; then
        sudo cp -a -- "$target" "$backup"
    fi
    sudo install -m 0755 "$built_binary" "$target"
    echo "Installed patched Picom: $target"
else
    echo "Build complete. Re-run with --install to install the binary."
fi
