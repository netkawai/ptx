#ifndef PTX_RANGE_H
#define PTX_RANGE_H

#include "adjustment.h"
#include "orientable.h"

#include <stdbool.h>

/*
 * PtxRange:
 *
 * Base class for widgets which visualize an adjustment.
 *
 * Widgets that are derived from `PtxRange` include [class@PtxScrollbar].
 *
 * Port of GtkRange (GTK 4): https://docs.gtk.org/gtk4/class.Range.html
 *
 * `PtxRange` owns the whole interaction model: scrolling by a
 * [enum@PtxScrollType] (ptx_range_move_slider()), slider travel that
 * respects `page_size`, fine-tuning (Shift while dragging) and the
 * `change-value` / `move-slider` virtual methods. A subclass only draws and
 * hit-tests.
 *
 * Differences from GTK4:
 * - GtkRange is a #GtkWidget with a #GdkWindow. ptx ranges are layout-less
 *   and live inside the terminal viewport; `allocation` replaces the widget
 *   allocation.
 * - There is no fill-level support (ptx has nothing that prebuffers).
 * - The `change_value` handler keeps GTK's signature and return value so it
 *   can be used as a virtual method.
 */

/**
 * PtxRange:
 * @orientable: the orientable interface
 * @adjustment: the adjustment which is the "model" object for the range
 * @allocation: the area the range occupies, in viewport pixels
 * @inverted: whether the range is inverted
 *
 * The `PtxRange` class structure.
 *
 * The vtable members stand in for the GtkRange virtual methods:
 * `change_value`, `move_slider`, `value_changed` and `adjust_bounds`.
 *
 * Since: 0.2
 */
typedef struct PtxRange PtxRange;

/**
 * PtxScrollType:
 * @PTX_SCROLL_NONE: no scrolling
 * @PTX_SCROLL_JUMP: jump to region in which the event occurred
 * @PTX_SCROLL_STEP_BACKWARD: step backward
 * @PTX_SCROLL_STEP_FORWARD: step forward
 * @PTX_SCROLL_PAGE_BACKWARD: page backward
 * @PTX_SCROLL_PAGE_FORWARD: page forward
 * @PTX_SCROLL_STEP_UP: step up
 * @PTX_SCROLL_STEP_DOWN: step down
 * @PTX_SCROLL_PAGE_UP: page up
 * @PTX_SCROLL_PAGE_DOWN: page down
 * @PTX_SCROLL_STEP_LEFT: step to the left
 * @PTX_SCROLL_STEP_RIGHT: step to the right
 * @PTX_SCROLL_PAGE_LEFT: page to the left
 * @PTX_SCROLL_PAGE_RIGHT: page to the right
 * @PTX_SCROLL_START: scroll to start
 * @PTX_SCROLL_END: scroll to end
 *
 * Scrolling types.
 *
 * Since: 0.2
 *
 * Equivalent of: GtkScrollType
 */
typedef enum {
    PTX_SCROLL_NONE,
    PTX_SCROLL_JUMP,
    PTX_SCROLL_STEP_BACKWARD,
    PTX_SCROLL_STEP_FORWARD,
    PTX_SCROLL_PAGE_BACKWARD,
    PTX_SCROLL_PAGE_FORWARD,
    PTX_SCROLL_STEP_UP,
    PTX_SCROLL_STEP_DOWN,
    PTX_SCROLL_PAGE_UP,
    PTX_SCROLL_PAGE_DOWN,
    PTX_SCROLL_STEP_LEFT,
    PTX_SCROLL_STEP_RIGHT,
    PTX_SCROLL_PAGE_LEFT,
    PTX_SCROLL_PAGE_RIGHT,
    PTX_SCROLL_START,
    PTX_SCROLL_END
} PtxScrollType;

/**
 * PtxRect:
 * @x: x coordinate of the top-left corner
 * @y: y coordinate of the top-left corner
 * @w: width
 * @h: height
 *
 * A rectangle in viewport pixels.
 *
 * Since: 0.2
 *
 * ptx extension: GDK's #GdkRectangle is integer-based; ptx uses floats
 * because the sliders are half-pixel positioned.
 */
typedef struct {
    float x, y, w, h;
} PtxRect;

struct PtxRange {
    PtxOrientable orientable;
    PtxAdjustment *adjustment;
    PtxRect        allocation;
    bool           inverted;

    /* gtk_range_change_value(): return %TRUE if the scroll was handled. */
    bool (*change_value)(PtxRange *self, PtxScrollType scroll, double new_value);
    /* gtk_range_move_slider(): defaults to ptx_range_default_move_slider(). */
    void (*move_slider)(PtxRange *self, PtxScrollType scroll);
    /* gtk_range_value_changed(): emitted after `value` is applied. */
    void (*value_changed)(PtxRange *self);
    /* gtk_range_adjust_bounds(): given the new value, may return another
     * one (e.g. snapping). */
    double (*adjust_bounds)(PtxRange *self, double new_value);
};

/* GtkRange API ------------------------------------------------------- */

/**
 * ptx_range_init:
 * @self: an uninitialized `PtxRange`
 * @adjustment: (nullable): the adjustment to control
 * @orientation: the range's orientation
 *
 * Initializes an embedded range.
 *
 * Since: 0.2
 *
 * Equivalent of: the `GtkRange` instance initialization
 */
void ptx_range_init(PtxRange *self, PtxAdjustment *adjustment,
                    PtxOrientation orientation);

/**
 * ptx_range_get_adjustment:
 * @self: a `PtxRange`
 *
 * Get the adjustment which is the "model" object for `PtxRange`.
 *
 * Returns: (transfer none) (nullable): a `PtxAdjustment`
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_range_get_adjustment()
 */
