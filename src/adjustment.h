#ifndef PTX_ADJUSTMENT_H
#define PTX_ADJUSTMENT_H

#include <stdbool.h>

/*
 * PtxAdjustment:
 *
 * A model for a numeric value.
 *
 * The `PtxAdjustment` has an associated lower and upper bound. It also
 * contains step and page increments, and a page size.
 *
 * Adjustments are used within several ptx widgets, including
 * [class@PtxScrollbar] and [class@PtxRange].
 *
 * The `PtxAdjustment` object does not update the value itself. Instead it
 * is left up to the owner of the `PtxAdjustment` to control the value.
 *
 * Port of GtkAdjustment (GTK 4): https://docs.gtk.org/gtk4/class.Adjustment.html
 */

typedef struct PtxAdjustment      PtxAdjustment;
typedef struct PtxAdjustmentClass PtxAdjustmentClass;

/**
 * PtxAdjustmentClass:
 * @changed: the default handler for the "changed" signal.
 * @value_changed: the default handler for the "value-changed" signal.
 *
 * The class structure for `PtxAdjustment`.
 *
 * In GTK4 these are virtual methods wired up with g_signal_connect().
 * ptx has no GObject signal machinery; the owner installs its handlers with
 * ptx_adjustment_connect(), which plays the role of g_signal_connect().
 */
struct PtxAdjustmentClass {
    void (*changed)(PtxAdjustment *self, void *user_data);
    void (*value_changed)(PtxAdjustment *self, void *user_data);
};

/**
 * PtxAdjustment:
 * @lower: the minimum value.
 * @upper: the maximum value.
 * @value: the current value.
 * @step_increment: the increment to use to make small changes.
 * @page_increment: the increment to use to make major changes.
 * @page_size: the size of the area a page covers.
 *
 * The values needed to set up a visible area.
 *
 * Unlike GTK4, the struct is public so an adjustment can be embedded in
 * another object without an allocation.
 */
struct PtxAdjustment {
    PtxAdjustmentClass klass;
    void   *user_data;

    double  lower;
    double  upper;
    double  value;
    double  step_increment;
    double  page_increment;
    double  page_size;

    /* Private: previous value, used by ptx to report deltas. */
    double  old_value;
};

/**
 * ptx_adjustment_new:
 * @value: the initial value
 * @lower: the minimum value
 * @upper: the maximum value
 * @step_increment: the increment to use to make small changes
 * @page_increment: the increment to use to make major changes
 * @page_size: the size of the area a page covers
 *
 * Creates a new `PtxAdjustment`.
 *
 * Returns: (transfer full): a new `PtxAdjustment`
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_adjustment_new()
 */
PtxAdjustment *ptx_adjustment_new(double value,
                                  double lower, double upper,
                                  double step_increment,
                                  double page_increment,
                                  double page_size);

/**
 * ptx_adjustment_unref:
 * @self: a `PtxAdjustment`
 *
 * Releases an adjustment created with ptx_adjustment_new().
 *
 * ptx objects are owner-allocated; this only exists for symmetry with
 * g_object_unref() on adjustments.
 *
 * Since: 0.2
 *
 * Equivalent of: g_object_unref() on a `GtkAdjustment`
 */
void ptx_adjustment_unref(PtxAdjustment *self);

/**
 * ptx_adjustment_connect:
 * @self: a `PtxAdjustment`
 * @klass: the handlers to install
 * @user_data: data passed to every handler
 *
 * Installs the default handlers for the "changed" and "value-changed"
 * signals. Either handler in @klass may be %NULL.
 *
 * Since: 0.2
 *
 * Equivalent of: g_signal_connect() for `GtkAdjustment` signals
 */
void ptx_adjustment_connect(PtxAdjustment *self, PtxAdjustmentClass klass,
                            void *user_data);

/* Getters ------------------------------------------------------------ */

/**
 * ptx_adjustment_get_lower:
 * @self: a `PtxAdjustment`
 *
 * Retrieves the minimum value of the adjustment.
 *
 * Returns: the minimum value
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_adjustment_get_lower()
 */
double ptx_adjustment_get_lower(const PtxAdjustment *self);

/**
 * ptx_adjustment_get_upper:
 * @self: a `PtxAdjustment`
 *
 * Retrieves the maximum value of the adjustment.
 *
 * Returns: the maximum value
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_adjustment_get_upper()
 */
double ptx_adjustment_get_upper(const PtxAdjustment *self);

