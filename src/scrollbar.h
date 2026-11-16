#ifndef PTX_SCROLLBAR_H
#define PTX_SCROLLBAR_H

#include "range.h"

#include <stdbool.h>
#include <SDL3/SDL.h>

/*
 * PtxScrollbar:
 *
 * Shows a horizontal or vertical scrollbar.
 *
 * Its position and movement are controlled by the adjustment that is passed
 * to or created by ptx_scrollbar_new(). See [class@PtxAdjustment] for more
 * details. The [property@PtxAdjustment:value] field sets the position of the
 * slider and must be between [property@PtxAdjustment:lower] and
 * [property@PtxAdjustment:upper] - [property@PtxAdjustment:page-size]. The
 * [property@PtxAdjustment:page-size] represents the size of the visible
 * scrollable area.
 *
 * The fields [property@PtxAdjustment:step-increment] and
 * [property@PtxAdjustment:page-increment] fields are added to or subtracted
 * from the [property@PtxAdjustment:value] when the user asks to move by a
 * step (using e.g. the cursor arrow keys) or by a page (using e.g. the Page
 * Down/Up keys).
 *
 * Port of GtkScrollbar (GTK 4): https://docs.gtk.org/gtk4/class.Scrollbar.html
 *
 * ## CSS nodes
 *
 * ```
 * scrollbar
 * ╰── range[.fine-tune]
 *     ╰── trough
 *         ╰── slider
 * ```
 *
 * ptx has no CSS tree; the libadwaita look (translucent rounded trough, and
 * a slider that thickens on hover or while dragging) is implemented in
 * ptx_scrollbar_render().
 */

/**
 * PtxScrollbar:
 * @range: the parent `PtxRange`
 * @hovering: whether the pointer is over the scrollbar
 * @dragging: whether the slider is being dragged
 * @fine_tune: whether fine-tuning is active
 *
 * The `PtxScrollbar` struct.
 *
 * `hovering`, `dragging` and `fine_tune` are ptx extensions standing in for
 * the Adwaita CSS classes `.hovering`, `.dragging` and the `range.fine-tune`
 * node.
 *
 * Since: 0.2
 */
typedef struct PtxScrollbar PtxScrollbar;

struct PtxScrollbar {
    PtxRange range;

    bool hovering;
    bool dragging;
    bool fine_tune;
};

/**
 * ptx_scrollbar_new:
 * @orientation: the scrollbar's orientation
 * @adjustment: (nullable): the `PtxAdjustment` to use, or %NULL to create
 *   one
 *
 * Creates a new scrollbar with the given orientation.
 *
 * Returns: (transfer full): the new `PtxScrollbar`
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_scrollbar_new()
 */
PtxScrollbar *ptx_scrollbar_new(PtxOrientation orientation,
                                PtxAdjustment *adjustment);

/**
 * ptx_scrollbar_free:
 * @self: (nullable): a `PtxScrollbar`
 *
 * Releases a scrollbar created with ptx_scrollbar_new(). The adjustment is
 * not freed; it is owned by whoever created it.
 *
 * Since: 0.2
 *
 * Equivalent of: g_object_unref() on a `GtkScrollbar`
 */
void ptx_scrollbar_free(PtxScrollbar *self);

/**
 * ptx_scrollbar_get_adjustment:
 * @self: a `PtxScrollbar`
 *
 * Returns the scrollbar's adjustment.
 *
 * Returns: (transfer none): the scrollbar's adjustment
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_scrollbar_get_adjustment()
 */
PtxAdjustment *ptx_scrollbar_get_adjustment(PtxScrollbar *self);

/**
 * ptx_scrollbar_set_adjustment:
 * @self: a `PtxScrollbar`
 * @adjustment: (nullable): a `PtxAdjustment`, or %NULL to create one
 *
 * Makes the scrollbar use the given adjustment.
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_scrollbar_set_adjustment()
 */
void ptx_scrollbar_set_adjustment(PtxScrollbar *self,
                                  PtxAdjustment *adjustment);

