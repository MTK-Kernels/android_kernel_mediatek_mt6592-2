

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

#ifndef __PUB_TOOL_ADDRINFO_H
#define __PUB_TOOL_ADDRINFO_H

#include "pub_tool_basics.h"   
#include "pub_tool_aspacemgr.h"  


typedef enum {
   Block_Mallocd = 111,
   Block_Freed,
   Block_MempoolChunk,
   Block_UserG,
   Block_ClientArenaMallocd,
   Block_ClientArenaFree,
   Block_ValgrindArenaMallocd,
   Block_ValgrindArenaFree,
} BlockKind;


typedef 
   enum { 
      Addr_Undescribed, 
      Addr_Unknown,     
      Addr_Block,       
      Addr_Stack,       
      Addr_DataSym,     
      Addr_Variable,    
      Addr_SectKind,    
      Addr_BrkSegment,  
      Addr_SegmentKind  
   }
   AddrTag;

typedef 
   enum {
      StackPos_stacked,         
      StackPos_below_stack_ptr, 
      StackPos_guard_page       
   }
   StackPos;



typedef
   struct _ThreadInfo {
      ThreadId tid;   
      UInt     tnr;   
   } ThreadInfo;

extern void VG_(initThreadInfo) (ThreadInfo *tinfo);

typedef
   struct _AddrInfo
   AddrInfo;
   
struct _AddrInfo {
   AddrTag tag;
   union {
      
      struct { } Undescribed;

      
      
      
      
      
      
      
      
      
      struct {
         ThreadInfo tinfo;
         Addr     IP;
         Int      frameNo;
         StackPos stackPos;
         PtrdiffT spoffset;
      } Stack;

      
      
      
      
      
      struct {
         BlockKind   block_kind;
         const HChar* block_desc;   
         SizeT       block_szB;
         PtrdiffT    rwoffset;
         ExeContext* allocated_at;  
         ThreadInfo  alloc_tinfo;   
         ExeContext* freed_at;      
      } Block;

      
      
      struct {
         HChar   *name;
         PtrdiffT offset;
      } DataSym;

      
      struct {
         XArray*  descr1;
         XArray*  descr2;
      } Variable;

      
      
      struct {
         HChar      *objname;
         VgSectKind kind;
      } SectKind;

      
      
      
      
      
      struct {
         Addr brk_limit; 
      } BrkSegment;

      struct {
         SegKind segkind;   
         HChar   *filename; 
         Bool    hasR;
         Bool    hasW;
         Bool    hasX;
      } SegmentKind;

      
      struct { } Unknown;

   } Addr;
};


extern void VG_(describe_addr) ( Addr a, AddrInfo* ai );

extern void VG_(clear_addrinfo) ( AddrInfo* ai);

extern void VG_(pp_addrinfo) ( Addr a, const AddrInfo* ai );

extern void VG_(pp_addrinfo_mc) ( Addr a, const AddrInfo* ai, Bool maybe_gcc );

#endif   

