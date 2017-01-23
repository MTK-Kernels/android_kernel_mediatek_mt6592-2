/*
   ----------------------------------------------------------------

   Notice that the above BSD-style license applies to this one file
   (helgrind.h) only.  The entire rest of Valgrind is licensed under
   the terms of the GNU General Public License, version 2.  See the
   COPYING file in the source distribution for details.

   ----------------------------------------------------------------

   This file is part of Helgrind, a Valgrind tool for detecting errors
   in threaded programs.

   Copyright (C) 2007-2013 OpenWorks LLP
      info@open-works.co.uk

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
   (helgrind.h) only.  The entire rest of Valgrind is licensed under
   the terms of the GNU General Public License, version 2.  See the
   COPYING file in the source distribution for details.

   ---------------------------------------------------------------- 
*/

#ifndef __HELGRIND_H
#define __HELGRIND_H

#include "valgrind.h"

typedef
   enum {
      VG_USERREQ__HG_CLEAN_MEMORY = VG_USERREQ_TOOL_BASE('H','G'),


      
      _VG_USERREQ__HG_SET_MY_PTHREAD_T = VG_USERREQ_TOOL_BASE('H','G') 
                                         + 256,
      _VG_USERREQ__HG_PTH_API_ERROR,              
      _VG_USERREQ__HG_PTHREAD_JOIN_POST,          
      _VG_USERREQ__HG_PTHREAD_MUTEX_INIT_POST,    
      _VG_USERREQ__HG_PTHREAD_MUTEX_DESTROY_PRE,  
      _VG_USERREQ__HG_PTHREAD_MUTEX_UNLOCK_PRE,   
      _VG_USERREQ__HG_PTHREAD_MUTEX_UNLOCK_POST,  
      _VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_PRE, 
      _VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_POST,    
      _VG_USERREQ__HG_PTHREAD_COND_SIGNAL_PRE,    
      _VG_USERREQ__HG_PTHREAD_COND_BROADCAST_PRE, 
      _VG_USERREQ__HG_PTHREAD_COND_WAIT_PRE,     
      _VG_USERREQ__HG_PTHREAD_COND_WAIT_POST,    
      _VG_USERREQ__HG_PTHREAD_COND_DESTROY_PRE,   
      _VG_USERREQ__HG_PTHREAD_RWLOCK_INIT_POST,   
      _VG_USERREQ__HG_PTHREAD_RWLOCK_DESTROY_PRE, 
      _VG_USERREQ__HG_PTHREAD_RWLOCK_LOCK_PRE,    
      _VG_USERREQ__HG_PTHREAD_RWLOCK_LOCK_POST,   
      _VG_USERREQ__HG_PTHREAD_RWLOCK_UNLOCK_PRE,  
      _VG_USERREQ__HG_PTHREAD_RWLOCK_UNLOCK_POST, 
      _VG_USERREQ__HG_POSIX_SEM_INIT_POST,        
      _VG_USERREQ__HG_POSIX_SEM_DESTROY_PRE,      
      _VG_USERREQ__HG_POSIX_SEM_POST_PRE,         
      _VG_USERREQ__HG_POSIX_SEM_WAIT_POST,        
      _VG_USERREQ__HG_PTHREAD_BARRIER_INIT_PRE,   
      _VG_USERREQ__HG_PTHREAD_BARRIER_WAIT_PRE,   
      _VG_USERREQ__HG_PTHREAD_BARRIER_DESTROY_PRE, 
      _VG_USERREQ__HG_PTHREAD_SPIN_INIT_OR_UNLOCK_PRE,  
      _VG_USERREQ__HG_PTHREAD_SPIN_INIT_OR_UNLOCK_POST, 
      _VG_USERREQ__HG_PTHREAD_SPIN_LOCK_PRE,      
      _VG_USERREQ__HG_PTHREAD_SPIN_LOCK_POST,     
      _VG_USERREQ__HG_PTHREAD_SPIN_DESTROY_PRE,   
      _VG_USERREQ__HG_CLIENTREQ_UNIMP,            
      _VG_USERREQ__HG_USERSO_SEND_PRE,        
      _VG_USERREQ__HG_USERSO_RECV_POST,       
      _VG_USERREQ__HG_USERSO_FORGET_ALL,      
      _VG_USERREQ__HG_RESERVED2,              
      _VG_USERREQ__HG_RESERVED3,              
      _VG_USERREQ__HG_RESERVED4,              
      _VG_USERREQ__HG_ARANGE_MAKE_UNTRACKED, 
      _VG_USERREQ__HG_ARANGE_MAKE_TRACKED,   
      _VG_USERREQ__HG_PTHREAD_BARRIER_RESIZE_PRE, 
      _VG_USERREQ__HG_CLEAN_MEMORY_HEAPBLOCK, 
      _VG_USERREQ__HG_PTHREAD_COND_INIT_POST,  
      _VG_USERREQ__HG_GNAT_MASTER_HOOK,       
      _VG_USERREQ__HG_GNAT_MASTER_COMPLETED_HOOK,
      _VG_USERREQ__HG_GET_ABITS               
   } Vg_TCheckClientRequest;




