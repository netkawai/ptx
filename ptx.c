#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vterm.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <util.h>       // Contains macOS native forkpty() 
#include <sys/ioctl.h>
#include <signal.h>
#include <string.h>

// Mutable globals to hold active terminal grid size dimensions
int current_cols = 80;
int current_rows = 24;
#define FONT_SIZE 16.0f

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;
    TTF_TextEngine *text_engine;
    
    VTerm *vterm;
    VTermScreen *vts;
    
    int pty_master;
    pid_t shell_pid;
    SDL_Thread *read_thread;
    bool running;
} TerminalApp;

// --- 1. LIBVTERM STATE CALLBACKS ---
static int screen_damage(VTermRect rect, void *user) { return 1; }
static int screen_moverect(VTermRect dest, VTermRect src, void *user) { return 1; }

static VTermScreenCallbacks screen_callbacks = {
    .damage   = screen_damage,
    .moverect = screen_moverect,
};

// --- 2. ASYNCHRONOUS PTY READER THREAD ---
static int pty_read_worker(void *data) {
    TerminalApp *app = (TerminalApp *)data;
    char read_buffer[1024];
    
    while (app->running) {
        ssize_t bytes_read = read(app->pty_master, read_buffer, sizeof(read_buffer));
        if (bytes_read <= 0) {
            app->running = false;
            break;
        }
        vterm_input_write(app->vterm, read_buffer, bytes_read);
    }
    return 0;
}

// --- 3. PROCESS SPAWNER ---
bool spawn_native_zsh_process(TerminalApp *app) {
    struct winsize ws = { .ws_row = current_rows, .ws_col = current_cols };
    
    app->shell_pid = forkpty(&app->pty_master, NULL, NULL, &ws);
    if (app->shell_pid < 0) return false;
    
    if (app->shell_pid == 0) {
        setenv("TERM", "xterm-256color", 1);
        execl("/bin/zsh", "-l", NULL); 
        exit(EXIT_FAILURE);
    }
    
    app->read_thread = SDL_CreateThread(pty_read_worker, "PTY_Reader", app);
    return true;
}

static int write_utf8(uint32_t cp, char *out) {
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    else if (cp < 0x800) { out[0] = (char)((cp >> 6) | 0xC0); out[1] = (char)((cp & 0x3F) | 0x80); return 2; }
    return 0;
}

// --- 4. RENDER ENGINE WITH LAYOUT SCALING & BLINKING CURSOR ---
void render_terminal_cells(TerminalApp *app) {
    SDL_SetRenderDrawColor(app->renderer, 30, 30, 30, 255); 
    SDL_RenderClear(app->renderer);

    int glyph_w = 0, glyph_h = 0;
    TTF_GetStringSize(app->font, "A", 0, &glyph_w, &glyph_h);

    // Render Text Character Grids up to the newly scaled terminal bounds
    for (int row = 0; row < current_rows; row++) {
        for (int col = 0; col < current_cols; col++) {
            VTermPos pos = { .row = row, .col = col };
            VTermScreenCell cell;
            
            vterm_screen_get_cell(app->vts, pos, &cell);
            
            float target_x = (float)(col * glyph_w);
            float target_y = (float)(row * glyph_h);

            if (cell.bg.type != VTERM_COLOR_DEFAULT_BG) {
                SDL_FRect cell_rect = { target_x, target_y, (float)glyph_w, (float)glyph_h };
                SDL_SetRenderDrawColor(app->renderer, cell.bg.rgb.red, cell.bg.rgb.green, cell.bg.rgb.blue, 255);
                SDL_RenderFillRect(app->renderer, &cell_rect);
            }

            if (cell.chars[0] == 0) continue;

            char utf8_payload[6] = {0};
            int offset = 0;
            for(int i = 0; i < 6 && cell.chars[i]; i++) {
                offset += write_utf8(cell.chars[i], utf8_payload + offset);
            }

            TTF_Text *text_obj = TTF_CreateText(app->text_engine, app->font, utf8_payload, 0);
            if (text_obj) {
                if (cell.fg.type == VTERM_COLOR_DEFAULT_FG) {
                    TTF_SetTextColor(text_obj, 240, 240, 240, 255);
                } else {
                    TTF_SetTextColor(text_obj, cell.fg.rgb.red, cell.fg.rgb.green, cell.fg.rgb.blue, 255);
                }
                TTF_DrawRendererText(text_obj, target_x, target_y);
                TTF_DestroyText(text_obj);
            }
        }
    }

    // --- LINUX 0.11 SPEC: NATIVE TEXT CURSOR CARET RENDERING ---
    VTermPos cursor_pos;
    vterm_state_get_cursorpos(vterm_obtain_state(app->vterm), &cursor_pos);
    
    // Create a smooth 500ms pulsing blink cycle using standard system ticks
    if ((SDL_GetTicks() / 500) % 2 == 0) {
        float cursor_x = (float)(cursor_pos.col * glyph_w);
        float cursor_y = (float)(cursor_pos.row * glyph_h);
        
        SDL_FRect cursor_rect = { cursor_x, cursor_y, (float)glyph_w, (float)glyph_h };
        
        // Draw standard Adwaita accent cursor overlay block (Semi-translucent white)
        SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(app->renderer, 255, 255, 255, 140);
        SDL_RenderFillRect(app->renderer, &cursor_rect);
    }

    SDL_RenderPresent(app->renderer);
}

