

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2006-2013 OpenWorks Ltd
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

#if defined(VGP_amd64_darwin)

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_vkiscnums.h"
#include "pub_core_threadstate.h"
#include "pub_core_aspacemgr.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcprint.h"
#include "pub_core_machine.h"
#include "pub_core_options.h"
#include "pub_core_signals.h"
#include "pub_core_tooliface.h"
#include "pub_core_trampoline.h"
#include "pub_core_sigframe.h"      



struct hacky_sigframe {
   
   ULong               returnAddr;
   UChar               lower_guardzone[512];  
   VexGuestAMD64State  gst;
   VexGuestAMD64State  gshadow1;
   VexGuestAMD64State  gshadow2;
   vki_siginfo_t       fake_siginfo;
   struct vki_ucontext fake_ucontext;
   UInt                magicPI;
   UInt                sigNo_private;
   vki_sigset_t        mask; 
   UInt                __pad[2];
   UChar               upper_guardzone[512]; 
   
   
};


void VG_(sigframe_create) ( ThreadId tid,
                            Addr sp_top_of_frame,
                            const vki_siginfo_t *siginfo,
                            const struct vki_ucontext *siguc,
                            void *handler,
                            UInt flags,
                            const vki_sigset_t *mask,
                            void *restorer )
{
   ThreadState* tst;
   Addr rsp;
   struct hacky_sigframe* frame;
   Int sigNo = siginfo->si_signo;

   vg_assert(VG_IS_16_ALIGNED(sizeof(struct hacky_sigframe)));

   sp_top_of_frame &= ~0xfUL;
   rsp = sp_top_of_frame - sizeof(struct hacky_sigframe);
   rsp -= 8; 

   tst = VG_(get_ThreadState)(tid);
   if (! ML_(sf_maybe_extend_stack)(tst, rsp, sp_top_of_frame - rsp, flags))
      return;

   vg_assert(VG_IS_16_ALIGNED(rsp+8));

   frame = (struct hacky_sigframe *) rsp;

   
   VG_(memset)(&frame->lower_guardzone, 0, sizeof frame->lower_guardzone);
   VG_(memset)(&frame->gst,      0, sizeof(VexGuestAMD64State));
   VG_(memset)(&frame->gshadow1, 0, sizeof(VexGuestAMD64State));
   VG_(memset)(&frame->gshadow2, 0, sizeof(VexGuestAMD64State));
   VG_(memset)(&frame->fake_siginfo,  0, sizeof(frame->fake_siginfo));
   VG_(memset)(&frame->fake_ucontext, 0, sizeof(frame->fake_ucontext));

   
   frame->gst           = tst->arch.vex;
   frame->gshadow1      = tst->arch.vex_shadow1;
   frame->gshadow2      = tst->arch.vex_shadow2;
   frame->sigNo_private = sigNo;
   frame->mask          = tst->sig_mask;
   frame->magicPI       = 0x31415927;

   frame->fake_siginfo.si_signo = sigNo;
   frame->fake_siginfo.si_code  = siginfo->si_code;

   
   vg_assert(rsp == (Addr)&frame->returnAddr);
   VG_(set_SP)(tid, rsp);
   VG_TRACK( post_reg_write, Vg_CoreSignal, tid, VG_O_STACK_PTR, sizeof(ULong));

   
   VG_(set_IP)(tid, (ULong)handler);
   VG_TRACK( post_reg_write, Vg_CoreSignal, tid, VG_O_INSTR_PTR, sizeof(ULong));

   
   VG_TRACK( pre_mem_write, Vg_CoreSignal, tid, "signal handler frame",
             (Addr)frame, 1*sizeof(ULong) );
   frame->returnAddr  = (ULong)&VG_(amd64_darwin_SUBST_FOR_sigreturn);

   /* XXX should tell the tool that these regs got written */
   tst->arch.vex.guest_RDI = (ULong) sigNo;
   tst->arch.vex.guest_RSI = (Addr)  &frame->fake_siginfo;
   tst->arch.vex.guest_RDX = (Addr)  &frame->fake_ucontext; 

   VG_TRACK( post_mem_write, Vg_CoreSignal, tid,
             (Addr)frame, 1*sizeof(ULong) );
   VG_TRACK( post_mem_write, Vg_CoreSignal, tid,
             (Addr)&frame->fake_siginfo, sizeof(frame->fake_siginfo));
   VG_TRACK( post_mem_write, Vg_CoreSignal, tid,
             (Addr)&frame->fake_ucontext, sizeof(frame->fake_ucontext));

   if (VG_(clo_trace_signals))
      VG_(message)(Vg_DebugMsg,
                   "sigframe_create (thread %d): "
                   "next EIP=%#lx, next ESP=%#lx\n",
                   tid, (Addr)handler, (Addr)frame );
}


void VG_(sigframe_destroy)( ThreadId tid, Bool isRT )
{
   ThreadState *tst;
   Addr rsp;
   Int sigNo;
   struct hacky_sigframe* frame;
 
   vg_assert(VG_(is_valid_tid)(tid));
   tst = VG_(get_ThreadState)(tid);

   
   rsp = VG_(get_SP)(tid);

   frame = (struct hacky_sigframe*)(rsp - 8);
   vg_assert(frame->magicPI == 0x31415927);

   vg_assert(VG_IS_16_ALIGNED((Addr)frame + 8));

   tst->arch.vex = frame->gst;
   tst->arch.vex_shadow1 = frame->gshadow1;
   tst->arch.vex_shadow2 = frame->gshadow2;
   tst->sig_mask = frame->mask;
   tst->tmp_sig_mask = frame->mask;
   sigNo = frame->sigNo_private;

   if (VG_(clo_trace_signals))
      VG_(message)(Vg_DebugMsg,
                   "sigframe_destroy (thread %d): "
                   "valid magic; next RIP=%#llx\n",
                   tid, tst->arch.vex.guest_RIP);

   VG_TRACK( die_mem_stack_signal, 
             (Addr)frame - VG_STACK_REDZONE_SZB, 
             sizeof(struct hacky_sigframe) );

   
   VG_TRACK( post_deliver_signal, tid, sigNo );
}

#endif 

