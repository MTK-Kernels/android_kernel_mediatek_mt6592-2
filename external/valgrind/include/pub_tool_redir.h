

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

#ifndef __PUB_TOOL_REDIR_H
#define __PUB_TOOL_REDIR_H

#include "config.h"           



#define VG_CONCAT4(_aa,_bb,_cc,_dd) _aa##_bb##_cc##_dd

#define VG_CONCAT6(_aa,_bb,_cc,_dd,_ee,_ff) _aa##_bb##_cc##_dd##_ee##_ff

#define VG_REPLACE_FUNCTION_EZU(_eclasstag,_soname,_fnname) \
   VG_CONCAT6(_vgr,_eclasstag,ZU_,_soname,_,_fnname)

#define VG_REPLACE_FUNCTION_EZZ(_eclasstag,_soname,_fnname) \
   VG_CONCAT6(_vgr,_eclasstag,ZZ_,_soname,_,_fnname)

#define VG_WRAP_FUNCTION_EZU(_eclasstag,_soname,_fnname) \
   VG_CONCAT6(_vgw,_eclasstag,ZU_,_soname,_,_fnname)

#define VG_WRAP_FUNCTION_EZZ(_eclasstag,_soname,_fnname) \
   VG_CONCAT6(_vgw,_eclasstag,ZZ_,_soname,_,_fnname)

#define VG_REPLACE_FUNCTION_ZU(_soname,_fnname) \
   VG_CONCAT6(_vgr,00000,ZU_,_soname,_,_fnname)

#define VG_REPLACE_FUNCTION_ZZ(_soname,_fnname) \
   VG_CONCAT6(_vgr,00000,ZZ_,_soname,_,_fnname)

#define VG_WRAP_FUNCTION_ZU(_soname,_fnname) \
   VG_CONCAT6(_vgw,00000,ZU_,_soname,_,_fnname)

#define VG_WRAP_FUNCTION_ZZ(_soname,_fnname) \
   VG_CONCAT6(_vgw,00000,ZZ_,_soname,_,_fnname)





#if defined(VGO_linux)
#  define  VG_Z_LIBC_SONAME  libcZdsoZa              

#elif defined(VGO_darwin) && (DARWIN_VERS <= DARWIN_10_6)
#  define  VG_Z_LIBC_SONAME  libSystemZdZaZddylib    

#elif defined(VGO_darwin) && (DARWIN_VERS == DARWIN_10_7 \
                              || DARWIN_VERS == DARWIN_10_8)
#  define  VG_Z_LIBC_SONAME  libsystemZucZaZddylib   

#elif defined(VGO_darwin) && (DARWIN_VERS >= DARWIN_10_9)
#  define  VG_Z_LIBC_SONAME  libsystemZumallocZddylib  

#else
#  error "Unknown platform"

#endif


#define  VG_Z_LIBSTDCXX_SONAME  libstdcZpZpZa           


#if defined(VGO_linux)
#  define  VG_Z_LIBPTHREAD_SONAME  libpthreadZdsoZd0     
#elif defined(VGO_darwin)
#  define  VG_Z_LIBPTHREAD_SONAME  libSystemZdZaZddylib  
#else
#  error "Unknown platform"
#endif


#if defined(VGO_linux)

#define  VG_Z_LD_LINUX_SO_3         ldZhlinuxZdsoZd3           
#define  VG_U_LD_LINUX_SO_3         "ld-linux.so.3"

#define  VG_Z_LD_LINUX_SO_2         ldZhlinuxZdsoZd2           
#define  VG_U_LD_LINUX_SO_2         "ld-linux.so.2"

#define  VG_Z_LD_LINUX_X86_64_SO_2  ldZhlinuxZhx86Zh64ZdsoZd2
                                                        
#define  VG_U_LD_LINUX_X86_64_SO_2  "ld-linux-x86-64.so.2"

#define  VG_Z_LD64_SO_1             ld64ZdsoZd1                
#define  VG_U_LD64_SO_1             "ld64.so.1"
#define  VG_U_LD64_SO_2             "ld64.so.2"                

#define  VG_Z_LD_SO_1               ldZdsoZd1                  
#define  VG_U_LD_SO_1               "ld.so.1"

#define  VG_U_LD_LINUX_AARCH64_SO_1 "ld-linux-aarch64.so.1"
#define  VG_U_LD_LINUX_ARMHF_SO_3   "ld-linux-armhf.so.3"

#endif


#if defined(VGO_darwin)

#define  VG_Z_DYLD               dyld                       
#define  VG_U_DYLD               "dyld"

#endif


#define VG_SO_SYN(name)       VgSoSyn##name
#define VG_SO_SYN_PREFIX     "VgSoSyn"
#define VG_SO_SYN_PREFIX_LEN 7

#endif   

