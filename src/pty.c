#include "pty.h"

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <util.h>
#elif defined(__linux__)
#include <pty.h>
#endif

int pty_spawn(pty_process_t *process, char *const command[])
{
    struct winsize size = {.ws_row = 24, .ws_col = 80};
    int master_fd;
    pid_t child_pid = forkpty(&master_fd, NULL, NULL, &size);
    if (child_pid == -1) return -1;
    if (child_pid == 0) {
        const char *shell = getenv("SHELL");
        if (command != NULL) {
            execvp(command[0], command);
        } else if (shell != NULL && shell[0] != '\0') {
            execl(shell, shell, "-l", (char *)NULL);
        } else {
            execl("/bin/sh", "sh", (char *)NULL);
        }
        _exit(127);
    }
    process->master_fd = master_fd;
    process->child_pid = child_pid;
    return 0;
}

int pty_resize(int master_fd, unsigned rows, unsigned columns)
{
    struct winsize size = {.ws_row = rows, .ws_col = columns};
    return ioctl(master_fd, TIOCSWINSZ, &size);
}

int pty_child_status(pid_t child_pid, int *status)
{
    pid_t result = waitpid(child_pid, status, WNOHANG);
    if (result == child_pid) return 1;
    if (result == -1 && errno != EINTR) return -1;
    return 0;
}