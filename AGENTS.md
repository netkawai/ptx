# AGENTS.md

Project guidance for agents working in this repository.

## What this is

`ptx` is a small terminal emulator written in C11. It uses **libvterm** for
VT/ANSI parsing and **SDL3 + SDL3_ttf** for rendering and input. It targets
Linux (including Wayland) and macOS.

## Build and run

```sh
./build-ptx.sh          # wraps CMake; produces build/ptx
./build/ptx             # smoke test
```

Dependencies are resolved through CMake config packages, falling back to
pkg-config: **SDL3**, **SDL3_ttf**, and **libvterm**. A plain CMake build works
too:

```sh
cmake -S . -B build && cmake --build build -j
```

## Layout

| Area | Files |
| --- | --- |
| Entry point / main loop | `src/main.c` |
| Shared types and tunables | `src/app.h` (`TerminalApp`, font/scrollback/menu constants) |
| PTY + libvterm wiring | `src/terminal.c`, `src/terminal.h` |
| Screen buffer and scrollback | `src/buffer.c`, `src/cell.c` |
| Rendering | `src/render.c` |
| Input and key mapping | `src/input.c` |
| Selection and clipboard | `src/selection.c` |
| Context menu overlay | `src/menu.c` |
| Layout / metrics | `src/layout.c` |
| String helpers | `src/strutil.c` |
| Build helpers | `scripts/`, `CMakeLists.txt` |

## Conventions

- C11 (`-Wall -Wextra`); do not introduce new warnings.
- 4-space indentation. Header guards use the `PTX_<NAME>_H` form.
- Public functions are prefixed by their module (`terminal_*`, `render_*`, ...).
- **libvterm is not thread-safe.** All access must be serialised on
  `app->vterm_lock`; the PTY reader thread and the render thread both touch it.
- Request a redraw with `app_mark_dirty(app)` instead of drawing directly.
- Scrollback cells use the compact `SbCell`, not a full `VTermScreenCell`.
- Prefer small, focused changes; keep platform-specific code guarded
  (`#ifdef __APPLE__`, `APPLE` in CMake).

## Verification

- Build cleanly with `./build-ptx.sh`.
- Launch `build/ptx` and confirm the shell works, resize behaves, scrollback and
  copy/paste still function.

## Memory notes

When we settle a design decision, record it in `.github/memory/`:

- Filename: `YYYY-MM-DD-short-slug.md`.
- Keep the existing frontmatter shape used by the other notes (title, date,
  category, related, sources, original_query).
- State the decision and the rationale, and link to the relevant files.
- Never include secrets, tokens, credentials, or personal chat/session links.

## Do not commit

- Secrets of any kind: API keys, tokens, `.env` files, credentials.
- Generated build output (`build/`).
