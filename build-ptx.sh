# build
# forkpty() lives in libutil on macOS/BSD, but in libc on glibc Linux.
case "$(uname -s)" in
    Darwin|*BSD) PTY_LIB="-lutil" ;;
    *)           PTY_LIB="" ;;
esac

LIBS=$(pkg-config --cflags --libs sdl3 sdl3-ttf)

# Embed an rpath so libraries found under our prefix (e.g. libvterm in
# ~/.local/lib) are resolved at runtime on platforms where that prefix is not
# on the default search path (notably Linux).
case "$LIBS" in
    *"-rpath,$HOME/.local/lib"*) ;;
    *) LIBS="$LIBS -Wl,-rpath,$HOME/.local/lib" ;;
esac

clang -std=gnu11 -Wall -Wextra -Wno-unused-parameter \
    ptx.c -o ptx \
    $LIBS \
    -I$HOME/.local/include -L$HOME/.local/lib \
    -lvterm $PTY_LIB

# macOS: a dylib's install name is often baked in as an absolute path at build
# time (libvterm commonly records /usr/local/lib even when installed under
# another prefix). rpath does not apply to absolute install names, so rewrite
# the recorded path to where the library actually lives.
if [ "$(uname -s)" = "Darwin" ]; then
    RECORDED=$(otool -L ptx | awk '/libvterm/ {print $1; exit}')
    REAL="$HOME/.local/lib/$(basename "$RECORDED")"
    if [ -n "$RECORDED" ] && [ "$RECORDED" != "$REAL" ] && [ -e "$REAL" ]; then
        install_name_tool -change "$RECORDED" "$REAL" ptx
    fi
fi
