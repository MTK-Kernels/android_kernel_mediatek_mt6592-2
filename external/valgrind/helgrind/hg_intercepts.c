

/*
   This file is part of Helgrind, a Valgrind tool for detecting errors
   in threaded programs.

   Copyright (C) 2007-2013 OpenWorks LLP
      info@open-works.co.uk

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

   Neither the names of the U.S. Department of Energy nor the
   University of California nor the names of its contributors may be
   used to endorse or promote products derived from this software
   without prior written permission.
*/




#include "pub_tool_basics.h"
#include "pub_tool_redir.h"
#include "pub_tool_clreq.h"
#include "helgrind.h"
#include "config.h"

#define TRACE_PTH_FNS 0
#define TRACE_QT4_FNS 0
#define TRACE_GNAT_FNS 0



#define PTH_FUNC(ret_ty, f, args...) \
   ret_ty I_WRAP_SONAME_FNNAME_ZZ(VG_Z_LIBPTHREAD_SONAME,f)(args); \
   ret_ty I_WRAP_SONAME_FNNAME_ZZ(VG_Z_LIBPTHREAD_SONAME,f)(args)



#undef DO_CREQ_v_W
#undef DO_CREQ_v_WW
#undef DO_CREQ_W_WW
#undef DO_CREQ_v_WWW

#define DO_CREQ_v_W(_creqF, _ty1F,_arg1F)                \
   do {                                                  \
      Word _arg1;                                        \
      assert(sizeof(_ty1F) == sizeof(Word));             \
      _arg1 = (Word)(_arg1F);                            \
      VALGRIND_DO_CLIENT_REQUEST_STMT((_creqF),          \
                                 _arg1, 0,0,0,0);        \
   } while (0)

#define DO_CREQ_v_WW(_creqF, _ty1F,_arg1F, _ty2F,_arg2F) \
   do {                                                  \
      Word _arg1, _arg2;                                 \
      assert(sizeof(_ty1F) == sizeof(Word));             \
      assert(sizeof(_ty2F) == sizeof(Word));             \
      _arg1 = (Word)(_arg1F);                            \
      _arg2 = (Word)(_arg2F);                            \
      VALGRIND_DO_CLIENT_REQUEST_STMT((_creqF),          \
                                 _arg1,_arg2,0,0,0);     \
   } while (0)

#define DO_CREQ_W_WW(_resF, _creqF, _ty1F,_arg1F,        \
                     _ty2F,_arg2F)                       \
   do {                                                  \
      Word _res, _arg1, _arg2;                           \
      assert(sizeof(_ty1F) == sizeof(Word));             \
      assert(sizeof(_ty2F) == sizeof(Word));             \
      _arg1 = (Word)(_arg1F);                            \
      _arg2 = (Word)(_arg2F);                            \
      _res = VALGRIND_DO_CLIENT_REQUEST_EXPR(2,          \
                                 (_creqF),               \
                                 _arg1,_arg2,0,0,0);     \
      _resF = _res;                                      \
   } while (0)

#define DO_CREQ_v_WWW(_creqF, _ty1F,_arg1F,              \
                      _ty2F,_arg2F, _ty3F, _arg3F)       \
   do {                                                  \
      Word _arg1, _arg2, _arg3;                          \
      assert(sizeof(_ty1F) == sizeof(Word));             \
      assert(sizeof(_ty2F) == sizeof(Word));             \
      assert(sizeof(_ty3F) == sizeof(Word));             \
      _arg1 = (Word)(_arg1F);                            \
      _arg2 = (Word)(_arg2F);                            \
      _arg3 = (Word)(_arg3F);                            \
      VALGRIND_DO_CLIENT_REQUEST_STMT((_creqF),          \
                                 _arg1,_arg2,_arg3,0,0); \
   } while (0)


#define DO_PthAPIerror(_fnnameF, _errF)                  \
   do {                                                  \
      const char* _fnname = (_fnnameF);                  \
      long  _err    = (long)(int)(_errF);                \
      const char* _errstr = lame_strerror(_err);         \
      DO_CREQ_v_WWW(_VG_USERREQ__HG_PTH_API_ERROR,       \
                    char*,_fnname,                       \
                    long,_err, char*,_errstr);           \
   } while (0)


#define _GNU_SOURCE 1

#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

__attribute__((noinline))
static int my_memcmp ( const void* ptr1, const void* ptr2, size_t size)
{
   const unsigned char* uchar_ptr1 = (const unsigned char*) ptr1;
   const unsigned char* uchar_ptr2 = (const unsigned char*) ptr2;
   size_t i;
   for (i = 0; i < size; ++i) {
      if (uchar_ptr1[i] != uchar_ptr2[i])
         return (uchar_ptr1[i] < uchar_ptr2[i]) ? -1 : 1;
   }
   return 0;
}

static const HChar* lame_strerror ( long err )
{
   switch (err) {
      case EPERM:       return "EPERM: Operation not permitted";
      case ENOENT:      return "ENOENT: No such file or directory";
      case ESRCH:       return "ESRCH: No such process";
      case EINTR:       return "EINTR: Interrupted system call";
      case EBADF:       return "EBADF: Bad file number";
      case EAGAIN:      return "EAGAIN: Try again";
      case ENOMEM:      return "ENOMEM: Out of memory";
      case EACCES:      return "EACCES: Permission denied";
      case EFAULT:      return "EFAULT: Bad address";
      case EEXIST:      return "EEXIST: File exists";
      case EINVAL:      return "EINVAL: Invalid argument";
      case EMFILE:      return "EMFILE: Too many open files";
      case ENOSYS:      return "ENOSYS: Function not implemented";
      case EOVERFLOW:   return "EOVERFLOW: Value too large "
                               "for defined data type";
      case EBUSY:       return "EBUSY: Device or resource busy";
      case ETIMEDOUT:   return "ETIMEDOUT: Connection timed out";
      case EDEADLK:     return "EDEADLK: Resource deadlock would occur";
      case EOPNOTSUPP:  return "EOPNOTSUPP: Operation not supported on "
                               "transport endpoint"; 
      default:          return "tc_intercepts.c: lame_strerror(): "
                               "unhandled case -- please fix me!";
   }
}



