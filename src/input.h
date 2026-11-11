#ifndef PTX_INPUT_H
#define PTX_INPUT_H

#include "app.h"

/* Map a window-space mouse position to a combined-buffer row/col. */
void mouse_to_cell(TerminalApp *app, float mx, float my, int *row, int *col);

/* Scroll so the viewport centres on the given y within the scrollbar track. */
void scrollbar_jump_to(TerminalApp *app, float my);

/* Translate a key press into pty input. Returns true if consumed (so the
 * caller should not also treat it as text input). */
bool handle_key(TerminalApp *app, SDL_Keycode key, SDL_Keymod mod);

#endif /* PTX_INPUT_H */
