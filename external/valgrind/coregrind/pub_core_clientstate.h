

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

#ifndef __PUB_CORE_CLIENTSTATE_H
#define __PUB_CORE_CLIENTSTATE_H


#include "pub_tool_clientstate.h"


extern Addr  VG_(clstk_start_base); 
extern Addr  VG_(clstk_end);        
extern UWord VG_(clstk_id);      

extern UWord* VG_(client_auxv);

extern Addr  VG_(brk_base);	 
extern Addr  VG_(brk_limit);	 

extern Int VG_(cl_exec_fd);

extern Int VG_(cl_cmdline_fd);

extern Int VG_(cl_auxv_fd);

extern struct vki_rlimit VG_(client_rlimit_data);
extern struct vki_rlimit VG_(client_rlimit_stack);

extern HChar* VG_(name_of_launcher);

extern Int VG_(fd_soft_limit);
extern Int VG_(fd_hard_limit);

extern Addr VG_(client___libc_freeres_wrapper);

extern Addr VG_(client__dl_sysinfo_int80);


extern SizeT* VG_(client__stack_cache_actsize__addr);

#endif   

