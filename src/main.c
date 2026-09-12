#include "event_loop.h"
#include "pty.h"
#include "terminal.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    event_loop_t *loop;
    pty_process_t pty;
    terminal_state_t terminal;
    int running;
    int child_reaped;
} app_t;

static volatile sig_atomic_t window_changed;

static void on_window_change(int signal_number)
{
    (void)signal_number;
    window_changed = 1;
}

static void write_all(int fd, const char *buffer, size_t length)
{
    while (length > 0) {
        ssize_t written = write(fd, buffer, length);
        if (written > 0) {
            buffer += written;
            length -= (size_t)written;
        } else if (written == -1 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
}

static void stdin_ready(int fd, unsigned events, void *userdata)
{
    app_t *app = userdata;
    if (!(events & EVENT_READ)) return;
    char buffer[8192];
    ssize_t length = read(fd, buffer, sizeof(buffer));
    if (length > 0) write_all(app->pty.master_fd, buffer, (size_t)length);
    if (length == 0) app->running = 0;
}

static void pty_ready(int fd, unsigned events, void *userdata)
{
    app_t *app = userdata;
    char buffer[8192];
    if (events & (EVENT_READ | EVENT_HANGUP)) {
        ssize_t length = read(fd, buffer, sizeof(buffer));
        if (length > 0) write_all(STDOUT_FILENO, buffer, (size_t)length);
        if (length == 0) app->running = 0;
    }
    if (events & EVENT_ERROR) app->running = 0;
}

static int synchronize_window(app_t *app)
{
    unsigned rows, columns;
    if (!window_changed || terminal_size(STDIN_FILENO, &rows, &columns) == -1) return 0;
    window_changed = 0;
    return pty_resize(app->pty.master_fd, rows, columns);
}

static int exit_code(int status)
{
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}

int main(int argc, char **argv)
{
    app_t app = {.pty = {.master_fd = -1}};
    char *const *command = argc > 1 ? &argv[1] : NULL;
    int status = 1;
    unsigned rows, columns;

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr, "badterm: stdin and stdout must be terminals\n");
        return 2;
    }
    if (terminal_raw_enable(&app.terminal, STDIN_FILENO) == -1) {
        perror("badterm: terminal_raw_enable");
        return 1;
    }
    if (terminal_size(STDIN_FILENO, &rows, &columns) == -1 ||
        pty_spawn(&app.pty, command) == -1) {
        perror("badterm: pty_spawn");
        terminal_restore(&app.terminal);
        return 1;
    }
    signal(SIGWINCH, on_window_change);
    app.loop = event_loop_create();
    if (app.loop == NULL || event_loop_add(app.loop, STDIN_FILENO, EVENT_READ, stdin_ready, &app) == -1 ||
        event_loop_add(app.loop, app.pty.master_fd, EVENT_READ, pty_ready, &app) == -1) {
        perror("badterm: event loop");
        app.running = 0;
        if (app.pty.child_pid > 0) kill(app.pty.child_pid, SIGTERM);
    } else {
        app.running = 1;
        window_changed = 1;
        while (app.running) {
            if (synchronize_window(&app) == -1 && errno != EINTR) app.running = 0;
            if (event_loop_dispatch(app.loop, 250) == -1) app.running = 0;
            int child_status = pty_child_status(app.pty.child_pid, &status);
            if (child_status == 1) {
                app.child_reaped = 1;
                app.running = 0;
            }
        }
    }

    terminal_restore(&app.terminal);
    if (app.loop != NULL) event_loop_destroy(app.loop);
    if (app.pty.master_fd >= 0) close(app.pty.master_fd);
    if (app.pty.child_pid > 0) {
        if (!app.child_reaped) {
            waitpid(app.pty.child_pid, &status, 0);
        }
    }
    return WIFEXITED(status) || WIFSIGNALED(status) ? exit_code(status) : 1;
}