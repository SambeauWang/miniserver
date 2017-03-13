#include <stdint.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <sys/queue.h>
#include <sys/epoll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#ifdef USE_LOG
#include "log.h"
#else
#define LOG_DBG(x)
#define log_error(x)	perror(x)
#endif

#include "event.h"
// #include "evsignal.h"

// extern struct event_list eventqueue;
// struct event_list eventqueue;

// extern volatile sig_atomic_t evsignal_caught;

/* due to limitations in the epoll interface, we need to keep track of
 * all file descriptors outself.
 */

extern struct event_list timequeue;
extern struct event_list eventqueue;
extern struct event_list addqueue;

struct evepoll {
	struct event *evread;
	struct event *evwrite;
};

struct epollop {
	struct evepoll *fds;
	int nfds;
	struct epoll_event *events;
	int nevents;
	int epfd;
	sigset_t evsigmask;
} epollop;

void *epoll_init	__P((void));
int epoll_add		__P((void *, struct event *));
int epoll_del		__P((void *, struct event *));
int epoll_recalc	__P((void *, int));
int epoll_dispatch	__P((void *, struct timeval *));

struct eventop epollops = {
	"epoll",
	epoll_init,
	epoll_add,
	epoll_del,
	epoll_recalc,
	epoll_dispatch
};

#define NEVENT	32000

void *
epoll_init(void)
{
	int epfd, nfiles = NEVENT;
	struct rlimit rl;

	/* Disable epollueue when this environment variable is set */
	if (getenv("EVENT_NOEPOLL"))
		return (NULL);

	memset(&epollop, 0, sizeof(epollop));

	if (getrlimit(RLIMIT_NOFILE, &rl) == 0 &&
	    rl.rlim_cur != RLIM_INFINITY)
		nfiles = rl.rlim_cur;

	/* Initalize the kernel queue */

	if ((epfd = epoll_create(nfiles)) == -1) {
		log_error("epoll_create");
		return (NULL);
	}

	epollop.epfd = epfd;

	/* Initalize fields */
	epollop.events = malloc(nfiles * sizeof(struct epoll_event));
	if (epollop.events == NULL)
		return (NULL);
	epollop.nevents = nfiles;

	epollop.fds = calloc(nfiles, sizeof(struct evepoll));
	if (epollop.fds == NULL) {
		free(epollop.events);
		return (NULL);
	}
	epollop.nfds = nfiles;

	// evsignal_init(&epollop.evsigmask);

	return (&epollop);
}

int
epoll_recalc(void *arg, int max)
{
	struct epollop *epollop = arg;

	if (max > epollop->nfds) {
		struct evepoll *fds;
		int nfds;

		nfds = epollop->nfds;
		while (nfds < max)
			nfds <<= 1;

		fds = realloc(epollop->fds, nfds * sizeof(struct evepoll));
		if (fds == NULL) {
			log_error("realloc");
			return (-1);
		}
		epollop->fds = fds;
		memset(fds + epollop->nfds, 0,
		    (nfds - epollop->nfds) * sizeof(struct evepoll));
		epollop->nfds = nfds;
	}

	return (0);// (evsignal_recalc(&epollop->evsigmask));
}

