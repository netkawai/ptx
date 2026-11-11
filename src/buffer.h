#ifndef PTX_BUFFER_H
#define PTX_BUFFER_H

#include "app.h"

/* ---- Scrollback ring --------------------------------------------- */

/* Store one scrolled-off line (called from the libvterm sb_pushline hook). */
void buffer_push_line(TerminalApp *app, int cols, const VTermScreenCell *cells);

/* Pop the newest stored line back into `cells` (sb_popline hook). Returns true
 * if a line was available. */
bool buffer_pop_line(TerminalApp *app, int cols, VTermScreenCell *cells);

/* Drop all stored lines (sb_clear hook). */
void buffer_clear(TerminalApp *app);

/* Release all memory owned by the scrollback. */
void buffer_free(TerminalApp *app);

/* ---- Viewport over scrollback + live screen ---------------------- */

/* Set the scroll offset (lines up from the bottom), clamped to the history. */
void scroll_set(TerminalApp *app, int offset);

/* Adjust the scroll offset by delta. */
void scroll_by(TerminalApp *app, int delta);

/* Number of columns a given combined-buffer row actually holds. */
int row_width(TerminalApp *app, int index);

/*
 * Fetch a cell from the "combined buffer": scrollback lines [0, sb.len) come
 * first, followed by the live screen rows. `index` is a combined row index.
 * Always fills `out` (empty cells get zeroed/default values) so backgrounds
 * still render for blank regions.
 */
bool combined_get_cell(TerminalApp *app, int index, int col, SbCell *out);

#endif /* PTX_BUFFER_H */
