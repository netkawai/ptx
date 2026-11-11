#ifndef PTX_CELL_H
#define PTX_CELL_H

#include "app.h"

/* Resolve a libvterm colour to 8-bit RGB. Returns false when the colour is
 * "default" (the renderer applies its own theme then). */
bool color_to_rgb(TerminalApp *app, const VTermColor *c,
                  uint8_t *r, uint8_t *g, uint8_t *b);

/* Convert a libvterm screen cell into our compact SbCell. */
void cell_from_screen(TerminalApp *app, SbCell *out,
                      const VTermScreenCell *vc);

#endif /* PTX_CELL_H */