PtxAdjustment *ptx_range_get_adjustment(PtxRange *self);

/**
 * ptx_range_set_adjustment:
 * @self: a `PtxRange`
 * @adjustment: a `PtxAdjustment`
 *
 * Sets the adjustment to be used as the "model" object for the `PtxRange`.
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_range_set_adjustment()
 */
void ptx_range_set_adjustment(PtxRange *self, PtxAdjustment *adjustment);

/**
 * ptx_range_set_increments:
 * @self: a `PtxRange`
 * @step: step size
 * @page: page size
 *
 * Sets the step and page sizes for the range.
 *
 * The step size is used when the user clicks the arrows or scrolls one line
 * with the mouse wheel. The page size is used for example when the user
 * presses Page Up or Page Down.
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_range_set_increments()
 */
void ptx_range_set_increments(PtxRange *self, double step, double page);

/**
 * ptx_range_set_range:
 * @self: a `PtxRange`
 * @min: minimum possible value
 * @max: maximum possible value
 *
 * Sets the allowable values in the `PtxRange`.
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_range_set_range()
 */
void ptx_range_set_range(PtxRange *self, double min, double max);

/**
 * ptx_range_set_value:
 * @self: a `PtxRange`
 * @value: new value of the range
 *
 * Sets the current value of the range.
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_range_set_value()
 */
void ptx_range_set_value(PtxRange *self, double value);

/**
 * ptx_range_get_value:
 * @self: a `PtxRange`
 *
 * Gets the current value of the range.
 *
 * Returns: current value of the range
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_range_get_value()
 */
double ptx_range_get_value(PtxRange *self);

/**
 * ptx_range_set_inverted:
 * @self: a `PtxRange`
 * @setting: %TRUE to invert the range
 *
 * Sets whether to invert the range direction.
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_range_set_inverted()
 */
void ptx_range_set_inverted(PtxRange *self, bool setting);

/**
 * ptx_range_get_inverted:
 * @self: a `PtxRange`
 *
 * Gets whether the range is inverted.
 *
 * Returns: %TRUE if the range is inverted
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_range_get_inverted()
 */
bool ptx_range_get_inverted(PtxRange *self);

/**
 * ptx_range_move_slider:
 * @self: a `PtxRange`
 * @scroll: a `PtxScrollType`
 *
 * Move the slider in response to keys, the mouse wheel or clicks.
 *
 * Since: 0.2
 *
 * Equivalent of: the "move-slider" action (GtkRange::move-slider vfunc)
 */
void ptx_range_move_slider(PtxRange *self, PtxScrollType scroll);

/**
 * ptx_range_default_move_slider:
 * @self: a `PtxRange`
 * @scroll: a `PtxScrollType`
 *
 * Default `move-slider` implementation: page/step arithmetic on the
 * adjustment, which is what GtkRange does for scrollbars.
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_range_real_move_slider()
 */
void ptx_range_default_move_slider(PtxRange *self, PtxScrollType scroll);

/**
 * ptx_range_change_value:
 * @self: a `PtxRange`
 * @scroll: a `PtxScrollType`
 * @new_value: the new value
 *
 * Tries the subclass `change_value` handler, then the default behaviour.
 *
 * Returns: %TRUE if the scroll was handled
 *
 * Since: 0.2
 *
 * Equivalent of: the "change-value" signal
 */
bool ptx_range_change_value(PtxRange *self, PtxScrollType scroll,
                            double new_value);

/* ptx extensions ----------------------------------------------------- */

/**
 * ptx_range_set_allocation:
 * @self: a `PtxRange`
 * @x: x position in viewport coordinates
 * @y: y position in viewport coordinates
 * @w: width in pixels
 * @h: height in pixels
 *
 * Sets the area the range occupies; recomputes the slider geometry.
 *
 * Since: 0.2
 *
 * ptx extension: GtkRange gets this from the layout manager.
 */
void ptx_range_set_allocation(PtxRange *self, float x, float y,
                              float w, float h);

/**
 * ptx_range_get_slider_range:
 * @self: a `PtxRange`
 * @slider_start: (out) (optional): return location for the slider's
 *   start, on the range's long axis
 * @slider_end: (out) (optional): return location for the slider's end,
 *   on the range's long axis
 *
 * Gets the slider's start and end along the range's long axis, in pixels.
 *
 * Returns: %FALSE when there is nothing to draw (no page, no upper bound)
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_range_get_slider_range()
 */
bool ptx_range_get_slider_range(PtxRange *self, float *slider_start,
                                float *slider_end);

/**
 * ptx_range_slider_geometry:
 * @self: a `PtxRange`
 * @spread: the width across the short axis
 * @trough: (out) (optional): return location for the trough rectangle
 * @slider: (out) (optional): return location for the slider rectangle
 *
 * Computes the trough and slider rectangles in viewport pixels, for drawing
 * and hit-testing.
 *
 * Returns: %TRUE if a slider exists to draw
 *
 * Since: 0.2
 *
 * ptx extension: GTK keeps this in the theme and GtkRange private API.
 */
bool ptx_range_slider_geometry(PtxRange *self, float spread,
                               PtxRect *trough, PtxRect *slider);

/**
 * ptx_range_jump_to:
 * @self: a `PtxRange`
 * @pointer_position: the pointer position on the long axis
 * @clamp_position: which end of the trough to clamp at
 *
 * Maps a click on the trough to a value, the way GtkRange does: the page is
 * centred on @pointer_position and clamped at @clamp_position.
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_range_jump()
 */
void ptx_range_jump_to(PtxRange *self, float pointer_position,
                       PtxPositionType clamp_position);

#endif /* PTX_RANGE_H */
