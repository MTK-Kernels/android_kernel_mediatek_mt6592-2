/***********************************************************************
*
* event.h
*
* Abstraction of select call into "event-handling" to make programming
* easier.
*
* Copyright (C) 2001 Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* $Id$
*
* LIC: GPL
*
***********************************************************************/

#define DEBUG_EVENT

#ifndef INCLUDE_EVENT_H
#define INCLUDE_EVENT_H 1

#ifdef __sun
#define __EXTENSIONS__ 1
#endif

struct EventSelector_t;

typedef void (*EventCallbackFunc)(struct EventSelector_t *es,
				 int fd, unsigned int flags,
				 void *data);

#include "eventpriv.h"

extern EventSelector *Event_CreateSelector(void);

extern void Event_DestroySelector(EventSelector *es);

extern int Event_HandleEvent(EventSelector *es);

extern EventHandler *Event_AddHandler(EventSelector *es,
				      int fd,
				      unsigned int flags,
				      EventCallbackFunc fn, void *data);

extern EventHandler *Event_AddHandlerWithTimeout(EventSelector *es,
						 int fd,
						 unsigned int flags,
						 struct timeval t,
						 EventCallbackFunc fn,
						 void *data);


extern EventHandler *Event_AddTimerHandler(EventSelector *es,
					   struct timeval t,
					   EventCallbackFunc fn,
					   void *data);

void Event_ChangeTimeout(EventHandler *handler, struct timeval t);

extern int Event_DelHandler(EventSelector *es,
			    EventHandler *eh);

extern EventCallbackFunc Event_GetCallback(EventHandler *eh);

extern void *Event_GetData(EventHandler *eh);

extern void Event_SetCallbackAndData(EventHandler *eh,
				     EventCallbackFunc fn,
				     void *data);

int Event_HandleSignal(EventSelector *es, int sig, void (*handler)(int sig));

int Event_HandleChildExit(EventSelector *es, pid_t pid,
			  void (*handler)(pid_t, int, void *), void *data);

extern int Event_EnableDebugging(char const *fname);

#ifdef DEBUG_EVENT
extern void Event_DebugMsg(char const *fmt, ...);
#define EVENT_DEBUG(x) Event_DebugMsg x
#else
#define EVENT_DEBUG(x) ((void) 0)
#endif

#define EVENT_FLAG_READABLE 1
#define EVENT_FLAG_WRITEABLE 2
#define EVENT_FLAG_WRITABLE EVENT_FLAG_WRITEABLE

#define EVENT_FLAG_TIMER 4

#define EVENT_FLAG_TIMEOUT 8

#define EVENT_TIMER_BITS (EVENT_FLAG_TIMER | EVENT_FLAG_TIMEOUT)
#endif