static void* mythread_wrapper ( void* xargsV )
{
   volatile Word* xargs = (volatile Word*) xargsV;
   void*(*fn)(void*) = (void*(*)(void*))xargs[0];
   void* arg         = (void*)xargs[1];
   pthread_t me = pthread_self();
   
   DO_CREQ_v_W(_VG_USERREQ__HG_SET_MY_PTHREAD_T, pthread_t,me);
   xargs[2] = 0;
   
   return (void*) fn( (void*)arg );
}

__attribute__((noinline))
static int pthread_create_WRK(pthread_t *thread, const pthread_attr_t *attr,
                              void *(*start) (void *), void *arg)
{
   int    ret;
   OrigFn fn;
   volatile Word xargs[3];

   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_create wrapper"); fflush(stderr);
   }
   xargs[0] = (Word)start;
   xargs[1] = (Word)arg;
   xargs[2] = 1; 
   VALGRIND_HG_DISABLE_CHECKING(&xargs, sizeof(xargs));

   CALL_FN_W_WWWW(ret, fn, thread,attr,mythread_wrapper,&xargs[0]);

   if (ret == 0) {
      while (xargs[2] != 0) {
         sched_yield();
      }
   } else { 
      DO_PthAPIerror( "pthread_create", ret );
   }

   VALGRIND_HG_ENABLE_CHECKING(&xargs, sizeof(xargs));

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " :: pth_create -> %d >>\n", ret);
   }
   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, pthreadZucreateZAZa, 
                 pthread_t *thread, const pthread_attr_t *attr,
                 void *(*start) (void *), void *arg) {
      return pthread_create_WRK(thread, attr, start, arg);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, pthreadZucreate, 
                 pthread_t *thread, const pthread_attr_t *attr,
                 void *(*start) (void *), void *arg) {
      return pthread_create_WRK(thread, attr, start, arg);
   }
   PTH_FUNC(int, pthreadZucreateZuZa, 
                 pthread_t *thread, const pthread_attr_t *attr,
                 void *(*start) (void *), void *arg) {
      
      assert(0);
   }
#else
#  error "Unsupported OS"
#endif


__attribute__((noinline))
static int pthread_join_WRK(pthread_t thread, void** value_pointer)
{
   int ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_join wrapper"); fflush(stderr);
   }

   CALL_FN_W_WW(ret, fn, thread,value_pointer);

   if (ret == 0 ) {
      DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_JOIN_POST, pthread_t,thread);
   } else { 
      DO_PthAPIerror( "pthread_join", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " :: pth_join -> %d >>\n", ret);
   }
   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, pthreadZujoin, 
            pthread_t thread, void** value_pointer) {
      return pthread_join_WRK(thread, value_pointer);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, pthreadZujoinZa, 
            pthread_t thread, void** value_pointer) {
      return pthread_join_WRK(thread, value_pointer);
   }
#else
#  error "Unsupported OS"
#endif




void I_WRAP_SONAME_FNNAME_ZU
   (Za,
    system__tasking__debug__master_hook)
     (void *dependent, void *master, int master_level);
void I_WRAP_SONAME_FNNAME_ZU
   (Za,
    system__tasking__debug__master_hook)
     (void *dependent, void *master, int master_level)
{
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_GNAT_FNS) {
     fprintf(stderr, "<< GNAT master_hook wrapper "
             "dependent %p master %p master_level %d\n",
             dependent, master, master_level); fflush(stderr);
   }

   
   CALL_FN_v_WWW(fn, dependent, master, master_level);

   DO_CREQ_v_WWW(_VG_USERREQ__HG_GNAT_MASTER_HOOK,
                 void*,dependent, void*,master, 
                 Word, (Word)master_level);

   if (TRACE_GNAT_FNS) {
      fprintf(stderr, " :: GNAT master_hook >>\n");
   }
}

void I_WRAP_SONAME_FNNAME_ZU
   (Za,
    system__tasking__debug__master_completed_hook)
     (void *self_id, int master_level);
void I_WRAP_SONAME_FNNAME_ZU
   (Za,
    system__tasking__debug__master_completed_hook)
     (void *self_id, int master_level)
{
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_GNAT_FNS) {
     fprintf(stderr, "<< GNAT master_completed_hook wrapper "
             "self_id %p master_level %d\n",
             self_id, master_level); fflush(stderr);
   }

   
   CALL_FN_v_WW(fn, self_id, master_level);

   DO_CREQ_v_WW(_VG_USERREQ__HG_GNAT_MASTER_COMPLETED_HOOK,
                 void*,self_id, Word,(Word)master_level);

   if (TRACE_GNAT_FNS) {
      fprintf(stderr, " :: GNAT master_completed_hook >>\n");
   }
}



PTH_FUNC(int, pthreadZumutexZuinit, 
              pthread_mutex_t *mutex,
              pthread_mutexattr_t* attr)
{
   int    ret;
   long   mbRec;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_mxinit %p", mutex); fflush(stderr);
   }

   mbRec = 0;
   if (attr) {
      int ty, zzz;
      zzz = pthread_mutexattr_gettype(attr, &ty);
      if (zzz == 0 && ty == PTHREAD_MUTEX_RECURSIVE)
         mbRec = 1;
   }

   CALL_FN_W_WW(ret, fn, mutex,attr);

   if (ret == 0 ) {
      DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_MUTEX_INIT_POST,
                   pthread_mutex_t*,mutex, long,mbRec);
   } else { 
      DO_PthAPIerror( "pthread_mutex_init", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " :: mxinit -> %d >>\n", ret);
   }
   return ret;
}


PTH_FUNC(int, pthreadZumutexZudestroy, 
              pthread_mutex_t *mutex)
{
   int    ret;
   unsigned long mutex_is_init;
   OrigFn fn;

   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_mxdestroy %p", mutex); fflush(stderr);
   }

   if (mutex != NULL) {
      static const pthread_mutex_t mutex_init = PTHREAD_MUTEX_INITIALIZER;
      mutex_is_init = my_memcmp(mutex, &mutex_init, sizeof(*mutex)) == 0;
   } else {
      mutex_is_init = 0;
   }

   DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_MUTEX_DESTROY_PRE,
                pthread_mutex_t*, mutex, unsigned long, mutex_is_init);

   CALL_FN_W_W(ret, fn, mutex);

   if (ret != 0) {
      DO_PthAPIerror( "pthread_mutex_destroy", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " :: mxdestroy -> %d >>\n", ret);
   }
   return ret;
}


