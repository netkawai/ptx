#include "layout.h"

#include <sys/ioctl.h>

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

    /* Geometry changed: stored selection coordinates no longer make sense. */
    app->has_selection = false;
    app->selecting = false;
    app->menu_visible = false;
    app->menu_hover = -1;
    app_mark_dirty(app);
}
