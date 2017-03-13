/*
 * Copyright 2000 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#ifdef USE_LOG
#include "log.h"
#else
#define LOG_DBG(x)
#define log_error(x)	perror(x)
#endif

#include "event.h"

#ifdef HAVE_SELECT
extern struct eventop selectops;
#endif
#ifdef HAVE_KQUEUE
extern struct eventop kqops;
#endif
#ifdef HAVE_EPOLL
extern struct eventop epollops;
#endif

/* In order of preference */
struct eventop *eventops[] = {
#ifdef HAVE_KQUEUE
	&kqops,
#endif
#ifdef HAVE_SELECT
	&selectops,
#endif
#ifdef HAVE_EPOLL
	&epollops,
#endif
	NULL
};

struct eventop *evsel;
void *evbase;

/* Prototypes */
void event_add_post(struct event *);

struct event_list timequeue;
struct event_list eventqueue;
struct event_list addqueue;

int event_inloop = 0;

void
event_init(void)
{
	int i;
	
	TAILQ_INIT(&timequeue);
	TAILQ_INIT(&eventqueue);
	TAILQ_INIT(&addqueue);
	
	evbase = NULL;
	for (i = 0; eventops[i] && !evbase; i++) {
		evsel = eventops[i];

		evbase = evsel->init();
	}
}

int
event_dispatch(void)
{
	struct timeval tv;
	struct event *ev;
	int res, maxfd;

	/* Calculate the initial events that we are waiting for */
	if (evsel->recalc(evbase, 0) == -1)
		return (-1);

	while (1) {
		timeout_next(&tv);

		event_inloop = 1;
		res = evsel->dispatch(evbase, &tv);
		event_inloop = 0;

		if (res == -1)
			return (-1);

		maxfd = 0;
		for (ev = TAILQ_FIRST(&addqueue); ev; 
		     ev = TAILQ_FIRST(&addqueue)) {
			TAILQ_REMOVE(&addqueue, ev, ev_add_next);
			ev->ev_flags &= ~EVLIST_ADD;
			
			event_add_post(ev);

			if (ev->ev_fd > maxfd)
				maxfd = ev->ev_fd;
		}

		if (evsel->recalc(evbase, maxfd) == -1)
			return (-1);

		timeout_process();
	}

	return (0);
}

void
event_set(struct event *ev, int fd, short events,
	  void (*callback)(int, short, void *), void *arg)
{
	ev->ev_callback = callback;
	ev->ev_arg = arg;
	ev->ev_fd = fd;
	ev->ev_events = events;
	ev->ev_flags = EVLIST_INIT;
}

/*
 * Checks if a specific event is pending or scheduled.
 */

int
event_pending(struct event *ev, short event, struct timeval *tv)
{
	int flags = 0;

	/*
	 * We might not have been able to add it to the actual queue yet,
	 * check if we will enqueue later.
	 */
	if (ev->ev_flags & (EVLIST_ADD|EVLIST_INSERTED))
		flags |= (ev->ev_events & (EV_READ|EV_WRITE));
	if (ev->ev_flags & EVLIST_TIMEOUT)
		flags |= EV_TIMEOUT;

	event &= (EV_TIMEOUT|EV_READ|EV_WRITE);

	/* See if there is a timeout that we should report */
	if (tv != NULL && (flags & event & EV_TIMEOUT))
		*tv = ev->ev_timeout;

	return (flags & event);
}

