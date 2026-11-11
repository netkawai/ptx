#include "input.h"
#include "buffer.h"
#include "selection.h"
#include "terminal.h"

#include <string.h>

void mouse_to_cell(TerminalApp *app, float mx, float my, int *row, int *col) {
    int start = app->sb.len - app->scroll_offset;
    if (start < 0) start = 0;
    *row = start + (app->glyph_h > 0 ? (int)(my / app->glyph_h) : 0);
    *col = app->glyph_w > 0 ? (int)(mx / app->glyph_w) : 0;
}

void scrollbar_jump_to(TerminalApp *app, float my) {
    int trackH = app->win_h;
    if (trackH <= 0) return;
    float frac = my / trackH;
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    int L = app->sb.len + app->rows;
    int offset = app->sb.len - (int)(frac * L);
    scroll_set(app, offset);
}

bool handle_key(TerminalApp *app, SDL_Keycode key, SDL_Keymod mod) {
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
        if (alt) terminal_write(app, "\x1b", 1);
        terminal_write(app, seq, strlen(seq));
        return true;
    }

    /* Ctrl+<key> control characters (Ctrl+C, Ctrl+D, ...). */
    if (ctrl && !shift && !gui) {
        if (key >= SDLK_A && key <= SDLK_Z) {
            char c = (char)(key - SDLK_A + 1);
            terminal_write(app, &c, 1);
            return true;
        }
        switch (key) {
            case SDLK_SPACE:        { char c = 0;  terminal_write(app, &c, 1); return true; }
            case SDLK_AT:           { char c = 0;  terminal_write(app, &c, 1); return true; }
            case SDLK_LEFTBRACKET:  { char c = 27; terminal_write(app, &c, 1); return true; }
            case SDLK_BACKSLASH:    { char c = 28; terminal_write(app, &c, 1); return true; }
            case SDLK_RIGHTBRACKET: { char c = 29; terminal_write(app, &c, 1); return true; }
            case SDLK_CARET:        { char c = 30; terminal_write(app, &c, 1); return true; }
            case SDLK_UNDERSCORE:   { char c = 31; terminal_write(app, &c, 1); return true; }
            default: break;
        }
    }

    return false;   /* let SDL_EVENT_TEXT_INPUT produce the character */
}
