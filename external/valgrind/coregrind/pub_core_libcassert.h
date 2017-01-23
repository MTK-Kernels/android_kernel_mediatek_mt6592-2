

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2013 Julian Seward
      jseward@acm.org

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

#ifndef __PUB_CORE_LIBCASSERT_H
#define __PUB_CORE_LIBCASSERT_H


#include "pub_tool_libcassert.h"
#include "pub_core_basics.h"      

#define I_die_here                                             \
   VG_(assert_fail) (True,                  \
                     "Unimplemented functionality",            \
                     __FILE__, __LINE__, __PRETTY_FUNCTION__,  \
                     "valgrind", VG_BUGS_TO, "")

#define vg_assert(expr)                                                 \
  ((void) (LIKELY(expr) ? 0 :                                           \
           (VG_(assert_fail) (True, #expr,                    \
                              __FILE__, __LINE__, __PRETTY_FUNCTION__,  \
                              ""),                                      \
                              0)))

#define vg_assert2(expr, format, args...)                               \
  ((void) (LIKELY(expr) ? 0 :                                           \
           (VG_(assert_fail) (True, #expr,                    \
                              __FILE__, __LINE__, __PRETTY_FUNCTION__,  \
                              format, ##args),                          \
                              0)))

__attribute__ ((__noreturn__))
extern void  VG_(core_panic)      ( const HChar* str );
__attribute__ ((__noreturn__))
extern void  VG_(core_panic_at)   ( const HChar* str, const UnwindStartRegs* );

extern void VG_(client_exit)( Int status );

__attribute__ ((__noreturn__))
extern void VG_(exit_now)( Int status );

extern void VG_(unimplemented) ( const HChar* msg )
            __attribute__((__noreturn__));

extern void VG_(show_sched_status) ( Bool host_stacktrace,
                                     Bool stack_usage,
                                     Bool exited_threads);

#endif   

