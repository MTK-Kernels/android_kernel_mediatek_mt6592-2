

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

#ifndef __PUB_CORE_LIBCPROC_H
#define __PUB_CORE_LIBCPROC_H


#include "config.h"           
#include "pub_tool_libcproc.h"

#ifdef ENABLE_INNER
#  define VALGRIND_LIB     "VALGRIND_LIB_INNER"
#else
#  define VALGRIND_LIB     "VALGRIND_LIB"
#endif

#define VALGRIND_OPTS    "VALGRIND_OPTS"

#ifdef ENABLE_INNER
#  define VALGRIND_LAUNCHER  "VALGRIND_LAUNCHER_INNER"
#else
#  define VALGRIND_LAUNCHER  "VALGRIND_LAUNCHER"
#endif


extern HChar **VG_(env_setenv)   ( HChar ***envp, const HChar* varname,
                                   const HChar *val );
extern void    VG_(env_unsetenv) ( HChar **env, const HChar *varname );
extern void    VG_(env_remove_valgrind_env_stuff) ( HChar** env ); 
extern HChar **VG_(env_clone)    ( HChar **env_clone );

extern Int  VG_(getgroups)( Int size, UInt* list );
extern Int  VG_(ptrace)( Int request, Int pid, void *addr, void *data );

extern void VG_(do_atfork_pre)    ( ThreadId tid );
extern void VG_(do_atfork_parent) ( ThreadId tid );
extern void VG_(do_atfork_child)  ( ThreadId tid );

extern void VG_(invalidate_icache) ( void *ptr, SizeT nbytes );

extern void VG_(flush_dcache) ( void *ptr, SizeT nbytes );

#endif   

