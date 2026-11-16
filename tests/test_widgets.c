#include "adwaita.h"
#include "../src/adjustment.h"

/* Keep assertions active even in Release builds: they are the test. */
#undef NDEBUG
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void test_adjustment_bounds(void) {
    PtxAdjustment *a = ptx_adjustment_new(0, 0, 100, 1, 10, 10);
    /* value must stay in [lower, upper - page_size] */
    ptx_adjustment_set_value(a, 95);
    assert(ptx_adjustment_get_value(a) == 90);
    ptx_adjustment_set_value(a, -5);
    assert(ptx_adjustment_get_value(a) == 0);
    ptx_adjustment_set_value(a, 90);
    assert(ptx_adjustment_get_value(a) == 90);
    ptx_adjustment_set_page_size(a, 40);
    /* 90 > 100 - 40: the page grew, so the value is pulled back in. */
    assert(ptx_adjustment_get_value(a) == 60);
    assert(ptx_adjustment_get_minimum_increment(a) == 1);
    ptx_adjustment_unref(a);
    puts("ok: adjustment bounds/clamp");
}

static int changed_count;
static void on_changed(PtxAdjustment *a, void *u) { (void)a; (void)u; changed_count++; }

static void test_configure_single_signal(void) {
    PtxAdjustment *a = ptx_adjustment_new(0, 0, 10, 1, 1, 1);
    ptx_adjustment_connect(a, (PtxAdjustmentClass){ .changed = on_changed }, NULL);
    changed_count = 0;
    ptx_adjustment_configure(a, 5, 0, 20, 2, 2, 2);
    assert(changed_count == 1);
    ptx_adjustment_configure(a, 5, 0, 20, 2, 2, 2);
    assert(changed_count == 1);   /* no-op configure: no signal */
    ptx_adjustment_unref(a);
    puts("ok: configure emits at most one changed");
}

static void test_page_click_centres(void) {
    PtxAdjustment *a = ptx_adjustment_new(0, 0, 120, 1, 10, 20);
    PtxScrollbar *sb = ptx_scrollbar_new(PTX_ORIENTATION_VERTICAL, a);
    ptx_scrollbar_allocate(sb, 0, 0, 12, 120);

    /* Click at the very bottom of the trough: page pinned to the end. */
    ptx_range_jump_to(&sb->range, 120.0f, PTX_POS_BOTTOM);
    assert(ptx_adjustment_get_value(a) == 100);
    ptx_range_jump_to(&sb->range, 0.0f, PTX_POS_TOP);
    assert(ptx_adjustment_get_value(a) == 10);   /* page/2, like GtkRange */
    ptx_scrollbar_free(sb);
    ptx_adjustment_unref(a);
    puts("ok: trough click centres the page");
}

static void test_slider_geometry(void) {
    PtxAdjustment *a = ptx_adjustment_new(0, 0, 120, 1, 20, 20);
    PtxScrollbar *sb = ptx_scrollbar_new(PTX_ORIENTATION_VERTICAL, a);
    ptx_scrollbar_allocate(sb, 100, 0, 12, 120);

    PtxRect trough, slider;
    assert(ptx_range_slider_geometry(&sb->range, 6.0f, &trough, &slider));
    assert(trough.x == 100 && trough.w == 6 && trough.h == 120);    /* page is 1/6 of the range -> slider is 1/6 of a 120px trough */
    assert(fabsf(slider.h - 20.0f) < 0.01f);
    assert(fabsf(slider.y) < 0.01f);             /* at the top = value 0 */

    /* Hidden when everything fits. */
    ptx_adjustment_set_page_size(a, 120);
    assert(!ptx_scrollbar_is_visible(sb));
    ptx_scrollbar_free(sb);
    ptx_adjustment_unref(a);
    puts("ok: slider geometry + visibility");
}

static void test_drag_tracks_pointer(void) {
    PtxAdjustment *a = ptx_adjustment_new(0, 0, 120, 1, 20, 20);
    PtxScrollbar *sb = ptx_scrollbar_new(PTX_ORIENTATION_VERTICAL, a);
    ptx_scrollbar_allocate(sb, 0, 0, 12, 120);

    /* travel = 100px for span-page = 100 units -> 1:1 */
    ptx_scrollbar_drag(sb, 0, 10, 0, 60, 0);
    assert(fabs(ptx_range_get_value(&sb->range) - 50.0) < 0.01);

    /* fine-tuning moves at 1/10 speed */
    sb->fine_tune = true;
    ptx_scrollbar_drag(sb, 0, 10, 0, 60, 0);
    assert(fabs(ptx_range_get_value(&sb->range) - 5.0) < 0.01);
    ptx_scrollbar_free(sb);
    ptx_adjustment_unref(a);
    puts("ok: drag tracks pointer, fine-tune is 1/10");
}

int main(void) {
    test_adjustment_bounds();
    test_configure_single_signal();
    test_page_click_centres();
    test_slider_geometry();
    test_drag_tracks_pointer();
    puts("all widget tests passed");
    return 0;
}
