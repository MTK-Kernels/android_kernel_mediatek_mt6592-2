/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VPX_PORTS_VPX_ONCE_H_
#define VPX_PORTS_VPX_ONCE_H_

#include "vpx_config.h"

#if CONFIG_MULTITHREAD && defined(_WIN32)
#include <windows.h>
#include <stdlib.h>
static void once(void (*func)(void))
{
    static CRITICAL_SECTION *lock;
    static LONG waiters;
    static int done;
    void *lock_ptr = &lock;

    if(done)
        return;

    InterlockedIncrement(&waiters);


    {
        
        CRITICAL_SECTION *new_lock = malloc(sizeof(CRITICAL_SECTION));
        InitializeCriticalSection(new_lock);
        if (InterlockedCompareExchangePointer(lock_ptr, new_lock, NULL) != NULL)
        {
            DeleteCriticalSection(new_lock);
            free(new_lock);
        }
    }


    EnterCriticalSection(lock);

    if (!done)
    {
        func();
        done = 1;
    }

    LeaveCriticalSection(lock);

    if(!InterlockedDecrement(&waiters))
    {
        DeleteCriticalSection(lock);
        free(lock);
        lock = NULL;
    }
}


#elif CONFIG_MULTITHREAD && defined(__OS2__)
#define INCL_DOS
#include <os2.h>
static void once(void (*func)(void))
{
    static int done;

    
    if(done)
        return;

    DosEnterCritSec();

    if (!done)
    {
        func();
        done = 1;
    }

    
    DosExitCritSec();
}


#elif CONFIG_MULTITHREAD && HAVE_PTHREAD_H
#include <pthread.h>
static void once(void (*func)(void))
{
    static pthread_once_t lock = PTHREAD_ONCE_INIT;
    pthread_once(&lock, func);
}


#else

static void once(void (*func)(void))
{
    static int done;

    if(!done)
    {
        func();
        done = 1;
    }
}
#endif

#endif  
