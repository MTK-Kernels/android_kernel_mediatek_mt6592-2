/***********************************************************************
*
* event_tcp.c -- implementation of event-driven socket I/O.
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

#include "event_tcp.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

static void free_state(EventTcpState *state);

typedef struct EventTcpConnectState_t {
    int fd;
    EventHandler *conn;
    EventTcpConnectFunc f;
    void *data;
} EventTcpConnectState;

static void
handle_accept(EventSelector *es,
	      int fd,
	      unsigned int flags,
	      void *data)
{
    int conn;
    EventTcpAcceptFunc f;

    EVENT_DEBUG(("tcp_handle_accept(es=%p, fd=%d, flags=%u, data=%p)\n", es, fd, flags, data));
    conn = accept(fd, NULL, NULL);
    if (conn < 0) return;
    f = (EventTcpAcceptFunc) data;

    f(es, conn);
}

static void
handle_connect(EventSelector *es,
	      int fd,
	      unsigned int flags,
	      void *data)
{
    int error = 0;
    socklen_t len = sizeof(error);
    EventTcpConnectState *state = (EventTcpConnectState *) data;

    EVENT_DEBUG(("tcp_handle_connect(es=%p, fd=%d, flags=%u, data=%p)\n", es, fd, flags, data));

    
    Event_DelHandler(es, state->conn);
    state->conn = NULL;

    
    if (flags & EVENT_FLAG_TIMEOUT) {
	errno = ETIMEDOUT;
	state->f(es, fd, EVENT_TCP_FLAG_TIMEOUT, state->data);
	free(state);
	return;
    }

    
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
	state->f(es, fd, EVENT_TCP_FLAG_IOERROR, state->data);
	free(state);
	return;
    }
    if (error) {
	errno = error;
	state->f(es, fd, EVENT_TCP_FLAG_IOERROR, state->data);
	free(state);
	return;
    }

    
    state->f(es, fd, EVENT_TCP_FLAG_COMPLETE, state->data);
    free(state);
}

EventHandler *
EventTcp_CreateAcceptor(EventSelector *es,
			int socket,
			EventTcpAcceptFunc f)
{
    int flags;

    EVENT_DEBUG(("EventTcp_CreateAcceptor(es=%p, socket=%d)\n", es, socket));
    
    flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1) {
	return NULL;
    }
    if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1) {
	return NULL;
    }

    return Event_AddHandler(es, socket, EVENT_FLAG_READABLE,
			    handle_accept, (void *) f);

}

static void
free_state(EventTcpState *state)
{
    if (!state) return;
    EVENT_DEBUG(("tcp_free_state(state=%p)\n", state));
    if (state->buf) free(state->buf);
    if (state->eh) Event_DelHandler(state->es, state->eh);
    free(state);
}

static void
handle_readable(EventSelector *es,
		int fd,
		unsigned int flags,
		void *data)
{
    EventTcpState *state = (EventTcpState *) data;
    int done = state->cur - state->buf;
    int togo = state->len - done;
    int nread = 0;
    int flag;

    EVENT_DEBUG(("tcp_handle_readable(es=%p, fd=%d, flags=%u, data=%p)\n", es, fd, flags, data));

    
    if (flags & EVENT_FLAG_TIMEOUT) {
	errno = ETIMEDOUT;
	(state->f)(es, state->socket, state->buf, done, EVENT_TCP_FLAG_TIMEOUT,
		   state->data);
	free_state(state);
	return;
    }
    if (state->delim < 0) {
	
	
	nread = read(fd, state->cur, togo);
	if (nread <= 0) {
	    if (nread < 0 && errno == ECONNRESET && done > 0) {
		nread = 0;
	    }
	    flag = (nread) ? EVENT_TCP_FLAG_IOERROR : EVENT_TCP_FLAG_EOF;
	    
	    (state->f)(es, state->socket, state->buf, done, flag, state->data);
	    free_state(state);
	    return;
	}
	state->cur += nread;
	done += nread;
	if (done >= state->len) {
	    
	    (state->f)(es, state->socket, state->buf, done,
		       EVENT_TCP_FLAG_COMPLETE, state->data);
	    free_state(state);
	    return;
	}
    } else {
	
	while ( (togo > 0) && (nread = read(fd, state->cur, 1)) == 1) {
	    togo--;
	    done++;
	    state->cur++;
	    if (*(state->cur - 1) == state->delim) break;
	}

	if (nread <= 0) {
	    
	    if (nread < 0 && errno == EAGAIN) return;
	}

	
	if (nread < 0) {
	    flag = EVENT_TCP_FLAG_IOERROR;
	} else if (nread == 0) {
	    flag = EVENT_TCP_FLAG_EOF;
	} else {
	    flag = EVENT_TCP_FLAG_COMPLETE;
	}
	(state->f)(es, state->socket, state->buf, done, flag, state->data);
	free_state(state);
	return;
    }
}

static void
handle_writeable(EventSelector *es,
		int fd,
		unsigned int flags,
		void *data)
{
    EventTcpState *state = (EventTcpState *) data;
    int done = state->cur - state->buf;
    int togo = state->len - done;
    int n;

    
    if (flags & EVENT_FLAG_TIMEOUT) {
	errno = ETIMEDOUT;
	(state->f)(es, state->socket, state->buf, done, EVENT_TCP_FLAG_TIMEOUT,
		   state->data);
	free_state(state);
	return;
    }

    
    n = write(fd, state->cur, togo);

    EVENT_DEBUG(("tcp_handle_writeable(es=%p, fd=%d, flags=%u, data=%p)\n", es, fd, flags, data));
    if (n <= 0) {
	
	if (state->f) {
	    (state->f)(es, state->socket, state->buf, done,
		       EVENT_TCP_FLAG_IOERROR,
		       state->data);
	} else {
	    close(fd);
	}
	free_state(state);
	return;
    }
    state->cur += n;
    done += n;
    if (done >= state->len) {
	/* Written enough! */
	if (state->f) {
	    (state->f)(es, state->socket, state->buf, done,
		       EVENT_TCP_FLAG_COMPLETE, state->data);
	} else {
	    close(fd);
	}
	free_state(state);
	return;
    }

}

