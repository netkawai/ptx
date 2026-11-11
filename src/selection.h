#ifndef PTX_SELECTION_H
#define PTX_SELECTION_H

#include "app.h"

/* Order the selection endpoints (start before end). */
void sel_normalize(TerminalApp *app, int *sr, int *sc, int *er, int *ec);

/* True if the given combined-buffer cell falls inside the selection. */
bool cell_selected(TerminalApp *app, int row, int col);

/* Copy the current selection to the system clipboard / primary selection. */
void copy_selection(TerminalApp *app);

/* Paste the system clipboard into the pty. */
void paste_clipboard(TerminalApp *app);

#endif /* PTX_SELECTION_H */
