/*
 * Box-drawing / block rendering tests.
 *
 * Regression cover for the opencode bug: a TUI's panels are drawn entirely
 * from U+2500..U+259F. Fonts cannot be trusted for these (advance sizes do
 * not match the terminal cell, strokes stop short of the cell edge, and the
 * double-line set is often missing), so ptx draws them as geometry. These
 * tests pin down that (a) the characters are handled by boxdraw at all and
 * (b) neighbouring cells join with no blank seam.
 */
#include "boxdraw.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

#include <SDL3/SDL.h>

static int blank_slices(SDL_Renderer *ren, int gw, int gh,
                        uint32_t cp, int count, int vertical) {
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    for (int i = 0; i < count; i++) {
        boxdraw_draw(ren, cp,
                     (float)(vertical ? 0 : i * gw),
                     (float)(vertical ? i * gh : 0),
                     (float)gw, (float)gh, 0xFFFFFF);
    }

    SDL_Surface *s = SDL_RenderReadPixels(ren, NULL);
    assert(s);
    Uint32 *px = (Uint32 *)s->pixels;
    SDL_PixelFormatDetails *det =
        (SDL_PixelFormatDetails *)SDL_GetPixelFormatDetails(s->format);

    int slices = vertical ? count * gh : count * gw;
    int across = vertical ? gw : gh;
    int blanks = 0;
    for (int a = 0; a < slices; a++) {
        int ink = 0;
        for (int b = 0; b < across; b++) {
            int x = vertical ? b : a;
            int y = vertical ? a : b;
            Uint8 r, g, bl, al;
            SDL_GetRGBA(px[y * (s->pitch / 4) + x], det, NULL, &r, &g, &bl, &al);
            if (r > 40) ink++;
        }
        if (!ink) blanks++;
    }
    SDL_DestroySurface(s);
    return blanks;
}

int main(void) {
    /* Every character opencode / ratatui use for panels must be ours. */
    const uint32_t must_handle[] = {
        0x2500, 0x2502, 0x250C, 0x2510, 0x2514, 0x2518,  /* single lines  */
        0x251C, 0x2524, 0x252C, 0x2534, 0x253C,          /* single tees   */
        0x2550, 0x2551, 0x2554, 0x2557, 0x255A, 0x255D,  /* double lines  */
        0x255E, 0x2560, 0x2561, 0x2563, 0x256A, 0x256C,  /* double tees   */
        0x256D, 0x256E, 0x256F, 0x2570,                  /* rounded       */
        0x2588, 0x2580, 0x2584, 0x2591, 0x2592, 0x2593,  /* blocks/shades */
        0x2800, 0x28FF,                                  /* braille       */
    };
    for (size_t i = 0; i < sizeof(must_handle) / sizeof(must_handle[0]); i++)
        assert(boxdraw_has_codepoint(must_handle[i]));

    /* Ordinary text must still go to the font. */
    assert(!boxdraw_has_codepoint('A'));
    assert(!boxdraw_has_codepoint(' '));
    assert(!boxdraw_has_codepoint(0x4E2D));   /* a CJK ideograph */

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *win = SDL_CreateWindow("t", 320, 320, SDL_WINDOW_HIDDEN);
    SDL_Renderer *ren = SDL_CreateRenderer(win, NULL);
    assert(win && ren);

    /* An 11x20 cell is an awkward, non-integer-friendly metric; joins must
     * still be seamless. */
    const int gw = 11, gh = 20;
    SDL_DestroyWindow(win);
    win = SDL_CreateWindow("t", gw * 20, gh * 20, SDL_WINDOW_HIDDEN);
    ren = SDL_CreateRenderer(win, NULL);
    assert(win && ren);
    assert(blank_slices(ren, gw, gh, 0x2500, 20, 0) == 0);  /* ─ */
    assert(blank_slices(ren, gw, gh, 0x2502, 20, 1) == 0);  /* │ */
    assert(blank_slices(ren, gw, gh, 0x2550, 20, 0) == 0);  /* ═ */
    assert(blank_slices(ren, gw, gh, 0x2551, 20, 1) == 0);  /* ║ */

    printf("all boxdraw tests passed\n");
    return 0;
}