int main(int argc, char *argv[]) {
    TerminalApp app = { .running = true };

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    TTF_Init();

    // CRITICAL: Passed SDL_WINDOW_RESIZABLE to unlock macOS window borders
    app.window = SDL_CreateWindow("ptx - Resizable zsh Engine", 720, 400, SDL_WINDOW_RESIZABLE);
    app.renderer = SDL_CreateRenderer(app.window, NULL);
    
    app.font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Courier New.ttf", FONT_SIZE);
    app.text_engine = TTF_CreateRendererTextEngine(app.renderer);

    SDL_StartTextInput(app.window);

    app.vterm = vterm_new(current_rows, current_cols);
    app.vts = vterm_obtain_screen(app.vterm);
    vterm_screen_set_callbacks(app.vts, &screen_callbacks, &app);
    vterm_screen_reset(app.vts, 1);

    if (!spawn_native_zsh_process(&app)) return -1;

    SDL_Event event;
    while (app.running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                app.running = false;
            }
            // --- DYNAMIC LAYOUT COMPUTATION AND RESIZE BRIDGE ---
            else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                int w = event.window.data1;
                int h = event.window.data2;

                int glyph_w = 0, glyph_h = 0;
                TTF_GetStringSize(app.font, "A", 0, &glyph_w, &glyph_h);

                if (glyph_w > 0 && glyph_h > 0) {
                    current_cols = w / glyph_w;
                    current_rows = h / glyph_h;

                    // Enforce structural minimum grids
                    if (current_cols < 20) current_cols = 20;
                    if (current_rows < 5)  current_rows = 5;

                    // Sync inner virtual text matrices
                    vterm_set_size(app.vterm, current_rows, current_cols);

                    // Issue TIOCSWINSZ packet to force zsh text-wrapping re-calculations
                    struct winsize ws = {
                        .ws_row = (unsigned short)current_rows,
                        .ws_col = (unsigned short)current_cols,
                        .ws_xpixel = (unsigned short)w,
                        .ws_ypixel = (unsigned short)h
                    };
                    ioctl(app.pty_master, TIOCSWINSZ, &ws);
                }
            }
            else if (event.type == SDL_EVENT_TEXT_INPUT) {
                write(app.pty_master, event.text.text, strlen(event.text.text));
            }
            else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_RETURN) {
                    write(app.pty_master, "\r", 1);
                } else if (event.key.key == SDLK_BACKSPACE) {
                    write(app.pty_master, "\x7f", 1); 
                }
            }
        }
        
        render_terminal_cells(&app);
        SDL_Delay(8);
    }

    kill(app.shell_pid, SIGKILL); 
    close(app.pty_master);
    vterm_free(app.vterm);
    TTF_DestroyRendererTextEngine(app.text_engine);
    TTF_CloseFont(app.font);
    TTF_Quit();
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();

    return 0;
}
