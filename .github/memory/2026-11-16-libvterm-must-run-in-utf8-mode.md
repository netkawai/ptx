---
title: "Run libvterm in UTF-8 mode or box-drawing glyphs decode as 'b'"
date: 2026-11-16T00:00:00
category: terminal-rendering
related: [tech-other]
sources: [N]
original_query: "I still see lots of 'b' b9, instead of drawing boxing style board, check more code"
---

# Run libvterm in UTF-8 mode

## Problem

opencode-style TUIs still rendered their borders as runs of `b`
(`bbbb`) instead of `┌──┐`, even after box-drawing characters were switched
to procedural geometry in `2026-11-16-draw-box-drawing-characters-as-geometry.md`.

That earlier note blamed the **font**. The real cause is one layer lower:
**libvterm was decoding the byte stream in non-UTF-8 mode.**

`vterm_new()` leaves `vt->mode.utf8` at 0, and `main.c` never called
`vterm_set_utf8()`. With UTF-8 off, libvterm routes each high byte through the
current GL 94-character charset instead of assembling a multi-byte sequence:

| UTF-8 bytes | U+ | what ptx rendered |
| --- | --- | --- |
| `e2 94 8c` (`┌`) | U+250C | `b` |
| `e2 94 80` (`─`) | U+2500 | `b` |
| `e2 94 82` (`│`) | U+2502 | `b` |

Each 3-byte glyph is not assembled; the bytes are consumed individually
through the active GL charset and collapse to a `b`-family cell (`0x62`). In
the DEC Special Graphics table `0x62` is U+2409 SYMBOL FOR HORIZONTAL TAB,
which is the glyph that showed up on screen. Because the
cells never contain U+2500..U+257F, `boxdraw_has_codepoint()` never matches and
the font path draws the control pictures instead. So the boxdraw work was
correct but was never reached.

## Decision

Call `vterm_set_utf8(app.vterm, 1)` immediately after `vterm_new()` in
`src/main.c`.

ptx advertises `TERM=xterm-256color` to the child, so applications emit UTF-8
and the engine must decode UTF-8. Leaving it off also breaks every wide
character, so this was a latent bug beyond the box-drawing symptom.

## How it was found

Feeding the stream to the vendored libvterm directly and dumping cells:

- `utf8=0` + UTF-8 `┌──┐` -> `bbbb`
- `utf8=1` + UTF-8 `┌──┐` -> `┌──┐` (correct)

Reading `render`, `boxdraw`, `buffer` and `cell` did not explain it because
those layers were all fine; the corruption happened before any of them ran.
Checking `vterm_get_utf8()` on the live object is the fastest way to diagnose
"box characters look like ASCII" reports.

## Gotchas

- DEC Special Graphics genuinely maps `b` -> U+2409. If a stream leaves GL set
  to DEC graphics (`ESC(0` without a later `ESC(B`), `b` is *supposed* to be a
  control picture. That is correct upstream behaviour, not a bug — do not
  "fix" it by special-casing `b`.
- When writing C test fixtures for UTF-8 bytes, `"\xe2\x94\x82b"` is wrong:
  `\x82b` is parsed as a single hex escape. Split the literal
  (`"\xe2\x94\x82" "b"`). This produced a misleading probe result once.

## Verification

`tests/test_utf8.c` decodes UTF-8 box glyphs, DEC graphics and a CJK
codepoint through the vendored libvterm and asserts the resulting cell
codepoints. Reverting the `vterm_set_utf8()` call makes the test abort on the
first assertion; restoring it passes. Full suite: `ctest` 3/3.

## Files

- `src/main.c` — `vterm_set_utf8(app.vterm, 1)` after `vterm_new()`
- `tests/test_utf8.c` — regression test
- `CMakeLists.txt` — `test_utf8` target linked against `vterm`
