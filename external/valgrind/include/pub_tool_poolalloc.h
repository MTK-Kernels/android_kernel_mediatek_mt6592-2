

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2011-2013 OpenWorks LLP info@open-works.co.uk,
                           Philippe Waroquiers philippe.waroquiers@skynet.be

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

#ifndef __PUB_TOOL_POOLALLOC_H
#define __PUB_TOOL_POOLALLOC_H

#include "pub_tool_basics.h"   



typedef  struct _PoolAlloc  PoolAlloc;

extern PoolAlloc* VG_(newPA) ( UWord  elemSzB,
                               UWord  nPerPool,
                               void*  (*alloc)(const HChar*, SizeT),
                               const  HChar* cc,
                               void   (*free_fn)(void*) );


extern void VG_(deletePA) ( PoolAlloc* pa);

extern void* VG_(allocEltPA) ( PoolAlloc* pa);

extern void VG_(freeEltPA) ( PoolAlloc* pa, void* p);


extern void VG_(addRefPA) ( PoolAlloc* pa);

extern UWord VG_(releasePA) ( PoolAlloc* pa);

extern UWord VG_(sizePA) ( PoolAlloc* pa);
#endif   

