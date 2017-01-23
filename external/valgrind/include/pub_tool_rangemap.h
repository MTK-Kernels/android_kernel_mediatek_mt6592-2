

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2014-2014 Mozilla Foundation

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


#ifndef __PUB_TOOL_RANGEMAP_H
#define __PUB_TOOL_RANGEMAP_H


typedef  struct _RangeMap  RangeMap;

RangeMap* VG_(newRangeMap) ( void*(*alloc_fn)(const HChar*,SizeT), 
                             const HChar* cc,
                             void(*free_fn)(void*),
                             UWord initialVal );

void VG_(deleteRangeMap) ( RangeMap* );

void VG_(bindRangeMap) ( RangeMap* rm,
                         UWord key_min, UWord key_max, UWord val );

void VG_(lookupRangeMap) ( UWord* key_min, UWord* key_max,
                           UWord* val, const RangeMap* rm, UWord key );

Word VG_(sizeRangeMap) ( const RangeMap* rm );

void VG_(indexRangeMap) ( UWord* key_min, UWord* key_max,
                          UWord* val, const RangeMap* rm, Word ix );

#endif   