int
epoll_dispatch(void *arg, struct timeval *tv)
{
	struct epollop *epollop = arg;
	struct epoll_event *events = epollop->events;
	struct evepoll *evep;
	struct event *ev;
	int i, res, timeout;

	// if (evsignal_deliver(&epollop->evsigmask) == -1)
	// 	return (-1);

	timeout = tv->tv_sec * 1000 + tv->tv_usec / 1000;
	res = epoll_wait(epollop->epfd, events, epollop->nevents, timeout);

	// if (evsignal_recalc(&epollop->evsigmask) == -1)
	// 	return (-1);

	if (res == -1) {
		if (errno != EINTR) {
			log_error("epoll_wait");
			return (-1);
		}

		// evsignal_process();
		return (0);
	} //else if (evsignal_caught)
		// evsignal_process();

	// LOG_DBG((LOG_MISC, 80, "%s: epoll_wait reports %d", __func__, res));

	for (i = 0; i < res; i++) {
		int which = 0, what;

		evep = (struct evepoll *)events[i].data.ptr;
		what = events[i].events;
		if (what & EPOLLIN) {
			which |= EV_READ;
		}

		if (what & EPOLLOUT) {
			which |= EV_WRITE;
		}

		if (!which)
			continue;

		if (which & EV_READ) {
			ev = evep->evread;
			if (!(ev->ev_events & EV_PERSIST))
				event_del(ev);
			// event_active(ev, EV_READ, 1);
			(*ev->ev_callback)(ev->ev_fd, res, ev->ev_arg);
		}
		if (which & EV_WRITE) {
			ev = evep->evwrite;
			if (!(ev->ev_events & EV_PERSIST))
				event_del(ev);
			// event_active(ev, EV_WRITE, 1);
			(*ev->ev_callback)(ev->ev_fd, res, ev->ev_arg);
		}
	}

	return (0);
}


int
epoll_add(void *arg, struct event *ev)
{
	struct epollop *epollop = arg;
	struct epoll_event epev;
	struct evepoll *evep;
	int fd, op, events;

	// if (ev->ev_events & EV_SIGNAL)
	// 	return (evsignal_add(&epollop->evsigmask, ev));

	fd = ev->ev_fd;
	if (fd >= epollop->nfds) {
		/* Extent the file descriptor array as necessary */
		if (epoll_recalc(epollop, fd) == -1)
			return (-1);
	}
	evep = &epollop->fds[fd];
	op = EPOLL_CTL_ADD;
	events = 0;
	if (evep->evread != NULL) {
		events |= EPOLLIN;
		op = EPOLL_CTL_MOD;
	}
	if (evep->evwrite != NULL) {
		events |= EPOLLOUT;
		op = EPOLL_CTL_MOD;
	}

	if (ev->ev_events & EV_READ)
		events |= EPOLLIN;
	if (ev->ev_events & EV_WRITE)
		events |= EPOLLOUT;

	epev.data.ptr = evep;
	epev.events = events;
	if (epoll_ctl(epollop->epfd, op, ev->ev_fd, &epev) == -1)
			return (-1);

	/* Update events responsible */
	if (ev->ev_events & EV_READ)
		evep->evread = ev;
	if (ev->ev_events & EV_WRITE)
		evep->evwrite = ev;

	return (0);
}

int
epoll_del(void *arg, struct event *ev)
{
	struct epollop *epollop = arg;
	struct epoll_event epev;
	struct evepoll *evep;
	int fd, events, op;
	int needwritedelete = 1, needreaddelete = 1;

	// if (ev->ev_events & EV_SIGNAL)
	// 	return (evsignal_del(&epollop->evsigmask, ev));

	fd = ev->ev_fd;
	if (fd >= epollop->nfds)
		return (0);
	evep = &epollop->fds[fd];

	op = EPOLL_CTL_DEL;
	events = 0;

	if (ev->ev_events & EV_READ)
		events |= EPOLLIN;
	if (ev->ev_events & EV_WRITE)
		events |= EPOLLOUT;

	if ((events & (EPOLLIN|EPOLLOUT)) != (EPOLLIN|EPOLLOUT)) {
		if ((events & EPOLLIN) && evep->evwrite != NULL) {
			needwritedelete = 0;
			events = EPOLLOUT;
			op = EPOLL_CTL_MOD;
		} else if ((events & EPOLLOUT) && evep->evread != NULL) {
			needreaddelete = 0;
			events = EPOLLIN;
			op = EPOLL_CTL_MOD;
		}
	}

	epev.events = events;
	epev.data.ptr = evep;

	if (epoll_ctl(epollop->epfd, op, fd, &epev) == -1)
		return (-1);

	if (needreaddelete)
		evep->evread = NULL;
	if (needwritedelete)
		evep->evwrite = NULL;

	return (0);
}
