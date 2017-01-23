
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

#ifndef __PUB_TOOL_ERRORMGR_H
#define __PUB_TOOL_ERRORMGR_H

#include "pub_tool_execontext.h"


typedef
   Int         
   ErrorKind;

typedef
   struct _Error
   Error;

ExeContext*  VG_(get_error_where)   ( const Error* err );
ErrorKind    VG_(get_error_kind)    ( const Error* err );
Addr         VG_(get_error_address) ( const Error* err );
const HChar* VG_(get_error_string)  ( const Error* err );
void*        VG_(get_error_extra)   ( const Error* err );

extern void VG_(maybe_record_error) ( ThreadId tid, ErrorKind ekind,
                                      Addr a, const HChar* s, void* extra );

extern Bool VG_(unique_error) ( ThreadId tid, ErrorKind ekind,
                                Addr a, const HChar* s, void* extra,
                                ExeContext* where, Bool print_error,
                                Bool allow_GDB_attach, Bool count_error );

extern Bool VG_(get_line) ( Int fd, HChar** bufpp, SizeT* nBufp, Int* lineno );


typedef
   Int         
   SuppKind;

typedef
   struct _Supp
   Supp;

SuppKind VG_(get_supp_kind)   ( const Supp* su );
HChar*   VG_(get_supp_string) ( const Supp* su );
void*    VG_(get_supp_extra)  ( const Supp* su );

void VG_(set_supp_kind)   ( Supp* su, SuppKind suppkind );
void VG_(set_supp_string) ( Supp* su, HChar* string );
void VG_(set_supp_extra)  ( Supp* su, void* extra );


#endif   

