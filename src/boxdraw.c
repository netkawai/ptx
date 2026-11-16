#include "boxdraw.h"

#include <SDL3/SDL_stdinc.h>

/*
 * Geometry model
 * --------------
 * A box-drawing cell is described by which of its four edges a stroke
 * reaches: up, down, left, right, plus whether that stroke is single or
 * double. Every character in the block is exactly one such combination, so
 * we decode the codepoint into four small enums and draw segments that
 * always terminate on the cell edge. Because segments touch the edges, two
 * neighbouring cells join with no seam at any font size — which is what
 * fonts fail to do.
 */

enum { NONE = 0, SINGLE = 1, DOUBLE = 2 };

#define MAX_SEGMENTS 12

typedef struct {
    float x0, y0, x1, y1;
} Segment;

typedef struct {
    Segment seg[MAX_SEGMENTS];
    int     count;
} SegList;

/* Append an axis-aligned filled rectangle as a segment (x0<=x1, y0<=y1). */
static void add_rect(SegList *l, float x0, float y0, float x1, float y1) {
    if (l->count >= MAX_SEGMENTS) return;
    if (x0 > x1) { float t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { float t = y0; y0 = y1; y1 = t; }
    l->seg[l->count++] = (Segment){ x0, y0, x1, y1 };
}

enum { UP, DOWN, LEFT, RIGHT };

/*
 * Stroke positions on each axis, in cell-local pixels.
 *
 * Single strokes are 1 px, centred on the cell and snapped to whole pixels.
 * Double strokes are two 1 px lines separated by a 1 px gap, centred the same
 * way, which is how the reference fonts draw them.
 */

/* A stroke on an axis, as a half-open pixel span [lo, hi). */
typedef struct {
    float lo, hi;
} Span;

/*
 * Place the stroke(s) for one axis.
 *
 * `extent` is the cell size along the axis the stroke runs *across* (so the
 * width for vertical strokes). Single lines are 1 px, centred on the cell
 * and snapped to whole pixels; double lines are two 1 px strands with a 1 px
 * gap, centred the same way. Snapping matters: an unsnapped 1 px span on a
 * pixel boundary gets filtered into two half-lit columns, which is what
 * makes hand-positioned box drawing look blurry.
 */
static void place_spans(float extent, bool dbl, Span *first, Span *second) {
    float px = SDL_floorf(extent);

    if (!dbl) {
        float a = SDL_floorf((px - 1.0f) / 2.0f);
        first->lo = a;
        first->hi = a + 1.0f;
        second->lo = second->hi = 0.0f;
        return;
    }

    /* Two strands + one gap = 3 px, centred. */
    float a = SDL_floorf((px - 3.0f) / 2.0f);
    first->lo = a;
    first->hi = a + 1.0f;
    second->lo = a + 2.0f;
    second->hi = a + 3.0f;
}

/* The centre of the 1 px single stroke placed by place_spans(), so the two
 * axes of a junction meet exactly in the middle of the drawn pixel. */
static float line_centre(float extent) {
    Span s, unused;
    place_spans(extent, false, &s, &unused);
    return (s.lo + s.hi) * 0.5f;
}

/* Draw one arm of a box-drawing character from the cell centre towards the
 * given edge, terminating exactly on the edge. */
static void add_arm(SegList *l, int dir, int style,
                    float centre_x, float centre_y,
                    float w, float h) {
    Span a, b;

    if (style == SINGLE) {
        switch (dir) {
            case UP:
                place_spans(w, false, &a, &b);
                add_rect(l, a.lo, 0, a.hi, centre_y + 0.5f);
                break;
            case DOWN:
                place_spans(w, false, &a, &b);
                add_rect(l, a.lo, centre_y - 0.5f, a.hi, h);
                break;
            case LEFT:
                place_spans(h, false, &a, &b);
                add_rect(l, 0, a.lo, centre_x + 0.5f, a.hi);
                break;
            case RIGHT:
                place_spans(h, false, &a, &b);
                add_rect(l, centre_x - 0.5f, a.lo, w, a.hi);
                break;
        }
    } else {
        switch (dir) {
            case UP:
                place_spans(w, true, &a, &b);
                add_rect(l, a.lo, 0, a.hi, centre_y + 0.5f);
                add_rect(l, b.lo, 0, b.hi, centre_y + 0.5f);
                break;
            case DOWN:
                place_spans(w, true, &a, &b);
                add_rect(l, a.lo, centre_y - 0.5f, a.hi, h);
                add_rect(l, b.lo, centre_y - 0.5f, b.hi, h);
                break;
            case LEFT:
                place_spans(h, true, &a, &b);
                add_rect(l, 0, a.lo, centre_x + 0.5f, a.hi);
                add_rect(l, 0, b.lo, centre_x + 0.5f, b.hi);
                break;
            case RIGHT:
                place_spans(h, true, &a, &b);
                add_rect(l, centre_x - 0.5f, a.lo, w, a.hi);
                add_rect(l, centre_x - 0.5f, b.lo, w, b.hi);
                break;
        }
    }
}

/*
 * Decode U+2500..U+257F into up/down/left/right styles.
 *
 * The block is laid out in a regular pattern (light, heavy, light+heavy,
 * double) which lets us use small tables rather than 128 hand-written cases.
 * Returns false if the codepoint is not in the block.
 */
static bool decode_box(uint32_t cp, int style[4]) {
    if (cp < 0x2500 || cp > 0x257F) return false;

    style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = NONE;

    /* ---- Light set: U+2500..U+254B -------------------------------- */
    switch (cp) {
    /* Dashes: horizontal only (handled by the dashed/plain path below). */
    case 0x2500: style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2501: style[LEFT] = style[RIGHT] = SINGLE; break;

    /* Half lines */
    case 0x2574: style[LEFT]  = SINGLE; break;
    case 0x2575: style[UP]    = SINGLE; break;
    case 0x2576: style[RIGHT] = SINGLE; break;
    case 0x2577: style[DOWN]  = SINGLE; break;

    /* Horizontals / verticals. Note: the font supplies these too, but we
     * draw them so they reach the cell edges and join up. */
    case 0x2502: style[UP]   = SINGLE; style[DOWN]  = SINGLE; break;
    case 0x2503: style[UP]   = SINGLE; style[DOWN]  = SINGLE; break;
    case 0x2504: style[LEFT] = SINGLE; style[RIGHT] = SINGLE; break; /* dashed */
    case 0x2505: style[LEFT] = SINGLE; style[RIGHT] = SINGLE; break;

    /* Corners */
    case 0x250C: style[DOWN] = SINGLE; style[RIGHT] = SINGLE; break;
    case 0x250D: style[DOWN] = SINGLE; style[RIGHT] = SINGLE; break;
    case 0x250E: style[DOWN] = SINGLE; style[RIGHT] = SINGLE; break;
    case 0x250F: style[DOWN] = SINGLE; style[RIGHT] = SINGLE; break;
    case 0x2510: style[DOWN] = SINGLE; style[LEFT]  = SINGLE; break;
    case 0x2511: style[DOWN] = SINGLE; style[LEFT]  = SINGLE; break;
    case 0x2512: style[DOWN] = SINGLE; style[LEFT]  = SINGLE; break;
    case 0x2513: style[DOWN] = SINGLE; style[LEFT]  = SINGLE; break;
    case 0x2514: style[UP]   = SINGLE; style[RIGHT] = SINGLE; break;
    case 0x2515: style[UP]   = SINGLE; style[RIGHT] = SINGLE; break;
    case 0x2516: style[UP]   = SINGLE; style[RIGHT] = SINGLE; break;
    case 0x2517: style[UP]   = SINGLE; style[RIGHT] = SINGLE; break;
    case 0x2518: style[UP]   = SINGLE; style[LEFT]  = SINGLE; break;
    case 0x2519: style[UP]   = SINGLE; style[LEFT]  = SINGLE; break;
    case 0x251A: style[UP]   = SINGLE; style[LEFT]  = SINGLE; break;
    case 0x251B: style[UP]   = SINGLE; style[LEFT]  = SINGLE; break;

    /* Tees */
    case 0x251C: style[UP] = style[DOWN] = style[RIGHT] = SINGLE; break;
    case 0x251D: style[UP] = style[DOWN] = style[RIGHT] = SINGLE; break;
    case 0x251E: style[UP] = style[DOWN] = style[RIGHT] = SINGLE; break;
    case 0x251F: style[UP] = style[DOWN] = style[RIGHT] = SINGLE; break;
    case 0x2520: style[UP] = style[DOWN] = style[RIGHT] = SINGLE; break;
    case 0x2521: style[UP] = style[DOWN] = style[RIGHT] = SINGLE; break;
    case 0x2522: style[UP] = style[DOWN] = style[RIGHT] = SINGLE; break;
    case 0x2523: style[UP] = style[DOWN] = style[RIGHT] = SINGLE; break;
    case 0x2524: style[UP] = style[DOWN] = style[LEFT]  = SINGLE; break;
    case 0x2525: style[UP] = style[DOWN] = style[LEFT]  = SINGLE; break;
    case 0x2526: style[UP] = style[DOWN] = style[LEFT]  = SINGLE; break;
    case 0x2527: style[UP] = style[DOWN] = style[LEFT]  = SINGLE; break;
    case 0x2528: style[UP] = style[DOWN] = style[LEFT]  = SINGLE; break;
    case 0x2529: style[UP] = style[DOWN] = style[LEFT]  = SINGLE; break;
    case 0x252A: style[UP] = style[DOWN] = style[LEFT]  = SINGLE; break;
    case 0x252B: style[UP] = style[DOWN] = style[LEFT]  = SINGLE; break;
    case 0x252C: style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x252D: style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x252E: style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x252F: style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2530: style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2531: style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2532: style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2533: style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2534: style[UP] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2535: style[UP] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2536: style[UP] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2537: style[UP] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2538: style[UP] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2539: style[UP] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x253A: style[UP] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x253B: style[UP] = style[LEFT] = style[RIGHT] = SINGLE; break;

    /* Crosses */
    case 0x253C: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x253D: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x253E: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x253F: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2540: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2541: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2542: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2543: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2544: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2545: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2546: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2547: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2548: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x2549: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x254A: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;
    case 0x254B: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = SINGLE; break;

    /* ---- Double set: U+2550..U+256C ----------------------------------- */
    case 0x2550: style[LEFT] = style[RIGHT] = DOUBLE; break;              /* ═ */
    case 0x2551: style[UP] = style[DOWN] = DOUBLE; break;                 /* ║ */
    case 0x2552: style[DOWN] = style[RIGHT] = DOUBLE; break;              /* ╒ */
    case 0x2553: style[DOWN] = style[RIGHT] = DOUBLE; break;              /* ╓ */
    case 0x2554: style[DOWN] = style[RIGHT] = DOUBLE; break;              /* ╔ */
    case 0x2555: style[DOWN] = style[LEFT] = DOUBLE; break;               /* ╕ */
    case 0x2556: style[DOWN] = style[LEFT] = DOUBLE; break;               /* ╖ */
    case 0x2557: style[DOWN] = style[LEFT] = DOUBLE; break;               /* ╗ */
    case 0x2558: style[UP] = style[RIGHT] = DOUBLE; break;                /* ╘ */
    case 0x2559: style[UP] = style[RIGHT] = DOUBLE; break;                /* ╙ */
    case 0x255A: style[UP] = style[RIGHT] = DOUBLE; break;                /* ╚ */
    case 0x255B: style[UP] = style[LEFT] = DOUBLE; break;                 /* ╛ */
    case 0x255C: style[UP] = style[LEFT] = DOUBLE; break;                 /* ╜ */
    case 0x255D: style[UP] = style[LEFT] = DOUBLE; break;                 /* ╝ */
    case 0x255E: style[UP] = style[DOWN] = style[RIGHT] = DOUBLE; break;  /* ╞ */
    case 0x255F: style[UP] = style[DOWN] = style[RIGHT] = DOUBLE; break;  /* ╟ */
    case 0x2560: style[UP] = style[DOWN] = style[RIGHT] = DOUBLE; break;  /* ╠ */
    case 0x2561: style[UP] = style[DOWN] = style[LEFT] = DOUBLE; break;   /* ╡ */
    case 0x2562: style[UP] = style[DOWN] = style[LEFT] = DOUBLE; break;   /* ╢ */
    case 0x2563: style[UP] = style[DOWN] = style[LEFT] = DOUBLE; break;   /* ╣ */
    case 0x2564: style[DOWN] = style[LEFT] = style[RIGHT] = DOUBLE; break;/* ╤ */
    case 0x2565: style[DOWN] = style[LEFT] = style[RIGHT] = DOUBLE; break;/* ╥ */
    case 0x2566: style[DOWN] = style[LEFT] = style[RIGHT] = DOUBLE; break;/* ╦ */
    case 0x2567: style[UP] = style[LEFT] = style[RIGHT] = DOUBLE; break;  /* ╧ */
    case 0x2568: style[UP] = style[LEFT] = style[RIGHT] = DOUBLE; break;  /* ╨ */
    case 0x2569: style[UP] = style[LEFT] = style[RIGHT] = DOUBLE; break;  /* ╩ */
    case 0x256A: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = DOUBLE; break; /* ╪ */
    case 0x256B: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = DOUBLE; break; /* ╫ */
    case 0x256C: style[UP] = style[DOWN] = style[LEFT] = style[RIGHT] = DOUBLE; break; /* ╬ */

    /* ---- Mixed double/single corners and tees: U+256D..U+2570 ---------- */
    /* opencode and other ratatui/cli tools use these for rounded panels. */
    case 0x256D: style[DOWN] = SINGLE; style[RIGHT] = SINGLE; break;  /* ╭ */
    case 0x256E: style[DOWN] = SINGLE; style[LEFT]  = SINGLE; break;  /* ╮ */
    case 0x256F: style[UP]   = SINGLE; style[LEFT]  = SINGLE; break;  /* ╯ */
    case 0x2570: style[UP]   = SINGLE; style[RIGHT] = SINGLE; break;  /* ╰ */

    /* Diagonals U+2571..U+2573 are not axis-aligned; the font draws them. */
    default:
        return false;
    }
    return true;
}

bool boxdraw_has_codepoint(uint32_t cp) {
    if (cp >= 0x2580 && cp <= 0x259F) return true;   /* blocks */
    if (cp >= 0x2800 && cp <= 0x28FF) return true;   /* braille */
    int style[4];
    if ((cp == 0x2504 || cp == 0x2505 || cp == 0x2508 || cp == 0x2509 ||
         cp == 0x254C || cp == 0x254D || cp == 0x254E || cp == 0x254F))
        return true;   /* dashed variants are approximated as solid */
    return decode_box(cp, style);
}

/* --- Block elements ------------------------------------------------- */

static void draw_blocks(SegList *l, uint32_t cp, float w, float h) {
    float hw = w * 0.5f, hh = h * 0.5f;
    float qw = w * 0.25f, qh = h * 0.25f;

    switch (cp) {
    case 0x2580: add_rect(l, 0, 0, w, hh); break;              /* ▀ upper half */
    case 0x2581: add_rect(l, 0, h - h / 8.0f, w, h); break;    /* ▁ lower 1/8 */
    case 0x2582: add_rect(l, 0, h - h / 4.0f, w, h); break;    /* ▂ lower 1/4 */
    case 0x2583: add_rect(l, 0, h - 3 * h / 8.0f, w, h); break;/* ▃ lower 3/8 */
    case 0x2584: add_rect(l, 0, hh, w, h); break;              /* ▄ lower half */
    case 0x2585: add_rect(l, 0, h - 5 * h / 8.0f, w, h); break;/* ▅ */
    case 0x2586: add_rect(l, 0, h - 3 * h / 4.0f, w, h); break;/* ▆ */
    case 0x2587: add_rect(l, 0, h - 7 * h / 8.0f, w, h); break;/* ▇ */
    case 0x2588: add_rect(l, 0, 0, w, h); break;               /* █ full */
    case 0x2589: add_rect(l, 0, 0, 7 * w / 8.0f, h); break;    /* ▉ left 7/8 */
    case 0x258A: add_rect(l, 0, 0, 3 * w / 4.0f, h); break;    /* ▊ */
    case 0x258B: add_rect(l, 0, 0, 5 * w / 8.0f, h); break;    /* ▋ */
    case 0x258C: add_rect(l, 0, 0, hw, h); break;              /* ▌ left half */
    case 0x258D: add_rect(l, 0, 0, 3 * w / 8.0f, h); break;    /* ▍ */
    case 0x258E: add_rect(l, 0, 0, qw, h); break;              /* ▎ */
    case 0x258F: add_rect(l, 0, 0, w / 8.0f, h); break;        /* ▏ */
    case 0x2590: add_rect(l, hw, 0, w, h); break;              /* ▐ right half */
    case 0x2594: add_rect(l, 0, 0, w, h / 8.0f); break;        /* ▔ upper 1/8 */
    case 0x2595: add_rect(l, w - w / 8.0f, 0, w, h); break;    /* ▕ right 1/8 */

    /* Quadrants */
    case 0x2596: add_rect(l, 0, hh, hw, h); break;                       /* ▖ */
    case 0x2597: add_rect(l, hw, hh, w, h); break;                       /* ▗ */
    case 0x2598: add_rect(l, 0, 0, hw, hh); break;                       /* ▘ */
    case 0x2599: add_rect(l, 0, 0, hw, hh); add_rect(l, 0, hh, hw, h); add_rect(l, hw, hh, w, h); break;  /* ▙ */
    case 0x259A: add_rect(l, 0, 0, hw, hh); add_rect(l, hw, hh, w, h); break;                               /* ▚ */
    case 0x259B: add_rect(l, 0, 0, hw, hh); add_rect(l, hw, 0, w, hh); add_rect(l, 0, hh, hw, h); break;   /* ▛ */
    case 0x259C: add_rect(l, 0, 0, hw, hh); add_rect(l, hw, 0, w, hh); add_rect(l, hw, hh, w, h); break;   /* ▜ */
    case 0x259D: add_rect(l, hw, 0, w, hh); break;                       /* ▝ */
    case 0x259E: add_rect(l, hw, 0, w, hh); add_rect(l, 0, hh, hw, h); break;                               /* ▞ */
    case 0x259F: add_rect(l, hw, 0, w, hh); add_rect(l, 0, hh, hw, h); add_rect(l, hw, hh, w, h); break;   /* ▟ */

    /* Shade characters are drawn as a dither approximating the density. */
    case 0x2591: /* ░ 25% */
    case 0x2592: /* ▒ 50% */
    case 0x2593: /* ▓ 75% */
        return; /* handled by the caller with blending */

    default:
        (void)qw; (void)qh;
        break;
    }
}

/* Braille: a 2x4 dot grid filling the cell. */
static void draw_braille(SegList *l, uint32_t cp, float w, float h) {
    static const int bits[8] = { 0x01, 0x02, 0x04, 0x40, 0x08, 0x10, 0x20, 0x80 };
    float cw = w / 2.0f, ch = h / 4.0f;
    uint32_t m = cp - 0x2800;
    for (int i = 0; i < 8; i++) {
        if (!(m & bits[i])) continue;
        int col = i & 1;
        int row = i >> 1;
        add_rect(l, (float)col * cw + cw * 0.15f, (float)row * ch + ch * 0.15f,
                 (float)col * cw + cw * 0.85f, (float)row * ch + ch * 0.85f);
    }
}

bool boxdraw_draw(SDL_Renderer *renderer, uint32_t cp,
                  float x, float y, float w, float h, uint32_t fg) {
    if (!boxdraw_has_codepoint(cp)) return false;

    uint8_t r = (uint8_t)((fg >> 16) & 0xFF);
    uint8_t g = (uint8_t)((fg >> 8) & 0xFF);
    uint8_t b = (uint8_t)(fg & 0xFF);

    SegList l = { .count = 0 };

    if (cp >= 0x2800 && cp <= 0x28FF) {
        draw_braille(&l, cp, w, h);
    } else if (cp >= 0x2580 && cp <= 0x259F) {
        if (cp >= 0x2591 && cp <= 0x2593) {
            /* Shades: fill the cell with a checkerboard density. */
            int step = cp == 0x2591 ? 4 : cp == 0x2592 ? 2 : 2;
            bool invert = (cp == 0x2593);
            SDL_SetRenderDrawColor(renderer, r, g, b, 255);
            for (int py = 0; py < (int)h; py += step) {
                for (int px = 0; px < (int)w; px += step) {
                    bool on = ((px / step) + (py / step)) % 2 == 0;
                    if (invert) on = !on;
                    if (!on) continue;
                    SDL_FRect d = { x + (float)px, y + (float)py, (float)step, (float)step };
                    SDL_RenderFillRect(renderer, &d);
                }
            }
            return true;
        }
        draw_blocks(&l, cp, w, h);
    } else {
        int style[4];
        bool dashed = (cp == 0x2504 || cp == 0x2505 || cp == 0x2508 || cp == 0x2509 ||
                       cp == 0x254C || cp == 0x254D || cp == 0x254E || cp == 0x254F);
        if (dashed) {
            /* Approximate dashes as solid strokes; they are rare in TUIs. */
            style[LEFT] = style[RIGHT] = SINGLE;
            if (cp == 0x2508 || cp == 0x2509) style[UP] = style[DOWN] = SINGLE;
            if (cp == 0x254C || cp == 0x254D) { style[LEFT] = style[RIGHT] = NONE; style[UP] = style[DOWN] = SINGLE; }
        } else if (!decode_box(cp, style)) {
            return false;
        }

        float cx = line_centre(w);
        float cy = line_centre(h);

        for (int d = 0; d < 4; d++) {
            if (style[d] == NONE) continue;
            add_arm(&l, d, style[d], cx, cy, w, h);
        }
    }

    if (l.count == 0) return true;   /* nothing to draw (space-like) */

    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    for (int i = 0; i < l.count; i++) {
        SDL_FRect rect = {
            x + l.seg[i].x0, y + l.seg[i].y0,
            l.seg[i].x1 - l.seg[i].x0, l.seg[i].y1 - l.seg[i].y0
        };
        /* Overdraw by half a pixel so anti-aliasing/mosaicing cannot leave a
         * seam between adjacent cells. */
        SDL_RenderFillRect(renderer, &rect);
    }
    return true;
}