/**
 * ptx_adjustment_get_value:
 * @self: a `PtxAdjustment`
 *
 * Gets the current value of the adjustment.
 *
 * Returns: the current value
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_adjustment_get_value()
 */
double ptx_adjustment_get_value(const PtxAdjustment *self);

/**
 * ptx_adjustment_get_step_increment:
 * @self: a `PtxAdjustment`
 *
 * Retrieves the step increment.
 *
 * Returns: the step increment
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_adjustment_get_step_increment()
 */
double ptx_adjustment_get_step_increment(const PtxAdjustment *self);

/**
 * ptx_adjustment_get_page_increment:
 * @self: a `PtxAdjustment`
 *
 * Retrieves the page increment.
 *
 * Returns: the page increment
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_adjustment_get_page_increment()
 */
double ptx_adjustment_get_page_increment(const PtxAdjustment *self);

/**
 * ptx_adjustment_get_page_size:
 * @self: a `PtxAdjustment`
 *
 * Retrieves the page size.
 *
 * Returns: the page size
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_adjustment_get_page_size()
 */
double ptx_adjustment_get_page_size(const PtxAdjustment *self);

/**
 * ptx_adjustment_get_minimum_increment:
 * @self: a `PtxAdjustment`
 *
 * Gets the smaller of step increment and page increment.
 *
 * Returns: the minimum increment
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_adjustment_get_minimum_increment()
 */
double ptx_adjustment_get_minimum_increment(const PtxAdjustment *self);

/* Setters ------------------------------------------------------------ */

/** ptx_adjustment_set_lower: @self: a `PtxAdjustment` @lower: the new minimum value
 *
 * Sets the minimum value. No-op if @lower is unchanged.
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_adjustment_set_lower() */
void ptx_adjustment_set_lower(PtxAdjustment *self, double lower);

/** ptx_adjustment_set_upper: @self: a `PtxAdjustment` @upper: the new maximum value
 *
 * Sets the maximum value. No-op if @upper is unchanged.
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_adjustment_set_upper() */
void ptx_adjustment_set_upper(PtxAdjustment *self, double upper);

/**
 * ptx_adjustment_set_value:
 * @self: a `PtxAdjustment`
 * @value: the new value
 *
 * Sets the `PtxAdjustment` value. The value is clamped to lie between
 * `lower` and `upper` - `page_size`.
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_adjustment_set_value()
 */
void ptx_adjustment_set_value(PtxAdjustment *self, double value);

/** ptx_adjustment_set_step_increment: @self: a `PtxAdjustment` @step_increment: the new step increment
 *
 * Sets the step increment. No-op if unchanged.
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_adjustment_set_step_increment() */
void ptx_adjustment_set_step_increment(PtxAdjustment *self, double step_increment);

/** ptx_adjustment_set_page_increment: @self: a `PtxAdjustment` @page_increment: the new page increment
 *
 * Sets the page increment. No-op if unchanged.
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_adjustment_set_page_increment() */
void ptx_adjustment_set_page_increment(PtxAdjustment *self, double page_increment);

/** ptx_adjustment_set_page_size: @self: a `PtxAdjustment` @page_size: the new page size
 *
 * Sets the page size. No-op if unchanged.
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_adjustment_set_page_size() */
void ptx_adjustment_set_page_size(PtxAdjustment *self, double page_size);

/**
 * ptx_adjustment_configure:
 * @self: a `PtxAdjustment`
 * @value: the new value
 * @lower: the new minimum value
 * @upper: the new maximum value
 * @step_increment: the new step increment
 * @page_increment: the new page increment
 * @page_size: the new page size
 *
 * Sets all properties of the adjustment at once.
 *
 * Use this function to avoid multiple emissions of the "changed" signal.
 * It emits at most one "changed" and one "value-changed".
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_adjustment_configure()
 */
void ptx_adjustment_configure(PtxAdjustment *self,
                              double value,
                              double lower, double upper,
                              double step_increment,
                              double page_increment,
                              double page_size);

/**
 * ptx_adjustment_clamp_page:
 * @self: a `PtxAdjustment`
 * @lower: the lower value
 * @upper: the upper value
 *
 * Updates the value to ensure that the range from @lower to @upper is
 * within the current page.
 *
 * Since: 0.2
 *
 * Equivalent of: gtk_adjustment_clamp_page()
 */
void ptx_adjustment_clamp_page(PtxAdjustment *self,
                               double lower, double upper);

#endif /* PTX_ADJUSTMENT_H */
