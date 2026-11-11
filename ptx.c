#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vterm.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

/*
 * forkpty() lives in <util.h> on macOS/BSD, but glibc (Linux) exposes it in
 * <pty.h> instead. POSIX does not standardise a pty header, so pick the right
 * one per platform.
 */
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#include <util.h>
#elif defined(__linux__)
#include <pty.h>
#elif defined(__sun)
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#else
#include <pty.h>        /* best-effort fallback for other libc implementations */
#endif

#define FONT_SIZE      16.0f
#define SCROLLBAR_W    12        /* reserved gutter on the right edge */
#define MAX_SCROLLBACK 5000      /* history lines kept in memory */

/* Flags packed into SbCell.flags */
#define F_FG        0x01
#define F_BG        0x02
#define F_BOLD      0x04
#define F_UNDERLINE 0x08
#define F_REVERSE   0x10

/*
 * A single stored cell. Deliberately compact: a full VTermScreenCell is ~56
 * bytes, which would cost tens of MB at MAX_SCROLLBACK. We only keep what the
 * renderer needs.
 */
typedef struct {
    uint32_t chars[VTERM_MAX_CHARS_PER_CELL];
    uint8_t  width;
    uint8_t  flags;
    uint8_t  fg_r, fg_g, fg_b;
    uint8_t  bg_r, bg_g, bg_b;
} SbCell;

typedef struct {
    SbCell *cells;
    int     cols;
} SbLine;

typedef struct {
    SbLine *lines;
    int     len;
    int     cap;
} Scrollback;

/* Growable byte buffer, used for building the copied selection text. */
typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} StrBuf;

typedef struct {
    SDL_Window      *window;
    SDL_Renderer    *renderer;
    TTF_Font        *font;
    TTF_TextEngine  *text_engine;

    VTerm           *vterm;
    VTermScreen     *vts;

    int              pty_master;
    pid_t            shell_pid;
    SDL_Thread      *read_thread;
    volatile bool    running;

    /* libvterm is not thread-safe: the PTY reader thread and the main/render
     * thread both touch it, so every access is serialised on this lock. */
    SDL_Mutex       *vterm_lock;
    SDL_AtomicInt    dirty;

    Scrollback       sb;
    int              scroll_offset;   /* lines scrolled up from the bottom */

    /* Selection is stored in absolute "combined buffer" coordinates
     * (scrollback lines first, then the live screen rows) so it stays put
     * while the view scrolls. */
    bool             has_selection;
    bool             selecting;
    int              sel_start_row, sel_start_col;
    int              sel_end_row,   sel_end_col;

    bool             dragging_scrollbar;

    bool             alt_held;

    /* Cached metrics */
    int              glyph_w, glyph_h;
    int              cols, rows;
    int              win_w, win_h;
} TerminalApp;

/* ------------------------------------------------------------------ */
/* Small string helpers                                                */
/* ------------------------------------------------------------------ */

