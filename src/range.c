#include "range.h"

#include <stdbool.h>
#include <stddef.h>

/* GTK4's GtkRange clamps the visible range to at least one step so a slider
 * never collapses to nothing. */
#define RANGE_MIN_SLIDER 8.0f

static double range_value(const PtxRange *self) {
    return ptx_adjustment_get_value(self->adjustment);
}

static double range_upper(const PtxRange *self) {
    return ptx_adjustment_get_upper(self->adjustment);
}

static double range_lower(const PtxRange *self) {
    return ptx_adjustment_get_lower(self->adjustment);
}

static double range_page_size(const PtxRange *self) {
    return ptx_adjustment_get_page_size(self->adjustment);
}

/* Coordinate helpers: the "long" axis is y for a vertical range and x for a
 * horizontal one; the "short" axis is the opposite. */
static float axis_long_start(const PtxRange *self) {
    return ptx_orientable_get_orientation(&self->orientable)
               == PTX_ORIENTATION_VERTICAL
           ? self->allocation.y : self->allocation.x;
}

static float axis_long_length(const PtxRange *self) {
    return ptx_orientable_get_orientation(&self->orientable)
               == PTX_ORIENTATION_VERTICAL
           ? self->allocation.h : self->allocation.w;
}

static float axis_short_start(const PtxRange *self) {
    return ptx_orientable_get_orientation(&self->orientable)
               == PTX_ORIENTATION_VERTICAL
           ? self->allocation.x : self->allocation.y;
}

void ptx_range_init(PtxRange *self, PtxAdjustment *adjustment,
                    PtxOrientation orientation) {
    *self = (PtxRange){0};
    ptx_orientable_set_orientation(&self->orientable, orientation);
    self->adjustment = adjustment;
}

PtxAdjustment *ptx_range_get_adjustment(PtxRange *self) {
    return self->adjustment;
}

void ptx_range_set_adjustment(PtxRange *self, PtxAdjustment *adjustment) {
    self->adjustment = adjustment;
}

void ptx_range_set_increments(PtxRange *self, double step, double page) {
    ptx_adjustment_set_step_increment(self->adjustment, step);
    ptx_adjustment_set_page_increment(self->adjustment, page);
}

void ptx_range_set_range(PtxRange *self, double min, double max) {
    ptx_adjustment_set_lower(self->adjustment, min);
    ptx_adjustment_set_upper(self->adjustment, max);
}

void ptx_range_set_value(PtxRange *self, double value) {
    ptx_adjustment_set_value(self->adjustment, value);
}

double ptx_range_get_value(PtxRange *self) {
    return range_value(self);
}

void ptx_range_set_inverted(PtxRange *self, bool setting) {
    self->inverted = setting;
}

bool ptx_range_get_inverted(PtxRange *self) {
    return self->inverted;
}

void ptx_range_set_allocation(PtxRange *self, float x, float y,
                              float w, float h) {
    self->allocation = (PtxRect){ x, y, w, h };
}

void ptx_range_default_move_slider(PtxRange *self, PtxScrollType scroll) {
    double value = range_value(self);
    double step  = ptx_adjustment_get_step_increment(self->adjustment);
    double page  = ptx_adjustment_get_page_increment(self->adjustment);

    switch (scroll) {
        case PTX_SCROLL_NONE:
            break;
        case PTX_SCROLL_JUMP:
            /* Handled by the caller, which knows the pointer position. */
            break;
        case PTX_SCROLL_STEP_BACKWARD:
        case PTX_SCROLL_STEP_UP:
        case PTX_SCROLL_STEP_LEFT:
            ptx_range_set_value(self, value - step);
            break;
        case PTX_SCROLL_STEP_FORWARD:
        case PTX_SCROLL_STEP_DOWN:
        case PTX_SCROLL_STEP_RIGHT:
            ptx_range_set_value(self, value + step);
            break;
        case PTX_SCROLL_PAGE_BACKWARD:
        case PTX_SCROLL_PAGE_UP:
        case PTX_SCROLL_PAGE_LEFT:
            ptx_range_set_value(self, value - page);
            break;
        case PTX_SCROLL_PAGE_FORWARD:
        case PTX_SCROLL_PAGE_DOWN:
        case PTX_SCROLL_PAGE_RIGHT:
            ptx_range_set_value(self, value + page);
            break;
        case PTX_SCROLL_START:
            ptx_range_set_value(self, range_lower(self));
            break;
        case PTX_SCROLL_END:
            ptx_range_set_value(self, range_upper(self));
            break;
    }
}