#define DO_CREQ_v_W(_creqF, _ty1F,_arg1F)                \
   do {                                                  \
      long int _arg1;                                    \
         \
      _arg1 = (long int)(_arg1F);                        \
      VALGRIND_DO_CLIENT_REQUEST_STMT(                   \
                                 (_creqF),               \
                                 _arg1, 0,0,0,0);        \
   } while (0)

#define DO_CREQ_W_W(_resF, _dfltF, _creqF, _ty1F,_arg1F) \
   do {                                                  \
      long int _arg1;                                    \
         \
      _arg1 = (long int)(_arg1F);                        \
      _qzz_res = VALGRIND_DO_CLIENT_REQUEST_EXPR(        \
                                 (_dfltF),               \
                                 (_creqF),               \
                                 _arg1, 0,0,0,0);        \
      _resF = _qzz_res;                                  \
   } while (0)

#define DO_CREQ_v_WW(_creqF, _ty1F,_arg1F, _ty2F,_arg2F) \
   do {                                                  \
      long int _arg1, _arg2;                             \
         \
         \
      _arg1 = (long int)(_arg1F);                        \
      _arg2 = (long int)(_arg2F);                        \
      VALGRIND_DO_CLIENT_REQUEST_STMT(                   \
                                 (_creqF),               \
                                 _arg1,_arg2,0,0,0);     \
   } while (0)

#define DO_CREQ_v_WWW(_creqF, _ty1F,_arg1F,              \
                      _ty2F,_arg2F, _ty3F, _arg3F)       \
   do {                                                  \
      long int _arg1, _arg2, _arg3;                      \
         \
         \
         \
      _arg1 = (long int)(_arg1F);                        \
      _arg2 = (long int)(_arg2F);                        \
      _arg3 = (long int)(_arg3F);                        \
      VALGRIND_DO_CLIENT_REQUEST_STMT(                   \
                                 (_creqF),               \
                                 _arg1,_arg2,_arg3,0,0); \
   } while (0)

#define DO_CREQ_W_WWW(_resF, _dfltF, _creqF, _ty1F,_arg1F, \
                      _ty2F,_arg2F, _ty3F, _arg3F)       \
   do {                                                  \
      long int _qzz_res;                                 \
      long int _arg1, _arg2, _arg3;                      \
         \
      _arg1 = (long int)(_arg1F);                        \
      _arg2 = (long int)(_arg2F);                        \
      _arg3 = (long int)(_arg3F);                        \
      _qzz_res = VALGRIND_DO_CLIENT_REQUEST_EXPR(        \
                                 (_dfltF),               \
                                 (_creqF),               \
                                 _arg1,_arg2,_arg3,0,0); \
      _resF = _qzz_res;                                  \
   } while (0)



#define _HG_CLIENTREQ_UNIMP(_qzz_str)                    \
   DO_CREQ_v_W(_VG_USERREQ__HG_CLIENTREQ_UNIMP,          \
               (char*),(_qzz_str))




#define VALGRIND_HG_MUTEX_INIT_POST(_mutex, _mbRec)          \
   DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_MUTEX_INIT_POST,     \
                void*,(_mutex), long,(_mbRec))

#define VALGRIND_HG_MUTEX_LOCK_PRE(_mutex, _isTryLock)       \
   DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_PRE,      \
                void*,(_mutex), long,(_isTryLock))

#define VALGRIND_HG_MUTEX_LOCK_POST(_mutex)                  \
   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_POST,      \
               void*,(_mutex))

#define VALGRIND_HG_MUTEX_UNLOCK_PRE(_mutex)                 \
   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_UNLOCK_PRE,     \
               void*,(_mutex))

