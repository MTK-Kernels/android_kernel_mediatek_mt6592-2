

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

#ifndef __PUB_TOOL_ASPACEMGR_H
#define __PUB_TOOL_ASPACEMGR_H

#include "pub_tool_basics.h"   


typedef
   enum {
      SkFree  = 0x01,  
      SkAnonC = 0x02,  
      SkAnonV = 0x04,  
      SkFileC = 0x08,  
      SkFileV = 0x10,  
      SkShmC  = 0x20,  
      SkResvn = 0x40   
   }
   SegKind;

typedef
   enum {
      SmLower,  
      SmFixed,  
      SmUpper   
   }
   ShrinkMode;

typedef
   struct {
      SegKind kind;
      
      Addr    start;    
      Addr    end;      
      
      ShrinkMode smode;
      
      ULong   dev;
      ULong   ino;
      Off64T  offset;
      UInt    mode;
      Int     fnIdx;    
      
      Bool    hasR;
      Bool    hasW;
      Bool    hasX;
      Bool    hasT;     
                        
      Bool    isCH;     
   }
   NSegment;


/* Collect up the start addresses of segments whose kind matches one of
   the kinds specified in kind_mask.
   The interface is a bit strange in order to avoid potential
   segment-creation races caused by dynamic allocation of the result
   buffer *starts.

   The function first computes how many entries in the result
   buffer *starts will be needed.  If this number <= nStarts,
   they are placed in starts[0..], and the number is returned.
   If nStarts is not large enough, nothing is written to
   starts[0..], and the negation of the size is returned.

   Correct use of this function may mean calling it multiple times in
   order to establish a suitably-sized buffer. */
extern Int VG_(am_get_segment_starts)( UInt kind_mask, Addr* starts,
                                       Int nStarts );

extern NSegment const * VG_(am_find_nsegment) ( Addr a ); 

extern const HChar* VG_(am_get_filename)( NSegment const * );

extern Bool VG_(am_is_valid_for_client) ( Addr start, SizeT len, 
                                          UInt prot );

extern void* VG_(am_shadow_alloc)(SizeT size);

extern SysRes VG_(am_munmap_valgrind)( Addr start, SizeT length );

#endif   

