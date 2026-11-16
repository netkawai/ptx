---
title: "Draw box-drawing and block characters as geometry, not font glyphs"
date: 2026-11-16T00:00:00
category: terminal-rendering
related: [tech-other]
sources: [N]
original_query: "opencode shows lots of b / broken boxing in ptx"
---

# Draw box-drawing and block characters as geometry, not font glyphs

## Problem

opencode (a TUI built with Bubble Tea/ratatui-style panels) draws its whole
UI from box-drawing characters. In ptx it showed hollow squares (`□`), stray
`b`-like glyphs, and dotted vertical borders.

Measured causes, in order of severity:

1. **Missing glyphs.** The previously preferred font (Liberation Mono) has
   no U+256D–U+2570 (`╭╮╰╯`), which ptx's own fallback chain then rendered
   as notdef boxes. SDL_ttf does not fall back to another face.
2. **Glyph advance ≠ cell size.** Box-drawing glyphs are routinely a pixel
   larger than the cell: in Adwaita Mono `─` is 11 px wide in a 10 px cell,
   and `│` is 21 px tall in a 20 px cell. Drawn at `col * glyph_w`, strokes
   therefore overlap or fall short, and neighbours do not join.
3. **Baseline vs cell top.** Vertical strokes stop ~1 px above the cell
   bottom, so consecutive `│` cells leave a gap and the border looks dotted.

No font choice fixes (2) and (3) reliably; they are a mismatch between text
metrics and a fixed terminal grid.

> **Update:** the stray `b` glyphs in the original report had a different,
> earlier cause — libvterm was not running in UTF-8 mode, so the box glyphs
> were never decoded to U+2500..U+257F and never reached `boxdraw` at all.
> See `2026-11-16-libvterm-must-run-in-utf8-mode.md`. The procedural
> rendering below is still correct and still needed.

## Decision

Render U+2500–U+257F (box drawing), U+2580–U+259F (block elements) and
U+2800–U+28FF (braille) procedurally as filled rectangles, in
`src/boxdraw.c`, and skip the font for those cells.

- Each box character is decoded into four arms (up/down/left/right) with a
  single/double style, then drawn as 1 px axes-aligned spans that terminate
  exactly on the cell edge. Joins are therefore seamless at any cell size.
- Strokes are snapped to whole pixels so they never straddle a boundary and
  get filtered into two half-lit columns.
- Double lines are two 1 px strands with a 1 px gap, centred.
- Blocks/quadrants cover exact cell fractions; shades are stippled; braille
  is a 2×4 dot grid.
- Diagonals (U+2571–U+2573) are still left to the font, as are characters
  outside these blocks.
- The default font list now prefers Adwaita Mono, which has the full sets;
  that only helps non-procedural text, since the boxes no longer depend on
  the font.

This is what foot, kitty, alacritty and wezterm do, for the same reason.

## Verification

`tests/test_boxdraw.c` asserts every character opencode uses is handled, that
ordinary text and CJK still go to the font, and that a 20-cell run of `─ │
═ ║` has **zero blank slices** at an awkward 11×20 cell size. A real opencode
style double-line panel was rendered to a bitmap and inspected.

## Files

- `src/boxdraw.{h,c}` — the renderer
- `src/render.c` — dispatches box/block/braille cells to `boxdraw_draw()`
- `src/main.c` — font candidates reordered
- `tests/test_boxdraw.c`, `CMakeLists.txt` — regression test

## Note for future widgets

This follows the GTK4-mirroring rule in
`2026-11-16-ui-toolkit-apis-must-mirror-gtk4.md`: the *API* stays a GTK port,
but the *implementation* may differ from what GTK would do where ptx's
constraints (a fixed terminal grid) demand it.
