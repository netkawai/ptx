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

pkg_check_modules(VTERM REQUIRED IMPORTED_TARGET vterm)

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
)

target_include_directories(ptx PRIVATE src)

target_link_libraries(ptx PRIVATE
  ${PTX_SDL3}
  ${PTX_SDL3_TTF}
  PkgConfig::VTERM
  Threads::Threads
)

# forkpty() lives in libutil on macOS/BSD.
if(APPLE OR CMAKE_SYSTEM_NAME MATCHES "BSD")
  target_link_libraries(ptx PRIVATE util)
endif()

if(NOT MSVC)
  target_compile_options(ptx PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# Ensure libraries found under a non-default prefix (e.g. libvterm in
# ~/.local/lib) are located at runtime on ELF platforms.
set_target_properties(ptx PROPERTIES
  BUILD_RPATH "${VTERM_LIBRARY_DIRS}"
  INSTALL_RPATH "${VTERM_LIBRARY_DIRS}"
)

# macOS: repair absolute dylib install names. Some libraries (notably libvterm)
# record an install name like /usr/local/lib/libvterm.0.dylib even when they
# live under another prefix; dyld resolves absolute names directly, so rpath
# does not help and the binary fails at launch. See the script for details.
if(APPLE)
  string(REPLACE ";" " " PTX_VTERM_LIB_DIRS "${VTERM_LIBRARY_DIRS}")
  add_custom_command(TARGET ptx POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E env
            "VTERM_LIBRARY_DIRS=${PTX_VTERM_LIB_DIRS}"
            /bin/sh "${CMAKE_CURRENT_SOURCE_DIR}/scripts/fix-macos-install-names.sh"
            "$<TARGET_FILE:ptx>"
    VERBATIM)
endif()

install(TARGETS ptx RUNTIME DESTINATION bin)
