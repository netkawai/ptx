#include "selection.h"
#include "buffer.h"
#include "strutil.h"
#include "terminal.h"

#include <stdlib.h>
#include <string.h>

void sel_normalize(TerminalApp *app, int *sr, int *sc, int *er, int *ec) {
    *sr = app->sel_start_row; *sc = app->sel_start_col;
    *er = app->sel_end_row;   *ec = app->sel_end_col;
    if (*sr > *er || (*sr == *er && *sc > *ec)) {
        int tr = *sr, tc = *sc;
        *sr = *er; *sc = *ec;
        *er = tr;  *ec = tc;
    }
}

bool cell_selected(TerminalApp *app, int row, int col) {
    if (!app->has_selection) return false;
    int sr, sc, er, ec;
    sel_normalize(app, &sr, &sc, &er, &ec);
    if (row < sr || row > er) return false;
    if (row == sr && col < sc) return false;
    if (row == er && col > ec) return false;
    return true;
}

void copy_selection(TerminalApp *app) {
    if (!app->has_selection) return;

    SDL_LockMutex(app->vterm_lock);

    int sr, sc, er, ec;
    sel_normalize(app, &sr, &sc, &er, &ec);

    StrBuf out = {0};
    for (int row = sr; row <= er; row++) {
        int width = row_width(app, row);
        int c0 = (row == sr) ? sc : 0;
        int c1 = (row == er) ? ec : width - 1;
        if (c1 >= width) c1 = width - 1;

        StrBuf line = {0};
        for (int col = c0; col <= c1; col++) {
            SbCell cell;
            if (!combined_get_cell(app, row, col, &cell)) continue;
            if (cell.width == 0) continue;         /* wide-char continuation */
            if (cell.chars[0] == 0) { strbuf_append(&line, " ", 1); continue; }
            char enc[4];
            for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i]; i++) {
                int n = utf8_encode(cell.chars[i], enc);
                if (n > 0) strbuf_append(&line, enc, (size_t)n);
            }
        }
        /* Trim trailing spaces, as terminals conventionally do. */
        while (line.len > 0 && line.data[line.len - 1] == ' ')
            line.data[--line.len] = '\0';

        if (line.len > 0) strbuf_append(&out, line.data, line.len);
        if (row < er) strbuf_append(&out, "\n", 1);
        free(line.data);
    }

    SDL_UnlockMutex(app->vterm_lock);

    if (out.len > 0) {
        SDL_SetClipboardText(out.data);
        SDL_SetPrimarySelectionText(out.data);   /* Linux middle-click paste */
    }
    free(out.data);
}

void paste_clipboard(TerminalApp *app) {
    char *text = SDL_GetClipboardText();
    if (!text) return;

    /* Normalise line endings: a raw terminal wants CR for "Enter". */
    size_t n = strlen(text);
    for (size_t i = 0; i < n; i++)
        if (text[i] == '\n') text[i] = '\r';

    if (n > 0) terminal_write(app, text, n);
    SDL_free(text);
    scroll_set(app, 0);
}
