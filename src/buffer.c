#include "buffer.h"
#include "cell.h"

#include <stdlib.h>
#include <string.h>

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
        if (app->scroll_offset > 0) app->scroll_offset--;
    }
    if (app->scroll_offset > sb->len) app->scroll_offset = sb->len;
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

    /* Keep the visible content anchored while scrolled up. */
    if (app->scroll_offset > 0) app->scroll_offset++;
    sb_trim(app);
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
    if (app->scroll_offset > 0) app->scroll_offset--;
    if (app->scroll_offset > sb->len) app->scroll_offset = sb->len;
    return true;
}

void buffer_clear(TerminalApp *app) {
    Scrollback *sb = &app->sb;
    for (int i = 0; i < sb->len; i++) free(sb->lines[i].cells);
    sb->len = 0;
    app->scroll_offset = 0;
}

void buffer_free(TerminalApp *app) {
    buffer_clear(app);
    free(app->sb.lines);
    app->sb.lines = NULL;
    app->sb.cap = 0;
}

/* ------------------------------------------------------------------ */
/* Viewport                                                            */
/* ------------------------------------------------------------------ */

void scroll_set(TerminalApp *app, int offset) {
    if (offset < 0) offset = 0;
    if (offset > app->sb.len) offset = app->sb.len;
    if (offset != app->scroll_offset) {
        app->scroll_offset = offset;
        app_mark_dirty(app);
    }
}

void scroll_by(TerminalApp *app, int delta) {
    scroll_set(app, app->scroll_offset + delta);
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
