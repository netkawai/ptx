#include "cell.h"

#include <string.h>

bool color_to_rgb(TerminalApp *app, const VTermColor *c,
                  uint8_t *r, uint8_t *g, uint8_t *b) {
    if (VTERM_COLOR_IS_DEFAULT_FG(c) || VTERM_COLOR_IS_DEFAULT_BG(c))
        return false;
    VTermColor tmp = *c;
    vterm_screen_convert_color_to_rgb(app->vts, &tmp);
    *r = tmp.rgb.red;
    *g = tmp.rgb.green;
    *b = tmp.rgb.blue;
    return true;
}

void cell_from_screen(TerminalApp *app, SbCell *out,
                      const VTermScreenCell *vc) {
    memset(out, 0, sizeof(*out));
    memcpy(out->chars, vc->chars, sizeof(out->chars));
    out->width = (uint8_t)vc->width;

    uint8_t r, g, b;
    if (color_to_rgb(app, &vc->fg, &r, &g, &b)) {
        out->flags |= F_FG;
        out->fg_r = r; out->fg_g = g; out->fg_b = b;
    }
    if (color_to_rgb(app, &vc->bg, &r, &g, &b)) {
        out->flags |= F_BG;
        out->bg_r = r; out->bg_g = g; out->bg_b = b;
    }
    if (vc->attrs.bold)      out->flags |= F_BOLD;
    if (vc->attrs.underline) out->flags |= F_UNDERLINE;
    if (vc->attrs.reverse)   out->flags |= F_REVERSE;
}
