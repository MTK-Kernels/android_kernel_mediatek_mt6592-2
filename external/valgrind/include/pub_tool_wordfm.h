

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2007-2013 Julian Seward
      jseward@acm.org

   This code is based on previous work by Nicholas Nethercote
   (coregrind/m_oset.c) which is

   Copyright (C) 2005-2013 Nicholas Nethercote
       njn@valgrind.org

   which in turn was derived partially from:

      AVL C library
      Copyright (C) 2000,2002  Daniel Nagy

      This program is free software; you can redistribute it and/or
      modify it under the terms of the GNU General Public License as
      published by the Free Software Foundation; either version 2 of
      the License, or (at your option) any later version.
      [...]

      (taken from libavl-0.4/debian/copyright)

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

#ifndef __PUB_TOOL_WORDFM_H
#define __PUB_TOOL_WORDFM_H

#include "pub_tool_basics.h"    



typedef  struct _WordFM  WordFM; 

WordFM* VG_(newFM) ( void* (*alloc_nofail)( const HChar* cc, SizeT ),
                     const HChar* cc,
                     void  (*dealloc)(void*),
                     Word  (*kCmp)(UWord,UWord) );

void VG_(deleteFM) ( WordFM*, void(*kFin)(UWord), void(*vFin)(UWord) );

Bool VG_(addToFM) ( WordFM* fm, UWord k, UWord v );

Bool VG_(delFromFM) ( WordFM* fm,
                      UWord* oldK, UWord* oldV, UWord key );

Bool VG_(lookupFM) ( const WordFM* fm, 
                     UWord* keyP, UWord* valP, UWord key );

Bool VG_(findBoundsFM)( const WordFM* fm,
                        UWord* kMinP, UWord* vMinP,
                        UWord* kMaxP, UWord* vMaxP,
                        UWord minKey, UWord minVal,
                        UWord maxKey, UWord maxVal,
                        UWord key );

UWord VG_(sizeFM) ( const WordFM* fm );

void VG_(initIterFM) ( WordFM* fm );

void VG_(initIterAtFM) ( WordFM* fm, UWord start_at );

Bool VG_(nextIterFM) ( WordFM* fm,
                       UWord* pKey, UWord* pVal );

void VG_(doneIterFM) ( WordFM* fm );

WordFM* VG_(dopyFM) ( WordFM* fm,
                      UWord(*dopyK)(UWord), UWord(*dopyV)(UWord) );



typedef  struct _WordBag  WordBag; 

WordBag* VG_(newBag) ( void* (*alloc_nofail)( const HChar* cc, SizeT ),
                       const HChar* cc,
                       void  (*dealloc)(void*) );

void VG_(deleteBag) ( WordBag* );

void VG_(addToBag)( WordBag*, UWord );

UWord VG_(elemBag) ( const WordBag*, UWord );

Bool VG_(delFromBag)( WordBag*, UWord );

Bool VG_(isEmptyBag)( const WordBag* );

Bool VG_(isSingletonTotalBag)( const WordBag* );

UWord VG_(anyElementOfBag)( const WordBag* );

UWord VG_(sizeUniqueBag)( const WordBag* ); 
UWord VG_(sizeTotalBag)( const WordBag* );  

void VG_(initIterBag)( WordBag* );
Bool VG_(nextIterBag)( WordBag*, UWord* pVal, UWord* pCount );
void VG_(doneIterBag)( WordBag* );


#endif 

