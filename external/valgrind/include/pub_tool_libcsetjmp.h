

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2010-2013 Mozilla Inc

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


#ifndef __PUB_TOOL_LIBCSETJMP_H
#define __PUB_TOOL_LIBCSETJMP_H

#include "pub_tool_basics.h"   




#include <setjmp.h>



#if defined(VGP_ppc32_linux)

#define VG_MINIMAL_JMP_BUF(_name)        UInt _name [32+1+1]
__attribute__((returns_twice))
UWord VG_MINIMAL_SETJMP(VG_MINIMAL_JMP_BUF(_env));
__attribute__((noreturn))
void  VG_MINIMAL_LONGJMP(VG_MINIMAL_JMP_BUF(_env));


#elif defined(VGP_ppc64be_linux) || defined(VGP_ppc64le_linux)

#define VG_MINIMAL_JMP_BUF(_name)        ULong _name [32+1+1]
__attribute__((returns_twice))
UWord VG_MINIMAL_SETJMP(VG_MINIMAL_JMP_BUF(_env));
__attribute__((noreturn))
void  VG_MINIMAL_LONGJMP(VG_MINIMAL_JMP_BUF(_env));


#elif defined(VGP_amd64_linux) || defined(VGP_amd64_darwin)

#define VG_MINIMAL_JMP_BUF(_name)        ULong _name [16+1]
__attribute__((returns_twice))
UWord VG_MINIMAL_SETJMP(VG_MINIMAL_JMP_BUF(_env));
__attribute__((noreturn))
void  VG_MINIMAL_LONGJMP(VG_MINIMAL_JMP_BUF(_env));


#elif defined(VGP_x86_linux) || defined(VGP_x86_darwin)

#define VG_MINIMAL_JMP_BUF(_name)        UInt _name [8+1]
__attribute__((returns_twice))
__attribute__((regparm(1))) 
UWord VG_MINIMAL_SETJMP(VG_MINIMAL_JMP_BUF(_env));
__attribute__((noreturn))
__attribute__((regparm(1))) 
void  VG_MINIMAL_LONGJMP(VG_MINIMAL_JMP_BUF(_env));

#elif defined(VGP_mips32_linux)

#define VG_MINIMAL_JMP_BUF(_name)        UInt _name [8+1+1+1+1]
__attribute__((returns_twice))
UWord VG_MINIMAL_SETJMP(VG_MINIMAL_JMP_BUF(_env));
__attribute__((noreturn))
void  VG_MINIMAL_LONGJMP(VG_MINIMAL_JMP_BUF(_env));

#else

#define VG_MINIMAL_JMP_BUF(_name) jmp_buf _name
#define VG_MINIMAL_SETJMP(_env)   ((UWord)(__builtin_setjmp((_env))))
#define VG_MINIMAL_LONGJMP(_env)  __builtin_longjmp((_env),1)

#endif

#endif   

