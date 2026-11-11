#!/bin/sh
# Repair absolute dylib install names on macOS.
#
# Some libraries (notably libvterm) are built with an install name like
# /usr/local/lib/libvterm.0.dylib even when they are installed under a
# different prefix (e.g. ~/.local/lib). dyld resolves absolute install names
# directly, so an rpath does not help and the binary fails at launch with
# "Library not loaded". This rewrites such names to where the library actually
# lives.
#
# Usage: fix-macos-install-names.sh <binary>
# Env:   VTERM_LIBRARY_DIRS  extra directories to search (space-separated)

set -eu

BIN="${1:-}"
[ -n "$BIN" ] || { echo "usage: $0 <binary>" >&2; exit 2; }
[ -f "$BIN" ] || exit 0

command -v otool >/dev/null 2>&1 || exit 0
command -v install_name_tool >/dev/null 2>&1 || exit 0

SEARCH_DIRS=""
for d in ${VTERM_LIBRARY_DIRS:-} "${HOME:-/tmp}/.local/lib" /usr/local/lib /opt/homebrew/lib /usr/lib; do
    [ -n "$d" ] && SEARCH_DIRS="$SEARCH_DIRS $d"
done

deps=$(mktemp "${TMPDIR:-/tmp}/ptx-deps.XXXXXX")
trap 'rm -f "$deps"' EXIT

# otool -L prints the binary itself first, then one dependency per line.
otool -L "$BIN" | tail -n +2 | awk '{ print $1 }' > "$deps"

CHANGED=0
while IFS= read -r rec; do
    case "$rec" in
        /usr/local/lib/*|/opt/homebrew/lib/*)
            base=$(basename "$rec")
            for d in $SEARCH_DIRS; do
                if [ -e "$d/$base" ] && [ "$d/$base" != "$rec" ]; then
                    install_name_tool -change "$rec" "$d/$base" "$BIN"
                    echo "ptx: rewrote install name $rec -> $d/$base"
                    CHANGED=1
                    break
                fi
            done
            ;;
    esac
done < "$deps"

# install_name_tool invalidates any code signature; re-sign ad-hoc so the
# binary still runs on Apple Silicon.
if [ "$CHANGED" = "1" ] && command -v codesign >/dev/null 2>&1; then
    codesign --force --sign - "$BIN" >/dev/null 2>&1 || true
fi
