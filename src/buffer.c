#include "buffer.h"
#include "adwaita.h"
#include "cell.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Adjustment owner                                                    */
/* ------------------------------------------------------------------ */

/*
 * The viewport is modelled the way a GtkScrolledWindow models one:
 * `vadjustment` is the model, the scrollbar is its view. `value` counts
 * lines scrolled up from the live screen, so lower = 0, upper = visible
 * lines, page_size = screen rows.
 */
static void viewport_value_changed(PtxAdjustment *adj, void *user_data) {
    TerminalApp *app = user_data;
    (void)adj;

    /* Keep the selection on the same text while the view moves (as GTK
     * text views do), so copying still yields what the user highlighted. */
    if (app->has_selection) {
        double delta = ptx_adjustment_get_value(adj) - adj->old_value;
        if (delta != 0.0) {
            app->sel_start_row -= (int)delta;
            app->sel_end_row   -= (int)delta;
        }
    }
    app_mark_dirty(app);
}

void buffer_init(TerminalApp *app) {
    app->vadjustment = ptx_adjustment_new(0.0, 0.0, 0.0, 1.0, 1.0, 0.0);
    PtxAdjustmentClass klass = { .changed = NULL,
                                 .value_changed = viewport_value_changed };
    ptx_adjustment_connect(app->vadjustment, klass, app);
}

void buffer_set_viewport(TerminalApp *app, int rows) {
    int lines = app->sb.len + rows;

    /* `value` keeps counting lines above the bottom, so it stays valid
     * across the resize; the upper bound is what moves. */
    ptx_adjustment_configure(app->vadjustment,
                             ptx_adjustment_get_value(app->vadjustment),
                             0.0, (double)lines,
                             1.0, rows > 1 ? (double)(rows - 1) : 1.0,
                             (double)rows);
    ptx_adjustment_clamp_page(app->vadjustment, 0.0, (double)rows);
}

/* ------------------------------------------------------------------ */
/* Scrollback ring                                                     */
/* ------------------------------------------------------------------ */

static void sb_trim(TerminalApp *app) {
    Scrollback *sb = &app->sb;
    while (sb->len > MAX_SCROLLBACK) {
        free(sb->lines[0].cells);
        memmove(&sb->lines[0], &sb->lines[1],
                (size_t)(sb->len - 1) * sizeof(SbLine));
        sb->len--;
        /* The dropped line is the oldest one, i.e. the top of the range:
         * it disappears from under a scrolled-up viewport, so keep the same
         * text on screen by dropping the offset to match. */
        viewport_offset_clamp(app);
    }
}

void viewport_offset_clamp(TerminalApp *app) {
    if (!app->vadjustment) return;
    /* Reconfiguring re-clamps `value` against the new upper bound. */
    ptx_adjustment_configure(app->vadjustment,
                             ptx_adjustment_get_value(app->vadjustment),
                             0.0, (double)(app->sb.len + app->rows),
                             ptx_adjustment_get_step_increment(app->vadjustment),
                             ptx_adjustment_get_page_increment(app->vadjustment),
                             ptx_adjustment_get_page_size(app->vadjustment));
}

void buffer_push_line(TerminalApp *app, int cols, const VTermScreenCell *cells) {
    Scrollback *sb = &app->sb;

    if (sb->len == sb->cap) {
        sb->cap = sb->cap ? sb->cap * 2 : 256;
        sb->lines = realloc(sb->lines, (size_t)sb->cap * sizeof(SbLine));
    }

    SbLine *line = &sb->lines[sb->len++];
    line->cols = cols;
    line->cells = malloc((size_t)cols * sizeof(SbCell));
    for (int i = 0; i < cols; i++)
        cell_from_screen(app, &line->cells[i], &cells[i]);

    sb_trim(app);

    /* A line pushed off the top extends the reachable distance from the
     * bottom by one, which keeps the visible text anchored. */
    viewport_offset_clamp(app);
}

bool buffer_pop_line(TerminalApp *app, int cols, VTermScreenCell *cells) {
    Scrollback *sb = &app->sb;
    if (sb->len == 0) return false;

    SbLine *line = &sb->lines[--sb->len];
    memset(cells, 0, (size_t)cols * sizeof(VTermScreenCell));
    for (int i = 0; i < cols && i < line->cols; i++) {
        SbCell *s = &line->cells[i];
        memcpy(cells[i].chars, s->chars, sizeof(cells[i].chars));
        cells[i].width = s->width ? s->width : 1;
        if (s->flags & F_FG) vterm_color_rgb(&cells[i].fg, s->fg_r, s->fg_g, s->fg_b);
        if (s->flags & F_BG) vterm_color_rgb(&cells[i].bg, s->bg_r, s->bg_g, s->bg_b);
        if (s->flags & F_BOLD)      cells[i].attrs.bold = 1;
        if (s->flags & F_UNDERLINE) cells[i].attrs.underline = 1;
        if (s->flags & F_REVERSE)   cells[i].attrs.reverse = 1;
    }
    free(line->cells);
    viewport_offset_clamp(app);
    return true;
}

void buffer_clear(TerminalApp *app) {
    Scrollback *sb = &app->sb;
    for (int i = 0; i < sb->len; i++) free(sb->lines[i].cells);
    sb->len = 0;
    ptx_adjustment_set_value(app->vadjustment, 0.0);
    viewport_offset_clamp(app);
}

void buffer_free(TerminalApp *app) {
    buffer_clear(app);
    free(app->sb.lines);
    app->sb.lines = NULL;
    app->sb.cap = 0;
    ptx_adjustment_unref(app->vadjustment);
    app->vadjustment = NULL;
}

/* ------------------------------------------------------------------ */
/* Viewport                                                            */
/* ------------------------------------------------------------------ */

int viewport_offset(TerminalApp *app) {
    return (int)ptx_adjustment_get_value(app->vadjustment);
}

void scroll_set(TerminalApp *app, int offset) {
    /* Offset counts lines up from the bottom; the adjustment clamps it to
     * [0, history]. */
    ptx_adjustment_set_value(app->vadjustment, (double)offset);
}

void scroll_by(TerminalApp *app, int delta) {
    scroll_set(app, viewport_offset(app) + delta);
}

void scroll_to_end(TerminalApp *app) {
    scroll_set(app, 0);
}

void scroll_to_start(TerminalApp *app) {
    /* The oldest reachable line: the top edge of the last page. */
    double max_value = ptx_adjustment_get_upper(app->vadjustment) -
                       ptx_adjustment_get_page_size(app->vadjustment);
    if (max_value < 0.0) max_value = 0.0;
    ptx_adjustment_set_value(app->vadjustment, max_value);
}

int row_width(TerminalApp *app, int index) {
    if (index < app->sb.len) return app->sb.lines[index].cols;
    return app->cols;
}

bool combined_get_cell(TerminalApp *app, int index, int col, SbCell *out) {
    memset(out, 0, sizeof(*out));
    if (index < 0 || index >= app->sb.len + app->rows) return false;

    if (index < app->sb.len) {
        SbLine *ln = &app->sb.lines[index];
        if (col < ln->cols) *out = ln->cells[col];
        return true;
    }

    VTermPos pos = { .row = index - app->sb.len, .col = col };
    VTermScreenCell vc;
    memset(&vc, 0, sizeof(vc));
    vterm_screen_get_cell(app->vts, pos, &vc);
    cell_from_screen(app, out, &vc);
    return true;
}
