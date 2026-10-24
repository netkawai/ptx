# build
# forkpty() lives in libutil on macOS/BSD, but in libc on glibc Linux.
case "$(uname -s)" in
    Darwin|*BSD) PTY_LIB="-lutil" ;;
    *)           PTY_LIB="" ;;
esac
clang ptx.c -o ptx $(pkg-config --cflags --libs sdl3 sdl3-ttf) -I$HOME/.local/include -L$HOME/.local/lib -lvterm $PTY_LIB
