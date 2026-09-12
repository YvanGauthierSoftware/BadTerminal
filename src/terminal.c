#if defined(__linux__) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "terminal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

int terminal_raw_enable(terminal_state_t *terminal, int fd)
{
    struct termios *saved = malloc(sizeof(*saved));
    if (saved == NULL || tcgetattr(fd, saved) == -1) {
        free(saved);
        return -1;
    }
    struct termios raw = *saved;
    cfmakeraw(&raw);
    raw.c_oflag |= OPOST;
    if (tcsetattr(fd, TCSAFLUSH, &raw) == -1) {
        free(saved);
        return -1;
    }
    terminal->fd = fd;
    terminal->saved = saved;
    terminal->active = 1;
    return 0;
}

void terminal_restore(terminal_state_t *terminal)
{
    if (terminal == NULL || !terminal->active) return;
    tcsetattr(terminal->fd, TCSAFLUSH, terminal->saved);
    free(terminal->saved);
    terminal->saved = NULL;
    terminal->active = 0;
}

int terminal_size(int fd, unsigned *rows, unsigned *columns)
{
    struct winsize size;
    if (ioctl(fd, TIOCGWINSZ, &size) == -1) return -1;
    *rows = size.ws_row == 0 ? 24 : size.ws_row;
    *columns = size.ws_col == 0 ? 80 : size.ws_col;
    return 0;
}