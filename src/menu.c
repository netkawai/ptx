#include "menu.h"
#include "selection.h"

#include <string.h>

/* SDL3 provides no menu widget, so the menu is just a rectangle we draw on
 * top of the terminal and hit-test against mouse events. */

enum { MENU_COPY, MENU_PASTE, MENU_ITEM_COUNT };

static const char *const menu_labels[MENU_ITEM_COUNT] = { "Copy", "Paste" };

static bool menu_item_enabled(TerminalApp *app, int item) {
    switch (item) {
        case MENU_COPY:  return app->has_selection;
        case MENU_PASTE: return true;
        default:         return false;
    }
}

static int menu_item_height(TerminalApp *app) {
    return app->glyph_h + 10;
}

static void menu_item_rect(TerminalApp *app, int item, SDL_FRect *r) {
    int item_h = menu_item_height(app);
    r->x = app->menu_x + MENU_PAD;
    r->y = app->menu_y + MENU_PAD + item * item_h;
    r->w = (float)(app->menu_w - MENU_PAD * 2);
    r->h = (float)item_h;
}

void menu_open(TerminalApp *app, float x, float y) {
    int text_w = 0;
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        int w = 0, h = 0;
        TTF_GetStringSize(app->font, menu_labels[i], strlen(menu_labels[i]), &w, &h);
        if (w > text_w) text_w = w;
    }

    app->menu_w = text_w + MENU_ITEM_PADX * 2;
    app->menu_h = MENU_PAD * 2 + menu_item_height(app) * MENU_ITEM_COUNT;

    /* Keep the whole menu on screen. */
    if (x + app->menu_w > app->win_w) x = (float)(app->win_w - app->menu_w);
    if (y + app->menu_h > app->win_h) y = (float)(app->win_h - app->menu_h);
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    app->menu_x = x;
    app->menu_y = y;
    app->menu_visible = true;
    app->menu_hover = -1;
    app->menu_ignore_up = true;   /* don't let the opening release select text */
    app_mark_dirty(app);
}

void menu_close(TerminalApp *app) {
    if (!app->menu_visible) return;
    app->menu_visible = false;
    app->menu_hover = -1;
    app_mark_dirty(app);
}

int menu_hit(TerminalApp *app, float mx, float my) {
    if (!app->menu_visible) return -1;
    if (mx < app->menu_x || mx >= app->menu_x + app->menu_w ||
        my < app->menu_y || my >= app->menu_y + app->menu_h)
        return -1;

    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        SDL_FRect r;
        menu_item_rect(app, i, &r);
        if (mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h)
            return i;
    }
    return -2;
}

void menu_activate(TerminalApp *app, int item) {
    if (!menu_item_enabled(app, item)) return;
    switch (item) {
        case MENU_COPY:  copy_selection(app);  break;
        case MENU_PASTE: paste_clipboard(app); break;
        default: break;
    }
}

void draw_context_menu(TerminalApp *app) {
    if (!app->menu_visible) return;

    SDL_Renderer *r = app->renderer;
    SDL_FRect frame = { app->menu_x, app->menu_y,
                        (float)app->menu_w, (float)app->menu_h };

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(r, 0, 0, 0, 90);
    SDL_FRect shadow = { app->menu_x + 3.0f, app->menu_y + 3.0f,
                         frame.w, frame.h };
    SDL_RenderFillRect(r, &shadow);

    SDL_SetRenderDrawColor(r, 46, 46, 50, 255);
    SDL_RenderFillRect(r, &frame);

    SDL_SetRenderDrawColor(r, 90, 90, 96, 255);
    SDL_RenderRect(r, &frame);

    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        SDL_FRect item;
        menu_item_rect(app, i, &item);
        bool enabled = menu_item_enabled(app, i);

        if (i == app->menu_hover && enabled) {
            SDL_SetRenderDrawColor(r, 70, 110, 170, 255);
            SDL_RenderFillRect(r, &item);
        }

        uint8_t c = enabled ? 235 : 120;
        TTF_Text *t = TTF_CreateText(app->text_engine, app->font,
                                     menu_labels[i], 0);
        if (t) {
            TTF_SetTextColor(t, c, c, enabled ? 235 : 125, 255);
            float tx = item.x + MENU_ITEM_PADX;
            float ty = item.y + (item.h - app->glyph_h) / 2.0f;
            TTF_DrawRendererText(t, tx, ty);
            TTF_DestroyText(t);
        }
    }
}
