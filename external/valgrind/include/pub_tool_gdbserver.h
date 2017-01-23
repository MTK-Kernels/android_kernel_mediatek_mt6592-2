

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2011-2013 Philippe Waroquiers

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

#ifndef __PUB_TOOL_GDBSERVER_H
#define __PUB_TOOL_GDBSERVER_H

#include "pub_tool_basics.h"   



extern void VG_(gdbserver) ( ThreadId tid );

extern Int VG_(dyn_vgdb_error);

typedef
   enum {
      software_breakpoint,
      hardware_breakpoint,
      write_watchpoint,
      read_watchpoint,
      access_watchpoint } PointKind;
extern const HChar* VG_(ppPointKind) (PointKind kind);


extern Bool VG_(is_watched)(PointKind kind, Addr addr, Int szB);

extern void VG_(needs_watchpoint) (
   
   
   
   
   
   
   
   
   
   
   
   
   
   Bool (*watchpoint) (PointKind kind, Bool insert, Addr addr, SizeT len)
);


// with gdb/vgdb has been lost : in such a case, output is written
extern UInt VG_(gdb_printf) ( const HChar *format, ... ) PRINTF_CHECK(1, 2);

typedef
   enum {
      kwd_report_none,
      kwd_report_all,
      kwd_report_duplicated_matches } kwd_report_error;
extern Int VG_(keyword_id) (const HChar* keywords, const HChar* input_word, 
                            kwd_report_error report);

extern Bool VG_(strtok_get_address_and_size) (Addr* address, 
                                              SizeT* szB, 
                                              HChar **ssaveptr);

extern void VG_(print_all_stats) (Bool memory_stats, Bool tool_stats);

#endif   