void
event_add(struct event *ev, struct timeval *tv)
{
	LOG_DBG((LOG_MISC, 55,
		 "event_add: event: %p, %s%s%scall %p",
		 ev,
		 ev->ev_events & EV_READ ? "EV_READ " : " ",
		 ev->ev_events & EV_WRITE ? "EV_WRITE " : " ",
		 tv ? "EV_TIMEOUT " : " ",
		 ev->ev_callback));
	if (tv != NULL) {
		struct timeval now;
		struct event *tmp;

		gettimeofday(&now, NULL);
		timeradd(&now, tv, &ev->ev_timeout);

		LOG_DBG((LOG_MISC, 55,
			 "event_add: timeout in %d seconds, call %p",
			 tv->tv_sec, ev->ev_callback));
		if (ev->ev_flags & EVLIST_TIMEOUT)
			TAILQ_REMOVE(&timequeue, ev, ev_timeout_next);

		/* Insert in right temporal order */
		for (tmp = TAILQ_FIRST(&timequeue); tmp;
		     tmp = TAILQ_NEXT(tmp, ev_timeout_next)) {
		     if (timercmp(&ev->ev_timeout, &tmp->ev_timeout, <=))
			     break;
		}

		if (tmp)
			TAILQ_INSERT_BEFORE(tmp, ev, ev_timeout_next);
		else
			TAILQ_INSERT_TAIL(&timequeue, ev, ev_timeout_next);

		ev->ev_flags |= EVLIST_TIMEOUT;
	}

	if (event_inloop) {
		/* We are in the event loop right now, we have to
		 * postpone the change until later.
		 */
		if (ev->ev_flags & EVLIST_ADD)
			return;

		TAILQ_INSERT_TAIL(&addqueue, ev, ev_add_next);
		ev->ev_flags |= EVLIST_ADD;
	} else
		event_add_post(ev);
}

void
event_add_post(struct event *ev)
{
	if ((ev->ev_events & (EV_READ|EV_WRITE)) &&
	    !(ev->ev_flags & EVLIST_INSERTED)) {
		TAILQ_INSERT_TAIL(&eventqueue, ev, ev_next);
		
		ev->ev_flags |= EVLIST_INSERTED;

		evsel->add(evbase, ev);
	}
}

void
event_del(struct event *ev)
{
	LOG_DBG((LOG_MISC, 80, "event_del: %p, callback %p",
		 ev, ev->ev_callback));

	if (ev->ev_flags & EVLIST_ADD) {
		TAILQ_REMOVE(&addqueue, ev, ev_add_next);

		ev->ev_flags &= ~EVLIST_ADD;
	}

	if (ev->ev_flags & EVLIST_TIMEOUT) {
		TAILQ_REMOVE(&timequeue, ev, ev_timeout_next);

		ev->ev_flags &= ~EVLIST_TIMEOUT;
	}

	if (ev->ev_flags & EVLIST_INSERTED) {
		TAILQ_REMOVE(&eventqueue, ev, ev_next);

		ev->ev_flags &= ~EVLIST_INSERTED;

		evsel->del(evbase, ev);
	}
}

// void
// event_active(struct event *ev, int res, short ncalls)
// {
// 	/* We get different kinds of events, add them together */
// 	if (ev->ev_flags & EVLIST_ACTIVE) {
// 		ev->ev_res |= res;
// 		return;
// 	}

// 	ev->ev_res = res;
// 	ev->ev_ncalls = ncalls;
// 	ev->ev_pncalls = NULL;
// 	event_queue_insert(ev, EVLIST_ACTIVE);
// }


int
timeout_next(struct timeval *tv)
{
	struct timeval now;
	struct event *ev;

	if ((ev = TAILQ_FIRST(&timequeue)) == NULL) {
		timerclear(tv);
		tv->tv_sec = TIMEOUT_DEFAULT;
		return (0);
	}

	if (gettimeofday(&now, NULL) == -1)
		return (-1);

	if (timercmp(&ev->ev_timeout, &now, <=)) {
		timerclear(tv);
		return (0);
	}

	timersub(&ev->ev_timeout, &now, tv);

	LOG_DBG((LOG_MISC, 60, "timeout_next: in %d seconds", tv->tv_sec));
	return (0);
}

void
timeout_process(void)
{
	struct timeval now;
	struct event *ev;

	gettimeofday(&now, NULL);

	while ((ev = TAILQ_FIRST(&timequeue)) != NULL) {
		if (timercmp(&ev->ev_timeout, &now, >))
			break;

		TAILQ_REMOVE(&timequeue, ev, ev_timeout_next);
		ev->ev_flags &= ~EVLIST_TIMEOUT;

		LOG_DBG((LOG_MISC, 60, "timeout_process: call %p",
			 ev->ev_callback));
		(*ev->ev_callback)(ev->ev_fd, EV_TIMEOUT, ev->ev_arg);
	}
}
