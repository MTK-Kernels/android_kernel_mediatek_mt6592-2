

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2004-2013 OpenWorks LLP
      info@open-works.net

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   The GNU General Public License is contained in the file COPYING.

   Neither the names of the U.S. Department of Energy nor the
   University of California nor the names of its contributors may be
   used to endorse or promote products derived from this software
   without prior written permission.
*/

#ifndef __VEX_GUEST_GENERIC_BB_TO_IR_H
#define __VEX_GUEST_GENERIC_BB_TO_IR_H

#include "libvex_basictypes.h"
#include "libvex_ir.h"              
#include "libvex.h"                 





typedef

   struct {

      UInt len;

      enum { Dis_StopHere, Dis_Continue, 
             Dis_ResteerU, Dis_ResteerC } whatNext;

      IRJumpKind jk_StopHere;

      Addr   continueAt;

   }

   DisResult;




typedef

   DisResult (*DisOneInstrFn) ( 

      
       IRSB*        irbb,

        Bool         (*resteerOkFn) ( void*, Addr ),

        Bool         resteerCisOk,

        void*        callback_opaque,

      
        const UChar* guest_code,

      
        Long         delta,

      
        Addr         guest_IP,

      
        VexArch      guest_arch,
        const VexArchInfo* archinfo,

      
        const VexAbiInfo*  abiinfo,

      
        VexEndness   host_endness,

      
        Bool         sigill_diag

   );



extern
IRSB* bb_to_IR ( 
         VexGuestExtents* vge,
         UInt*            n_sc_extents,
         UInt*            n_guest_instrs, 
         VexRegisterUpdates* pxControl,
          void*            callback_opaque,
          DisOneInstrFn    dis_instr_fn,
          const UChar*     guest_code,
          Addr             guest_IP_bbstart,
          Bool             (*chase_into_ok)(void*,Addr),
          VexEndness       host_endness,
          Bool             sigill_diag,
          VexArch          arch_guest,
          const VexArchInfo* archinfo_guest,
          const VexAbiInfo*  abiinfo_both,
          IRType           guest_word_type,
          UInt             (*needs_self_check)
                                    (void*, VexRegisterUpdates*,
                                            const VexGuestExtents*),
          Bool             (*preamble_function)(void*,IRSB*),
          Int              offB_GUEST_CMSTART,
          Int              offB_GUEST_CMLEN,
          Int              offB_GUEST_IP,
          Int              szB_GUEST_IP
      );


#endif 

