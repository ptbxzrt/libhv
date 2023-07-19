#ifndef HV_LIBEVENT_COMPAT_H_
#define HV_LIBEVENT_COMPAT_H

#include "hexport.h"
#include "hloop.h"
#include "hbase.h"

#define EV_READ HV_READ
#define EV_WRITE HV_WRITE
#define EV_PERSIST 0x0010
#define EV_TIMEOUT 0x0020

typedef int evutil_socket_t;
typedef void (*event_callback_fn)(evutil_socket_t fd, short events, void *callback_arg);

typedef struct event {
    struct event_base *base;

    hio_t *io;
    int fd;
    short events;

    event_callback_fn callback;
    void *callback_arg;

    htimer_t *timer;
    int timeout;
} event;

typedef struct event_base {
    hloop_t *loop;
    htimer_t *timer;
    int timeout;
} event_base;

HV_INLINE struct event_base *event_base_new(void) {
    struct event_base *base = NULL;
    HV_ALLOC(base, sizeof(struct event_base));
    base->loop = hloop_new(HLOOP_FLAG_QUIT_WHEN_NO_ACTIVE_EVENTS);
    base->timer = NULL;
    return base;
}

HV_INLINE void event_base_free(struct event_base *base) {
    if (base->timer != NULL) {
        htimer_del(base->timer);
    }
    if (base->loop != NULL) {
        hloop_free(&(base->loop));
    }
    HV_FREE(base);
}

/**
 * Start the event loop, quit when
 *     something calls event_base_loopbreak() or event_base_loopexit(), or
 *     there's no active/pending event.
 *
 * @param base the event_base
 * @param flags not supported yet
 * @return 0 if successful, -1 if base is NULL, or -2 if base has already started
 *
 * NOTE: for event_base_loop() in libevent-2.1.12 return
 *        0 if successful,
 *       -1 if an error occurred,
 *        1 if we exited because no events were pending or active.
 */
HV_INLINE int event_base_loop(struct event_base *base, int flags) {
    return hloop_run(base->loop);
}

/**
 * Start the event loop, quit when
 *     something calls event_base_loopbreak() or event_base_loopexit(), or
 *     there's no active/pending event.
 *
 * @param base the event_base
 * @return 0 if successful, -1 if base is NULL, or -2 if base has already started
 *
 * NOTE: for event_base_dispatch() in libevent-2.1.12 return
 *        0 if successful,
 *       -1 if an error occurred,
 *        1 if we exited because no events were pending or active.
 */
HV_INLINE int event_base_dispatch(struct event_base *base) {
    return event_base_loop(base, 0);
}

/**
 * Quit the event loop.
 *
 * @param base the event_base
 * @return always return 0
 *
 * NOTE: works and returns slightly different from event_base_loopbreak() in libevent-2.1.12
 */
HV_INLINE int event_base_loopbreak(struct event_base *base) {
    return hloop_stop(base->loop);
}

HV_INLINE int timeval_to_ms(struct timeval *tv) {
    return (tv->tv_sec * 1000) + (tv->tv_usec / 1000);
}

HV_INLINE void on_loopexit_timeout(htimer_t *timer) {
    hloop_stop(hevent_loop(timer));
}

HV_INLINE void on_loopexit_directly(hevent_t *hevent) {
    hloop_stop(hevent_loop(hevent));
}

HV_INLINE int event_base_loopexit(struct event_base *base, const struct timeval *tv) {
    if (tv != NULL) {
        int timeout = timeval_to_ms(tv);
        base->timer = htimer_add(base->loop, on_loopexit_timeout, timeout, INFINITE);
        if (base->timer == NULL) {
            return -1;
        }
        base->timeout = timeout;
    }
    else {
        hevent_t hev;
        memset(&hev, 0, sizeof(hev));
        hev.cb = on_loopexit_directly;
        hloop_post_event(base->loop, &hev);
    }
    return 0;
}

HV_INLINE void on_readable(hio_t *io) {
    struct event *ev = (struct event *)hevent_userdata(io);
    int fd = hio_fd(io);
    short events = ev->events;
    event_callback_fn callback = ev->callback;
    void *callback_arg = ev->callback_arg;
    if (callback) {
        callback(fd, EV_READ, callback_arg);
    }
    if (!(events & EV_PERSIST)) {
        hio_del(io, EV_READ);
    }

    if (ev->timer != NULL) {
        htimer_reset(ev->timer, ev->timeout);
    }
    if (ev->base->timer != NULL) {
        htimer_reset(ev->base->timer, ev->base->timeout);
    }
}