#define VALGRIND_HG_MUTEX_UNLOCK_POST(_mutex)                \
   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_UNLOCK_POST,    \
               void*,(_mutex))

#define VALGRIND_HG_MUTEX_DESTROY_PRE(_mutex)                \
   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_DESTROY_PRE,    \
               void*,(_mutex))


#define VALGRIND_HG_SEM_INIT_POST(_sem, _value)              \
   DO_CREQ_v_WW(_VG_USERREQ__HG_POSIX_SEM_INIT_POST,         \
                void*, (_sem), unsigned long, (_value))

#define VALGRIND_HG_SEM_WAIT_POST(_sem)                      \
   DO_CREQ_v_W(_VG_USERREQ__HG_POSIX_SEM_WAIT_POST,          \
               void*,(_sem))

#define VALGRIND_HG_SEM_POST_PRE(_sem)                       \
   DO_CREQ_v_W(_VG_USERREQ__HG_POSIX_SEM_POST_PRE,           \
               void*,(_sem))

#define VALGRIND_HG_SEM_DESTROY_PRE(_sem)                    \
   DO_CREQ_v_W(_VG_USERREQ__HG_POSIX_SEM_DESTROY_PRE,        \
               void*, (_sem))


#define VALGRIND_HG_BARRIER_INIT_PRE(_bar, _count, _resizable) \
   DO_CREQ_v_WWW(_VG_USERREQ__HG_PTHREAD_BARRIER_INIT_PRE,   \
                 void*,(_bar),                               \
                 unsigned long,(_count),                     \
                 unsigned long,(_resizable))

#define VALGRIND_HG_BARRIER_WAIT_PRE(_bar)                   \
   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_BARRIER_WAIT_PRE,     \
               void*,(_bar))

#define VALGRIND_HG_BARRIER_RESIZE_PRE(_bar, _newcount)      \
   DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_BARRIER_RESIZE_PRE,  \
                void*,(_bar),                                \
                unsigned long,(_newcount))

#define VALGRIND_HG_BARRIER_DESTROY_PRE(_bar)                \
   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_BARRIER_DESTROY_PRE,  \
               void*,(_bar))


#define VALGRIND_HG_CLEAN_MEMORY(_qzz_start, _qzz_len)       \
   DO_CREQ_v_WW(VG_USERREQ__HG_CLEAN_MEMORY,                 \
                void*,(_qzz_start),                          \
                unsigned long,(_qzz_len))

#define VALGRIND_HG_CLEAN_MEMORY_HEAPBLOCK(_qzz_blockstart)  \
   (__extension__                                            \
   ({long int _npainted;                                     \
     DO_CREQ_W_W(_npainted, (-2),                 \
                 _VG_USERREQ__HG_CLEAN_MEMORY_HEAPBLOCK,     \
                            void*,(_qzz_blockstart));        \
     _npainted;                                              \
   }))


#define VALGRIND_HG_DISABLE_CHECKING(_qzz_start, _qzz_len)   \
   DO_CREQ_v_WW(_VG_USERREQ__HG_ARANGE_MAKE_UNTRACKED,       \
                 void*,(_qzz_start),                         \
                 unsigned long,(_qzz_len))

#define VALGRIND_HG_ENABLE_CHECKING(_qzz_start, _qzz_len)    \
   DO_CREQ_v_WW(_VG_USERREQ__HG_ARANGE_MAKE_TRACKED,         \
                 void*,(_qzz_start),                         \
                 unsigned long,(_qzz_len))


#define VALGRIND_HG_ENABLE_CHECKING(_qzz_start, _qzz_len)    \
   DO_CREQ_v_WW(_VG_USERREQ__HG_ARANGE_MAKE_TRACKED,         \
                 void*,(_qzz_start),                         \
                 unsigned long,(_qzz_len))


#define VALGRIND_HG_GET_ABITS(zza,zzabits,zznbytes)          \
   (__extension__                                            \
   ({long int _res;                                          \
      DO_CREQ_W_WWW(_res, (-2),                   \
                    _VG_USERREQ__HG_GET_ABITS,               \
                    void*,(zza), void*,(zzabits),            \
                    unsigned long,(zznbytes));               \
      _res;                                                  \
   }))



#define ANNOTATE_CONDVAR_LOCK_WAIT(cv, lock) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_CONDVAR_LOCK_WAIT")

#define ANNOTATE_CONDVAR_WAIT(cv) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_CONDVAR_WAIT")

