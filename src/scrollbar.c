#include "scrollbar.h"
#include "app.h"
#include "sdl_private.h"

#include <stdlib.h>

/* Geometry of the libadwaita scrollbar at 1x scale, in pixels: a 6 px
 * slider centred in the gutter, widening to 9 px while hovered or dragged. */
#define THUMB_WIDTH       6.0f
#define THUMB_WIDTH_HOVER 9.0f
#define THUMB_MARGIN      2.0f

/* Adwaita colours, as the theme draws them: a barely-there trough and a
 * translucent grey slider that darkens while the bar is active. */
#define TROUGH_RGBA       0, 0, 0, 25
#define SLIDER_RGBA       120, 120, 120, 140
#define SLIDER_RGBA_HOVER 120, 120, 120, 210

/* A scrollbar owns the adjustment only when it created it (GTK passes the
 * adjustment straight through when the caller supplies one). */
static PtxAdjustment *scrollbar_default_adjustment(void) {
    return ptx_adjustment_new(0.0, 0.0, 0.0, 1.0, 1.0, 0.0);
}

PtxScrollbar *ptx_scrollbar_new(PtxOrientation orientation,
                                PtxAdjustment *adjustment) {
    PtxScrollbar *self = calloc(1, sizeof(*self));
    if (!self) return NULL;

    if (!adjustment) adjustment = scrollbar_default_adjustment();
    ptx_range_init(&self->range, adjustment, orientation);
    return self;
}

void ptx_scrollbar_free(PtxScrollbar *self) {
    free(self);
}

PtxAdjustment *ptx_scrollbar_get_adjustment(PtxScrollbar *self) {
    return ptx_range_get_adjustment(&self->range);
}

void ptx_scrollbar_set_adjustment(PtxScrollbar *self,
                                  PtxAdjustment *adjustment) {
    if (!adjustment) adjustment = scrollbar_default_adjustment();
    ptx_range_set_adjustment(&self->range, adjustment);
    if (self->range.value_changed) self->range.value_changed(&self->range);
}

void ptx_scrollbar_allocate(PtxScrollbar *self, float x, float y,
                            float w, float h) {
    ptx_range_set_allocation(&self->range, x, y, w, h);
}

bool ptx_scrollbar_is_visible(PtxScrollbar *self) {
    PtxAdjustment *adj = ptx_scrollbar_get_adjustment(self);
    return ptx_adjustment_get_upper(adj) - ptx_adjustment_get_lower(adj) >
           ptx_adjustment_get_page_size(adj);
}

bool ptx_scrollbar_is_active(PtxScrollbar *self) {
    return self->hovering || self->dragging;
}

bool ptx_scrollbar_get_slider(PtxScrollbar *self, PtxRect *slider) {
    if (!ptx_scrollbar_is_visible(self)) {
        *slider = (PtxRect){0};
        return false;
    }
    float spread = ptx_scrollbar_is_active(self) ? THUMB_WIDTH_HOVER
                                                  : THUMB_WIDTH;
    return ptx_range_slider_geometry(&self->range, spread, NULL, slider);
}

bool ptx_scrollbar_hit(PtxScrollbar *self, float x, float y) {
    PtxRect a = self->range.allocation;
    return x >= a.x && x < a.x + a.w && y >= a.y && y < a.y + a.h;
}

bool ptx_scrollbar_hit_slider(PtxScrollbar *self, float x, float y) {
    PtxRect slider;
    if (!ptx_scrollbar_get_slider(self, &slider)) return false;
    /* Keep the full gutter width as the grab area: Adwaita widens the slider
     * on hover, so hit-testing the thin idle shape would feel inconsistent. */
    PtxRect a = self->range.allocation;
    return x >= a.x && x < a.x + a.w &&
           y >= slider.y && y < slider.y + slider.h;
}