HV_INLINE void on_writable(hio_t *io) {
    struct event *ev = (struct event *)hevent_userdata(io);
    int fd = hio_fd(io);
    short events = ev->events;
    event_callback_fn callback = ev->callback;
    void *callback_arg = ev->callback_arg;
    if (callback) {
        callback(fd, EV_WRITE, callback_arg);
    }
    if (!(events & EV_PERSIST)) {
        hio_del(io, EV_WRITE);
    }

    if (ev->timer != NULL) {
        htimer_reset(ev->timer, ev->timeout);
    }
    if (ev->base->timer != NULL) {
        htimer_reset(ev->base->timer, ev->base->timeout);
    }
}

HV_INLINE void on_timeout(htimer_t *timer) {
    struct event *ev = (struct event *)hevent_userdata(timer);
    short events = ev->events;
    event_callback_fn callback = ev->callback;
    void *callback_arg = ev->callback_arg;
    if (callback) {
        callback(ev->fd, EV_TIMEOUT, callback_arg);
    }
    if (!(events & EV_PERSIST)) {
        event_del(ev);
    }

    if (ev->timer != NULL) {
        htimer_reset(ev->timer, ev->timeout);
    }
    if (ev->base->timer != NULL) {
        htimer_reset(ev->base->timer, ev->base->timeout);
    }
}

HV_INLINE void on_active(hevent_t *hev) {
    struct event *ev = (struct event *)hevent_userdata(hev);
    int active_events = (int)hev->privdata;
    event_callback_fn callback = ev->callback;
    void *callback_arg = ev->callback_arg;
    if (callback) {
        callback(ev->fd, active_events, callback_arg);
    }

    if (ev->base->timer != NULL) {
        htimer_reset(ev->base->timer, ev->base->timeout);
    }
}

int event_assign(struct event *ev, struct event_base *base, evutil_socket_t fd, short events, event_callback_fn callback, void *callback_arg) {
    if (ev == NULL) {
        return -1;
    }
    ev->io = NULL;
    ev->timer = NULL;
    ev->base = base;
    ev->fd = fd;
    ev->events = events;
    ev->callback = callback;
    ev->callback_arg = callback_arg;
    return 0;
}

HV_INLINE struct event *event_new(struct event_base *base, evutil_socket_t fd, short events, event_callback_fn callback, void *callback_arg) {
    struct event *ev = NULL;
    HV_ALLOC(ev, sizeof(struct event));
    ev->io = NULL;
    ev->timer = NULL;
    ev->base = base;
    ev->fd = fd;
    ev->events = events;
    ev->callback = callback;
    ev->callback_arg = callback_arg;
    return ev;
}

int event_add(struct event *ev, const struct timeval *tv) {
    int fd = ev->fd;
    struct event_base *base = ev->base;
    short events = ev->events;
    if (fd >= 0) {
        ev->io = hio_get(base->loop, fd);
        hevent_set_userdata(ev->io, ev);
        if (events & EV_READ) {
            hio_add(ev->io, on_readable, HV_READ);
        }
        if (events & EV_WRITE) {
            hio_add(ev->io, on_writable, EV_WRITE);
        }
    }
    if (tv != NULL) {
        ev->timeout = timeval_to_ms(tv);
        ev->timer = htimer_add(base->loop, on_timeout, ev->timeout, INFINITE);
        hevent_set_userdata(ev->timer, ev);
    }
    return 0;
}

void event_active(struct event *ev, int res, short ncalls) {
    hidle_add(ev->base->loop, NULL, 1);

    hevent_t hev;
    memset(&hev, 0, sizeof(hev));
    hev.cb = on_active;
    hev.userdata = ev;
    hev.privdata = (void *)res;
    hloop_post_event(ev->base->loop, &hev);
}

int event_del(struct event *ev) {
    if (ev->io != NULL) {
        short hv_events = hio_events(ev->io);
        if (hv_events & EV_READ) {
            hio_del(ev->io, EV_READ);
        }
        if (hv_events & EV_WRITE) {
            hio_del(ev->io, EV_WRITE);
        }
    }
    if (ev->timer != NULL) {
        htimer_del(ev->timer);
    }
    return 0;
}

HV_INLINE void event_free(struct event *ev) {
    event_del(ev);
    if (ev->io != NULL) {
        hio_close(ev->io);
    }
    HV_FREE(ev);
}

HV_INLINE evutil_socket_t event_get_fd(const struct event *base) {
    return hio_fd(base);
}

HV_INLINE int event_pending(const struct event *base, short events, struct timeval *tv) {
    int added_events = hio_events(base);
    if (added_events & events) return 1;
    return 0;
}

HV_INLINE int event_initialized(const struct event *base) {
    return 1;
}

#endif