/* ptx extensions (widget implementation) ----------------------------- */

/**
 * ptx_scrollbar_allocate:
 * @self: a `PtxScrollbar`
 * @x: x position
 * @y: y position
 * @w: width
 * @h: height
 *
 * Places the scrollbar; called from the layout pass.
 *
 * Since: 0.2
 *
 * ptx extension
 */
void ptx_scrollbar_allocate(PtxScrollbar *self, float x, float y,
                            float w, float h);

/**
 * ptx_scrollbar_is_visible:
 * @self: a `PtxScrollbar`
 *
 * Whether the slider is worth drawing (there is more content than page).
 *
 * Returns: %TRUE if the scrollbar has an adjustable range
 *
 * Since: 0.2
 *
 * ptx extension: GTK hides the trough via CSS (overlay scrolling).
 */
bool ptx_scrollbar_is_visible(PtxScrollbar *self);

/**
 * ptx_scrollbar_is_active:
 * @self: a `PtxScrollbar`
 *
 * Whether the bar is hovered or being dragged (Adwaita's `.hovering` /
 * `.dragging` classes).
 *
 * Returns: %TRUE if the bar is active
 *
 * Since: 0.2
 *
 * ptx extension
 */
bool ptx_scrollbar_is_active(PtxScrollbar *self);

/**
 * ptx_scrollbar_get_slider:
 * @self: a `PtxScrollbar`
 * @slider: (out): return location for the slider rectangle
 *
 * Gets the slider rectangle in viewport pixels, so the application can
 * hit-test exactly what is drawn.
 *
 * Returns: %TRUE if there is a slider
 *
 * Since: 0.2
 *
 * ptx extension
 */
bool ptx_scrollbar_get_slider(PtxScrollbar *self, PtxRect *slider);

/**
 * ptx_scrollbar_hit:
 * @self: a `PtxScrollbar`
 * @x: x in viewport coordinates
 * @y: y in viewport coordinates
 *
 * Returns: %TRUE if (@x, @y) is inside the trough
 *
 * Since: 0.2
 *
 * ptx extension
 */
bool ptx_scrollbar_hit(PtxScrollbar *self, float x, float y);

/**
 * ptx_scrollbar_hit_slider:
 * @self: a `PtxScrollbar`
 * @x: x in viewport coordinates
 * @y: y in viewport coordinates
 *
 * Returns: %TRUE if (@x, @y) is over the slider (GtkRange starts a secondary
 *   drag there)
 *
 * Since: 0.2
 *
 * ptx extension
 */
bool ptx_scrollbar_hit_slider(PtxScrollbar *self, float x, float y);

/**
 * ptx_scrollbar_click_trough:
 * @self: a `PtxScrollbar`
 * @x: x in viewport coordinates
 * @y: y in viewport coordinates
 *
 * Handles a click on the trough: jumps one page towards the click.
 *
 * Since: 0.2
 *
 * ptx extension
 */
void ptx_scrollbar_click_trough(PtxScrollbar *self, float x, float y);

/**
 * ptx_scrollbar_drag:
 * @self: a `PtxScrollbar`
 * @start_x: where the drag began, x
 * @start_y: where the drag began, y
 * @x: current pointer x
 * @y: current pointer y
 * @start_value: the adjustment value when the drag began
 *
 * Converts a pointer drag into a value, honouring fine-tuning.
 *
 * Since: 0.2
 *
 * ptx extension: GtkRange handles this in its event handlers.
 */
void ptx_scrollbar_drag(PtxScrollbar *self, float start_x, float start_y,
                        float x, float y, double start_value);

/**
 * ptx_scrollbar_render:
 * @self: a `PtxScrollbar`
 * @renderer: the SDL renderer to draw with
 *
 * Draws the trough and the slider.
 *
 * Since: 0.2
 *
 * ptx extension: in GTK this is the snapshot vfunc.
 */
void ptx_scrollbar_render(PtxScrollbar *self, SDL_Renderer *renderer);

#endif /* PTX_SCROLLBAR_H */
