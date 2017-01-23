/*
  This file is part of drd, a thread error detector.

  Copyright (C) 2006-2013 Bart Van Assche <bvanassche@acm.org>.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
  02111-1307, USA.

  The GNU General Public License is contained in the file COPYING.
*/




#ifndef __DRD_CLIENTREQ_H
#define __DRD_CLIENTREQ_H


#include "drd.h"
#include "drd_basics.h" 


enum {
   VG_USERREQ__SET_PTHREAD_COND_INITIALIZER = VG_USERREQ_TOOL_BASE('D', 'r'),
   

   
   VG_USERREQ__DRD_START_NEW_SEGMENT,
   

   
   VG_USERREQ__SET_PTHREADID,
   
   
   
   
   VG_USERREQ__SET_JOINABLE,
   

   
   VG_USERREQ__ENTERING_PTHREAD_CREATE,
   
   
   VG_USERREQ__LEFT_PTHREAD_CREATE,
   

   
   
   VG_USERREQ__POST_THREAD_JOIN,
   

   
   VG_USERREQ__PRE_THREAD_CANCEL,
   
   
   VG_USERREQ__POST_THREAD_CANCEL,
   

   
   VG_USERREQ__PRE_MUTEX_INIT,
   
   
   VG_USERREQ__POST_MUTEX_INIT,
   
   
   VG_USERREQ__PRE_MUTEX_DESTROY,
   
   
   VG_USERREQ__POST_MUTEX_DESTROY,
   
   
   VG_USERREQ__PRE_MUTEX_LOCK,
   
   
   VG_USERREQ__POST_MUTEX_LOCK,
   
   
   VG_USERREQ__PRE_MUTEX_UNLOCK,
   
   
   VG_USERREQ__POST_MUTEX_UNLOCK,
   
   
   VG_USERREQ__PRE_SPIN_INIT_OR_UNLOCK,
   
   
   VG_USERREQ__POST_SPIN_INIT_OR_UNLOCK,
   


   
   VG_USERREQ__PRE_COND_INIT,
   
   
   VG_USERREQ__POST_COND_INIT,
   
   
   VG_USERREQ__PRE_COND_DESTROY,
   
   
   VG_USERREQ__POST_COND_DESTROY,
   
   VG_USERREQ__PRE_COND_WAIT,
   
   VG_USERREQ__POST_COND_WAIT,
   
   VG_USERREQ__PRE_COND_SIGNAL,
   
   VG_USERREQ__POST_COND_SIGNAL,
   
   VG_USERREQ__PRE_COND_BROADCAST,
   
   VG_USERREQ__POST_COND_BROADCAST,
   

   
   VG_USERREQ__PRE_SEM_INIT,
   
   
   VG_USERREQ__POST_SEM_INIT,
   
   
   VG_USERREQ__PRE_SEM_DESTROY,
   
   
   VG_USERREQ__POST_SEM_DESTROY,
   
   
   VG_USERREQ__PRE_SEM_OPEN,
   
   
   VG_USERREQ__POST_SEM_OPEN,
   
   
   VG_USERREQ__PRE_SEM_CLOSE,
   
   
   VG_USERREQ__POST_SEM_CLOSE,
   
   
   VG_USERREQ__PRE_SEM_WAIT,
   
   
   VG_USERREQ__POST_SEM_WAIT,
   
   
   VG_USERREQ__PRE_SEM_POST,
   
   
   VG_USERREQ__POST_SEM_POST,
   

   
   VG_USERREQ__PRE_BARRIER_INIT,
   
   
   VG_USERREQ__POST_BARRIER_INIT,
   
   
   VG_USERREQ__PRE_BARRIER_DESTROY,
   
   
   VG_USERREQ__POST_BARRIER_DESTROY,
   
   
   VG_USERREQ__PRE_BARRIER_WAIT,
   
   
   VG_USERREQ__POST_BARRIER_WAIT,
   

   
   VG_USERREQ__PRE_RWLOCK_INIT,
   
   
   VG_USERREQ__POST_RWLOCK_INIT,
   
   
   VG_USERREQ__PRE_RWLOCK_DESTROY,
   
   
   VG_USERREQ__POST_RWLOCK_DESTROY,
   
   
   VG_USERREQ__PRE_RWLOCK_RDLOCK,
   
   
   VG_USERREQ__POST_RWLOCK_RDLOCK,
   
   
   VG_USERREQ__PRE_RWLOCK_WRLOCK,
   
   
   VG_USERREQ__POST_RWLOCK_WRLOCK,
   
   
   VG_USERREQ__PRE_RWLOCK_UNLOCK,
   
   
   VG_USERREQ__POST_RWLOCK_UNLOCK
   

};

typedef enum {
   mutex_type_unknown          = -1,
   mutex_type_invalid_mutex    = 0,
   mutex_type_recursive_mutex  = 1,
   mutex_type_errorcheck_mutex = 2,
   mutex_type_default_mutex    = 3,
   mutex_type_spinlock         = 4,
   mutex_type_cxa_guard        = 5,
} MutexT;

typedef enum {
   pthread_rwlock = 1,
   user_rwlock    = 2,
} RwLockT;

typedef enum {
   pthread_barrier = 1,
   gomp_barrier    = 2,
} BarrierT;


extern Bool DRD_(g_free_is_write);

void DRD_(clientreq_init)(void);


#endif 
