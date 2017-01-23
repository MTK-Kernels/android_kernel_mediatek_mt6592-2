

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2007-2013 OpenWorks LLP
      info@open-works.co.uk

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

#ifndef __PUB_TOOL_XARRAY_H
#define __PUB_TOOL_XARRAY_H

#include "pub_tool_basics.h"    



typedef  struct _XArray  XArray;

typedef Int (*XACmpFn_t)(const void *, const void *);

extern XArray* VG_(newXA) ( void*(*alloc_fn)(const HChar*,SizeT), 
                            const HChar* cc,
                            void(*free_fn)(void*),
                            Word elemSzB );

extern void VG_(deleteXA) ( XArray* );

extern void VG_(setCmpFnXA) ( XArray*, XACmpFn_t);

extern Word VG_(addToXA) ( XArray*, const void* elem );

extern Word VG_(addBytesToXA) ( XArray* xao, const void* bytesV, Word nbytes );

extern void VG_(sortXA) ( XArray* );

extern Bool VG_(lookupXA) ( const XArray*, const void* key, 
                            Word* first, Word* last );

extern Bool VG_(lookupXA_UNSAFE) ( const XArray* xao, const void* key,
                                   Word* first, Word* last,
                                   XACmpFn_t cmpFn );

extern Word VG_(sizeXA) ( const XArray* );

extern void VG_(hintSizeXA) ( XArray*, Word);

extern void* VG_(indexXA) ( const XArray*, Word );

extern void VG_(dropTailXA) ( XArray*, Word );

extern void VG_(dropHeadXA) ( XArray*, Word );

extern void VG_(removeIndexXA)( XArray*, Word );

extern void VG_(insertIndexXA)( XArray*, Word, const void* elem );

extern XArray* VG_(cloneXA)( const HChar* cc, const XArray* xa );

extern void VG_(getContentsXA_UNSAFE)( XArray* sr,
                                       void** ctsP,
                                       Word*  usedP );

extern void VG_(xaprintf)( XArray* dst, const HChar* format, ... )
                         PRINTF_CHECK(2, 3);

#endif   

