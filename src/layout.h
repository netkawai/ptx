#ifndef PTX_LAYOUT_H
#define PTX_LAYOUT_H

#include "app.h"

/* Recompute the grid size for a window of w x h pixels, resize libvterm and
 * notify the child via TIOCSWINSZ. */
void recompute_layout(TerminalApp *app, int w, int h);

#endif /* PTX_LAYOUT_H */
