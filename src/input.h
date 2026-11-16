#ifndef PTX_INPUT_H
#define PTX_INPUT_H

#include "app.h"

/* Map a window-space mouse position to a combined-buffer row/col. */
void mouse_to_cell(TerminalApp *app, float mx, float my, int *row, int *col);

/* Scroll the viewport by `wheel_y` notches (positive = towards the
 * bottom). Uses GTK4's wheel step of min(3 * step, page * 0.9). */
void scroll_view_wheel(TerminalApp *app, double wheel_y);

/* Translate a key press into pty input. Returns true if consumed (so the
 * caller should not also treat it as text input). */
bool handle_key(TerminalApp *app, SDL_Keycode key, SDL_Keymod mod);

#endif /* PTX_INPUT_H */
