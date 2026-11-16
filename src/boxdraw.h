#ifndef PTX_BOXDRAW_H
#define PTX_BOXDRAW_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * Procedural rendering of Unicode box-drawing (U+2500–U+257F), block
 * elements (U+2580–U+259F) and braille patterns (U+2800–U+28FF).
 *
 * Terminals cannot rely on the font for these: a monospace face's glyph
 * advance rarely matches the terminal's cell metrics (Adwaita Mono draws
 * box characters 21 px tall in a 20 px cell, so verticals break at every
 * row), fonts leave strokes detached (lines do not join between cells),
 * and most monospace fonts lack the double-line set entirely (opencode and
 * other TUIs use U+256D..U+257F heavily), which makes them draw as boxes.
 * Drawing the geometry ourselves guarantees pixel-perfect joins at any font
 * size, and it is what foot, kitty, alacritty and wezterm do.
 */

/* True when the codepoint is drawn by this module instead of the font. */
bool boxdraw_has_codepoint(uint32_t cp);

/*
 * Draw one cell. The cell is the rectangle (x, y, w, h); `fg` is the
 * assembled 0xRRGGBB colour. Returns false if `cp` is not handled, in which
 * case the caller should render it with the font.
 */
bool boxdraw_draw(SDL_Renderer *renderer, uint32_t cp,
                  float x, float y, float w, float h, uint32_t fg);

#endif /* PTX_BOXDRAW_H */
