/*
  ----------------------------------------------------------------

  Notice that the following BSD-style license applies to this one
  file (drd.h) only.  The rest of Valgrind is licensed under the
  terms of the GNU General Public License, version 2, unless
  otherwise indicated.  See the COPYING file in the source
  distribution for details.

  ----------------------------------------------------------------

  This file is part of DRD, a Valgrind tool for verification of
  multithreaded programs.

  Copyright (C) 2006-2013 Bart Van Assche <bvanassche@acm.org>.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  2. The origin of this software must not be misrepresented; you must
  not claim that you wrote the original software.  If you use this
  software in a product, an acknowledgment in the product
  documentation would be appreciated but is not required.

  3. Altered source versions must be plainly marked as such, and must
  not be misrepresented as being the original software.

  4. The name of the author may not be used to endorse or promote
  products derived from this software without specific prior written
  permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  ----------------------------------------------------------------

  Notice that the above BSD-style license applies to this one file
  (drd.h) only.  The entire rest of Valgrind is licensed under
  the terms of the GNU General Public License, version 2.  See the
  COPYING file in the source distribution for details.

  ----------------------------------------------------------------
*/

#ifndef __VALGRIND_DRD_H
#define __VALGRIND_DRD_H


#include "valgrind.h"


#define DRD_GET_VALGRIND_THREADID                                          \
    (unsigned)VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                           \
                                   VG_USERREQ__DRD_GET_VALGRIND_THREAD_ID, \
                                   0, 0, 0, 0, 0)

#define DRD_GET_DRD_THREADID                                            \
    (unsigned)VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                        \
                                   VG_USERREQ__DRD_GET_DRD_THREAD_ID,   \
                                   0, 0, 0, 0, 0)


#define DRD_IGNORE_VAR(x) ANNOTATE_BENIGN_RACE_SIZED(&(x), sizeof(x), "")

#define DRD_STOP_IGNORING_VAR(x)                                       \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_FINISH_SUPPRESSION, \
                                   &(x), sizeof(x), 0, 0, 0)

#define DRD_TRACE_VAR(x)                                             \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_START_TRACE_ADDR, \
                                   &(x), sizeof(x), 0, 0, 0)

#define DRD_STOP_TRACING_VAR(x)                                       \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_STOP_TRACE_ADDR, \
                                   &(x), sizeof(x), 0, 0, 0)


#ifndef __HELGRIND_H

#define ANNOTATE_HAPPENS_BEFORE(addr)                                       \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_ANNOTATE_HAPPENS_BEFORE, \
                                   addr, 0, 0, 0, 0)

#define ANNOTATE_HAPPENS_AFTER(addr)                                       \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_ANNOTATE_HAPPENS_AFTER, \
                                   addr, 0, 0, 0, 0)

#else 

#undef ANNOTATE_CONDVAR_LOCK_WAIT
#undef ANNOTATE_CONDVAR_WAIT
#undef ANNOTATE_CONDVAR_SIGNAL
#undef ANNOTATE_CONDVAR_SIGNAL_ALL
#undef ANNOTATE_PURE_HAPPENS_BEFORE_MUTEX
#undef ANNOTATE_PUBLISH_MEMORY_RANGE
#undef ANNOTATE_BARRIER_INIT
#undef ANNOTATE_BARRIER_WAIT_BEFORE
#undef ANNOTATE_BARRIER_WAIT_AFTER
#undef ANNOTATE_BARRIER_DESTROY
#undef ANNOTATE_PCQ_CREATE
#undef ANNOTATE_PCQ_DESTROY
#undef ANNOTATE_PCQ_PUT
#undef ANNOTATE_PCQ_GET
#undef ANNOTATE_BENIGN_RACE
#undef ANNOTATE_BENIGN_RACE_SIZED
#undef ANNOTATE_IGNORE_READS_BEGIN
#undef ANNOTATE_IGNORE_READS_END
#undef ANNOTATE_IGNORE_WRITES_BEGIN
#undef ANNOTATE_IGNORE_WRITES_END
#undef ANNOTATE_IGNORE_READS_AND_WRITES_BEGIN
#undef ANNOTATE_IGNORE_READS_AND_WRITES_END
#undef ANNOTATE_NEW_MEMORY
#undef ANNOTATE_TRACE_MEMORY
#undef ANNOTATE_THREAD_NAME

#endif 

#define ANNOTATE_CONDVAR_LOCK_WAIT(cv, mtx) do { } while(0)

#define ANNOTATE_CONDVAR_SIGNAL(cv) do { } while(0)

#define ANNOTATE_CONDVAR_SIGNAL_ALL(cv) do { } while(0)

#define ANNOTATE_CONDVAR_WAIT(cv) do { } while(0)

#define ANNOTATE_PURE_HAPPENS_BEFORE_MUTEX(mtx) do { } while(0)