PTH_FUNC(int, pthreadZumutexZulock, 
              pthread_mutex_t *mutex)
{
   int    ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_mxlock %p", mutex); fflush(stderr);
   }

   DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_PRE,
                pthread_mutex_t*,mutex, long,0);

   CALL_FN_W_W(ret, fn, mutex);


   if (ret == 0 ) {
      DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_POST,
                  pthread_mutex_t*,mutex);
   } else { 
      DO_PthAPIerror( "pthread_mutex_lock", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " :: mxlock -> %d >>\n", ret);
   }
   return ret;
}


PTH_FUNC(int, pthreadZumutexZutrylock, 
              pthread_mutex_t *mutex)
{
   int    ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_mxtrylock %p", mutex); fflush(stderr);
   }

   DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_PRE,
                pthread_mutex_t*,mutex, long,1);

   CALL_FN_W_W(ret, fn, mutex);


   if (ret == 0 ) {
      DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_POST,
                  pthread_mutex_t*,mutex);
   } else { 
      if (ret != EBUSY)
         DO_PthAPIerror( "pthread_mutex_trylock", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " :: mxtrylock -> %d >>\n", ret);
   }
   return ret;
}


PTH_FUNC(int, pthreadZumutexZutimedlock, 
         pthread_mutex_t *mutex,
         void* timeout)
{
   int    ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_mxtimedlock %p %p", mutex, timeout); 
      fflush(stderr);
   }

   DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_PRE,
                pthread_mutex_t*,mutex, long,1);

   CALL_FN_W_WW(ret, fn, mutex,timeout);


   if (ret == 0 ) {
      DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_POST,
                  pthread_mutex_t*,mutex);
   } else { 
      if (ret != ETIMEDOUT)
         DO_PthAPIerror( "pthread_mutex_timedlock", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " :: mxtimedlock -> %d >>\n", ret);
   }
   return ret;
}


PTH_FUNC(int, pthreadZumutexZuunlock, 
              pthread_mutex_t *mutex)
{
   int    ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);

   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_mxunlk %p", mutex); fflush(stderr);
   }

   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_UNLOCK_PRE,
               pthread_mutex_t*,mutex);

   CALL_FN_W_W(ret, fn, mutex);

   if (ret == 0 ) {
      DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_UNLOCK_POST,
                  pthread_mutex_t*,mutex);
   } else { 
      DO_PthAPIerror( "pthread_mutex_unlock", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " mxunlk -> %d >>\n", ret);
   }
   return ret;
}




__attribute__((noinline))
static int pthread_cond_wait_WRK(pthread_cond_t* cond,
                                 pthread_mutex_t* mutex)
{
   int ret;
   OrigFn fn;
   unsigned long mutex_is_valid;

   VALGRIND_GET_ORIG_FN(fn);

   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_cond_wait %p %p", cond, mutex);
      fflush(stderr);
   }

   DO_CREQ_W_WW(mutex_is_valid,
                _VG_USERREQ__HG_PTHREAD_COND_WAIT_PRE,
                pthread_cond_t*,cond, pthread_mutex_t*,mutex);
   assert(mutex_is_valid == 1 || mutex_is_valid == 0);

   if (mutex_is_valid) {
      DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_UNLOCK_PRE,
                  pthread_mutex_t*,mutex);
   }

   CALL_FN_W_WW(ret, fn, cond,mutex);

   if (ret == 0 && mutex_is_valid) {
      
      DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_POST,
                  pthread_mutex_t*,mutex);
   }

   if (ret == 0 && mutex_is_valid) {
      DO_CREQ_v_WWW(_VG_USERREQ__HG_PTHREAD_COND_WAIT_POST,
                    pthread_cond_t*,cond, pthread_mutex_t*,mutex, long,0);
   }

   if (ret != 0) {
      DO_PthAPIerror( "pthread_cond_wait", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " cowait -> %d >>\n", ret);
   }

   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, pthreadZucondZuwaitZAZa, 
                 pthread_cond_t* cond, pthread_mutex_t* mutex) {
      return pthread_cond_wait_WRK(cond, mutex);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, pthreadZucondZuwaitZa, 
                 pthread_cond_t* cond, pthread_mutex_t* mutex) {
      return pthread_cond_wait_WRK(cond, mutex);
   }
#else
#  error "Unsupported OS"
#endif


__attribute__((noinline))
static int pthread_cond_timedwait_WRK(pthread_cond_t* cond,
                                      pthread_mutex_t* mutex, 
                                      struct timespec* abstime)
{
   int ret;
   OrigFn fn;
   unsigned long mutex_is_valid;
   Bool abstime_is_valid;
   VALGRIND_GET_ORIG_FN(fn);

   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_cond_timedwait %p %p %p", 
                      cond, mutex, abstime);
      fflush(stderr);
   }

   DO_CREQ_W_WW(mutex_is_valid,
                _VG_USERREQ__HG_PTHREAD_COND_WAIT_PRE,
                pthread_cond_t*,cond, pthread_mutex_t*,mutex);
   assert(mutex_is_valid == 1 || mutex_is_valid == 0);

   abstime_is_valid = abstime->tv_nsec >= 0 && abstime->tv_nsec < 1000000000;

   if (mutex_is_valid && abstime_is_valid) {
      DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_UNLOCK_PRE,
                  pthread_mutex_t*,mutex);
   }

   CALL_FN_W_WWW(ret, fn, cond,mutex,abstime);

   if (!abstime_is_valid && ret != EINVAL) {
      DO_PthAPIerror("Bug in libpthread: pthread_cond_timedwait "
                     "invalid abstime did not cause"
                     " EINVAL", ret);
   }

   if ((ret == 0 || ret == ETIMEDOUT) && mutex_is_valid) {
      
      DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_POST,
                  pthread_mutex_t*,mutex);
   }

   if ((ret == 0 || ret == ETIMEDOUT) && mutex_is_valid) {
      DO_CREQ_v_WWW(_VG_USERREQ__HG_PTHREAD_COND_WAIT_POST,
                    pthread_cond_t*,cond, pthread_mutex_t*,mutex,
                    long,ret == ETIMEDOUT);
   }

   if (ret != 0 && ret != ETIMEDOUT) {
      DO_PthAPIerror( "pthread_cond_timedwait", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " cotimedwait -> %d >>\n", ret);
   }

   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, pthreadZucondZutimedwaitZAZa, 
                 pthread_cond_t* cond, pthread_mutex_t* mutex, 
                 struct timespec* abstime) {
      return pthread_cond_timedwait_WRK(cond, mutex, abstime);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, pthreadZucondZutimedwait, 
                 pthread_cond_t* cond, pthread_mutex_t* mutex, 
                 struct timespec* abstime) {
      return pthread_cond_timedwait_WRK(cond, mutex, abstime);
   }
   PTH_FUNC(int, pthreadZucondZutimedwaitZDZa, 
                 pthread_cond_t* cond, pthread_mutex_t* mutex, 
                 struct timespec* abstime) {
      return pthread_cond_timedwait_WRK(cond, mutex, abstime);
   }
   PTH_FUNC(int, pthreadZucondZutimedwaitZuZa, 
                 pthread_cond_t* cond, pthread_mutex_t* mutex, 
                 struct timespec* abstime) {
      assert(0);
   }