bool ptx_range_change_value(PtxRange *self, PtxScrollType scroll,
                            double new_value) {
    if (self->change_value && self->change_value(self, scroll, new_value))
        return true;

    if (scroll == PTX_SCROLL_JUMP)
        ptx_range_set_value(self, new_value);
    else
        ptx_range_default_move_slider(self, scroll);

    if (self->value_changed) self->value_changed(self);
    return true;
}

void ptx_range_move_slider(PtxRange *self, PtxScrollType scroll) {
    if (self->move_slider) {
        self->move_slider(self, scroll);
        return;
    }
    ptx_range_default_move_slider(self, scroll);
    if (self->value_changed) self->value_changed(self);
}

/* Fraction (0..1) of the way along the trough at which the slider starts and
 * how much of the trough it covers. GtkRange uses
 *   (value - lower) / (upper - lower)                      for the start
 *   page_size / (upper - lower)                            for the size
 * with the bottom of the model at the top of the widget for vertical,
 * non-inverted ranges. */
static bool slider_fractions(const PtxRange *self, float *start, float *size) {
    double upper = range_upper(self);
    double lower = range_lower(self);
    double value = range_value(self);
    double page  = range_page_size(self);

    double span = upper - lower;
    if (span <= 0.0 || page <= 0.0) return false;

    double s = (value - lower) / span;
    double z = page / span;
    if (z > 1.0) z = 1.0;
    if (s < 0.0) s = 0.0;
    if (s + z > 1.0) s = 1.0 - z;

    *start = (float)s;
    *size  = (float)z;
    return true;
}

bool ptx_range_slider_geometry(PtxRange *self, float spread,
                               PtxRect *trough, PtxRect *slider) {
    float long_start  = axis_long_start(self);
    float long_length = axis_long_length(self);
    float short_start = axis_short_start(self);

    if (trough) {
        if (ptx_orientable_get_orientation(&self->orientable) == PTX_ORIENTATION_VERTICAL)
            *trough = (PtxRect){ short_start, long_start, spread, long_length };
        else
            *trough = (PtxRect){ long_start, short_start, long_length, spread };
    }

    float start = 0.0f, size = 0.0f;
    if (!slider_fractions(self, &start, &size)) return false;

    float slider_len = long_length * size;
    if (slider_len < RANGE_MIN_SLIDER) slider_len = RANGE_MIN_SLIDER;
    float slider_pos = long_start + (long_length - slider_len) * start;

    if (slider) {
        if (ptx_orientable_get_orientation(&self->orientable) == PTX_ORIENTATION_VERTICAL)
            *slider = (PtxRect){ short_start, slider_pos, spread, slider_len };
        else
            *slider = (PtxRect){ slider_pos, short_start, slider_len, spread };
    }
    return true;
}

bool ptx_range_get_slider_range(PtxRange *self, float *slider_start,
                                float *slider_end) {
    PtxRect slider;
    if (!ptx_range_slider_geometry(self, 0.0f, NULL, &slider)) return false;

    bool vertical = ptx_orientable_get_orientation(&self->orientable)
                    == PTX_ORIENTATION_VERTICAL;
    *slider_start = vertical ? slider.y : slider.x;
    *slider_end   = *slider_start + (vertical ? slider.h : slider.w);
    return true;
}

void ptx_range_jump_to(PtxRange *self, float pointer_position,
                       PtxPositionType clamp_position) {
    double upper = range_upper(self);
    double lower = range_lower(self);
    double page  = range_page_size(self);

    float length = axis_long_length(self);
    double span = upper - lower;
    if (length <= 0.0f || span <= 0.0) return;

    /* GtkRange's `gtk_range_jump()`: the clickable span is reduced by the
     * page size so the page cannot overshoot at either end, and the page is
     * then *centred* on the click rather than the slider teleporting under
     * the pointer. */
    double value;
    switch (clamp_position) {
        case PTX_POS_LEFT:
        case PTX_POS_TOP:
            value = lower + page / 2.0;
            break;
        case PTX_POS_RIGHT:
        case PTX_POS_BOTTOM:
            value = lower + page / 2.0 +
                    (double)(length - page) * span *
                        (double)(pointer_position - axis_long_start(self)) /
                        (double)length / (double)length;
            break;
        default:
            return;
    }

    double max_value = upper - page;
    if (max_value < lower) max_value = lower;
    if (value < lower) value = lower;
    if (value > max_value) value = max_value;

    ptx_range_change_value(self, PTX_SCROLL_JUMP, value);
}