#define ANNOTATE_MUTEX_IS_USED_AS_CONDVAR(mtx) do { } while(0)

#define ANNOTATE_PUBLISH_MEMORY_RANGE(addr, size) do { } while(0)

#define ANNOTATE_UNPUBLISH_MEMORY_RANGE(addr, size) do { } while(0)

#define ANNOTATE_SWAP_MEMORY_RANGE(addr, size) do { } while(0)

#ifndef __HELGRIND_H

#define ANNOTATE_RWLOCK_CREATE(rwlock)                                     \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_ANNOTATE_RWLOCK_CREATE, \
                                   rwlock, 0, 0, 0, 0);

#define ANNOTATE_RWLOCK_DESTROY(rwlock)                                     \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_ANNOTATE_RWLOCK_DESTROY, \
                                   rwlock, 0, 0, 0, 0);

#define ANNOTATE_RWLOCK_ACQUIRED(rwlock, is_w)                               \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_ANNOTATE_RWLOCK_ACQUIRED, \
                                   rwlock, is_w, 0, 0, 0)

#endif 

#define ANNOTATE_READERLOCK_ACQUIRED(rwlock) ANNOTATE_RWLOCK_ACQUIRED(rwlock, 0)

#define ANNOTATE_WRITERLOCK_ACQUIRED(rwlock) ANNOTATE_RWLOCK_ACQUIRED(rwlock, 1)

#ifndef __HELGRIND_H

#define ANNOTATE_RWLOCK_RELEASED(rwlock, is_w)                               \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_ANNOTATE_RWLOCK_RELEASED, \
                                   rwlock, is_w, 0, 0, 0);

#endif 

#define ANNOTATE_READERLOCK_RELEASED(rwlock) ANNOTATE_RWLOCK_RELEASED(rwlock, 0)

#define ANNOTATE_WRITERLOCK_RELEASED(rwlock) ANNOTATE_RWLOCK_RELEASED(rwlock, 1)

#define ANNOTATE_SEM_INIT_PRE(sem, value)                                 \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_ANNOTATE_SEM_INIT_PRE, \
                                   sem, value, 0, 0, 0);

#define ANNOTATE_SEM_DESTROY_POST(sem)                                        \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_ANNOTATE_SEM_DESTROY_POST, \
                                   sem, 0, 0, 0, 0);

#define ANNOTATE_SEM_WAIT_PRE(sem)                                        \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_ANNOTATE_SEM_WAIT_PRE, \
                                   sem, 0, 0, 0, 0)

#define ANNOTATE_SEM_WAIT_POST(sem)                                        \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_ANNOTATE_SEM_WAIT_POST, \
                                   sem, 0, 0, 0, 0)

#define ANNOTATE_SEM_POST_PRE(sem)                                        \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_ANNOTATE_SEM_POST_PRE, \
                                   sem, 0, 0, 0, 0)

#define ANNOTATE_BARRIER_INIT(barrier, count, reinitialization_allowed) \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_ANNOTATION_UNIMP,    \
                                   "ANNOTATE_BARRIER_INIT", barrier,    \
                                   count, reinitialization_allowed, 0)

#define ANNOTATE_BARRIER_DESTROY(barrier)                               \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_ANNOTATION_UNIMP,    \
                                   "ANNOTATE_BARRIER_DESTROY",          \
                                   barrier, 0, 0, 0)

#define ANNOTATE_BARRIER_WAIT_BEFORE(barrier)                           \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_ANNOTATION_UNIMP,    \
                                   "ANNOTATE_BARRIER_WAIT_BEFORE",      \
                                   barrier, 0, 0, 0)

#define ANNOTATE_BARRIER_WAIT_AFTER(barrier)                            \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_ANNOTATION_UNIMP,    \
                                   "ANNOTATE_BARRIER_WAIT_AFTER",       \
                                   barrier, 0, 0, 0)

#define ANNOTATE_PCQ_CREATE(pcq) do { } while(0)

#define ANNOTATE_PCQ_DESTROY(pcq) do { } while(0)

#define ANNOTATE_PCQ_PUT(pcq) do { } while(0)

#define ANNOTATE_PCQ_GET(pcq) do { } while(0)

#define ANNOTATE_BENIGN_RACE(addr, descr) \
   ANNOTATE_BENIGN_RACE_SIZED(addr, sizeof(*addr), descr)

#define ANNOTATE_BENIGN_RACE_SIZED(addr, size, descr)                   \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_START_SUPPRESSION,   \
                                   addr, size, 0, 0, 0)

#define ANNOTATE_IGNORE_READS_BEGIN()                                \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_RECORD_LOADS,     \
                                   0, 0, 0, 0, 0);


#define ANNOTATE_IGNORE_READS_END()                                  \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_RECORD_LOADS,     \
                                   1, 0, 0, 0, 0);