#else
#  error "Unsupported OS"
#endif


__attribute__((noinline))
static int pthread_cond_signal_WRK(pthread_cond_t* cond)
{
   int ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);

   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_cond_signal %p", cond);
      fflush(stderr);
   }

   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_COND_SIGNAL_PRE,
               pthread_cond_t*,cond);

   CALL_FN_W_W(ret, fn, cond);

   if (ret != 0) {
      DO_PthAPIerror( "pthread_cond_signal", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " cosig -> %d >>\n", ret);
   }

   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, pthreadZucondZusignalZAZa, 
                 pthread_cond_t* cond) {
      return pthread_cond_signal_WRK(cond);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, pthreadZucondZusignal, 
                 pthread_cond_t* cond) {
      return pthread_cond_signal_WRK(cond);
   }
#else
#  error "Unsupported OS"
#endif


__attribute__((noinline))
static int pthread_cond_broadcast_WRK(pthread_cond_t* cond)
{
   int ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);

   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_cond_broadcast %p", cond);
      fflush(stderr);
   }

   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_COND_BROADCAST_PRE,
               pthread_cond_t*,cond);

   CALL_FN_W_W(ret, fn, cond);

   if (ret != 0) { 
      DO_PthAPIerror( "pthread_cond_broadcast", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " cobro -> %d >>\n", ret);
   }

   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, pthreadZucondZubroadcastZAZa, 
                 pthread_cond_t* cond) {
      return pthread_cond_broadcast_WRK(cond);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, pthreadZucondZubroadcast, 
                 pthread_cond_t* cond) {
      return pthread_cond_broadcast_WRK(cond);
   }
#else
#   error "Unsupported OS"
#endif

__attribute__((noinline))
static int pthread_cond_init_WRK(pthread_cond_t* cond, pthread_condattr_t *cond_attr)
{
   int ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);

   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_cond_init %p", cond);
      fflush(stderr);
   }

   CALL_FN_W_WW(ret, fn, cond, cond_attr);

   if (ret == 0) {
      DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_COND_INIT_POST,
                   pthread_cond_t*,cond, pthread_condattr_t*, cond_attr);
   } else {
      DO_PthAPIerror( "pthread_cond_init", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " coinit -> %d >>\n", ret);
   }

   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, pthreadZucondZuinitZAZa, 
	    pthread_cond_t* cond, pthread_condattr_t* cond_attr) {
     return pthread_cond_init_WRK(cond, cond_attr);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, pthreadZucondZuinit, 
	    pthread_cond_t* cond, pthread_condattr_t * cond_attr) {
     return pthread_cond_init_WRK(cond, cond_attr);
   }
#else
#  error "Unsupported OS"
#endif


__attribute__((noinline))
static int pthread_cond_destroy_WRK(pthread_cond_t* cond)
{
   int ret;
   unsigned long cond_is_init;
   OrigFn fn;

   VALGRIND_GET_ORIG_FN(fn);

   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_cond_destroy %p", cond);
      fflush(stderr);
   }

   if (cond != NULL) {
      const pthread_cond_t cond_init = PTHREAD_COND_INITIALIZER;
      cond_is_init = my_memcmp(cond, &cond_init, sizeof(*cond)) == 0;
   } else {
     cond_is_init = 0;
   }

   DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_COND_DESTROY_PRE,
                pthread_cond_t*, cond, unsigned long, cond_is_init);

   CALL_FN_W_W(ret, fn, cond);

   if (ret != 0) {
      DO_PthAPIerror( "pthread_cond_destroy", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " codestr -> %d >>\n", ret);
   }

   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, pthreadZucondZudestroyZAZa, 
                 pthread_cond_t* cond) {
      return pthread_cond_destroy_WRK(cond);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, pthreadZucondZudestroy, 
                 pthread_cond_t* cond) {
      return pthread_cond_destroy_WRK(cond);
   }
#else
#  error "Unsupported OS"
#endif



#if defined(HAVE_PTHREAD_BARRIER_INIT)


PTH_FUNC(int, pthreadZubarrierZuinit, 
         pthread_barrier_t* bar,
         pthread_barrierattr_t* attr, unsigned long count)
{
   int ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);

   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_barrier_init %p %p %lu",
                      bar, attr, count);
      fflush(stderr);
   }

   DO_CREQ_v_WWW(_VG_USERREQ__HG_PTHREAD_BARRIER_INIT_PRE,
                 pthread_barrier_t*, bar,
                 unsigned long, count,
                 unsigned long, 0);

   CALL_FN_W_WWW(ret, fn, bar,attr,count);

   if (ret != 0) {
      DO_PthAPIerror( "pthread_barrier_init", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, "  pthread_barrier_init -> %d >>\n", ret);
   }

   return ret;
}


PTH_FUNC(int, pthreadZubarrierZuwait, 
              pthread_barrier_t* bar)
{
   int ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);

   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_barrier_wait %p", bar);
      fflush(stderr);
   }

   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_BARRIER_WAIT_PRE,
               pthread_barrier_t*,bar);

   CALL_FN_W_W(ret, fn, bar);

   if (ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD) {
      DO_PthAPIerror( "pthread_barrier_wait", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, "  pthread_barrier_wait -> %d >>\n", ret);
   }

   return ret;
}


