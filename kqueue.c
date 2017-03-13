#include <sys/types.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/event.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#ifdef USE_LOG
#include "log.h"
#else
#define LOG_DBG(x)
#define log_error(x)	perror(x)
#endif

#include "event.h"

extern struct event_list timequeue;
extern struct event_list eventqueue;
extern struct event_list addqueue;

#define NEVENT		64

struct kqop {
	struct kevent *changes;
	int nchanges;
	struct kevent *events;
	int nevents;
	int kq;
} kqop;

void *kq_init	__P((void));
int kq_add	__P((void *, struct event *));
int kq_del	__P((void *, struct event *));
int kq_recalc	__P((void *, int));
int kq_dispatch	__P((void *, struct timeval *));

struct eventop kqops = {
	"kqueue",
	kq_init,
	kq_add,
	kq_del,
	kq_recalc,
	kq_dispatch
};

void *
kq_init(void)
{
	int kq;

	memset(&kqop, 0, sizeof(kqop));

	/* Initalize the kernel queue */
	
	if ((kq = kqueue()) == -1) {
		log_error("kqueue");
		return (NULL);
	}

	kqop.kq = kq;

	/* Initalize fields */
	kqop.changes = malloc(NEVENT * sizeof(struct kevent));
	if (kqop.changes == NULL)
		return (NULL);
	kqop.events = malloc(NEVENT * sizeof(struct kevent));
	if (kqop.events == NULL) {
		free (kqop.changes);
		return (NULL);
	}
	kqop.nevents = NEVENT;

	return (&kqop);
}

int
kq_recalc(void *arg, int max)
{
	return (0);
}

int
kq_insert(struct kqop *kqop, struct kevent *kev)
{
	int nevents = kqop->nevents;

	if (kqop->nchanges == nevents) {
		struct kevent *newchange;
		struct kevent *newresult;

		nevents *= 2;

		newchange = realloc(kqop->changes,
				    nevents * sizeof(struct kevent));
		if (newchange == NULL) {
			log_error(__FUNCTION__": malloc");
			return (-1);
		}
		kqop->changes = newchange;

		newresult = realloc(kqop->changes,
				    nevents * sizeof(struct kevent));

		/*
		 * If we fail, we don't have to worry about freeing,
		 * the next realloc will pick it up.
		 */
		if (newresult == NULL) {
			log_error(__FUNCTION__": malloc");
			return (-1);
		}
		kqop->events = newchange;

		kqop->nevents = nevents;
	}

	memcpy(&kqop->changes[kqop->nchanges++], kev, sizeof(struct kevent));

	return (0);
}

int
kq_dispatch(void *arg, struct timeval *tv)
{
	struct kqop *kqop = arg;
	struct kevent *changes = kqop->changes;
	struct kevent *events = kqop->events;
	struct event *ev;
	struct timespec ts;
	int i, res;

	TIMEVAL_TO_TIMESPEC(tv, &ts);

	res = kevent(kqop->kq, changes, kqop->nchanges,
		     events, kqop->nevents, &ts);
	kqop->nchanges = 0;
	if (res == -1) {
		log_error("kevent");
		return (-1);
	}

	LOG_DBG((LOG_MISC, 80, __FUNCTION__": kevent reports %d", res));

	for (i = 0; i < res; i++) {
		int which = 0;

		if (events[i].flags & EV_ERROR) {
			/* 
			 * Error messages that can happen, when a delete
			 * fails.  EBADF happens when the file discriptor
			 * has been closed, ENOENT when the file discriptor
			 * was closed and then reopened.
			 */
			if (events[i].data == EBADF ||
			    events[i].data == ENOENT)
				continue;
			return (-1);
		}

		ev = events[i].udata;

		if (events[i].filter == EVFILT_READ) {
			which |= EV_READ;
		} else if (events[i].filter == EVFILT_WRITE) {
			which |= EV_WRITE;
		}

		if (which) {
			event_del(ev);
			(*ev->ev_callback)(ev->ev_fd, which, ev->ev_arg);
		}
	}

	return (0);
}

/*
 * Nothing to be done here.
 */

int
kq_add(void *arg, struct event *ev)
{
	struct kqop *kqop = arg;
	struct kevent kev;

	if (ev->ev_events & EV_READ) {
 		memset(&kev, 0, sizeof(kev));
		kev.ident = ev->ev_fd;
		kev.filter = EVFILT_READ;
		kev.flags = EV_ADD;
		kev.udata = ev;
		
		if (kq_insert(kqop, &kev) == -1)
			return (-1);
	}

	if (ev->ev_events & EV_WRITE) {
 		memset(&kev, 0, sizeof(kev));
		kev.ident = ev->ev_fd;
		kev.filter = EVFILT_WRITE;
		kev.flags = EV_ADD;
		kev.udata = ev;
		
		if (kq_insert(kqop, &kev) == -1)
			return (-1);
	}

	return (0);
}

int
kq_del(void *arg, struct event *ev)
{
	struct kqop *kqop = arg;
	struct kevent kev;

	if (ev->ev_events & EV_READ) {
 		memset(&kev, 0, sizeof(kev));
		kev.ident = ev->ev_fd;
		kev.filter = EVFILT_READ;
		kev.flags = EV_DELETE;
		
		if (kq_insert(kqop, &kev) == -1)
			return (-1);
	}

	if (ev->ev_events & EV_WRITE) {
 		memset(&kev, 0, sizeof(kev));
		kev.ident = ev->ev_fd;
		kev.filter = EVFILT_WRITE;
		kev.flags = EV_DELETE;
		
		if (kq_insert(kqop, &kev) == -1)
			return (-1);
	}

	return (0);
}