static void strbuf_append(StrBuf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 256;
        while (cap < b->len + n + 1) cap *= 2;
        b->data = realloc(b->data, cap);
        b->cap = cap;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

static int utf8_encode(uint32_t cp, char *out) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    } else if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    } else if (cp < 0x110000) {
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Colour resolution                                                   */
/* ------------------------------------------------------------------ */

/* Returns false when the colour is "default" (we apply our own theme then). */
static bool color_to_rgb(TerminalApp *app, const VTermColor *c,
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

static void cell_from_screen(TerminalApp *app, SbCell *out,
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

/* ------------------------------------------------------------------ */
/* Scrollback                                                          */
/* ------------------------------------------------------------------ */

static void sb_trim(TerminalApp *app) {
    Scrollback *sb = &app->sb;
    while (sb->len > MAX_SCROLLBACK) {
        free(sb->lines[0].cells);
        memmove(&sb->lines[0], &sb->lines[1],
                (size_t)(sb->len - 1) * sizeof(SbLine));
        sb->len--;
        if (app->scroll_offset > 0) app->scroll_offset--;
    }
    if (app->scroll_offset > sb->len) app->scroll_offset = sb->len;
}

static int cb_sb_pushline(int cols, const VTermScreenCell *cells, void *user) {
    TerminalApp *app = user;
    Scrollback *sb = &app->sb;

    if (sb->len == sb->cap) {
        sb->cap = sb->cap ? sb->cap * 2 : 256;
        sb->lines = realloc(sb->lines, (size_t)sb->cap * sizeof(SbLine));
    }

    SbLine *line = &sb->lines[sb->len++];
    line->cols = cols;
    line->cells = malloc((size_t)cols * sizeof(SbCell));
    for (int i = 0; i < cols; i++)
        cell_from_screen(app, &line->cells[i], &cells[i]);

    /* Keep the visible content anchored while scrolled up. */
    if (app->scroll_offset > 0) app->scroll_offset++;
    sb_trim(app);
    return 1;
}

static int cb_sb_popline(int cols, VTermScreenCell *cells, void *user) {
    TerminalApp *app = user;
    Scrollback *sb = &app->sb;
    if (sb->len == 0) return 0;

    SbLine *line = &sb->lines[--sb->len];
    memset(cells, 0, (size_t)cols * sizeof(VTermScreenCell));
    for (int i = 0; i < cols && i < line->cols; i++) {
        SbCell *s = &line->cells[i];
        memcpy(cells[i].chars, s->chars, sizeof(cells[i].chars));
        cells[i].width = s->width ? s->width : 1;
        if (s->flags & F_FG) vterm_color_rgb(&cells[i].fg, s->fg_r, s->fg_g, s->fg_b);
        if (s->flags & F_BG) vterm_color_rgb(&cells[i].bg, s->bg_r, s->bg_g, s->bg_b);
        if (s->flags & F_BOLD)      cells[i].attrs.bold = 1;
        if (s->flags & F_UNDERLINE) cells[i].attrs.underline = 1;
        if (s->flags & F_REVERSE)   cells[i].attrs.reverse = 1;
    }
    free(line->cells);
    if (app->scroll_offset > 0) app->scroll_offset--;
    if (app->scroll_offset > sb->len) app->scroll_offset = sb->len;
    return 1;
}

static int cb_sb_clear(void *user) {
    TerminalApp *app = user;
    Scrollback *sb = &app->sb;
    for (int i = 0; i < sb->len; i++) free(sb->lines[i].cells);
    sb->len = 0;
    app->scroll_offset = 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* libvterm screen callbacks                                           */
/* ------------------------------------------------------------------ */

static int cb_damage(VTermRect rect, void *user) {
    (void)rect;
    SDL_SetAtomicInt(&((TerminalApp *)user)->dirty, 1);
    return 1;
}
static int cb_moverect(VTermRect dest, VTermRect src, void *user) {
    (void)dest; (void)src;
    SDL_SetAtomicInt(&((TerminalApp *)user)->dirty, 1);
    return 1;
}
static int cb_movecursor(VTermPos pos, VTermPos old, int visible, void *user) {
    (void)pos; (void)old; (void)visible;
    SDL_SetAtomicInt(&((TerminalApp *)user)->dirty, 1);
    return 1;
}
static int cb_bell(void *user) { (void)user; return 1; }
static int cb_resize(int rows, int cols, void *user) {
    (void)rows; (void)cols;
    SDL_SetAtomicInt(&((TerminalApp *)user)->dirty, 1);
    return 1;
}

static VTermScreenCallbacks screen_callbacks = {
    .damage      = cb_damage,
    .moverect    = cb_moverect,
    .movecursor  = cb_movecursor,
    .bell        = cb_bell,
    .resize      = cb_resize,
    .sb_pushline = cb_sb_pushline,
    .sb_popline  = cb_sb_popline,
    .sb_clear    = cb_sb_clear,
};

/* ------------------------------------------------------------------ */
/* PTY reader thread                                                   */
/* ------------------------------------------------------------------ */

static int pty_read_worker(void *data) {
    TerminalApp *app = data;
    char read_buffer[4096];

    while (app->running) {
        ssize_t bytes_read = read(app->pty_master, read_buffer, sizeof(read_buffer));
        if (bytes_read < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                SDL_Delay(4);
                continue;
            }
            app->running = false;
            break;
        }
        if (bytes_read == 0) {
            app->running = false;   /* EOF: the shell exited on its own */
            break;
        }
        SDL_LockMutex(app->vterm_lock);
        vterm_input_write(app->vterm, read_buffer, (size_t)bytes_read);
        SDL_UnlockMutex(app->vterm_lock);
        SDL_SetAtomicInt(&app->dirty, 1);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Process spawn                                                       */
/* ------------------------------------------------------------------ */

static const char *const fallback_shells[] = {
    "/bin/zsh", "/usr/bin/zsh",
    "/bin/bash", "/usr/bin/bash",
    "/bin/sh", NULL
};

static const char *pick_login_shell(void) {
    const char *env_shell = getenv("SHELL");
    if (env_shell && env_shell[0] == '/' && access(env_shell, X_OK) == 0)
        return env_shell;
    for (size_t i = 0; fallback_shells[i]; i++)
        if (access(fallback_shells[i], X_OK) == 0)
            return fallback_shells[i];
    return NULL;
}

static bool spawn_native_shell_process(TerminalApp *app) {
    struct winsize ws = {
        .ws_row = (unsigned short)app->rows,
        .ws_col = (unsigned short)app->cols,
    };

    const char *shell = pick_login_shell();

    app->shell_pid = forkpty(&app->pty_master, NULL, NULL, &ws);
    if (app->shell_pid < 0) return false;

    if (app->shell_pid == 0) {
        setenv("TERM", "xterm-256color", 1);
        if (shell) execl(shell, shell, "-l", (char *)NULL);
        execl("/bin/sh", "sh", (char *)NULL);
        _exit(EXIT_FAILURE);
    }

    int flags = fcntl(app->pty_master, F_GETFL, 0);
    if (flags != -1) fcntl(app->pty_master, F_SETFL, flags | O_NONBLOCK);

    app->read_thread = SDL_CreateThread(pty_read_worker, "PTY_Reader", app);
    return true;
}

static void pty_write(TerminalApp *app, const char *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(app->pty_master, buf + off, len - off);
        if (n > 0) { off += (size_t)n; continue; }
        if (n < 0 && (errno == EINTR)) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { SDL_Delay(1); continue; }
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Scrollback viewport helpers                                         */
/* ------------------------------------------------------------------ */

static void scroll_set(TerminalApp *app, int offset) {
    if (offset < 0) offset = 0;
    if (offset > app->sb.len) offset = app->sb.len;
    if (offset != app->scroll_offset) {
        app->scroll_offset = offset;
        SDL_SetAtomicInt(&app->dirty, 1);
    }
}

static void scroll_by(TerminalApp *app, int delta) {
    scroll_set(app, app->scroll_offset + delta);
}

/* Number of columns a given combined-buffer row actually holds. */
static int row_width(TerminalApp *app, int index) {
    if (index < app->sb.len) return app->sb.lines[index].cols;
    return app->cols;
}

/*
 * Fetch a cell from the "combined buffer": scrollback lines [0, sb.len) come
 * first, followed by the live screen rows. `index` is a combined row index.
 * Always fills `out` (empty cells get zeroed/default values) so backgrounds
 * still render for blank regions.
 */
static bool combined_get_cell(TerminalApp *app, int index, int col, SbCell *out) {
    memset(out, 0, sizeof(*out));
    if (index < 0 || index >= app->sb.len + app->rows) return false;

    if (index < app->sb.len) {
        SbLine *ln = &app->sb.lines[index];
        if (col < ln->cols) *out = ln->cells[col];
        return true;
    }

    VTermPos pos = { .row = index - app->sb.len, .col = col };
    VTermScreenCell vc;
    memset(&vc, 0, sizeof(vc));
    vterm_screen_get_cell(app->vts, pos, &vc);
    cell_from_screen(app, out, &vc);
    return true;
}

/* ------------------------------------------------------------------ */
/* Selection                                                           */
/* ------------------------------------------------------------------ */

static void sel_normalize(TerminalApp *app, int *sr, int *sc, int *er, int *ec) {
    *sr = app->sel_start_row; *sc = app->sel_start_col;
    *er = app->sel_end_row;   *ec = app->sel_end_col;
    if (*sr > *er || (*sr == *er && *sc > *ec)) {
        int tr = *sr, tc = *sc;
        *sr = *er; *sc = *ec;
        *er = tr;  *ec = tc;
    }
}

static bool cell_selected(TerminalApp *app, int row, int col) {
    if (!app->has_selection) return false;
    int sr, sc, er, ec;
    sel_normalize(app, &sr, &sc, &er, &ec);
    if (row < sr || row > er) return false;
    if (row == sr && col < sc) return false;
    if (row == er && col > ec) return false;
    return true;
}

static void copy_selection(TerminalApp *app) {
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

static void paste_clipboard(TerminalApp *app) {
    char *text = SDL_GetClipboardText();
    if (!text) return;

    /* Normalise line endings: a raw terminal wants CR for "Enter". */
    size_t n = strlen(text);
    for (size_t i = 0; i < n; i++)
        if (text[i] == '\n') text[i] = '\r';

    if (n > 0) pty_write(app, text, n);
    SDL_free(text);
    scroll_set(app, 0);
}

/* ------------------------------------------------------------------ */
/* Rendering                                                           */
/* ------------------------------------------------------------------ */

static void draw_scrollbar(TerminalApp *app) {
    int trackH = app->win_h;
    int x = app->win_w - SCROLLBAR_W;
    if (x < 0) return;

    SDL_SetRenderDrawColor(app->renderer, 45, 45, 45, 255);
    SDL_FRect track = { (float)x, 0.0f, (float)SCROLLBAR_W, (float)trackH };
    SDL_RenderFillRect(app->renderer, &track);

    int H = app->sb.len;
    if (H <= 0) return;   /* nothing to scroll: no thumb */

    int L = H + app->rows;
    float viewFrac = (float)app->rows / (float)L;
    float thumbH = viewFrac * trackH;
    if (thumbH < 24.0f) thumbH = 24.0f;
    if (thumbH > trackH) thumbH = (float)trackH;

    float startFrac = (float)(H - app->scroll_offset) / (float)L;
    float thumbY = startFrac * trackH;
    if (thumbY + thumbH > trackH) thumbY = trackH - thumbH;
    if (thumbY < 0) thumbY = 0;

    SDL_SetRenderDrawColor(app->renderer, 130, 130, 130, 255);
    SDL_FRect thumb = { (float)x + 2.0f, thumbY,
                        (float)SCROLLBAR_W - 4.0f, thumbH };
    SDL_RenderFillRect(app->renderer, &thumb);
}

static void draw_cursor(TerminalApp *app, bool blink_on) {
    if (app->scroll_offset != 0 || !blink_on) return;

    VTermPos cur;
    vterm_state_get_cursorpos(vterm_obtain_state(app->vterm), &cur);
    if (cur.row < 0 || cur.row >= app->rows || cur.col < 0 || cur.col >= app->cols)
        return;

    SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(app->renderer, 255, 255, 255, 140);
    SDL_FRect r = { (float)(cur.col * app->glyph_w), (float)(cur.row * app->glyph_h),
                    (float)app->glyph_w, (float)app->glyph_h };
    SDL_RenderFillRect(app->renderer, &r);
}

static void render_terminal_cells(TerminalApp *app) {
    SDL_SetRenderDrawColor(app->renderer, 30, 30, 30, 255);
    SDL_RenderClear(app->renderer);

    int gw = app->glyph_w, gh = app->glyph_h;
    if (gw > 0 && gh > 0) {
        SDL_LockMutex(app->vterm_lock);

        int start = app->sb.len - app->scroll_offset;
        if (start < 0) start = 0;

        for (int row = 0; row < app->rows; row++) {
            int index = start + row;
            for (int col = 0; col < app->cols; col++) {
                SbCell cell;
                if (!combined_get_cell(app, index, col, &cell)) continue;

                float x = (float)(col * gw);
                float y = (float)(row * gh);

                bool has_fg = cell.flags & F_FG;
                bool has_bg = cell.flags & F_BG;
                bool rev    = cell.flags & F_REVERSE;

                uint8_t fr = 240, fg = 240, fb = 240;
                uint8_t br = 30,  bg = 30,  bb = 30;
                if (has_fg) { fr = cell.fg_r; fg = cell.fg_g; fb = cell.fg_b; }
                if (has_bg) { br = cell.bg_r; bg = cell.bg_g; bb = cell.bg_b; }
                if (rev) {
                    uint8_t t;
                    t = fr; fr = br; br = t;
                    t = fg; fg = bg; bg = t;
                    t = fb; fb = bb; bb = t;
                }

                if (has_bg || rev) {
                    SDL_SetRenderDrawColor(app->renderer, br, bg, bb, 255);
                    SDL_FRect r = { x, y, (float)gw, (float)gh };
                    SDL_RenderFillRect(app->renderer, &r);
                }

                if (cell_selected(app, index, col)) {
                    SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(app->renderer, 80, 130, 200, 120);
                    SDL_FRect r = { x, y, (float)gw, (float)gh };
                    SDL_RenderFillRect(app->renderer, &r);
                }

                if (cell.chars[0] == 0 || cell.width == 0) continue;

                char utf8_payload[32] = {0};
                int offset = 0;
                for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i]; i++) {
                    int n = utf8_encode(cell.chars[i], utf8_payload + offset);
                    if (n <= 0) break;
                    offset += n;
                    if (offset >= (int)sizeof(utf8_payload) - 4) break;
                }

                TTF_Text *text_obj = TTF_CreateText(app->text_engine, app->font,
                                                    utf8_payload, 0);
                if (text_obj) {
                    TTF_SetTextColor(text_obj, fr, fg, fb, 255);
                    TTF_DrawRendererText(text_obj, x, y);
                    TTF_DestroyText(text_obj);
                }
            }
        }

        SDL_UnlockMutex(app->vterm_lock);
    }

    draw_scrollbar(app);

    static bool blink_state = true;
    bool blink_on = (SDL_GetTicks() / 500) % 2 == 0;
    if (blink_on != blink_state) {
        blink_state = blink_on;
        SDL_SetAtomicInt(&app->dirty, 1);
    }
    draw_cursor(app, blink_on);

    SDL_RenderPresent(app->renderer);
}

/* ------------------------------------------------------------------ */
/* Layout                                                              */
/* ------------------------------------------------------------------ */

static void recompute_layout(TerminalApp *app, int w, int h) {
    app->win_w = w;
    app->win_h = h;

    int usable = w - SCROLLBAR_W;
    if (usable < 1) usable = 1;

    int cols = app->glyph_w > 0 ? usable / app->glyph_w : 80;
    int rows = app->glyph_h > 0 ? h / app->glyph_h : 24;
    if (cols < 20) cols = 20;
    if (rows < 5)  rows = 5;

    app->cols = cols;
    app->rows = rows;

    SDL_LockMutex(app->vterm_lock);
    vterm_set_size(app->vterm, rows, cols);
    SDL_UnlockMutex(app->vterm_lock);

    struct winsize ws = {
        .ws_row = (unsigned short)rows,
        .ws_col = (unsigned short)cols,
        .ws_xpixel = (unsigned short)w,
        .ws_ypixel = (unsigned short)h,
    };
    ioctl(app->pty_master, TIOCSWINSZ, &ws);

    /* Geometry changed: stored selection coordinates no longer make sense. */
    app->has_selection = false;
    app->selecting = false;
    SDL_SetAtomicInt(&app->dirty, 1);
}

/* ------------------------------------------------------------------ */
/* Input                                                               */
/* ------------------------------------------------------------------ */

/* Map a cell-space mouse position to a combined-buffer row/col. */
static void mouse_to_cell(TerminalApp *app, float mx, float my, int *row, int *col) {
    int start = app->sb.len - app->scroll_offset;
    if (start < 0) start = 0;
    *row = start + (app->glyph_h > 0 ? (int)(my / app->glyph_h) : 0);
    *col = app->glyph_w > 0 ? (int)(mx / app->glyph_w) : 0;
}

static void scrollbar_jump_to(TerminalApp *app, float my) {
    int trackH = app->win_h;
    if (trackH <= 0) return;
    float frac = my / trackH;
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    int L = app->sb.len + app->rows;
    int offset = app->sb.len - (int)(frac * L);
    scroll_set(app, offset);
}

/* Returns true if the key was consumed (do not let TEXT_INPUT also fire). */
static bool handle_key(TerminalApp *app, SDL_Keycode key, SDL_Keymod mod) {
    bool ctrl  = mod & SDL_KMOD_CTRL;
    bool shift = mod & SDL_KMOD_SHIFT;
    bool alt   = mod & SDL_KMOD_ALT;
    bool gui   = mod & SDL_KMOD_GUI;

    /* Copy / paste: Ctrl+Shift+C/V (and Cmd+C/V on macOS/Windows). */
    if ((ctrl && shift && key == SDLK_C) || (gui && key == SDLK_C)) {
        copy_selection(app);
        return true;
    }
    if ((ctrl && shift && key == SDLK_V) || (gui && key == SDLK_V)) {
        paste_clipboard(app);
        return true;
    }

    /* Shift+PageUp/Down and Shift+Home/End scroll the viewport. */
    if (shift && key == SDLK_PAGEUP)   { scroll_by(app, app->rows - 1);  return true; }
    if (shift && key == SDLK_PAGEDOWN) { scroll_by(app, -(app->rows - 1)); return true; }
    if (shift && key == SDLK_HOME)     { scroll_set(app, app->sb.len);   return true; }
    if (shift && key == SDLK_END)      { scroll_set(app, 0);             return true; }

    /* Any other key snaps the view back to the live screen. */
    if (app->scroll_offset != 0) scroll_set(app, 0);

    const char *seq = NULL;
    switch (key) {
        case SDLK_ESCAPE:    seq = "\x1b";     break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:  seq = "\r";       break;
        case SDLK_BACKSPACE: seq = "\x7f";     break;
        case SDLK_TAB:       seq = "\t";       break;
        case SDLK_UP:        seq = "\x1b[A";   break;
        case SDLK_DOWN:      seq = "\x1b[B";   break;
        case SDLK_RIGHT:     seq = "\x1b[C";   break;
        case SDLK_LEFT:      seq = "\x1b[D";   break;
        case SDLK_HOME:      seq = "\x1b[H";   break;
        case SDLK_END:       seq = "\x1b[F";   break;
        case SDLK_INSERT:    seq = "\x1b[2~";  break;
        case SDLK_DELETE:    seq = "\x1b[3~";  break;
        case SDLK_PAGEUP:    seq = "\x1b[5~";  break;
        case SDLK_PAGEDOWN:  seq = "\x1b[6~";  break;
        case SDLK_F1:        seq = "\x1bOP";   break;
        case SDLK_F2:        seq = "\x1bOQ";   break;
        case SDLK_F3:        seq = "\x1bOR";   break;
        case SDLK_F4:        seq = "\x1bOS";   break;
        case SDLK_F5:        seq = "\x1b[15~"; break;
        case SDLK_F6:        seq = "\x1b[17~"; break;
        case SDLK_F7:        seq = "\x1b[18~"; break;
        case SDLK_F8:        seq = "\x1b[19~"; break;
        case SDLK_F9:        seq = "\x1b[20~"; break;
        case SDLK_F10:       seq = "\x1b[21~"; break;
        case SDLK_F11:       seq = "\x1b[23~"; break;
        case SDLK_F12:       seq = "\x1b[24~"; break;
        default: break;
    }
    if (seq) {
        if (alt) pty_write(app, "\x1b", 1);
        pty_write(app, seq, strlen(seq));
        return true;
    }

    /* Ctrl+<key> control characters (Ctrl+C, Ctrl+D, ...). */
    if (ctrl && !shift && !gui) {
        if (key >= SDLK_A && key <= SDLK_Z) {
            char c = (char)(key - SDLK_A + 1);
            pty_write(app, &c, 1);
            return true;
        }
        switch (key) {
            case SDLK_SPACE:  { char c = 0;  pty_write(app, &c, 1); return true; }
            case SDLK_AT:     { char c = 0;  pty_write(app, &c, 1); return true; }
            case SDLK_LEFTBRACKET:  { char c = 27; pty_write(app, &c, 1); return true; }
            case SDLK_BACKSLASH:    { char c = 28; pty_write(app, &c, 1); return true; }
            case SDLK_RIGHTBRACKET: { char c = 29; pty_write(app, &c, 1); return true; }
            case SDLK_CARET:        { char c = 30; pty_write(app, &c, 1); return true; }
            case SDLK_UNDERSCORE:   { char c = 31; pty_write(app, &c, 1); return true; }
            default: break;
        }
    }

    return false;   /* let SDL_EVENT_TEXT_INPUT produce the character */
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    TerminalApp app = { .running = true };
    SDL_SetAtomicInt(&app.dirty, 1);
    app.vterm_lock = SDL_CreateMutex();

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    TTF_Init();

    app.window = SDL_CreateWindow("ptx - Resizable Shell Engine", 720, 400,
                                  SDL_WINDOW_RESIZABLE);
    app.renderer = SDL_CreateRenderer(app.window, NULL);

#if defined(__APPLE__)
    const char *font_candidates[] = {
        "/System/Library/Fonts/Supplemental/Courier New.ttf",
    };
#else
    const char *font_candidates[] = {
        "/usr/share/fonts/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/Adwaita/AdwaitaMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf",
    };
#endif
    const char *font_path = NULL;
    for (size_t i = 0; i < sizeof(font_candidates) / sizeof(font_candidates[0]); i++) {
        app.font = TTF_OpenFont(font_candidates[i], FONT_SIZE);
        if (app.font) { font_path = font_candidates[i]; break; }
    }
    if (!app.font) {
        SDL_Log("Failed to load a monospace font: %s", SDL_GetError());
        return -1;
    }
    SDL_Log("Using font: %s", font_path);

    TTF_GetStringSize(app.font, "M", 1, &app.glyph_w, &app.glyph_h);
    if (app.glyph_w <= 0) app.glyph_w = (int)(FONT_SIZE / 2);
    if (app.glyph_h <= 0) app.glyph_h = (int)FONT_SIZE;
    app.text_engine = TTF_CreateRendererTextEngine(app.renderer);

    SDL_StartTextInput(app.window);

    int ww = 720, wh = 400;
    SDL_GetWindowSize(app.window, &ww, &wh);

    app.vterm = vterm_new(24, 80);
    app.vts = vterm_obtain_screen(app.vterm);
    vterm_screen_set_callbacks(app.vts, &screen_callbacks, &app);
    vterm_screen_reset(app.vts, 1);

    if (!spawn_native_shell_process(&app)) return -1;

    recompute_layout(&app, ww, wh);

    SDL_Event event;
    while (app.running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                app.running = false;
            }
            else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                int w = event.window.data1, h = event.window.data2;
                recompute_layout(&app, w, h);
            }
            else if (event.type == SDL_EVENT_TEXT_INPUT) {
                if (app.alt_held) pty_write(&app, "\x1b", 1);
                pty_write(&app, event.text.text, strlen(event.text.text));
                scroll_set(&app, 0);
            }
            else if (event.type == SDL_EVENT_KEY_DOWN) {
                app.alt_held = (event.key.mod & SDL_KMOD_ALT) != 0;
                handle_key(&app, event.key.key, event.key.mod);
            }
            else if (event.type == SDL_EVENT_KEY_UP) {
                app.alt_held = (event.key.mod & SDL_KMOD_ALT) != 0;
            }
            else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                scroll_by(&app, (int)(event.wheel.y * 3.0f));
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                float mx = event.button.x, my = event.button.y;
                if (event.button.button == SDL_BUTTON_LEFT) {
                    if (mx >= app.win_w - SCROLLBAR_W) {
                        app.dragging_scrollbar = true;
                        scrollbar_jump_to(&app, my);
                    } else if (mx < app.win_w - SCROLLBAR_W) {
                        int row, col;
                        mouse_to_cell(&app, mx, my, &row, &col);
                        app.selecting = true;
                        app.has_selection = false;
                        app.sel_start_row = app.sel_end_row = row;
                        app.sel_start_col = app.sel_end_col = col;
                        SDL_SetAtomicInt(&app.dirty, 1);
                    }
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    if (app.dragging_scrollbar) {
                        app.dragging_scrollbar = false;
                    } else if (app.selecting) {
                        app.selecting = false;
                        if (app.has_selection) copy_selection(&app);
                    }
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                float mx = event.motion.x, my = event.motion.y;
                if (app.dragging_scrollbar) {
                    scrollbar_jump_to(&app, my);
                } else if (app.selecting && (event.motion.state & SDL_BUTTON_LMASK)) {
                    int row, col;
                    mouse_to_cell(&app, mx, my, &row, &col);
                    app.sel_end_row = row;
                    app.sel_end_col = col;
                    app.has_selection = true;
                    SDL_SetAtomicInt(&app.dirty, 1);
                }
            }
        }

        if (SDL_GetAtomicInt(&app.dirty)) {
            SDL_SetAtomicInt(&app.dirty, 0);
            render_terminal_cells(&app);
        }
        SDL_Delay(4);
    }

    close(app.pty_master);
    if (app.read_thread) {
        SDL_WaitThread(app.read_thread, NULL);
        app.read_thread = NULL;
    }
    kill(app.shell_pid, SIGKILL);
    for (int attempt = 0; attempt < 100; attempt++) {
        pid_t reaped = waitpid(app.shell_pid, NULL, WNOHANG);
        if (reaped == app.shell_pid || reaped < 0) break;
        SDL_Delay(10);
    }

    for (int i = 0; i < app.sb.len; i++) free(app.sb.lines[i].cells);
    free(app.sb.lines);

    vterm_free(app.vterm);
    TTF_DestroyRendererTextEngine(app.text_engine);
    TTF_CloseFont(app.font);
    TTF_Quit();
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_DestroyMutex(app.vterm_lock);
    SDL_Quit();

    return 0;
}
