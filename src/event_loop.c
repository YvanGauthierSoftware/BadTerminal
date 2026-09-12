#include "event_loop.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/epoll.h>
#define EVENT_BACKEND_EPOLL 1
#elif defined(__APPLE__)
#include <sys/event.h>
#define EVENT_BACKEND_KQUEUE 1
#else
#error "badterm supports Linux and macOS"
#endif

typedef struct event_watch {
    int fd;
    unsigned events;
    event_callback callback;
    void *userdata;
} event_watch_t;

struct event_loop {
    int backend_fd;
    event_watch_t *watches;
    size_t count;
    size_t capacity;
};

static event_watch_t *find_watch(event_loop_t *loop, int fd)
{
    for (size_t index = 0; index < loop->count; index++) {
        if (loop->watches[index].fd == fd) {
            return &loop->watches[index];
        }
    }
    return NULL;
}

event_loop_t *event_loop_create(void)
{
    event_loop_t *loop = calloc(1, sizeof(*loop));
    if (loop == NULL) {
        return NULL;
    }

#if EVENT_BACKEND_EPOLL
    loop->backend_fd = epoll_create1(EPOLL_CLOEXEC);
#else
    loop->backend_fd = kqueue();
#endif
    if (loop->backend_fd == -1) {
        free(loop);
        return NULL;
    }
    return loop;
}

void event_loop_destroy(event_loop_t *loop)
{
    if (loop == NULL) {
        return;
    }
    close(loop->backend_fd);
    free(loop->watches);
    free(loop);
}

static int update_backend(event_loop_t *loop, int fd, unsigned events, int add)
{
#if EVENT_BACKEND_EPOLL
    struct epoll_event event = {0};
    event.data.fd = fd;
    if (events & EVENT_READ) event.events |= EPOLLIN;
    if (events & EVENT_WRITE) event.events |= EPOLLOUT;
    if (add) {
        return epoll_ctl(loop->backend_fd, EPOLL_CTL_ADD, fd, &event);
    }
    return epoll_ctl(loop->backend_fd, EPOLL_CTL_DEL, fd, NULL);
#else
    struct kevent changes[2];
    int count = 0;
    if (events & EVENT_READ) {
        EV_SET(&changes[count++], fd, EVFILT_READ, add ? EV_ADD : EV_DELETE, 0, 0, NULL);
    } else {
        EV_SET(&changes[count++], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    }
    if (events & EVENT_WRITE) {
        EV_SET(&changes[count++], fd, EVFILT_WRITE, add ? EV_ADD : EV_DELETE, 0, 0, NULL);
    } else {
        EV_SET(&changes[count++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    }
    if (kevent(loop->backend_fd, changes, count, NULL, 0, NULL) == -1) {
        return errno == ENOENT ? 0 : -1;
    }
    return 0;
#endif
}

int event_loop_add(event_loop_t *loop, int fd, unsigned events,
                   event_callback callback, void *userdata)
{
    if (find_watch(loop, fd) != NULL) {
        errno = EEXIST;
        return -1;
    }
    if (loop->count == loop->capacity) {
        size_t capacity = loop->capacity == 0 ? 4 : loop->capacity * 2;
        event_watch_t *watches = realloc(loop->watches, capacity * sizeof(*watches));
        if (watches == NULL) return -1;
        loop->watches = watches;
        loop->capacity = capacity;
    }
    if (update_backend(loop, fd, events, 1) == -1) return -1;
    loop->watches[loop->count++] = (event_watch_t){fd, events, callback, userdata};
    return 0;
}

int event_loop_remove(event_loop_t *loop, int fd)
{
    event_watch_t *watch = find_watch(loop, fd);
    if (watch == NULL) return 0;
    if (update_backend(loop, fd, 0, 0) == -1 && errno != ENOENT) return -1;
    *watch = loop->watches[--loop->count];
    return 0;
}

int event_loop_dispatch(event_loop_t *loop, int timeout_ms)
{
#if EVENT_BACKEND_EPOLL
    struct epoll_event events[16];
    int count = epoll_wait(loop->backend_fd, events, 16, timeout_ms);
    if (count == -1) return errno == EINTR ? 0 : -1;
    for (int index = 0; index < count; index++) {
        event_watch_t *watch = find_watch(loop, events[index].data.fd);
        if (watch == NULL) continue;
        unsigned flags = 0;
        if (events[index].events & (EPOLLIN | EPOLLPRI)) flags |= EVENT_READ;
        if (events[index].events & EPOLLOUT) flags |= EVENT_WRITE;
        if (events[index].events & EPOLLERR) flags |= EVENT_ERROR;
        if (events[index].events & EPOLLHUP) flags |= EVENT_HANGUP;
        watch->callback(watch->fd, flags, watch->userdata);
    }
#else
    struct kevent events[16];
    struct timespec timeout = {timeout_ms / 1000, (timeout_ms % 1000) * 1000000L};
    int count = kevent(loop->backend_fd, NULL, 0, events, 16, &timeout);
    if (count == -1) return errno == EINTR ? 0 : -1;
    for (int index = 0; index < count; index++) {
        event_watch_t *watch = find_watch(loop, (int)events[index].ident);
        if (watch == NULL) continue;
        unsigned flags = events[index].filter == EVFILT_READ ? EVENT_READ : EVENT_WRITE;
        if (events[index].flags & EV_EOF) flags |= EVENT_HANGUP;
        if (events[index].flags & EV_ERROR) flags |= EVENT_ERROR;
        watch->callback(watch->fd, flags, watch->userdata);
    }
#endif
    return 0;
}