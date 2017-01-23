

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2005-2013 Nicholas Nethercote
      njn@valgrind.org

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

#ifndef __PUB_TOOL_OSET_H
#define __PUB_TOOL_OSET_H

#include "pub_tool_basics.h"   



typedef struct _OSet     OSet;


typedef Word  (*OSetCmp_t)         ( const void* key, const void* elem );
typedef void* (*OSetAlloc_t)       ( const HChar* cc, SizeT szB );
typedef void  (*OSetFree_t)        ( void* p );



extern OSet* VG_(OSetWord_Create) ( OSetAlloc_t alloc_fn, const HChar* cc, 
                                    OSetFree_t free_fn );
extern void  VG_(OSetWord_Destroy) ( OSet* os );



extern Word  VG_(OSetWord_Size)         ( const OSet* os );
extern void  VG_(OSetWord_Insert)       ( OSet* os, UWord val );
extern Bool  VG_(OSetWord_Contains)     ( const OSet* os, UWord val );
extern Bool  VG_(OSetWord_Remove)       ( OSet* os, UWord val );
extern void  VG_(OSetWord_ResetIter)    ( OSet* os );
extern Bool  VG_(OSetWord_Next)         ( OSet* os, UWord* val );




extern OSet* VG_(OSetGen_Create)    ( PtrdiffT keyOff, OSetCmp_t cmp,
                                      OSetAlloc_t alloc_fn, const HChar* cc,
                                      OSetFree_t free_fn);


extern OSet* VG_(OSetGen_Create_With_Pool)    ( PtrdiffT keyOff, OSetCmp_t cmp,
                                                OSetAlloc_t alloc_fn,
                                                const HChar* cc,
                                                OSetFree_t free_fn,
                                                SizeT poolSize,
                                                SizeT maxEltSize);

extern void  VG_(OSetGen_Destroy)   ( OSet* os );
extern void* VG_(OSetGen_AllocNode) ( const OSet* os, SizeT elemSize );
extern void  VG_(OSetGen_FreeNode)  ( const OSet* os, void* elem );

extern OSet* VG_(OSetGen_EmptyClone) (const OSet* os);



extern Word  VG_(OSetGen_Size)         ( const OSet* os );
extern void  VG_(OSetGen_Insert)       ( OSet* os, void* elem );
extern Bool  VG_(OSetGen_Contains)     ( const OSet* os, const void* key );
extern void* VG_(OSetGen_Lookup)       ( const OSet* os, const void* key );
extern void* VG_(OSetGen_LookupWithCmp)( OSet* os,
                                         const void* key, OSetCmp_t cmp );
extern void* VG_(OSetGen_Remove)       ( OSet* os, const void* key );
extern void  VG_(OSetGen_ResetIter)    ( OSet* os );
extern void  VG_(OSetGen_ResetIterAt)  ( OSet* os, const void* key );
extern void* VG_(OSetGen_Next)         ( OSet* os );


#endif   