PTH_FUNC(int, pthreadZubarrierZudestroy, 
         pthread_barrier_t* bar)
{
   int ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);

   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_barrier_destroy %p", bar);
      fflush(stderr);
   }

   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_BARRIER_DESTROY_PRE,
               pthread_barrier_t*,bar);

   CALL_FN_W_W(ret, fn, bar);

   if (ret != 0) {
      DO_PthAPIerror( "pthread_barrier_destroy", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, "  pthread_barrier_destroy -> %d >>\n", ret);
   }

   return ret;
}

#endif   



#if defined(HAVE_PTHREAD_SPIN_LOCK) \
    && !defined(DISABLE_PTHREAD_SPINLOCK_INTERCEPT)



__attribute__((noinline))
static int pthread_spin_init_or_unlock_WRK(pthread_spinlock_t* lock,
                                           int pshared) {
   int    ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_spin_iORu %p", lock); fflush(stderr);
   }

   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_SPIN_INIT_OR_UNLOCK_PRE,
               pthread_spinlock_t*, lock);

   CALL_FN_W_WW(ret, fn, lock,pshared);

   if (ret == 0 ) {
      DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_SPIN_INIT_OR_UNLOCK_POST,
                  pthread_spinlock_t*,lock);
   } else { 
      DO_PthAPIerror( "pthread_spinlock_{init,unlock}", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " :: spiniORu -> %d >>\n", ret);
   }
   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, pthreadZuspinZuinit, 
            pthread_spinlock_t* lock, int pshared) {
      return pthread_spin_init_or_unlock_WRK(lock, pshared);
   }
   PTH_FUNC(int, pthreadZuspinZuunlock, 
            pthread_spinlock_t* lock) {
      
      return pthread_spin_init_or_unlock_WRK(lock, 0);
   }
#elif defined(VGO_darwin)
#else
#  error "Unsupported OS"
#endif


#if defined(VGO_linux)

PTH_FUNC(int, pthreadZuspinZudestroy, 
              pthread_spinlock_t* lock)
{
   int    ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_spin_destroy %p", lock);
      fflush(stderr);
   }

   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_SPIN_DESTROY_PRE,
               pthread_spinlock_t*,lock);

   CALL_FN_W_W(ret, fn, lock);

   if (ret != 0) {
      DO_PthAPIerror( "pthread_spin_destroy", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " :: spindestroy -> %d >>\n", ret);
   }
   return ret;
}

#elif defined(VGO_darwin)
#else
#  error "Unsupported OS"
#endif


#if defined(VGO_linux)

PTH_FUNC(int, pthreadZuspinZulock, 
              pthread_spinlock_t* lock)
{
   int    ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_spinlock %p", lock);
      fflush(stderr);
   }

   DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_SPIN_LOCK_PRE,
                pthread_spinlock_t*,lock, long,0);

   CALL_FN_W_W(ret, fn, lock);


   if (ret == 0 ) {
      DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_SPIN_LOCK_POST,
                  pthread_spinlock_t*,lock);
   } else { 
      DO_PthAPIerror( "pthread_spin_lock", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " :: spinlock -> %d >>\n", ret);
   }
   return ret;
}

#elif defined(VGO_darwin)
#else
#  error "Unsupported OS"
#endif


#if defined(VGO_linux)

PTH_FUNC(int, pthreadZuspinZutrylock, 
              pthread_spinlock_t* lock)
{
   int    ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_spin_trylock %p", lock);
      fflush(stderr);
   }

   DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_SPIN_LOCK_PRE,
                pthread_spinlock_t*,lock, long,1);

   CALL_FN_W_W(ret, fn, lock);


   if (ret == 0 ) {
      DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_SPIN_LOCK_POST,
                  pthread_spinlock_t*,lock);
   } else {
      if (ret != EBUSY)
         DO_PthAPIerror( "pthread_spin_trylock", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " :: spin_trylock -> %d >>\n", ret);
   }
   return ret;
}

#elif defined(VGO_darwin)
#else
#  error "Unsupported OS"
#endif

#endif 



#if defined(HAVE_PTHREAD_RWLOCK_T)


__attribute__((noinline))
static int pthread_rwlock_init_WRK(pthread_rwlock_t *rwl,
                                   pthread_rwlockattr_t* attr)
{
   int    ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_rwl_init %p", rwl); fflush(stderr);
   }

   CALL_FN_W_WW(ret, fn, rwl,attr);

   if (ret == 0 ) {
      DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_RWLOCK_INIT_POST,
                  pthread_rwlock_t*,rwl);
   } else { 
      DO_PthAPIerror( "pthread_rwlock_init", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " :: rwl_init -> %d >>\n", ret);
   }
   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, pthreadZurwlockZuinit, 
                 pthread_rwlock_t *rwl,
                 pthread_rwlockattr_t* attr) {
      return pthread_rwlock_init_WRK(rwl, attr);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, pthreadZurwlockZuinitZa, 
                 pthread_rwlock_t *rwl,
                 pthread_rwlockattr_t* attr) {
      return pthread_rwlock_init_WRK(rwl, attr);
   }
#else
#  error "Unsupported OS"
#endif


__attribute__((noinline))
static int pthread_rwlock_destroy_WRK(pthread_rwlock_t* rwl)
{
   int    ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_rwl_destroy %p", rwl); fflush(stderr);
   }

   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_RWLOCK_DESTROY_PRE,
               pthread_rwlock_t*,rwl);

   CALL_FN_W_W(ret, fn, rwl);

   if (ret != 0) {
      DO_PthAPIerror( "pthread_rwlock_destroy", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " :: rwl_destroy -> %d >>\n", ret);
   }
   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, pthreadZurwlockZudestroy, 
                 pthread_rwlock_t *rwl) {
      return pthread_rwlock_destroy_WRK(rwl);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, pthreadZurwlockZudestroyZa, 
                 pthread_rwlock_t *rwl) {
      return pthread_rwlock_destroy_WRK(rwl);
   }
#else
#  error "Unsupported OS"
#endif


