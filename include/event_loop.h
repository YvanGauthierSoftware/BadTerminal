#ifndef BADTERM_EVENT_LOOP_H
#define BADTERM_EVENT_LOOP_H

typedef struct event_loop event_loop_t;

typedef void (*event_callback)(int fd, unsigned events, void *userdata);

enum {
    EVENT_READ = 1u << 0,
    EVENT_WRITE = 1u << 1,
    EVENT_ERROR = 1u << 2,
    EVENT_HANGUP = 1u << 3
};

event_loop_t *event_loop_create(void);
void event_loop_destroy(event_loop_t *loop);
int event_loop_add(event_loop_t *loop, int fd, unsigned events,
                   event_callback callback, void *userdata);
int event_loop_remove(event_loop_t *loop, int fd);
int event_loop_dispatch(event_loop_t *loop, int timeout_ms);

#endif