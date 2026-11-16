---
title: "Vendor libvterm as a separate shared library instead of an external package"
date: 2026-11-16T00:00:00
category: build
related: [terminal-rendering]
sources: [N]
original_query: "compile libvterm source code in ptx instead of using it as external library; license is different so keep it in a separate folder, download latest original sources, update CMakeLists.txt, keep libvterm as a shared library"
---

# Vendor libvterm as a separate shared library

## Problem

ptx linked against whatever libvterm the host provided
(`pkg_check_modules(VTERM ... vterm)`). That made the build depend on an
externally installed 0.3.x package, and the header used to compile ptx could
silently disagree with the library loaded at runtime.

## Decision

Copy upstream libvterm's library sources into `libvterm/` and build them from
this repository as their own **shared** library target (`vterm`); the `ptx`
executable links against it and does not compile those sources itself.

- Source: <https://github.com/neovim/libvterm>, commit
  `934bc2fb` (2025-11-20), upstream version 0.3.3. Unmodified.
- Licence: upstream is **MIT**, ptx is **0BSD**. They are compatible, but the
  separate directory with its own `LICENSE` and own CMake target keeps the
  vendored code clearly delimited from ptx's own sources.
- `libvterm/CMakeLists.txt` defines the target, `SOVERSION 0` to match
  upstream's libtool `-version-info 0:0:0` (→ `libvterm.so.0` /
  `libvterm.0.dylib`).
- The ptx build tree gets `RUNPATH=$ORIGIN/../libvterm` automatically from
  CMake, so `build/ptx` finds `build/libvterm/libvterm.so.0` with no
  `LD_LIBRARY_PATH` or install.
- The old `pkg_check_modules(VTERM ...)` dependency and the macOS
  `scripts/fix-macos-install-names.sh` workaround are removed: they existed
  only because a foreign libvterm recorded `/usr/local/lib` as its install
  name. A locally built target has no such problem.

## Rationale

Vendoring pins an exact, known-good libvterm, removes a system dependency,
and makes it possible to patch the terminal engine later without waiting for
a distro package. Building it as a shared library rather than compiling the
`.c` files into `ptx` keeps the third-party code — and its licence — at
arm's length, per the project's separation rule.

## Gotchas found while wiring it up

- Inside `add_subdirectory(libvterm)`, paths are relative to that directory.
  Adding `${CMAKE_CURRENT_SOURCE_DIR}/libvterm/...` prefixes produces
  `libvterm/libvterm/include`, which sends the compiler at a *stale system*
  `vterm.h` (no `struct.ok` error), yielding confusing "no member named
  `premove` / `sb_pushline4`" failures.
- Upstream is built with `-Wall`, not `-Wextra`; it is not `-Wextra` clean.
  Only `-Wall` is enabled for the target, and its headers are included as
  `SYSTEM` so they don't pollute ptx's warnings.

## Verification

`./build-ptx.sh` builds `build/libvterm/libvterm.so.0.0.0` and
`build/ptx` with no warnings from ptx sources; `ldd build/ptx` resolves
`libvterm.so.0` from the build tree; `ctest` passes; `build/ptx` launches
under the dummy video driver.

## Files

- `libvterm/{include,src,LICENSE}` — upstream sources
- `libvterm/CMakeLists.txt` — the `vterm` shared-library target
- `libvterm/README.md` — provenance and update instructions
- `CMakeLists.txt` — `add_subdirectory(libvterm)`, link `vterm`
- `AGENTS.md` — documents the vendored directory
