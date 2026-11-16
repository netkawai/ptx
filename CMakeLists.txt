cmake_minimum_required(VERSION 3.16)
project(ptx VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)   # forkpty / GNU extensions

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# ---- Dependencies -------------------------------------------------
# Prefer the upstream CMake config packages when the SDK ships them, and fall
# back to pkg-config otherwise. libvterm only ships a .pc file.
find_package(SDL3 CONFIG QUIET)
if(SDL3_FOUND)
  set(PTX_SDL3 SDL3::SDL3)
else()
  pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
  set(PTX_SDL3 PkgConfig::SDL3)
endif()

find_package(SDL3_ttf CONFIG QUIET)
if(SDL3_ttf_FOUND)
  set(PTX_SDL3_TTF SDL3_ttf::SDL3_ttf)
else()
  pkg_check_modules(SDL3_TTF REQUIRED IMPORTED_TARGET sdl3-ttf)
  set(PTX_SDL3_TTF PkgConfig::SDL3_TTF)
endif()

# libvterm is vendored (MIT, see libvterm/README.md) and built here as a
# shared library; ptx links against it rather than compiling it in.
add_subdirectory(libvterm)

# ---- Target -------------------------------------------------------
add_executable(ptx
  src/main.c
  src/strutil.c
  src/cell.c
  src/buffer.c
  src/terminal.c
  src/selection.c
  src/menu.c
  src/render.c
  src/input.c
  src/layout.c
  src/boxdraw.c
  src/adjustment.c
  src/orientable.c
  src/range.c
  src/scrollbar.c
  src/sdl_private.c
)

target_include_directories(ptx PRIVATE src)

target_link_libraries(ptx PRIVATE
  ${PTX_SDL3}
  ${PTX_SDL3_TTF}
  vterm
  Threads::Threads
)

# forkpty() lives in libutil on macOS/BSD.
if(APPLE OR CMAKE_SYSTEM_NAME MATCHES "BSD")
  target_link_libraries(ptx PRIVATE util)
endif()

if(NOT MSVC)
  target_compile_options(ptx PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

install(TARGETS ptx RUNTIME DESTINATION bin)

# ---- Tests --------------------------------------------------------
# Widget model tests (adjustment / range / scrollbar) need no window.
enable_testing()
add_executable(test_widgets
  tests/test_widgets.c
  src/adjustment.c
  src/orientable.c
  src/range.c
  src/scrollbar.c
  src/sdl_private.c
)
target_include_directories(test_widgets PRIVATE src)
target_link_libraries(test_widgets PRIVATE ${PTX_SDL3} m)
if(NOT MSVC)
  target_compile_options(test_widgets PRIVATE -Wall -Wextra)
endif()
add_test(NAME widgets COMMAND test_widgets)

add_executable(test_boxdraw tests/test_boxdraw.c src/boxdraw.c)
target_include_directories(test_boxdraw PRIVATE src)
target_link_libraries(test_boxdraw PRIVATE ${PTX_SDL3})
if(NOT MSVC)
  target_compile_options(test_boxdraw PRIVATE -Wall -Wextra)
endif()
add_test(NAME boxdraw COMMAND test_boxdraw)

# Terminal-engine tests: UTF-8/box-glyph decoding through the vendored
# libvterm. Needs no window; this is the "stray b" regression test.
add_executable(test_utf8 tests/test_utf8.c)
target_link_libraries(test_utf8 PRIVATE vterm)
if(NOT MSVC)
  target_compile_options(test_utf8 PRIVATE -Wall -Wextra)
endif()
add_test(NAME utf8 COMMAND test_utf8)