EventTcpState *
EventTcp_ReadBuf(EventSelector *es,
		 int socket,
		 int len,
		 int delim,
		 EventTcpIOFinishedFunc f,
		 int timeout,
		 void *data)
{
    EventTcpState *state;
    int flags;
    struct timeval t;

    EVENT_DEBUG(("EventTcp_ReadBuf(es=%p, socket=%d, len=%d, delim=%d, timeout=%d)\n", es, socket, len, delim, timeout));
    if (len <= 0) return NULL;
    if (socket < 0) return NULL;

    
    flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1) {
	return NULL;
    }
    if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1) {
	return NULL;
    }

    state = malloc(sizeof(EventTcpState));
    if (!state) return NULL;

    memset(state, 0, sizeof(EventTcpState));

    state->socket = socket;

    state->buf = malloc(len);
    if (!state->buf) {
	free_state(state);
	return NULL;
    }

    state->cur = state->buf;
    state->len = len;
    state->f = f;
    state->es = es;

    if (timeout <= 0) {
	t.tv_sec = -1;
	t.tv_usec = -1;
    } else {
	t.tv_sec = timeout;
	t.tv_usec = 0;
    }

    state->eh = Event_AddHandlerWithTimeout(es, socket, EVENT_FLAG_READABLE,
					    t, handle_readable,
					    (void *) state);
    if (!state->eh) {
	free_state(state);
	return NULL;
    }
    state->data = data;
    state->delim = delim;
    EVENT_DEBUG(("EventTcp_ReadBuf() -> %p\n", state));

    return state;
}

EventTcpState *
EventTcp_WriteBuf(EventSelector *es,
		  int socket,
		  char *buf,
		  int len,
		  EventTcpIOFinishedFunc f,
		  int timeout,
		  void *data)
{
    EventTcpState *state;
    int flags;
    struct timeval t;

    EVENT_DEBUG(("EventTcp_WriteBuf(es=%p, socket=%d, len=%d, timeout=%d)\n", es, socket, len, timeout));
    if (len <= 0) return NULL;
    if (socket < 0) return NULL;

    
    flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1) {
	return NULL;
    }
    if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1) {
	return NULL;
    }

    state = malloc(sizeof(EventTcpState));
    if (!state) return NULL;

    memset(state, 0, sizeof(EventTcpState));

    state->socket = socket;

    state->buf = malloc(len);
    if (!state->buf) {
	free_state(state);
	return NULL;
    }
    memcpy(state->buf, buf, len);

    state->cur = state->buf;
    state->len = len;
    state->f = f;
    state->es = es;

    if (timeout <= 0) {
	t.tv_sec = -1;
	t.tv_usec = -1;
    } else {
	t.tv_sec = timeout;
	t.tv_usec = 0;
    }

    state->eh = Event_AddHandlerWithTimeout(es, socket, EVENT_FLAG_WRITEABLE,
					    t, handle_writeable,
					    (void *) state);
    if (!state->eh) {
	free_state(state);
	return NULL;
    }

    state->data = data;
    state->delim = -1;
    EVENT_DEBUG(("EventTcp_WriteBuf() -> %p\n", state));
    return state;
}

void
EventTcp_Connect(EventSelector *es,
		 int fd,
		 struct sockaddr const *addr,
		 socklen_t addrlen,
		 EventTcpConnectFunc f,
		 int timeout,
		 void *data)
{
    int flags;
    int n;
    EventTcpConnectState *state;
    struct timeval t;

    
    flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
	f(es, fd, EVENT_TCP_FLAG_IOERROR, data);
	return;
    }

    n = connect(fd, addr, addrlen);
    if (n < 0) {
	if (errno != EINPROGRESS) {
	    f(es, fd, EVENT_TCP_FLAG_IOERROR, data);
	    return;
	}
    }

    if (n == 0) { 
	f(es, fd, EVENT_TCP_FLAG_COMPLETE, data);
	return;
    }

    state = malloc(sizeof(*state));
    if (!state) {
	f(es, fd, EVENT_TCP_FLAG_IOERROR, data);
	return;
    }
    state->f = f;
    state->fd = fd;
    state->data = data;

    if (timeout <= 0) {
	t.tv_sec = -1;
	t.tv_usec = -1;
    } else {
	t.tv_sec = timeout;
	t.tv_usec = 0;
    }

    state->conn = Event_AddHandlerWithTimeout(es, fd, EVENT_FLAG_WRITEABLE,
					      t, handle_connect,
					      (void *) state);
    if (!state->conn) {
	free(state);
	f(es, fd, EVENT_TCP_FLAG_IOERROR, data);
	return;
    }
}

void
EventTcp_CancelPending(EventTcpState *s)
{
    free_state(s);
}
