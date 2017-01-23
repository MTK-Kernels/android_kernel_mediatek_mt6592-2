

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

#ifndef __PUB_TOOL_REPLACEMALLOC_H
#define __PUB_TOOL_REPLACEMALLOC_H

#include "pub_tool_basics.h"   


extern void* VG_(cli_malloc) ( SizeT align, SizeT nbytes );
extern void  VG_(cli_free)   ( void* p );
extern SizeT VG_(cli_malloc_usable_size)( void* p );


extern Long VG_(free_queue_volume);
extern Long VG_(free_queue_length);

extern Bool VG_(addr_is_in_block)( Addr a, Addr start,
                                   SizeT size, SizeT rz_szB );


extern Bool VG_(clo_trace_malloc);
extern UInt VG_(clo_alignment);

extern Bool VG_(replacement_malloc_process_cmd_line_option) ( const HChar* arg );

extern SizeT VG_(malloc_effective_client_redzone_size)(void);

#endif   

