#include "terminal.h"
#include "buffer.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>   /* struct winsize */
#include <unistd.h>

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

/* ------------------------------------------------------------------ */
/* libvterm screen callbacks                                           */
/* ------------------------------------------------------------------ */

static int cb_damage(VTermRect rect, void *user) {
    (void)rect;
    app_mark_dirty(user);
    return 1;
}
static int cb_moverect(VTermRect dest, VTermRect src, void *user) {
    (void)dest; (void)src;
    app_mark_dirty(user);
    return 1;
}
static int cb_movecursor(VTermPos pos, VTermPos old, int visible, void *user) {
    (void)pos; (void)old; (void)visible;
    app_mark_dirty(user);
    return 1;
}
static int cb_bell(void *user) { (void)user; return 1; }
static int cb_resize(int rows, int cols, void *user) {
    (void)rows; (void)cols;
    app_mark_dirty(user);
    return 1;
}

static int cb_sb_pushline(int cols, const VTermScreenCell *cells, void *user) {
    buffer_push_line(user, cols, cells);
    return 1;
}
static int cb_sb_popline(int cols, VTermScreenCell *cells, void *user) {
    return buffer_pop_line(user, cols, cells) ? 1 : 0;
}
static int cb_sb_clear(void *user) {
    buffer_clear(user);
    return 1;
}

VTermScreenCallbacks ptx_screen_callbacks = {
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
        app_mark_dirty(app);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Spawn                                                               */
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

bool terminal_spawn(TerminalApp *app) {
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

void terminal_write(TerminalApp *app, const char *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(app->pty_master, buf + off, len - off);
        if (n > 0) { off += (size_t)n; continue; }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { SDL_Delay(1); continue; }
        break;
    }
}
