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

#define COLS 80
#define ROWS 24
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

// --- 2. ASYNCHRONOUS PTY READER THREAD ENGINE ---
static int pty_read_worker(void *data) {
    TerminalApp *app = (TerminalApp *)data;
    char read_buffer[4096]; // Increased buffer size for fast stream chunks
    
    printf("[DEBUG] PTY Reader Thread started safely.\n");
    while (app->running) {
        ssize_t bytes_read = read(app->pty_master, read_buffer, sizeof(read_buffer));
        if (bytes_read <= 0) {
            printf("[DEBUG] PTY Read <= 0. Shell process likely closed.\n");
            app->running = false;
            break;
        }
        
        // Pass stream blocks directly into libvterm state engines
        vterm_input_write(app->vterm, read_buffer, bytes_read);
    }
    return 0;
}

// --- 3. MACOS PROCESS SPAWNER (zsh Integration) ---
bool spawn_native_zsh_process(TerminalApp *app) {
    struct winsize ws = { .ws_row = ROWS, .ws_col = COLS };
    
    app->shell_pid = forkpty(&app->pty_master, NULL, NULL, &ws);
    if (app->shell_pid < 0) {
        printf("[ERROR] forkpty failed.\n");
        return false;
    }
    
    if (app->shell_pid == 0) {
        setenv("TERM", "xterm-256color", 1);
        execl("/bin/zsh", "-l", NULL); 
        exit(EXIT_FAILURE);
    }
    
    printf("[DEBUG] Spawned zsh with PID: %d, Master FD: %d\n", app->shell_pid, app->pty_master);
    app->read_thread = SDL_CreateThread(pty_read_worker, "PTY_Reader", app);
    return true;
}

// Helper to convert Unicode Code Points from libvterm into clean UTF-8 string bytes
static int write_utf8(uint32_t cp, char *out) {
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    else if (cp < 0x800) { out[0] = (char)((cp >> 6) | 0xC0); out[1] = (char)((cp & 0x3F) | 0x80); return 2; }
    else if (cp < 0x10000) { out[0] = (char)((cp >> 12) | 0xE0); out[1] = (char)(((cp >> 6) & 0x3F) | 0x80); out[2] = (char)((cp & 0x3F) | 0x80); return 3; }
    return 0;
}

// --- 4. OPTIMIZED HIGH-PERFORMANCE RENDER DRIVER ---
void render_terminal_cells(TerminalApp *app) {
    SDL_SetRenderDrawColor(app->renderer, 30, 30, 30, 255); // Adwaita Dark Slate
    SDL_RenderClear(app->renderer);

    int glyph_w = 0, glyph_h = 0;
    TTF_GetStringSize(app->font, "A", 0, &glyph_w, &glyph_h);

    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            VTermPos pos = { .row = row, .col = col };
            VTermScreenCell cell;
            
            vterm_screen_get_cell(app->vts, pos, &cell);
            
            float target_x = (float)(col * glyph_w);
            float target_y = (float)(row * glyph_h);

            // Draw custom backgrounds if specified
            if (cell.bg.type != VTERM_COLOR_DEFAULT_BG) {
                SDL_FRect cell_rect = { target_x, target_y, (float)glyph_w, (float)glyph_h };
                SDL_SetRenderDrawColor(app->renderer, cell.bg.rgb.red, cell.bg.rgb.green, cell.bg.rgb.blue, 255);
                SDL_RenderFillRect(app->renderer, &cell_rect);
            }

            // Fix the array check: `cell.chars` is an array. If `cell.chars[0] == 0`, the slot is empty.
            if (cell.chars[0] == 0) continue;

            char utf8_payload[7] = {0};
            int offset = 0;
            for(int i = 0; i < 6 && cell.chars[i]; i++) {
                offset += write_utf8(cell.chars[i], utf8_payload + offset);
            }

            // PERFORMANCE FIX: Instead of keeping hundreds of permanent overhead objects on the heap,
            // render text immediately using SDL3_ttf's stack rendering optimizations.
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
    SDL_RenderPresent(app->renderer);
}

int main(int argc, char *argv[]) {
    TerminalApp app = { .running = true };

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        printf("[ERROR] SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }
    TTF_Init();

    app.window = SDL_CreateWindow("ptx - zsh Engine", 720, 400, 0);
    app.renderer = SDL_CreateRenderer(app.window, NULL);
    
    app.font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Courier New.ttf", FONT_SIZE);
    if (!app.font) {
        printf("[ERROR] Failed to load font!\n");
        return -1;
    }
    app.text_engine = TTF_CreateRendererTextEngine(app.renderer);

    // CRITICAL CRITICAL FIX: SDL3 forces TextInput to be explicitly started per focus window.
    // Without this, alphanumeric keys are completely eaten and ignored by the subsystem.
    SDL_StartTextInput(app.window);
    printf("[DEBUG] Enabled SDL3 native TextInput engine routing paths.\n");

    app.vterm = vterm_new(ROWS, COLS);
    app.vts = vterm_obtain_screen(app.vterm);
    vterm_screen_set_callbacks(app.vts, &screen_callbacks, &app);
    vterm_screen_reset(app.vts, 1);

    if (!spawn_native_zsh_process(&app)) {
        return -1;
    }

    SDL_Event event;
    while (app.running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                app.running = false;
            }
            else if (event.type == SDL_EVENT_TEXT_INPUT) {
                // Verified payload delivery: captures text input characters
                // printf("[EVENT] Text Input received: '%s' (Length: %zu)\n", event.text.text, strlen(event.text.text));
                write(app.pty_master, event.text.text, strlen(event.text.text));
            }
            else if (event.type == SDL_EVENT_KEY_DOWN) {
                // Handled specifically for structural shell control symbols
                if (event.key.key == SDLK_RETURN) {
                    //printf("[EVENT] Return key mapped to master.\n");
                    write(app.pty_master, "\r", 1);
                } else if (event.key.key == SDLK_BACKSPACE) {
                    //printf("[EVENT] Backspace key mapped to master.\n");
                    write(app.pty_master, "\x7f", 1); 
                }
            }
        }
        
        render_terminal_cells(&app);
        SDL_Delay(8); // Targets standard 60-120fps clock cadences smoothly
    }

    // --- EXIT INTEGRATIONS ---
    printf("[DEBUG] Terminating background targets and closing frames.\n");
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
