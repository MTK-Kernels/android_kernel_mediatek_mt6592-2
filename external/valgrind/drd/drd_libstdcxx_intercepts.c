
/*
  This file is part of DRD, a thread error detector.

  Copyright (C) 2014 Bart Van Assche <bvanassche@acm.org>.

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


#include "drd_basics.h"     
#include "drd_clientreq.h"
#include "pub_tool_redir.h" 

int __cxa_guard_acquire(void* guard);
void __cxa_guard_release(void* guard) __attribute__((__nothrow__));
void __cxa_guard_abort(void* guard) __attribute__((__nothrow__));

#define LIBSTDCXX_FUNC(ret_ty, zf, implf, argl_decl, argl)             \
   ret_ty VG_WRAP_FUNCTION_ZZ(VG_Z_LIBSTDCXX_SONAME,zf) argl_decl;     \
   ret_ty VG_WRAP_FUNCTION_ZZ(VG_Z_LIBSTDCXX_SONAME,zf) argl_decl      \
   { return implf argl; }

#undef __always_inline 
#if __GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ >= 2
#define __always_inline __inline__ __attribute__((always_inline))
#else
#define __always_inline __inline__
#endif

static __always_inline
int __cxa_guard_acquire_intercept(void *guard)
{
   int   ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PRE_MUTEX_LOCK,
                                   guard, mutex_type_cxa_guard, 0, 0, 0);
   CALL_FN_W_W(ret, fn, guard);
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__POST_MUTEX_LOCK,
                                   guard, 1, 0, 0, 0);
   if (ret == 0) {
      VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PRE_MUTEX_UNLOCK,
                                      guard, mutex_type_cxa_guard, 0, 0, 0);
      VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__POST_MUTEX_UNLOCK,
                                      guard, 0, 0, 0, 0);
   }
   return ret;
}

LIBSTDCXX_FUNC(int, ZuZucxaZuguardZuacquire, __cxa_guard_acquire_intercept,
               (void *guard), (guard));
LIBSTDCXX_FUNC(int, ZuZucxaZuguardZuacquireZAZACXXABIZu1Zd3,
               __cxa_guard_acquire_intercept, (void *guard), (guard));

static __always_inline
void __cxa_guard_abort_release_intercept(void *guard)
{
   int ret;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PRE_MUTEX_UNLOCK,
                                   guard, mutex_type_cxa_guard, 0, 0, 0);
   CALL_FN_W_W(ret, fn, guard);
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__POST_MUTEX_UNLOCK,
                                   guard, 0, 0, 0, 0);
}

LIBSTDCXX_FUNC(void, ZuZucxaZuguardZurelease,
               __cxa_guard_abort_release_intercept, (void *guard), (guard));
LIBSTDCXX_FUNC(void, ZuZucxaZuguardZuabort,
               __cxa_guard_abort_release_intercept, (void *guard), (guard));
