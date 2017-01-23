

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

#ifndef __PUB_TOOL_DEBUGINFO_H
#define __PUB_TOOL_DEBUGINFO_H

#include "pub_tool_basics.h"   


extern Bool VG_(get_filename) ( Addr a, const HChar** filename );
extern Bool VG_(get_fnname)   ( Addr a, const HChar** fnname );
extern Bool VG_(get_linenum)  ( Addr a, UInt* linenum );
extern Bool VG_(get_fnname_w_offset)
                              ( Addr a, const HChar** fnname );

/* This one is the most general.  It gives filename, line number and
   optionally directory name.  filename and linenum may not be NULL.
   dirname may be NULL, meaning that the caller does not want
   directory name info.
   If dirname is non-null, directory info is written to *dirname, if
   it is available; if not available, '\0' is written to the first
   byte.

   The character strings returned in *filename and *dirname are not
   persistent. They will be freed when the DebugInfo they belong to
   is discarded.

   Returned value indicates whether any filename/line info could be
   found. */
extern Bool VG_(get_filename_linenum)
                              ( Addr a, 
                                const HChar** filename,
                                const HChar** dirname,
                                UInt* linenum );

extern Bool VG_(get_fnname_if_entry) ( Addr a, const HChar** fnname );

typedef
   enum {
      Vg_FnNameNormal,        
      Vg_FnNameMain,          
      Vg_FnNameBelowMain      
   } Vg_FnNameKind;           

extern Vg_FnNameKind VG_(get_fnname_kind) ( const HChar* name );

extern Vg_FnNameKind VG_(get_fnname_kind_from_IP) ( Addr ip );

extern Bool VG_(get_datasym_and_offset)( Addr data_addr,
                                         const HChar** dname,
                                         PtrdiffT* offset );

/* Try to form some description of DATA_ADDR by looking at the DWARF3
   debug info we have.  This considers all global variables, and 8
   frames in the stacks of all threads.  Result is written at the ends
   of DNAME{1,2}V, which are XArray*s of HChar, that have been
   initialised by the caller, and True is returned.  If no description
   is created, False is returned.  Regardless of the return value,
   DNAME{1,2}V are guaranteed to be zero terminated after the call.

   Note that after the call, DNAME{1,2} may have more than one
   trailing zero, so callers should establish the useful text length
   using VG_(strlen) on the contents, rather than VG_(sizeXA) on the
   XArray itself.
*/
Bool VG_(get_data_description)( 
         void*  dname1v,
         void*  dname2v,
        Addr data_addr
     );

extern Bool VG_(get_objname)  ( Addr a, const HChar** objname );


typedef  struct _InlIPCursor InlIPCursor;

/* Returns info about the code address %eip:  the address, function
   name (if known) and filename/line number (if known), like this:

      0x4001BF05: realloc (vg_replace_malloc.c:339)

   eip can possibly corresponds to inlined function call(s).
   To describe eip and the inlined function calls, the following must
   be done:
       InlIPCursor *iipc = VG_(new_IIPC)(eip);
       do {
          buf = VG_(describe_IP)(eip, iipc);
          ... use buf ...
       } while (VG_(next_IIPC)(iipc));
       VG_(delete_IIPC)(iipc);

   To only describe eip, without the inlined calls at eip, give a NULL iipc:
       buf = VG_(describe_IP)(eip, NULL);   

   Note, that the returned string is allocated in a static buffer local to
   VG_(describe_IP). That buffer will be overwritten with every invocation.
   Therefore, callers need to possibly stash away the string.
*/
extern const HChar* VG_(describe_IP)(Addr eip, const InlIPCursor* iipc);

extern InlIPCursor* VG_(new_IIPC)(Addr eip);
extern Bool VG_(next_IIPC)(InlIPCursor *iipc);
extern void VG_(delete_IIPC)(InlIPCursor *iipc);




typedef
   struct {
      PtrdiffT base;       
      SizeT    szB;        
      Bool     spRel;      
      Bool     isVec;      
      HChar    name[16];   
   }
   StackBlock;

extern void* 
             VG_(di_get_stack_blocks_at_ip)( Addr ip, Bool arrays_only );



typedef
   struct {
      Addr  addr;
      SizeT szB;
      Bool  isVec;      
      HChar name[16];   
      HChar soname[16]; 
   }
   GlobalBlock;

extern void* 
VG_(di_get_global_blocks_from_dihandle) ( ULong di_handle,
                                          Bool  arrays_only );



typedef  struct _DebugInfo  DebugInfo;

DebugInfo* VG_(find_DebugInfo) ( Addr a );

Addr          VG_(DebugInfo_get_text_avma)   ( const DebugInfo *di );
SizeT         VG_(DebugInfo_get_text_size)   ( const DebugInfo *di );
Addr          VG_(DebugInfo_get_bss_avma)    ( const DebugInfo *di );
SizeT         VG_(DebugInfo_get_bss_size)    ( const DebugInfo *di );
Addr          VG_(DebugInfo_get_plt_avma)    ( const DebugInfo *di );
SizeT         VG_(DebugInfo_get_plt_size)    ( const DebugInfo *di );
Addr          VG_(DebugInfo_get_gotplt_avma) ( const DebugInfo *di );
SizeT         VG_(DebugInfo_get_gotplt_size) ( const DebugInfo *di );
Addr          VG_(DebugInfo_get_got_avma)    ( const DebugInfo *di );
SizeT         VG_(DebugInfo_get_got_size)    ( const DebugInfo *di );
const HChar*  VG_(DebugInfo_get_soname)      ( const DebugInfo *di );
const HChar*  VG_(DebugInfo_get_filename)    ( const DebugInfo *di );
PtrdiffT      VG_(DebugInfo_get_text_bias)   ( const DebugInfo *di );

const DebugInfo* VG_(next_DebugInfo)    ( const DebugInfo *di );

typedef
   enum {
      Vg_SectUnknown,
      Vg_SectText,
      Vg_SectData,
      Vg_SectBSS,
      Vg_SectGOT,
      Vg_SectPLT,
      Vg_SectGOTPLT,
      Vg_SectOPD
   }
   VgSectKind;

const HChar* VG_(pp_SectKind)( VgSectKind kind );

VgSectKind VG_(DebugInfo_sect_kind)( const HChar** name, Addr a);


#endif   

