#!/bin/bash
# Build fbterm 1.7-5 with a configurable framebuffer redraw fallback.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
SOURCE_DIR="${FBTERM_SOURCE:-$HOME/src/fbterm-1.7}"
BUILD_DIR="${FBTERM_BUILD_DIR:-$ROOT/.build/fbterm-1.7}"
PREFIX="${FBTERM_PREFIX:-$HOME/.local}"
PATCH_FILE="$ROOT/patches/fbterm/001-configurable-redraw-scrolling.patch"
install_binary=0

usage() {
    cat <<'USAGE'
Usage: scripts/build-patched-fbterm.sh [options]

Options:
  --source DIR      Debian fbterm 1.7-5 source tree (default: $HOME/src/fbterm-1.7)
  --build-dir DIR   Disposable build tree (default: REPO/.build/fbterm-1.7)
  --prefix DIR      Installation prefix (default: $HOME/.local)
  --install         Install the built binary as PREFIX/bin/fbterm
  -h, --help        Show this help

The validated build disables optional GPM and legacy VESA support. The system
/usr/bin/fbterm remains the rollback path when the default prefix is used.
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --source) SOURCE_DIR=${2:?missing value for --source}; shift ;;
        --build-dir) BUILD_DIR=${2:?missing value for --build-dir}; shift ;;
        --prefix) PREFIX=${2:?missing value for --prefix}; shift ;;
        --install) install_binary=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "ERROR: unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if [[ ${EUID:-$(id -u)} -eq 0 ]]; then
    echo "ERROR: build and install this user-local fbterm as a normal user" >&2
    exit 1
fi

for command_name in patch make sha256sum; do
    command -v "$command_name" >/dev/null 2>&1 || {
        echo "ERROR: missing command: $command_name" >&2
        exit 1
    }
done

[[ -r "$SOURCE_DIR/debian/changelog" ]] || {
    echo "ERROR: missing Debian fbterm source tree: $SOURCE_DIR" >&2
    exit 1
}
grep -q '^fbterm (1\.7-5)' "$SOURCE_DIR/debian/changelog" || {
    echo "ERROR: only Debian fbterm 1.7-5 is supported" >&2
    exit 1
}
[[ -r "$PATCH_FILE" ]] || { echo "ERROR: missing patch: $PATCH_FILE" >&2; exit 1; }

case "$BUILD_DIR" in
    ""|/|"$HOME"|"$ROOT")
        echo "ERROR: unsafe build directory: $BUILD_DIR" >&2
        exit 1
        ;;
esac

rm -rf -- "$BUILD_DIR"
mkdir -p -- "$BUILD_DIR"
cp -a -- "$SOURCE_DIR/." "$BUILD_DIR/"

if patch --batch --forward --dry-run -d "$BUILD_DIR" -p1 < "$PATCH_FILE" >/dev/null 2>&1; then
    patch --batch --forward -d "$BUILD_DIR" -p1 < "$PATCH_FILE"
elif patch --batch --reverse --dry-run -d "$BUILD_DIR" -p1 < "$PATCH_FILE" >/dev/null 2>&1; then
    echo "FbTerm redraw patch is already present in the source tree."
else
    echo "ERROR: redraw patch does not apply cleanly to $SOURCE_DIR" >&2
    exit 1
fi

(
    cd "$BUILD_DIR"
    ./configure --disable-gpm --disable-vesa --prefix="$PREFIX"
    make -j"${JOBS:-2}"
)

built_binary="$BUILD_DIR/src/fbterm"
[[ -x "$built_binary" ]] || { echo "ERROR: build did not create $built_binary" >&2; exit 1; }
sha256sum "$built_binary"

if [[ "$install_binary" == "1" ]]; then
    target="$PREFIX/bin/fbterm"
    backup="$target.before-innogpu-redraw"
    install -d -m 0755 "$PREFIX/bin"
    if [[ -e "$target" && ! -e "$backup" ]]; then
        cp -a -- "$target" "$backup"
    fi
    install -m 0755 "$built_binary" "$target"
    echo "Installed patched FbTerm: $target"
    echo "Rollback: remove $target to use /usr/bin/fbterm"
else
    echo "Build complete. Re-run with --install to install the user-local binary."
fi
