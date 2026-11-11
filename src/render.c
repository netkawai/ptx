#include "render.h"
#include "buffer.h"
#include "menu.h"
#include "selection.h"
#include "strutil.h"

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

void render_terminal_cells(TerminalApp *app) {
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
        app_mark_dirty(app);
    }
    draw_cursor(app, blink_on);

    draw_context_menu(app);

    SDL_RenderPresent(app->renderer);
}
