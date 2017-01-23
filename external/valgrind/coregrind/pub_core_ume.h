

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

#ifndef __PUB_CORE_UME_H
#define __PUB_CORE_UME_H

#include "pub_core_basics.h"   



typedef
   struct {
      const HChar** argv;   

      Addr exe_base;     
      Addr exe_end;      

#if !defined(VGO_darwin)
      Addr     phdr;          
      Int      phnum;         
      UInt     stack_prot;    
      PtrdiffT interp_offset; 
#else
      Addr  stack_start;      
      Addr  stack_end;        
      Addr  text;             
      Bool  dynamic;          
      HChar* executable_path; 
#endif

      Addr entry;        
      Addr init_ip;      
      Addr brkbase;      
      Addr init_toc;     
                         
                         

      
      HChar*  interp_name;  
      HChar*  interp_args;  
   }
   ExeInfo;

extern SysRes VG_(pre_exec_check)(const HChar* exe_name, Int* out_fd,
                                  Bool allow_setuid);

extern Int VG_(do_exec)(const HChar* exe, ExeInfo* info);

#endif 

