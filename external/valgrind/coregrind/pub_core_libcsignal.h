

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

#ifndef __PUB_CORE_LIBCSIGNAL_H
#define __PUB_CORE_LIBCSIGNAL_H


#include "pub_tool_libcsignal.h"


extern Int  VG_(sigfillset)  ( vki_sigset_t* set );
extern Int  VG_(sigemptyset) ( vki_sigset_t* set );

extern Bool VG_(isfullsigset)  ( const vki_sigset_t* set );
extern Bool VG_(isemptysigset) ( const vki_sigset_t* set );
extern Bool VG_(iseqsigset)    ( const vki_sigset_t* set1,
                                 const vki_sigset_t* set2 );

extern Int  VG_(sigaddset)   ( vki_sigset_t* set, Int signum );
extern Int  VG_(sigdelset)   ( vki_sigset_t* set, Int signum );
extern Int  VG_(sigismember) ( const vki_sigset_t* set, Int signum );

extern void VG_(sigaddset_from_set) ( vki_sigset_t* dst, const vki_sigset_t* src );
extern void VG_(sigdelset_from_set) ( vki_sigset_t* dst, const vki_sigset_t* src );
extern void VG_(sigintersectset)    ( vki_sigset_t* dst, const vki_sigset_t* src );
extern void VG_(sigcomplementset)   ( vki_sigset_t* dst, const vki_sigset_t* src );


extern Int VG_(sigaction)   ( Int signum,
                              const vki_sigaction_toK_t* act,
                              vki_sigaction_fromK_t* oldact );

extern void VG_(convert_sigaction_fromK_to_toK)(
                const vki_sigaction_fromK_t*, vki_sigaction_toK_t*);


extern Int VG_(kill)        ( Int pid, Int signo );
extern Int VG_(tkill)       ( Int lwpid, Int signo );

extern Int VG_(sigtimedwait_zero)( const vki_sigset_t *, vki_siginfo_t * );

#endif   

