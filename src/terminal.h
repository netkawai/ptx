#ifndef PTX_TERMINAL_H
#define PTX_TERMINAL_H

#include "app.h"

/* Fork the user's shell into a pty and start the reader thread. */
bool terminal_spawn(TerminalApp *app);

/* Write bytes to the child, handling partial writes and EAGAIN. */
void terminal_write(TerminalApp *app, const char *buf, size_t len);

/* libvterm screen callbacks. Register with
 * vterm_screen_set_callbacks(app->vts, &ptx_screen_callbacks, app). */
extern VTermScreenCallbacks ptx_screen_callbacks;

#endif /* PTX_TERMINAL_H */
