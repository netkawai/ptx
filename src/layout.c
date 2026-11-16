#include "layout.h"
#include "buffer.h"
#include "scrollbar.h"

#include <sys/ioctl.h>

void layout_init(TerminalApp *app) {
    app->vscrollbar = ptx_scrollbar_new(PTX_ORIENTATION_VERTICAL,
                                        app->vadjustment);
    if (app->vscrollbar)
        ptx_range_set_increments(&app->vscrollbar->range, 1.0, 1.0);
}

void layout_free(TerminalApp *app) {
    ptx_scrollbar_free(app->vscrollbar);
    app->vscrollbar = NULL;
}

void recompute_layout(TerminalApp *app, int w, int h) {
    app->win_w = w;
    app->win_h = h;

    int usable = w - SCROLLBAR_W;
    if (usable < 1) usable = 1;

    int cols = app->glyph_w > 0 ? usable / app->glyph_w : 80;
    int rows = app->glyph_h > 0 ? h / app->glyph_h : 24;
    if (cols < 20) cols = 20;
    if (rows < 5)  rows = 5;

    app->cols = cols;
    app->rows = rows;

    SDL_LockMutex(app->vterm_lock);
    vterm_set_size(app->vterm, rows, cols);
    SDL_UnlockMutex(app->vterm_lock);

    struct winsize ws = {
        .ws_row = (unsigned short)rows,
        .ws_col = (unsigned short)cols,
        .ws_xpixel = (unsigned short)w,
        .ws_ypixel = (unsigned short)h,
    };
    ioctl(app->pty_master, TIOCSWINSZ, &ws);

    /* The scrollbar lives in the right gutter; `page_size` is the number of
     * rows on screen, so the slider covers exactly the visible portion. */
    buffer_set_viewport(app, rows);
    if (app->vscrollbar) {
        ptx_range_set_increments(&app->vscrollbar->range, 1.0,
                                 rows > 1 ? (double)(rows - 1) : 1.0);
        ptx_scrollbar_allocate(app->vscrollbar,
                               (float)(w - SCROLLBAR_W), 0.0f,
                               (float)SCROLLBAR_W, (float)h);
    }

    /* Geometry changed: stored selection coordinates no longer make sense. */
    app->has_selection = false;
    app->selecting = false;
    app->menu_visible = false;
    app->menu_hover = -1;
    app_mark_dirty(app);
}
