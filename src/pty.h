#ifndef BADTERM_PTY_H
#define BADTERM_PTY_H

#include <sys/types.h>

typedef struct {
    int master_fd;
    pid_t child_pid;
} pty_process_t;

int pty_spawn(pty_process_t *process, char *const command[], unsigned rows, unsigned columns);
int pty_resize(int master_fd, unsigned rows, unsigned columns);
int pty_child_status(pid_t child_pid, int *status);

#endif