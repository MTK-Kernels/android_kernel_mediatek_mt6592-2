/***********************************************************************
*
* eventpriv.h
*
* Abstraction of select call into "event-handling" to make programming
* easier.  This header includes "private" definitions which users
* of the event-handling code should not care about.
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

#ifndef INCLUDE_EVENTPRIV_H
#define INCLUDE_EVENTPRIV_H 1
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct EventHandler_t {
    struct EventHandler_t *next; 
    int fd;			
    unsigned int flags;		
    struct timeval tmout;	
    EventCallbackFunc fn;	
    void *data;			
} EventHandler;

typedef struct EventSelector_t {
    EventHandler *handlers;	
    int nestLevel;		
    int opsPending;		
    int destroyPending;		
} EventSelector;

#define EVENT_FLAG_DELETED 256
#endif