__attribute__((noinline))
static int pthread_rwlock_wrlock_WRK(pthread_rwlock_t* rwlock)
{
   int    ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_rwl_wlk %p", rwlock); fflush(stderr);
   }

   DO_CREQ_v_WWW(_VG_USERREQ__HG_PTHREAD_RWLOCK_LOCK_PRE,
                 pthread_rwlock_t*,rwlock, 
                 long,1, long,0);

   CALL_FN_W_W(ret, fn, rwlock);

   if (ret == 0 ) {
      DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_RWLOCK_LOCK_POST,
                   pthread_rwlock_t*,rwlock, long,1);
   } else { 
      DO_PthAPIerror( "pthread_rwlock_wrlock", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " :: rwl_wlk -> %d >>\n", ret);
   }
   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, pthreadZurwlockZuwrlock, 
                 pthread_rwlock_t* rwlock) {
      return pthread_rwlock_wrlock_WRK(rwlock);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, pthreadZurwlockZuwrlockZa, 
                 pthread_rwlock_t* rwlock) {
      return pthread_rwlock_wrlock_WRK(rwlock);
   }
#else
#  error "Unsupported OS"
#endif


__attribute__((noinline))
static int pthread_rwlock_rdlock_WRK(pthread_rwlock_t* rwlock)
{
   int    ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_rwl_rlk %p", rwlock); fflush(stderr);
   }

   DO_CREQ_v_WWW(_VG_USERREQ__HG_PTHREAD_RWLOCK_LOCK_PRE,
                 pthread_rwlock_t*,rwlock,
                 long,0, long,0);

   CALL_FN_W_W(ret, fn, rwlock);

   if (ret == 0 ) {
      DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_RWLOCK_LOCK_POST,
                   pthread_rwlock_t*,rwlock, long,0);
   } else { 
      DO_PthAPIerror( "pthread_rwlock_rdlock", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " :: rwl_rlk -> %d >>\n", ret);
   }
   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, pthreadZurwlockZurdlock, 
                 pthread_rwlock_t* rwlock) {
      return pthread_rwlock_rdlock_WRK(rwlock);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, pthreadZurwlockZurdlockZa, 
                 pthread_rwlock_t* rwlock) {
      return pthread_rwlock_rdlock_WRK(rwlock);
   }
#else
#  error "Unsupported OS"
#endif


__attribute__((noinline))
static int pthread_rwlock_trywrlock_WRK(pthread_rwlock_t* rwlock)
{
   int    ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_rwl_trywlk %p", rwlock); fflush(stderr);
   }

   DO_CREQ_v_WWW(_VG_USERREQ__HG_PTHREAD_RWLOCK_LOCK_PRE,
                 pthread_rwlock_t*,rwlock, 
                 long,1, long,1);

   CALL_FN_W_W(ret, fn, rwlock);


   if (ret == 0 ) {
      DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_RWLOCK_LOCK_POST,
                   pthread_rwlock_t*,rwlock, long,1);
   } else { 
      if (ret != EBUSY)
         DO_PthAPIerror( "pthread_rwlock_trywrlock", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " :: rwl_trywlk -> %d >>\n", ret);
   }
   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, pthreadZurwlockZutrywrlock, 
                 pthread_rwlock_t* rwlock) {
      return pthread_rwlock_trywrlock_WRK(rwlock);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, pthreadZurwlockZutrywrlockZa, 
                 pthread_rwlock_t* rwlock) {
      return pthread_rwlock_trywrlock_WRK(rwlock);
   }
#else
#  error "Unsupported OS"
#endif


__attribute__((noinline))
static int pthread_rwlock_tryrdlock_WRK(pthread_rwlock_t* rwlock)
{
   int    ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_rwl_tryrlk %p", rwlock); fflush(stderr);
   }

   DO_CREQ_v_WWW(_VG_USERREQ__HG_PTHREAD_RWLOCK_LOCK_PRE,
                 pthread_rwlock_t*,rwlock, 
                 long,0, long,1);

   CALL_FN_W_W(ret, fn, rwlock);


   if (ret == 0 ) {
      DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_RWLOCK_LOCK_POST,
                   pthread_rwlock_t*,rwlock, long,0);
   } else { 
      if (ret != EBUSY)
         DO_PthAPIerror( "pthread_rwlock_tryrdlock", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " :: rwl_tryrlk -> %d >>\n", ret);
   }
   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, pthreadZurwlockZutryrdlock, 
                 pthread_rwlock_t* rwlock) {
      return pthread_rwlock_tryrdlock_WRK(rwlock);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, pthreadZurwlockZutryrdlockZa, 
                 pthread_rwlock_t* rwlock) {
      return pthread_rwlock_tryrdlock_WRK(rwlock);
   }
#else
#  error "Unsupported OS"
#endif


__attribute__((noinline))
static int pthread_rwlock_unlock_WRK(pthread_rwlock_t* rwlock)
{
   int    ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_PTH_FNS) {
      fprintf(stderr, "<< pthread_rwl_unlk %p", rwlock); fflush(stderr);
   }

   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_RWLOCK_UNLOCK_PRE,
               pthread_rwlock_t*,rwlock);

   CALL_FN_W_W(ret, fn, rwlock);

   if (ret == 0 ) {
      DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_RWLOCK_UNLOCK_POST,
                  pthread_rwlock_t*,rwlock);
   } else { 
      DO_PthAPIerror( "pthread_rwlock_unlock", ret );
   }

   if (TRACE_PTH_FNS) {
      fprintf(stderr, " :: rwl_unlk -> %d >>\n", ret);
   }
   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, pthreadZurwlockZuunlock, 
                 pthread_rwlock_t* rwlock) {
      return pthread_rwlock_unlock_WRK(rwlock);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, pthreadZurwlockZuunlockZa, 
                 pthread_rwlock_t* rwlock) {
      return pthread_rwlock_unlock_WRK(rwlock);
   }
#else
#  error "Unsupported OS"
#endif

#endif 



#include <semaphore.h>
#include <fcntl.h>       

#define TRACE_SEM_FNS 0