#define ANNOTATE_IGNORE_WRITES_BEGIN()                                \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_RECORD_STORES,     \
                                   0, 0, 0, 0, 0)

#define ANNOTATE_IGNORE_WRITES_END()                                  \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_RECORD_STORES,     \
                                   1, 0, 0, 0, 0)

#define ANNOTATE_IGNORE_READS_AND_WRITES_BEGIN() \
   do { ANNOTATE_IGNORE_READS_BEGIN(); ANNOTATE_IGNORE_WRITES_BEGIN(); } while(0)

#define ANNOTATE_IGNORE_READS_AND_WRITES_END() \
   do { ANNOTATE_IGNORE_READS_END(); ANNOTATE_IGNORE_WRITES_END(); } while(0)

#define ANNOTATE_NEW_MEMORY(addr, size)                           \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_CLEAN_MEMORY,  \
                                   addr, size, 0, 0, 0)

#define ANNOTATE_TRACE_MEMORY(addr) DRD_TRACE_VAR(*(char*)(addr))

#define ANNOTATE_THREAD_NAME(name)                                      \
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__DRD_SET_THREAD_NAME,     \
                                   name, 0, 0, 0, 0)



enum {
   
   
   
   VG_USERREQ__DRD_CLEAN_MEMORY = VG_USERREQ_TOOL_BASE('H','G'),
   

   
   VG_USERREQ__DRD_GET_VALGRIND_THREAD_ID = VG_USERREQ_TOOL_BASE('D','R'),
   
   
   VG_USERREQ__DRD_GET_DRD_THREAD_ID,
   

   
   
   VG_USERREQ__DRD_START_SUPPRESSION,
   
   
   
   VG_USERREQ__DRD_FINISH_SUPPRESSION,
   

   
   VG_USERREQ__DRD_START_TRACE_ADDR,
   
   
   VG_USERREQ__DRD_STOP_TRACE_ADDR,
   

   
   VG_USERREQ__DRD_RECORD_LOADS,
   
   
   VG_USERREQ__DRD_RECORD_STORES,
   

   
   VG_USERREQ__DRD_SET_THREAD_NAME,
   

   
   VG_USERREQ__DRD_ANNOTATION_UNIMP,
   

   VG_USERREQ__DRD_ANNOTATE_SEM_INIT_PRE,
   
   VG_USERREQ__DRD_ANNOTATE_SEM_DESTROY_POST,
   
   VG_USERREQ__DRD_ANNOTATE_SEM_WAIT_PRE,
   
   VG_USERREQ__DRD_ANNOTATE_SEM_WAIT_POST,
   
   VG_USERREQ__DRD_ANNOTATE_SEM_POST_PRE,
   

   
   VG_USERREQ__DRD_IGNORE_MUTEX_ORDERING,
   

   VG_USERREQ__DRD_ANNOTATE_RWLOCK_CREATE
      = VG_USERREQ_TOOL_BASE('H','G') + 256 + 14,
   
   VG_USERREQ__DRD_ANNOTATE_RWLOCK_DESTROY
      = VG_USERREQ_TOOL_BASE('H','G') + 256 + 15,
   
   VG_USERREQ__DRD_ANNOTATE_RWLOCK_ACQUIRED
      = VG_USERREQ_TOOL_BASE('H','G') + 256 + 17,
   
   VG_USERREQ__DRD_ANNOTATE_RWLOCK_RELEASED
      = VG_USERREQ_TOOL_BASE('H','G') + 256 + 18,
   

   
   VG_USERREQ__HELGRIND_ANNOTATION_UNIMP
      = VG_USERREQ_TOOL_BASE('H','G') + 256 + 32,
   

   
   VG_USERREQ__DRD_ANNOTATE_HAPPENS_BEFORE
      = VG_USERREQ_TOOL_BASE('H','G') + 256 + 33,
   
   
   VG_USERREQ__DRD_ANNOTATE_HAPPENS_AFTER
      = VG_USERREQ_TOOL_BASE('H','G') + 256 + 34,
   

};



#ifdef __cplusplus
template <typename T>
inline T ANNOTATE_UNPROTECTED_READ(const volatile T& x) {
   ANNOTATE_IGNORE_READS_BEGIN();
   const T result = x;
   ANNOTATE_IGNORE_READS_END();
   return result;
}
#define ANNOTATE_BENIGN_RACE_STATIC(static_var, description)		\
   namespace {								\
      static class static_var##_annotator				\
      {									\
      public:								\
	 static_var##_annotator()					\
	 {								\
	    ANNOTATE_BENIGN_RACE_SIZED(&static_var, sizeof(static_var),	\
				       #static_var ": " description);	\
	 }								\
      } the_##static_var##_annotator;					\
   }
#endif


#endif 
