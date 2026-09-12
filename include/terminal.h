#ifndef BADTERM_TERMINAL_H
#define BADTERM_TERMINAL_H

typedef struct {
    int fd;
    int active;
    struct termios *saved;
} terminal_state_t;

int terminal_raw_enable(terminal_state_t *terminal, int fd);
void terminal_restore(terminal_state_t *terminal);
int terminal_size(int fd, unsigned *rows, unsigned *columns);

#endif