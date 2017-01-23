

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2004-2013 OpenWorks LLP
      info@open-works.net

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   The GNU General Public License is contained in the file COPYING.

   Neither the names of the U.S. Department of Energy nor the
   University of California nor the names of its contributors may be
   used to endorse or promote products derived from this software
   without prior written permission.
*/

#ifndef __VEX_MAIN_GLOBALS_H
#define __VEX_MAIN_GLOBALS_H

#include "libvex_basictypes.h"
#include "libvex.h"



extern Bool vex_initdone;

__attribute__ ((noreturn))
extern void (*vex_failure_exit) ( void );

extern void (*vex_log_bytes) ( const HChar*, SizeT nbytes );

extern Int vex_debuglevel;

extern Int vex_traceflags;

extern VexControl vex_control;


#define VEX_TRACE_FE     (1 << 7)  
#define VEX_TRACE_OPT1   (1 << 6)  
#define VEX_TRACE_INST   (1 << 5)  
#define VEX_TRACE_OPT2   (1 << 4)  
#define VEX_TRACE_TREES  (1 << 3)  
#define VEX_TRACE_VCODE  (1 << 2)  
#define VEX_TRACE_RCODE  (1 << 1)  
#define VEX_TRACE_ASM    (1 << 0)  


#endif 