__attribute__((noinline))
static int sem_init_WRK(sem_t* sem, int pshared, unsigned long value)
{
   OrigFn fn;
   int    ret;
   VALGRIND_GET_ORIG_FN(fn);

   if (TRACE_SEM_FNS) {
      fprintf(stderr, "<< sem_init(%p,%d,%lu) ", sem,pshared,value);
      fflush(stderr);
   }

   CALL_FN_W_WWW(ret, fn, sem,pshared,value);

   if (ret == 0) {
      DO_CREQ_v_WW(_VG_USERREQ__HG_POSIX_SEM_INIT_POST,
                   sem_t*, sem, unsigned long, value);
   } else {
      DO_PthAPIerror( "sem_init", errno );
   }

   if (TRACE_SEM_FNS) {
      fprintf(stderr, " sem_init -> %d >>\n", ret);
      fflush(stderr);
   }

   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, semZuinitZAZa, 
                 sem_t* sem, int pshared, unsigned long value) {
      return sem_init_WRK(sem, pshared, value);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, semZuinit, 
                 sem_t* sem, int pshared, unsigned long value) {
      return sem_init_WRK(sem, pshared, value);
   }
#else
#  error "Unsupported OS"
#endif


__attribute__((noinline))
static int sem_destroy_WRK(sem_t* sem)
{
   OrigFn fn;
   int    ret;
   VALGRIND_GET_ORIG_FN(fn);

   if (TRACE_SEM_FNS) {
      fprintf(stderr, "<< sem_destroy(%p) ", sem);
      fflush(stderr);
   }

   DO_CREQ_v_W(_VG_USERREQ__HG_POSIX_SEM_DESTROY_PRE, sem_t*, sem);

   CALL_FN_W_W(ret, fn, sem);

   if (ret != 0) {
      DO_PthAPIerror( "sem_destroy", errno );
   }

   if (TRACE_SEM_FNS) {
      fprintf(stderr, " sem_destroy -> %d >>\n", ret);
      fflush(stderr);
   }

   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, semZudestroyZAZa,  
                 sem_t* sem) {
      return sem_destroy_WRK(sem);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, semZudestroy,  
                 sem_t* sem) {
      return sem_destroy_WRK(sem);
   }
#else
#  error "Unsupported OS"
#endif


__attribute__((noinline))
static int sem_wait_WRK(sem_t* sem)
{
   OrigFn fn;
   int    ret;
   VALGRIND_GET_ORIG_FN(fn);

   if (TRACE_SEM_FNS) {
      fprintf(stderr, "<< sem_wait(%p) ", sem);
      fflush(stderr);
   }

   CALL_FN_W_W(ret, fn, sem);

   if (ret == 0) {
      DO_CREQ_v_W(_VG_USERREQ__HG_POSIX_SEM_WAIT_POST, sem_t*,sem);
   } else {
      DO_PthAPIerror( "sem_wait", errno );
   }

   if (TRACE_SEM_FNS) {
      fprintf(stderr, " sem_wait -> %d >>\n", ret);
      fflush(stderr);
   }

   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, semZuwait, sem_t* sem) { 
      return sem_wait_WRK(sem);
   }
   PTH_FUNC(int, semZuwaitZAZa, sem_t* sem) { 
      return sem_wait_WRK(sem);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, semZuwait, sem_t* sem) { 
      return sem_wait_WRK(sem);
   }
   PTH_FUNC(int, semZuwaitZDZa, sem_t* sem) { 
      return sem_wait_WRK(sem);
   }
#else
#  error "Unsupported OS"
#endif


__attribute__((noinline))
static int sem_post_WRK(sem_t* sem)
{
   OrigFn fn;
   int    ret;

   VALGRIND_GET_ORIG_FN(fn);

   if (TRACE_SEM_FNS) {
      fprintf(stderr, "<< sem_post(%p) ", sem);
      fflush(stderr);
   }

   DO_CREQ_v_W(_VG_USERREQ__HG_POSIX_SEM_POST_PRE, sem_t*,sem);

   CALL_FN_W_W(ret, fn, sem);

   if (ret != 0) {
      DO_PthAPIerror( "sem_post", errno );
   }

   if (TRACE_SEM_FNS) {
      fprintf(stderr, " sem_post -> %d >>\n", ret);
      fflush(stderr);
   }

   return ret;
}
#if defined(VGO_linux)
   PTH_FUNC(int, semZupost, sem_t* sem) { 
      return sem_post_WRK(sem);
   }
   PTH_FUNC(int, semZupostZAZa, sem_t* sem) { 
      return sem_post_WRK(sem);
   }
#elif defined(VGO_darwin)
   PTH_FUNC(int, semZupost, sem_t* sem) { 
      return sem_post_WRK(sem);
   }
#else
#  error "Unsupported OS"
#endif


PTH_FUNC(sem_t*, semZuopen,
                 const char* name, long oflag,
                 long mode, unsigned long value)
{
   
   OrigFn fn;
   sem_t* ret;
   VALGRIND_GET_ORIG_FN(fn);

   if (TRACE_SEM_FNS) {
      fprintf(stderr, "<< sem_open(\"%s\",%ld,%lx,%lu) ",
                      name,oflag,mode,value);
      fflush(stderr);
   }

   CALL_FN_W_WWWW(ret, fn, name,oflag,mode,value);

   if (ret != SEM_FAILED && (oflag & O_CREAT)) {
      DO_CREQ_v_WW(_VG_USERREQ__HG_POSIX_SEM_INIT_POST,
                   sem_t*, ret, unsigned long, value);
   } 
   if (ret == SEM_FAILED) {
      DO_PthAPIerror( "sem_open", errno );
   }

   if (TRACE_SEM_FNS) {
      fprintf(stderr, " sem_open -> %p >>\n", ret);
      fflush(stderr);
   }

   return ret;
}


PTH_FUNC(int, sem_close, sem_t* sem)
{
   OrigFn fn;
   int    ret;
   VALGRIND_GET_ORIG_FN(fn);

   if (TRACE_SEM_FNS) {
      fprintf(stderr, "<< sem_close(%p) ", sem);
      fflush(stderr);
   }

   DO_CREQ_v_W(_VG_USERREQ__HG_POSIX_SEM_DESTROY_PRE, sem_t*, sem);

   CALL_FN_W_W(ret, fn, sem);

   if (ret != 0) {
      DO_PthAPIerror( "sem_close", errno );
   }

   if (TRACE_SEM_FNS) {
      fprintf(stderr, " close -> %d >>\n", ret);
      fflush(stderr);
   }

   return ret;
}




