/***********************************************************************
*
* event.c
*
* Abstraction of select call into "event-handling" to make programming
* easier.
*
* Copyright (C) 2001 Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id$";

#include "event.h"
#include <stdlib.h>
#include <errno.h>

static void DestroySelector(EventSelector *es);
static void DestroyHandler(EventHandler *eh);
static void DoPendingChanges(EventSelector *es);

EventSelector *
Event_CreateSelector(void)
{
    EventSelector *es = malloc(sizeof(EventSelector));
    if (!es) return NULL;
    es->handlers = NULL;
    es->nestLevel = 0;
    es->destroyPending = 0;
    es->opsPending = 0;
    EVENT_DEBUG(("CreateSelector() -> %p\n", (void *) es));
    return es;
}

void
Event_DestroySelector(EventSelector *es)
{
    if (es->nestLevel) {
	es->destroyPending = 1;
	es->opsPending = 1;
	return;
    }
    DestroySelector(es);
}

int
Event_HandleEvent(EventSelector *es)
{
    fd_set readfds, writefds;
    fd_set *rd, *wr;
    unsigned int flags;

    struct timeval abs_timeout, now;

    struct timeval timeout;
    struct timeval *tm;
    EventHandler *eh;

    int r = 0;
    int errno_save = 0;
    int foundTimeoutEvent = 0;
    int foundReadEvent = 0;
    int foundWriteEvent = 0;
    int maxfd = -1;
    int pastDue;

    
    abs_timeout.tv_sec = 0;
    abs_timeout.tv_usec = 0;

    EVENT_DEBUG(("Enter Event_HandleEvent(es=%p)\n", (void *) es));

    
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    eh = es->handlers;
    for (eh=es->handlers; eh; eh=eh->next) {
	if (eh->flags & EVENT_FLAG_DELETED) continue;
	if (eh->flags & EVENT_FLAG_READABLE) {
	    foundReadEvent = 1;
	    FD_SET(eh->fd, &readfds);
	    if (eh->fd > maxfd) maxfd = eh->fd;
	}
	if (eh->flags & EVENT_FLAG_WRITEABLE) {
	    foundWriteEvent = 1;
	    FD_SET(eh->fd, &writefds);
	    if (eh->fd > maxfd) maxfd = eh->fd;
	}
	if (eh->flags & EVENT_TIMER_BITS) {
	    if (!foundTimeoutEvent) {
		abs_timeout = eh->tmout;
		foundTimeoutEvent = 1;
	    } else {
		if (eh->tmout.tv_sec < abs_timeout.tv_sec ||
		    (eh->tmout.tv_sec == abs_timeout.tv_sec &&
		     eh->tmout.tv_usec < abs_timeout.tv_usec)) {
		    abs_timeout = eh->tmout;
		}
	    }
	}
    }
    if (foundReadEvent) {
	rd = &readfds;
    } else {
	rd = NULL;
    }
    if (foundWriteEvent) {
	wr = &writefds;
    } else {
	wr = NULL;
    }

    if (foundTimeoutEvent) {
	gettimeofday(&now, NULL);
	
	timeout.tv_usec = abs_timeout.tv_usec - now.tv_usec;
	timeout.tv_sec = abs_timeout.tv_sec - now.tv_sec;
	if (timeout.tv_usec < 0) {
	    timeout.tv_usec += 1000000;
	    timeout.tv_sec--;
	}
	if (timeout.tv_sec < 0 ||
	    (timeout.tv_sec == 0 && timeout.tv_usec < 0)) {
	    timeout.tv_sec = 0;
	    timeout.tv_usec = 0;
	}
	tm = &timeout;
    } else {
	tm = NULL;
    }

    if (foundReadEvent || foundWriteEvent || foundTimeoutEvent) {
	for(;;) {
	    r = select(maxfd+1, rd, wr, NULL, tm);
	    if (r < 0) {
		if (errno == EINTR) continue;
	    }
	    break;
	}
    }

    if (foundTimeoutEvent) gettimeofday(&now, NULL);
    errno_save = errno;
    es->nestLevel++;

    if (r >= 0) {
	
	for (eh=es->handlers; eh; eh=eh->next) {

	    
	    if (eh->flags & EVENT_FLAG_DELETED) continue;

	    flags = 0;
	    if ((eh->flags & EVENT_FLAG_READABLE) &&
		FD_ISSET(eh->fd, &readfds)) {
		flags |= EVENT_FLAG_READABLE;
	    }
	    if ((eh->flags & EVENT_FLAG_WRITEABLE) &&
		FD_ISSET(eh->fd, &writefds)) {
		flags |= EVENT_FLAG_WRITEABLE;
	    }
	    if (eh->flags & EVENT_TIMER_BITS) {
		pastDue = (eh->tmout.tv_sec < now.tv_sec ||
			   (eh->tmout.tv_sec == now.tv_sec &&
			    eh->tmout.tv_usec <= now.tv_usec));
		if (pastDue) {
		    flags |= EVENT_TIMER_BITS;
		    if (eh->flags & EVENT_FLAG_TIMER) {
			
			es->opsPending = 1;
			eh->flags |= EVENT_FLAG_DELETED;
		    }
		}
	    }
	    
	    if (flags) {
		EVENT_DEBUG(("Enter callback: eh=%p flags=%u\n", eh, flags));
		eh->fn(es, eh->fd, flags, eh->data);
		EVENT_DEBUG(("Leave callback: eh=%p flags=%u\n", eh, flags));
	    }
	}
    }

    es->nestLevel--;

    if (!es->nestLevel && es->opsPending) {
	DoPendingChanges(es);
    }
    errno = errno_save;
    return r;
}

EventHandler *
Event_AddHandler(EventSelector *es,
		 int fd,
		 unsigned int flags,
		 EventCallbackFunc fn,
		 void *data)
{
    EventHandler *eh;

    
    flags &= (~(EVENT_TIMER_BITS | EVENT_FLAG_DELETED));

    
    if (fd < 0) {
	errno = EBADF;
	return NULL;
    }

    eh = malloc(sizeof(EventHandler));
    if (!eh) return NULL;
    eh->fd = fd;
    eh->flags = flags;
    eh->tmout.tv_usec = 0;
    eh->tmout.tv_sec = 0;
    eh->fn = fn;
    eh->data = data;

    
    eh->next = es->handlers;
    es->handlers = eh;

    EVENT_DEBUG(("Event_AddHandler(es=%p, fd=%d, flags=%u) -> %p\n", es, fd, flags, eh));
    return eh;
}

EventHandler *
Event_AddHandlerWithTimeout(EventSelector *es,
			    int fd,
			    unsigned int flags,
			    struct timeval t,
			    EventCallbackFunc fn,
			    void *data)
{
    EventHandler *eh;
    struct timeval now;

    
    if (t.tv_sec < 0 || t.tv_usec < 0) {
	return Event_AddHandler(es, fd, flags, fn, data);
    }

    
    flags &= (~(EVENT_FLAG_TIMER | EVENT_FLAG_DELETED));
    flags |= EVENT_FLAG_TIMEOUT;

    
    if (fd < 0) {
	errno = EBADF;
	return NULL;
    }

    
    if (t.tv_usec >= 1000000) {
	errno = EINVAL;
	return NULL;
    }

    eh = malloc(sizeof(EventHandler));
    if (!eh) return NULL;

    
    gettimeofday(&now, NULL);

    t.tv_sec += now.tv_sec;
    t.tv_usec += now.tv_usec;
    if (t.tv_usec >= 1000000) {
	t.tv_usec -= 1000000;
	t.tv_sec++;
    }

    eh->fd = fd;
    eh->flags = flags;
    eh->tmout = t;
    eh->fn = fn;
    eh->data = data;

    
    eh->next = es->handlers;
    es->handlers = eh;

    EVENT_DEBUG(("Event_AddHandlerWithTimeout(es=%p, fd=%d, flags=%u, t=%d/%d) -> %p\n", es, fd, flags, t.tv_sec, t.tv_usec, eh));
    return eh;
}


EventHandler *
Event_AddTimerHandler(EventSelector *es,
		      struct timeval t,
		      EventCallbackFunc fn,
		      void *data)
{
    EventHandler *eh;
    struct timeval now;

    
    if (t.tv_sec < 0 || t.tv_usec < 0 || t.tv_usec >= 1000000) {
	errno = EINVAL;
	return NULL;
    }

    eh = malloc(sizeof(EventHandler));
    if (!eh) return NULL;

    
    gettimeofday(&now, NULL);

    t.tv_sec += now.tv_sec;
    t.tv_usec += now.tv_usec;
    if (t.tv_usec >= 1000000) {
	t.tv_usec -= 1000000;
	t.tv_sec++;
    }

    eh->fd = -1;
    eh->flags = EVENT_FLAG_TIMER;
    eh->tmout = t;
    eh->fn = fn;
    eh->data = data;

    
    eh->next = es->handlers;
    es->handlers = eh;

    EVENT_DEBUG(("Event_AddTimerHandler(es=%p, t=%d/%d) -> %p\n", es, t.tv_sec,t.tv_usec, eh));
    return eh;
}

int
Event_DelHandler(EventSelector *es,
		 EventHandler *eh)
{
    
    EventHandler *cur, *prev;
    EVENT_DEBUG(("Event_DelHandler(es=%p, eh=%p)\n", es, eh));
    for (cur=es->handlers, prev=NULL; cur; prev=cur, cur=cur->next) {
	if (cur == eh) {
	    if (es->nestLevel) {
		eh->flags |= EVENT_FLAG_DELETED;
		es->opsPending = 1;
		return 0;
	    } else {
		if (prev) prev->next = cur->next;
		else      es->handlers = cur->next;

		DestroyHandler(cur);
		return 0;
	    }
	}
    }

    
    return 1;
}

void
DestroySelector(EventSelector *es)
{
    EventHandler *cur, *next;
    for (cur=es->handlers; cur; cur=next) {
	next = cur->next;
	DestroyHandler(cur);
    }

    free(es);
}

void
DestroyHandler(EventHandler *eh)
{
    EVENT_DEBUG(("DestroyHandler(eh=%p)\n", eh));
    free(eh);
}

void
DoPendingChanges(EventSelector *es)
{
    EventHandler *cur, *prev, *next;

    es->opsPending = 0;

    
    if (es->destroyPending) {
	DestroySelector(es);
	return;
    }

    
    cur = es->handlers;
    prev = NULL;
    while(cur) {
	if (!(cur->flags & EVENT_FLAG_DELETED)) {
	    prev = cur;
	    cur = cur->next;
	    continue;
	}

	
	if (prev) {
	    prev->next = cur->next;
	} else {
	    es->handlers = cur->next;
	}
	next = cur->next;
	DestroyHandler(cur);
	cur = next;
    }
}

EventCallbackFunc
Event_GetCallback(EventHandler *eh)
{
    return eh->fn;
}

void *
Event_GetData(EventHandler *eh)
{
    return eh->data;
}

void
Event_SetCallbackAndData(EventHandler *eh,
			 EventCallbackFunc fn,
			 void *data)
{
    eh->fn = fn;
    eh->data = data;
}

#ifdef DEBUG_EVENT
#include <stdarg.h>
#include <stdio.h>
FILE *Event_DebugFP = NULL;
void
Event_DebugMsg(char const *fmt, ...)
{
    va_list ap;
    struct timeval now;

    if (!Event_DebugFP) return;

    gettimeofday(&now, NULL);

    fprintf(Event_DebugFP, "%03d.%03d ", (int) now.tv_sec % 1000,
	    (int) now.tv_usec / 1000);

    va_start(ap, fmt);
    vfprintf(Event_DebugFP, fmt, ap);
    va_end(ap);
    fflush(Event_DebugFP);
}

#endif

int
Event_EnableDebugging(char const *fname)
{
#ifndef DEBUG_EVENT
    return 0;
#else
    Event_DebugFP = fopen(fname, "w");
    return (Event_DebugFP != NULL);
#endif
}

void
Event_ChangeTimeout(EventHandler *h, struct timeval t)
{
    struct timeval now;

    
    if (t.tv_sec < 0 || t.tv_usec < 0 || t.tv_usec >= 1000000) {
	return;
    }
    
    gettimeofday(&now, NULL);

    t.tv_sec += now.tv_sec;
    t.tv_usec += now.tv_usec;
    if (t.tv_usec >= 1000000) {
	t.tv_usec -= 1000000;
	t.tv_sec++;
    }

    h->tmout = t;
}
