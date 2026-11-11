#ifndef PTX_MENU_H
#define PTX_MENU_H

#include "app.h"

/* Open the context menu with its top-left near (x, y), clamped on screen. */
void menu_open(TerminalApp *app, float x, float y);

/* Close the menu if it is open. */
void menu_close(TerminalApp *app);

/* Returns the item index, -1 for outside the menu, or -2 for inside the
 * frame but not over an item. */
int menu_hit(TerminalApp *app, float mx, float my);

/* Run the action for the given item (no-op if the item is disabled). */
void menu_activate(TerminalApp *app, int item);

/* Draw the menu on top of everything else. */
void draw_context_menu(TerminalApp *app);

#endif /* PTX_MENU_H */