#define QT4_FUNC(ret_ty, f, args...) \
   ret_ty I_WRAP_SONAME_FNNAME_ZU(libQtCoreZdsoZa,f)(args); \
   ret_ty I_WRAP_SONAME_FNNAME_ZU(libQtCoreZdsoZa,f)(args)

#define QT5_FUNC(ret_ty, f, args...) \
   ret_ty I_WRAP_SONAME_FNNAME_ZU(libQt5CoreZdsoZa,f)(args); \
   ret_ty I_WRAP_SONAME_FNNAME_ZU(libQt5CoreZdsoZa,f)(args)

__attribute__((noinline))
static void QMutex_lock_WRK(void* self)
{
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_QT4_FNS) {
      fprintf(stderr, "<< QMutex::lock %p", self); fflush(stderr);
   }

   DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_PRE,
                void*,self, long,0);

   CALL_FN_v_W(fn, self);

   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_POST,
               void*, self);

   if (TRACE_QT4_FNS) {
      fprintf(stderr, " :: Q::lock done >>\n");
   }
}

QT4_FUNC(void, _ZN6QMutex4lockEv, void* self) {
    QMutex_lock_WRK(self);
}
QT5_FUNC(void, _ZN6QMutex4lockEv, void* self) {
    QMutex_lock_WRK(self);
}

__attribute__((noinline))
static void QMutex_unlock_WRK(void* self)
{
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);

   if (TRACE_QT4_FNS) {
      fprintf(stderr, "<< QMutex::unlock %p", self); fflush(stderr);
   }

   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_UNLOCK_PRE,
               void*, self);

   CALL_FN_v_W(fn, self);

   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_UNLOCK_POST,
               void*, self);

   if (TRACE_QT4_FNS) {
      fprintf(stderr, " Q::unlock done >>\n");
   }
}

QT4_FUNC(void, _ZN6QMutex6unlockEv, void* self) {
    QMutex_unlock_WRK(self);
}
QT5_FUNC(void, _ZN6QMutex6unlockEv, void* self) {
    QMutex_unlock_WRK(self);
}

__attribute__((noinline))
static long QMutex_tryLock_WRK(void* self)
{
   OrigFn fn;
   long   ret;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_QT4_FNS) {
      fprintf(stderr, "<< QMutex::tryLock %p", self); fflush(stderr);
   }

   DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_PRE,
                void*,self, long,1);

   CALL_FN_W_W(ret, fn, self);

   
   if (ret & 0xFF) {
      DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_POST,
                  void*, self);
   }

   if (TRACE_QT4_FNS) {
      fprintf(stderr, " :: Q::tryLock -> %lu >>\n", ret);
   }
   
   return ret;
}

QT4_FUNC(long, _ZN6QMutex7tryLockEv, void* self) {
    return QMutex_tryLock_WRK(self);
}
QT5_FUNC(long, _ZN6QMutex7tryLockEv, void* self) {
    return QMutex_tryLock_WRK(self);
}

__attribute__((noinline))
static long QMutex_tryLock_int_WRK(void* self, long arg2)
{
   OrigFn fn;
   long   ret;
   VALGRIND_GET_ORIG_FN(fn);
   if (TRACE_QT4_FNS) {
      fprintf(stderr, "<< QMutex::tryLock(int) %p %d", self, (int)arg2);
      fflush(stderr);
   }

   DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_PRE,
                void*,self, long,1);

   CALL_FN_W_WW(ret, fn, self,arg2);

   
   if (ret & 0xFF) {
      DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_POST,
                  void*, self);
   }

   if (TRACE_QT4_FNS) {
      fprintf(stderr, " :: Q::tryLock(int) -> %lu >>\n", ret);
   }
   
   return ret;
}

QT4_FUNC(long, _ZN6QMutex7tryLockEi, void* self, long arg2) {
    return QMutex_tryLock_int_WRK(self, arg2);
}
QT5_FUNC(long, _ZN6QMutex7tryLockEi, void* self, long arg2) {
    return QMutex_tryLock_int_WRK(self, arg2);
}

__attribute__((noinline))
static void* QMutex_constructor_WRK(void* mutex, long recmode)
{
   OrigFn fn;
   long   ret;
   VALGRIND_GET_ORIG_FN(fn);
   CALL_FN_W_WW(ret, fn, mutex, recmode);
   
   DO_CREQ_v_WW(_VG_USERREQ__HG_PTHREAD_MUTEX_INIT_POST,
                void*,mutex, long,1);
   return (void*)ret;
}

QT4_FUNC(void*, _ZN6QMutexC1ENS_13RecursionModeE, void* self, long recmode) {
    return QMutex_constructor_WRK(self, recmode);
}
QT5_FUNC(void*, _ZN6QMutexC1ENS_13RecursionModeE, void* self, long recmode) {
    return QMutex_constructor_WRK(self, recmode);
}

__attribute__((noinline))
static void* QMutex_destructor_WRK(void* mutex)
{
   OrigFn fn;
   long   ret;
   VALGRIND_GET_ORIG_FN(fn);
   DO_CREQ_v_W(_VG_USERREQ__HG_PTHREAD_MUTEX_DESTROY_PRE,
               void*,mutex);
   CALL_FN_W_W(ret, fn, mutex);
   return (void*)ret;
}

QT4_FUNC(void*, _ZN6QMutexD1Ev, void* self) {
    return QMutex_destructor_WRK(self);
}
QT5_FUNC(void*, _ZN6QMutexD1Ev, void* self) {
    return QMutex_destructor_WRK(self);
}

QT4_FUNC(void*, _ZN6QMutexC2ENS_13RecursionModeE,
         void* mutex,
         long  recmode)
{
   assert(0);
   
   return NULL;
}

QT5_FUNC(void*, _ZN6QMutexC2ENS_13RecursionModeE, void* self, long recmode)
{
   assert(0);
   
   return NULL;
}

QT4_FUNC(void*, _ZN6QMutexD2Ev, void* mutex)
{
   assert(0);
   return NULL;
}

QT5_FUNC(void*, _ZN6QMutexD2Ev, void* self)
{
   assert(0);
   
   return NULL;
}





#include "../shared/vg_replace_strmem.c"

