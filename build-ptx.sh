# build
clang ptx.c -o ptx $(pkg-config --cflags --libs sdl3 sdl3-ttf) -I$HOME/.local/include -L$HOME/.local/lib -lvterm -lutil
