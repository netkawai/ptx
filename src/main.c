#include "app.h"
#include "buffer.h"
#include "input.h"
#include "layout.h"
#include "menu.h"
#include "render.h"
#include "selection.h"
#include "terminal.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void handle_event(TerminalApp *app, const SDL_Event *event) {
    switch (event->type) {
    case SDL_EVENT_QUIT:
        app->running = false;
        break;

    case SDL_EVENT_WINDOW_RESIZED:
        recompute_layout(app, event->window.data1, event->window.data2);
        break;

    case SDL_EVENT_TEXT_INPUT:
        if (app->alt_held) terminal_write(app, "\x1b", 1);
        terminal_write(app, event->text.text, strlen(event->text.text));
        scroll_set(app, 0);
        break;

    case SDL_EVENT_KEY_DOWN:
        app->alt_held = (event->key.mod & SDL_KMOD_ALT) != 0;
        if (app->menu_visible) {
            bool was_escape = (event->key.key == SDLK_ESCAPE);
            menu_close(app);
            if (!was_escape)
                handle_key(app, event->key.key, event->key.mod);
        } else {
            handle_key(app, event->key.key, event->key.mod);
        }
        break;

    case SDL_EVENT_KEY_UP:
        app->alt_held = (event->key.mod & SDL_KMOD_ALT) != 0;
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        if (app->menu_visible)
            menu_close(app);
        else
            scroll_by(app, (int)(event->wheel.y * 3.0f));
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        float mx = event->button.x, my = event->button.y;
        SDL_Keymod mod = SDL_GetModState();

        /* Shift+Right click, or Shift+Ctrl+click on macOS, opens the context
         * menu. SDL may report Ctrl+click as a right click, so both spellings
         * are accepted. */
        bool menu_trigger =
            (event->button.button == SDL_BUTTON_RIGHT && (mod & SDL_KMOD_SHIFT)) ||
            (event->button.button == SDL_BUTTON_LEFT  && (mod & SDL_KMOD_SHIFT) && (mod & SDL_KMOD_CTRL));

        if (menu_trigger) {
            menu_open(app, mx, my);
        } else if (app->menu_visible) {
            int hit = menu_hit(app, mx, my);
            if (hit >= 0) {
                menu_activate(app, hit);
                menu_close(app);
            } else if (hit == -1) {
                menu_close(app);   /* click outside dismisses */
            }
            /* hit == -2: on the frame padding, keep it open */
        } else if (event->button.button == SDL_BUTTON_LEFT) {
            if (mx >= app->win_w - SCROLLBAR_W) {
                app->dragging_scrollbar = true;
                scrollbar_jump_to(app, my);
            } else {
                int row, col;
                mouse_to_cell(app, mx, my, &row, &col);
                app->selecting = true;
                app->has_selection = false;
                app->sel_start_row = app->sel_end_row = row;
                app->sel_start_col = app->sel_end_col = col;
                app_mark_dirty(app);
            }
        }
        break;
    }

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (app->menu_ignore_up) {
            app->menu_ignore_up = false;   /* release of the opening click */
        } else if (event->button.button == SDL_BUTTON_LEFT) {
            if (app->dragging_scrollbar) {
                app->dragging_scrollbar = false;
            } else if (app->selecting) {
                app->selecting = false;
                if (app->has_selection) copy_selection(app);
            }
        }
        break;

    case SDL_EVENT_MOUSE_MOTION: {
        float mx = event->motion.x, my = event->motion.y;
        if (app->menu_visible) {
            int hit = menu_hit(app, mx, my);
            int hover = (hit >= 0) ? hit : -1;
            if (hover != app->menu_hover) {
                app->menu_hover = hover;
                app_mark_dirty(app);
            }
        } else if (app->dragging_scrollbar) {
            scrollbar_jump_to(app, my);
        } else if (app->selecting && (event->motion.state & SDL_BUTTON_LMASK)) {
            int row, col;
            mouse_to_cell(app, mx, my, &row, &col);
            app->sel_end_row = row;
            app->sel_end_col = col;
            app->has_selection = true;
            app_mark_dirty(app);
        }
        break;
    }

    default:
        break;
    }
}

static bool load_font(TerminalApp *app) {
#if defined(__APPLE__)
    static const char *const candidates[] = {
        "/System/Library/Fonts/Supplemental/Courier New.ttf",
    };
#else
    static const char *const candidates[] = {
        "/usr/share/fonts/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/Adwaita/AdwaitaMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf",
    };
#endif
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        app->font = TTF_OpenFont(candidates[i], FONT_SIZE);
        if (app->font) {
            SDL_Log("Using font: %s", candidates[i]);
            return true;
        }
    }
    SDL_Log("Failed to load a monospace font: %s", SDL_GetError());
    return false;
}

static void shutdown_app(TerminalApp *app) {
    close(app->pty_master);
    if (app->read_thread) {
        SDL_WaitThread(app->read_thread, NULL);
        app->read_thread = NULL;
    }
    kill(app->shell_pid, SIGKILL);
    /* Non-blocking reap: a shell stuck in an uninterruptible sleep must not
     * freeze application shutdown. */
    for (int attempt = 0; attempt < 100; attempt++) {
        pid_t reaped = waitpid(app->shell_pid, NULL, WNOHANG);
        if (reaped == app->shell_pid || reaped < 0) break;
        SDL_Delay(10);
    }

    buffer_free(app);
    vterm_free(app->vterm);
    TTF_DestroyRendererTextEngine(app->text_engine);
    TTF_CloseFont(app->font);
    TTF_Quit();
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    SDL_DestroyMutex(app->vterm_lock);
    SDL_Quit();
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    TerminalApp app = { .running = true };
    app.rows = 24;
    app.cols = 80;
    app_mark_dirty(&app);
    app.vterm_lock = SDL_CreateMutex();

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    TTF_Init();

    app.window = SDL_CreateWindow("ptx - Resizable Shell Engine", 720, 400,
                                  SDL_WINDOW_RESIZABLE);
    app.renderer = SDL_CreateRenderer(app.window, NULL);

    if (!load_font(&app))
        return -1;

    TTF_GetStringSize(app.font, "M", 1, &app.glyph_w, &app.glyph_h);
    if (app.glyph_w <= 0) app.glyph_w = (int)(FONT_SIZE / 2);
    if (app.glyph_h <= 0) app.glyph_h = (int)FONT_SIZE;
    app.text_engine = TTF_CreateRendererTextEngine(app.renderer);

    SDL_StartTextInput(app.window);

    app.vterm = vterm_new(app.rows, app.cols);
    app.vts = vterm_obtain_screen(app.vterm);
    vterm_screen_set_callbacks(app.vts, &ptx_screen_callbacks, &app);
    vterm_screen_reset(app.vts, 1);

    if (!terminal_spawn(&app))
        return -1;

    int ww = 720, wh = 400;
    SDL_GetWindowSize(app.window, &ww, &wh);
    recompute_layout(&app, ww, wh);

    SDL_Event event;
    while (app.running) {
        while (SDL_PollEvent(&event))
            handle_event(&app, &event);

        if (SDL_GetAtomicInt(&app.dirty)) {
            SDL_SetAtomicInt(&app.dirty, 0);
            render_terminal_cells(&app);
        }
        SDL_Delay(4);
    }

    shutdown_app(&app);
    return 0;
}
