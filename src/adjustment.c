#include "adjustment.h"

#include <stdlib.h>

/* The page never extends past `upper`, so the highest reachable value is
 * `upper - page_size` (GTK4 calls this "page-related correction"). */
static double clamp_value(const PtxAdjustment *self, double value) {
    double max_page = self->upper - self->lower - self->page_size;
    if (max_page < 0.0) max_page = 0.0;
    if (value < self->lower) return self->lower;
    if (value > self->lower + max_page) return self->lower + max_page;
    return value;
}

static void emit_changed(PtxAdjustment *self) {
    if (self->klass.changed) self->klass.changed(self, self->user_data);
}

static void emit_value_changed(PtxAdjustment *self) {
    if (self->klass.value_changed)
        self->klass.value_changed(self, self->user_data);
}

PtxAdjustment *ptx_adjustment_new(double value,
                                  double lower, double upper,
                                  double step_increment,
                                  double page_increment,
                                  double page_size) {
    PtxAdjustment *self = calloc(1, sizeof(*self));
    if (!self) return NULL;

    self->lower          = lower;
    self->upper          = upper;
    self->value          = lower;
    self->step_increment = step_increment;
    self->page_increment = page_increment;
    self->page_size      = page_size;
    self->old_value      = lower;

    ptx_adjustment_set_value(self, value);
    return self;
}

void ptx_adjustment_unref(PtxAdjustment *self) {
    free(self);
}

void ptx_adjustment_connect(PtxAdjustment *self, PtxAdjustmentClass klass,
                            void *user_data) {
    self->klass     = klass;
    self->user_data = user_data;
}

/* ------------------------------------------------------------------ */
/* Getters                                                             */
/* ------------------------------------------------------------------ */

double ptx_adjustment_get_lower(const PtxAdjustment *self) {
    return self->lower;
}

double ptx_adjustment_get_upper(const PtxAdjustment *self) {
    return self->upper;
}

double ptx_adjustment_get_value(const PtxAdjustment *self) {
    return self->value;
}

double ptx_adjustment_get_step_increment(const PtxAdjustment *self) {
    return self->step_increment;
}

double ptx_adjustment_get_page_increment(const PtxAdjustment *self) {
    return self->page_increment;
}

double ptx_adjustment_get_page_size(const PtxAdjustment *self) {
    return self->page_size;
}

double ptx_adjustment_get_minimum_increment(const PtxAdjustment *self) {
    return self->step_increment < self->page_increment
         ? self->step_increment : self->page_increment;
}

/* ------------------------------------------------------------------ */
/* Setters                                                             */
/* ------------------------------------------------------------------ */

void ptx_adjustment_set_lower(PtxAdjustment *self, double lower) {
    if (self->lower == lower) return;
    self->lower = lower;
    ptx_adjustment_set_value(self, self->value);
    emit_changed(self);
}

void ptx_adjustment_set_upper(PtxAdjustment *self, double upper) {
    if (self->upper == upper) return;
    self->upper = upper;
    ptx_adjustment_set_value(self, self->value);
    emit_changed(self);
}

void ptx_adjustment_set_value(PtxAdjustment *self, double value) {
    value = clamp_value(self, value);
    if (value == self->value) return;

    self->old_value = self->value;
    self->value = value;
    emit_value_changed(self);
}

void ptx_adjustment_set_step_increment(PtxAdjustment *self, double step_increment) {
    if (self->step_increment == step_increment) return;
    self->step_increment = step_increment;
    emit_changed(self);
}

void ptx_adjustment_set_page_increment(PtxAdjustment *self, double page_increment) {
    if (self->page_increment == page_increment) return;
    self->page_increment = page_increment;
    emit_changed(self);
}

void ptx_adjustment_set_page_size(PtxAdjustment *self, double page_size) {
    if (self->page_size == page_size) return;
    self->page_size = page_size;
    /* Re-apply the current value so it is clamped against the new page,
     * whatever order the properties were set in. */
    self->old_value = self->value;
    self->value = clamp_value(self, self->value);
    if (self->value != self->old_value) emit_value_changed(self);
    emit_changed(self);
}

void ptx_adjustment_configure(PtxAdjustment *self,
                              double value,
                              double lower, double upper,
                              double step_increment,
                              double page_increment,
                              double page_size) {
    bool changed = false;

    if (self->lower != lower ||
        self->upper != upper ||
        self->step_increment != step_increment ||
        self->page_increment != page_increment ||
        self->page_size != page_size) {
        self->lower          = lower;
        self->upper          = upper;
        self->step_increment = step_increment;
        self->page_increment = page_increment;
        self->page_size      = page_size;
        changed = true;
    }

    value = clamp_value(self, value);
    if (value != self->value) {
        self->old_value = self->value;
        self->value = value;
        emit_value_changed(self);
    }
    if (changed) emit_changed(self);
}

void ptx_adjustment_clamp_page(PtxAdjustment *self,
                               double lower, double upper) {
    bool need_value = false;
    double value = self->value;

    if (lower < value) {
        value = lower;
        need_value = true;
    }
    if (upper > value + self->page_size) {
        value = upper - self->page_size;
        need_value = true;
    }
    if (need_value) ptx_adjustment_set_value(self, value);
}
