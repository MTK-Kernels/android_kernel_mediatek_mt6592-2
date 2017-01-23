

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

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_xarray.h"
#include "pub_core_clientstate.h"



Addr  VG_(clstk_start_base)  = 0;
Addr  VG_(clstk_end)   = 0;
UWord VG_(clstk_id)    = 0;

UWord* VG_(client_auxv) = NULL;

Addr  VG_(brk_base)    = 0;       
Addr  VG_(brk_limit)   = 0;       

Int VG_(cl_exec_fd) = -1;

Int VG_(cl_cmdline_fd) = -1;

Int VG_(cl_auxv_fd) = -1;


XArray*  VG_(args_for_client) = NULL;

XArray*  VG_(args_for_valgrind) = NULL;

Int VG_(args_for_valgrind_noexecpass) = 0;

const HChar* VG_(args_the_exename) = NULL;

struct vki_rlimit VG_(client_rlimit_data);
struct vki_rlimit VG_(client_rlimit_stack);

HChar* VG_(name_of_launcher) = NULL;

Int VG_(fd_soft_limit) = -1;
Int VG_(fd_hard_limit) = -1;

Addr VG_(client___libc_freeres_wrapper) = 0;

Addr VG_(client__dl_sysinfo_int80) = 0;

SizeT* VG_(client__stack_cache_actsize__addr) = 0;

