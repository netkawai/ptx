#ifndef PTX_APP_H
#define PTX_APP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>   /* pid_t */

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vterm.h>

/* ---- Tunables ---------------------------------------------------- */
#define FONT_SIZE      16.0f
#define SCROLLBAR_W    12        /* reserved gutter on the right edge */
#define MAX_SCROLLBACK 5000      /* history lines kept in memory */

#define MENU_PAD        6        /* padding around the context menu items */
#define MENU_ITEM_PADX  12       /* horizontal padding inside a menu item */

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

    /* Context menu. SDL3 has no native menu widget, so this is a small
     * overlay we draw and hit-test ourselves. */
    bool             menu_visible;
    bool             menu_ignore_up;   /* swallow release of the opening click */
    float            menu_x, menu_y;   /* top-left, window coordinates */
    int              menu_w, menu_h;
    int              menu_hover;       /* hovered item index, or -1 */

    /* Cached metrics */
    int              glyph_w, glyph_h;
    int              cols, rows;
    int              win_w, win_h;
} TerminalApp;

/* Mark the app dirty so the main loop redraws. Safe to call from any thread. */
static inline void app_mark_dirty(TerminalApp *app) {
    SDL_SetAtomicInt(&app->dirty, 1);
}

#endif /* PTX_APP_H */
