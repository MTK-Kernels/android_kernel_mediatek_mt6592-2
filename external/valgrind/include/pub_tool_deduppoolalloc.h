

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2014-2014 Philippe Waroquiers philippe.waroquiers@skynet.be

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

#ifndef __PUB_TOOL_DEDUPPOOLALLOC_H
#define __PUB_TOOL_DEDUPPOOLALLOC_H

#include "pub_tool_basics.h"   



typedef  struct _DedupPoolAlloc  DedupPoolAlloc;

extern DedupPoolAlloc* VG_(newDedupPA) ( SizeT  poolSzB,
                                         SizeT  eltAlign,
                                         void*  (*alloc)(const HChar*, SizeT),
                                         const  HChar* cc,
                                         void   (*free_fn)(void*) );

extern const void* VG_(allocEltDedupPA) (DedupPoolAlloc *ddpa,
                                         SizeT eltSzB, const void *elt);

extern UInt VG_(allocFixedEltDedupPA) (DedupPoolAlloc *ddpa,
                                       SizeT eltSzB, const void *elt);

extern void* VG_(indexEltNumber) (DedupPoolAlloc *ddpa,
                                  UInt eltNr);

extern void VG_(freezeDedupPA) (DedupPoolAlloc *ddpa,
                                void (*shrink_block)(void*, SizeT));

extern UInt VG_(sizeDedupPA) (DedupPoolAlloc *ddpa);

extern void VG_(deleteDedupPA) ( DedupPoolAlloc *ddpa);

#endif   

