#ifndef PTX_LAYOUT_H
#define PTX_LAYOUT_H

#include "app.h"

/* Create the viewport widgets (the vertical scrollbar). Call after the
 * adjustment exists. */
void layout_init(TerminalApp *app);

/* Destroy widgets created by layout_init(). */
void layout_free(TerminalApp *app);

/* Recompute the grid size for a window of w x h pixels, resize libvterm,
 * notify the child via TIOCSWINSZ, and re-allocate the scrollbar. */
void recompute_layout(TerminalApp *app, int w, int h);

#endif /* PTX_LAYOUT_H */