void ptx_scrollbar_click_trough(PtxScrollbar *self, float x, float y) {
    PtxRect a = self->range.allocation;
    if (ptx_orientable_get_orientation(&self->range.orientable)
            == PTX_ORIENTATION_VERTICAL) {
        /* Above the slider scrolls towards the top, below towards the
         * bottom: PAGE_BACKWARD / PAGE_FORWARD, as GtkRange does. */
        PtxRect slider;
        bool has_slider = ptx_scrollbar_get_slider(self, &slider);
        if (!has_slider) return;
        if (y < slider.y)
            ptx_range_move_slider(&self->range, PTX_SCROLL_PAGE_BACKWARD);
        else if (y > slider.y + slider.h)
            ptx_range_move_slider(&self->range, PTX_SCROLL_PAGE_FORWARD);
    } else {
        if (x < a.x + a.w / 2.0f)
            ptx_range_move_slider(&self->range, PTX_SCROLL_PAGE_BACKWARD);
        else
            ptx_range_move_slider(&self->range, PTX_SCROLL_PAGE_FORWARD);
    }
}

void ptx_scrollbar_drag(PtxScrollbar *self, float start_x, float start_y,
                        float x, float y, double start_value) {
    PtxRect a = self->range.allocation;
    bool vertical = ptx_orientable_get_orientation(&self->range.orientable)
                    == PTX_ORIENTATION_VERTICAL;

    float length = vertical ? (a.h - a.y) : (a.w);
    if (length <= 0.0f) return;

    PtxAdjustment *adj = ptx_scrollbar_get_adjustment(self);
    double page = ptx_adjustment_get_page_size(adj);
    double lower = ptx_adjustment_get_lower(adj);
    double upper = ptx_adjustment_get_upper(adj);
    double span = upper - lower;
    if (span <= 0.0) return;

    /* The slider travels `length - slider_size` pixels for `span - page`
     * units of value, so dragging 1:1 tracks the pointer. */
    float slider_size = (float)(length * page / span);
    float travel = length - slider_size;
    if (travel < 1.0f) travel = 1.0f;

    float delta = vertical ? (y - start_y) : (x - start_x);

    /* GtkRange fine-tuning: holding Shift while dragging moves at one tenth
     * of the pointer speed. */
    if (self->fine_tune) delta /= 10.0f;

    double value = start_value + (double)delta * (span - page) / travel;
    ptx_range_set_value(&self->range, value);
}

void ptx_scrollbar_render(PtxScrollbar *self, SDL_Renderer *renderer) {
    PtxRect trough, slider;
    PtxRect a = self->range.allocation;

    /* GtkScrollbar draws nothing when the page covers the whole range; with
     * overlay scrolling Adwaita additionally fades out when idle. ptx keeps
     * it drawn so the gutter never jumps. */
    if (!ptx_scrollbar_is_visible(self)) return;

    bool active = self->hovering || self->dragging;
    float spread = active ? THUMB_WIDTH_HOVER : THUMB_WIDTH;

    if (!ptx_range_slider_geometry(&self->range, spread, &trough, &slider))
        return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    /* Trough (transparent in Adwaita: it exists for hit-testing only). */
    SDL_SetRenderDrawColor(renderer, TROUGH_RGBA);
    SDL_FRect tr = { trough.x, trough.y, trough.w, trough.h };
    SDL_RenderFillRect(renderer, &tr);

    /* Slider: rounded, centred in the gutter, thicker while active. */
    float cx = a.x + a.w / 2.0f;
    SDL_FRect sr = { cx - slider.w / 2.0f,
                     slider.y + THUMB_MARGIN,
                     slider.w,
                     slider.h - THUMB_MARGIN * 2.0f };
    if (sr.h < 2.0f) sr.h = 2.0f;

    if (active) SDL_SetRenderDrawColor(renderer, SLIDER_RGBA_HOVER);
    else        SDL_SetRenderDrawColor(renderer, SLIDER_RGBA);

    /* Adwaita draws a fully rounded pill: radius = half the slider width. */
    ptx_sdl_fill_rounded_rect(renderer, sr, sr.w / 2.0f);
}
