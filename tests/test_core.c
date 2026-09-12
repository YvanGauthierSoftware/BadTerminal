#include "event_loop.h"
#include "pty.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

typedef struct {
    int called;
    char value;
} pipe_context_t;

static void pipe_ready(int fd, unsigned events, void *userdata)
{
    pipe_context_t *context = userdata;
    if (!(events & EVENT_READ)) return;
    if (read(fd, &context->value, 1) == 1) context->called = 1;
}

static void test_event_loop(void)
{
    int pipe_fds[2];
    CHECK(pipe(pipe_fds) == 0);
    if (failures != 0) return;

    event_loop_t *loop = event_loop_create();
    pipe_context_t context = {0};
    CHECK(loop != NULL);
    CHECK(event_loop_add(loop, pipe_fds[0], EVENT_READ, pipe_ready, &context) == 0);
    CHECK(write(pipe_fds[1], "E", 1) == 1);
    CHECK(event_loop_dispatch(loop, 1000) == 0);
    CHECK(context.called == 1);
    CHECK(context.value == 'E');
    CHECK(event_loop_remove(loop, pipe_fds[0]) == 0);

    event_loop_destroy(loop);
    close(pipe_fds[0]);
    close(pipe_fds[1]);
}

static void test_pty(void)
{
    pty_process_t process = {.master_fd = -1};
    char *command[] = {"/bin/sh", "-c", "printf PTY_TEST; exit 23", NULL};
    CHECK(pty_spawn(&process, command) == 0);
    if (failures != 0) return;

    CHECK(pty_resize(process.master_fd, 42, 133) == 0);
    struct winsize size = {0};
    CHECK(ioctl(process.master_fd, TIOCGWINSZ, &size) == 0);
    CHECK(size.ws_row == 42);
    CHECK(size.ws_col == 133);

    char output[64] = {0};
    ssize_t length = read(process.master_fd, output, sizeof(output) - 1);
    CHECK(length > 0);
    CHECK(strstr(output, "PTY_TEST") != NULL);

    int status = 0;
    CHECK(waitpid(process.child_pid, &status, 0) == process.child_pid);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 23);
    close(process.master_fd);
}

int main(void)
{
    test_event_loop();
    test_pty();
    if (failures != 0) {
        fprintf(stderr, "%d test assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("core tests: ok");
    return EXIT_SUCCESS;
}