#define ANNOTATE_CONDVAR_SIGNAL(cv) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_CONDVAR_SIGNAL")
  
#define ANNOTATE_CONDVAR_SIGNAL_ALL(cv) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_CONDVAR_SIGNAL_ALL")


#define ANNOTATE_HAPPENS_BEFORE(obj) \
   DO_CREQ_v_W(_VG_USERREQ__HG_USERSO_SEND_PRE, void*,(obj))

#define ANNOTATE_HAPPENS_AFTER(obj) \
   DO_CREQ_v_W(_VG_USERREQ__HG_USERSO_RECV_POST, void*,(obj))

#define ANNOTATE_HAPPENS_BEFORE_FORGET_ALL(obj) \
   DO_CREQ_v_W(_VG_USERREQ__HG_USERSO_FORGET_ALL, void*,(obj))

#define ANNOTATE_PUBLISH_MEMORY_RANGE(pointer, size) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_PUBLISH_MEMORY_RANGE")




#define ANNOTATE_PURE_HAPPENS_BEFORE_MUTEX(mu) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_PURE_HAPPENS_BEFORE_MUTEX")



#define ANNOTATE_NEW_MEMORY(address, size) \
   VALGRIND_HG_CLEAN_MEMORY((address), (size))



#define ANNOTATE_PCQ_CREATE(pcq) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_PCQ_CREATE")

#define ANNOTATE_PCQ_DESTROY(pcq) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_PCQ_DESTROY")

#define ANNOTATE_PCQ_PUT(pcq) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_PCQ_PUT")

#define ANNOTATE_PCQ_GET(pcq) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_PCQ_GET")



#define ANNOTATE_BENIGN_RACE(pointer, description) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_BENIGN_RACE")

#define ANNOTATE_BENIGN_RACE_SIZED(address, size, description) \
   VALGRIND_HG_DISABLE_CHECKING(address, size)

#define ANNOTATE_IGNORE_READS_BEGIN() \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_IGNORE_READS_BEGIN")

#define ANNOTATE_IGNORE_READS_END() \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_IGNORE_READS_END")

#define ANNOTATE_IGNORE_WRITES_BEGIN() \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_IGNORE_WRITES_BEGIN")

#define ANNOTATE_IGNORE_WRITES_END() \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_IGNORE_WRITES_END")

#define ANNOTATE_IGNORE_READS_AND_WRITES_BEGIN() \
   do { \
      ANNOTATE_IGNORE_READS_BEGIN(); \
      ANNOTATE_IGNORE_WRITES_BEGIN(); \
   } while (0)

#define ANNOTATE_IGNORE_READS_AND_WRITES_END() \
   do { \
      ANNOTATE_IGNORE_WRITES_END(); \
      ANNOTATE_IGNORE_READS_END(); \
   } while (0)



#define ANNOTATE_TRACE_MEMORY(address) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_TRACE_MEMORY")

#define ANNOTATE_THREAD_NAME(name) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_THREAD_NAME")


#define ANNOTATE_RWLOCK_CREATE(lock)                         \
   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_RWLOCK_INIT_POST,     \
               void*,(lock))
    
#define ANNOTATE_RWLOCK_DESTROY(lock)                        \
   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_RWLOCK_DESTROY_PRE,   \
               void*,(lock))

#define ANNOTATE_RWLOCK_ACQUIRED(lock, is_w)                 \
  DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_RWLOCK_LOCK_POST,     \
               void*,(lock), unsigned long,(is_w))

#define ANNOTATE_RWLOCK_RELEASED(lock, is_w)                 \
  DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_RWLOCK_UNLOCK_PRE,     \
              void*,(lock)) 



#define ANNOTATE_BARRIER_INIT(barrier, count, reinitialization_allowed) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_BARRIER_INIT")

#define ANNOTATE_BARRIER_WAIT_BEFORE(barrier) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_BARRIER_DESTROY")

#define ANNOTATE_BARRIER_WAIT_AFTER(barrier) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_BARRIER_DESTROY")

#define ANNOTATE_BARRIER_DESTROY(barrier) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_BARRIER_DESTROY")



#define ANNOTATE_EXPECT_RACE(address, description) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_EXPECT_RACE")

#define ANNOTATE_NO_OP(arg) \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_NO_OP")

#define ANNOTATE_FLUSH_STATE() \
   _HG_CLIENTREQ_UNIMP("ANNOTATE_FLUSH_STATE")

#endif 
