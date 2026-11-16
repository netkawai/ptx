# libvterm is built as a shared library (not compiled into the ptx binary).
# It is vendored under libvterm/ with its own MIT LICENSE; keeping it a
# separate target keeps ptx's 0BSD sources and this third-party code apart.
#
# NB: when added via add_subdirectory(), relative paths in this file are
# interpreted relative to this directory. Do not prefix them with
# ${CMAKE_CURRENT_SOURCE_DIR}/libvterm again.

add_library(vterm SHARED
  src/encoding.c
  src/keyboard.c
  src/mouse.c
  src/parser.c
  src/pen.c
  src/screen.c
  src/state.c
  src/unicode.c
  src/vterm.c
)

# `include/` is the public API (vterm.h). Marking it SYSTEM keeps upstream's
# warnings out of ptx's build; the PRIVATE `src/` path is only for our own
# compilation of the library.
target_include_directories(vterm SYSTEM PUBLIC include)
target_include_directories(vterm PRIVATE src)

# libvterm is C99; it needs _XOPEN_SOURCE for some libc declarations on
# Solaris/XPG systems, as in upstream's Makefile.
target_compile_definitions(vterm PRIVATE _XOPEN_SOURCE=700)
set_target_properties(vterm PROPERTIES C_STANDARD 99 C_EXTENSIONS ON)

# Upstream builds with -Wall -Wpedantic but is not warning-clean under
# -Wextra, so do not add extra diagnostics here. Errors are still fatal.
if(NOT MSVC)
  target_compile_options(vterm PRIVATE -Wall)
endif()

# Mirror upstream's libtool -version-info 0:0:0 -> libvterm.so.0 (ELF) or
# libvterm.0.dylib (macOS).
set_target_properties(vterm PROPERTIES VERSION 0.0.0 SOVERSION 0)
