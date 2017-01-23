

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2013 Nicholas Nethercote
      njn@valgrind.org
   Copyright (C) 2004-2013 Paul Mackerras
      paulus@samba.org

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

#if defined(VGP_ppc32_linux)

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
#include "pub_core_sigframe.h"
#include "pub_core_signals.h"
#include "pub_core_tooliface.h"
#include "pub_core_trampoline.h"
#include "pub_core_transtab.h"      
#include "priv_sigframe.h"

/* This module creates and removes signal frames for signal deliveries
   on ppc32-linux.

   Note, this file contains kernel-specific knowledge in the form of
   'struct sigframe' and 'struct rt_sigframe'.  How does that relate
   to the vki kernel interface stuff?

   Either a 'struct sigframe' or a 'struct rtsigframe' is pushed 
   onto the client's stack.  This contains a subsidiary
   vki_ucontext.  That holds the vcpu's state across the signal, 
   so that the sighandler can mess with the vcpu state if it
   really wants.

   FIXME: sigcontexting is basically broken for the moment.  When
   delivering a signal, the integer registers and %eflags are
   correctly written into the sigcontext, however the FP and SSE state
   is not.  When returning from a signal, only the integer registers
   are restored from the sigcontext; the rest of the CPU state is
   restored to what it was before the signal.

   This will be fixed.
*/





struct vg_sig_private {
   UInt magicPI;
   UInt sigNo_private;
   VexGuestPPC32State vex_shadow1;
   VexGuestPPC32State vex_shadow2;
};

struct nonrt_sigframe {
   UInt gap1[16];
   struct vki_sigcontext sigcontext;
   struct vki_mcontext mcontext;
   struct vg_sig_private priv;
   unsigned char abigap[224];    
};

struct rt_sigframe {
   UInt gap1[20];
   vki_siginfo_t siginfo;
   struct vki_ucontext ucontext;
   struct vg_sig_private priv;
   unsigned char abigap[224];    
};

#define SET_SIGNAL_LR(zztst, zzval)                          \
   do { tst->arch.vex.guest_LR = (zzval);                    \
      VG_TRACK( post_reg_write, Vg_CoreSignal, tst->tid,     \
                offsetof(VexGuestPPC32State,guest_LR),       \
                sizeof(UWord) );                             \
   } while (0)

#define SET_SIGNAL_GPR(zztst, zzn, zzval)                    \
   do { tst->arch.vex.guest_GPR##zzn = (zzval);              \
      VG_TRACK( post_reg_write, Vg_CoreSignal, tst->tid,     \
                offsetof(VexGuestPPC32State,guest_GPR##zzn), \
                sizeof(UWord) );                             \
   } while (0)


static 
void stack_mcontext ( struct vki_mcontext *mc, 
                      ThreadState* tst, 
                      Bool use_rt_sigreturn,
                      UInt fault_addr )
{
   VG_TRACK( pre_mem_write, Vg_CoreSignal, tst->tid, "signal frame mcontext",
             (Addr)mc, sizeof(struct vki_pt_regs) );

#  define DO(gpr)  mc->mc_gregs[VKI_PT_R0+gpr] = tst->arch.vex.guest_GPR##gpr
   DO(0);  DO(1);  DO(2);  DO(3);  DO(4);  DO(5);  DO(6);  DO(7);
   DO(8);  DO(9);  DO(10); DO(11); DO(12); DO(13); DO(14); DO(15);
   DO(16); DO(17); DO(18); DO(19); DO(20); DO(21); DO(22); DO(23);
   DO(24); DO(25); DO(26); DO(27); DO(28); DO(29); DO(30); DO(31);
#  undef DO

   mc->mc_gregs[VKI_PT_NIP]     = tst->arch.vex.guest_CIA;
   mc->mc_gregs[VKI_PT_MSR]     = 0xf032;   
   mc->mc_gregs[VKI_PT_ORIG_R3] = tst->arch.vex.guest_GPR3;
   mc->mc_gregs[VKI_PT_CTR]     = tst->arch.vex.guest_CTR;
   mc->mc_gregs[VKI_PT_LNK]     = tst->arch.vex.guest_LR;
   mc->mc_gregs[VKI_PT_XER]     = LibVEX_GuestPPC32_get_XER(&tst->arch.vex);
   mc->mc_gregs[VKI_PT_CCR]     = LibVEX_GuestPPC32_get_CR(&tst->arch.vex);
   mc->mc_gregs[VKI_PT_MQ]      = 0;
   mc->mc_gregs[VKI_PT_TRAP]    = 0;
   mc->mc_gregs[VKI_PT_DAR]     = fault_addr;
   mc->mc_gregs[VKI_PT_DSISR]   = 0;
   mc->mc_gregs[VKI_PT_RESULT]  = 0;
   VG_TRACK( post_mem_write, Vg_CoreSignal, tst->tid, 
             (Addr)mc, sizeof(struct vki_pt_regs) );

   

   
   VG_TRACK(pre_mem_write, Vg_CoreSignal, tst->tid, "signal frame mcontext",
            (Addr)&mc->mc_pad, sizeof(mc->mc_pad));
   mc->mc_pad[0] = 0; 
   mc->mc_pad[1] = 0; 
   VG_TRACK( post_mem_write,  Vg_CoreSignal, tst->tid, 
             (Addr)&mc->mc_pad, sizeof(mc->mc_pad) );
   
   VG_(discard_translations)( (Addr)&mc->mc_pad, 
                              sizeof(mc->mc_pad), "stack_mcontext" );   

   
   SET_SIGNAL_LR(tst, (Addr)(use_rt_sigreturn 
                               ? (Addr)&VG_(ppc32_linux_SUBST_FOR_rt_sigreturn)
                               : (Addr)&VG_(ppc32_linux_SUBST_FOR_sigreturn)
                      ));
}












//..    /* retaddr, sigNo, siguContext fields are to be written */


//..    /* retaddr, sigNo, pSiginfo, puContext fields are to be written */


void VG_(sigframe_create)( ThreadId tid, 
                           Addr sp_top_of_frame,
                           const vki_siginfo_t *siginfo,
                           const struct vki_ucontext *siguc,
                           void *handler, 
                           UInt flags,
                           const vki_sigset_t *mask,
		           void *restorer )
{
   struct vg_sig_private *priv;
   Addr sp;
   ThreadState *tst;
   Int sigNo = siginfo->si_signo;
   Addr faultaddr;

   
   sp_top_of_frame &= ~0xf;

   if (flags & VKI_SA_SIGINFO) {
      sp = sp_top_of_frame - sizeof(struct rt_sigframe);
   } else {
      sp = sp_top_of_frame - sizeof(struct nonrt_sigframe);
   }

   tst = VG_(get_ThreadState)(tid);

   if (! ML_(sf_maybe_extend_stack)(tst, sp, sp_top_of_frame - sp, flags))
      return;

   vg_assert(VG_IS_16_ALIGNED(sp));

   
   VG_TRACK( pre_mem_write, Vg_CoreSignal, tid, "signal handler frame",
             sp, sizeof(UWord) );
   *(Addr *)sp = tst->arch.vex.guest_GPR1;
   VG_TRACK( post_mem_write, Vg_CoreSignal, tid, 
             sp, sizeof(UWord) );

   faultaddr = (Addr)siginfo->_sifields._sigfault._addr;
   if (sigNo == VKI_SIGILL && siginfo->si_code > 0)
      faultaddr = tst->arch.vex.guest_CIA;

   if (flags & VKI_SA_SIGINFO) {
      struct rt_sigframe *frame = (struct rt_sigframe *) sp;
      struct vki_ucontext *ucp = &frame->ucontext;

      VG_TRACK( pre_mem_write, Vg_CoreSignal, tid, "signal frame siginfo",
                (Addr)&frame->siginfo, sizeof(frame->siginfo) );
      VG_(memcpy)(&frame->siginfo, siginfo, sizeof(*siginfo));
      VG_TRACK( post_mem_write, Vg_CoreSignal, tid, 
                (Addr)&frame->siginfo, sizeof(frame->siginfo) );

      VG_TRACK( pre_mem_write, Vg_CoreSignal, tid, "signal frame ucontext",
                (Addr)ucp, offsetof(struct vki_ucontext, uc_pad) );
      ucp->uc_flags = 0;
      ucp->uc_link = 0;
      ucp->uc_stack = tst->altstack;
      VG_TRACK( post_mem_write, Vg_CoreSignal, tid, (Addr)ucp,
                offsetof(struct vki_ucontext, uc_pad) );

      VG_TRACK( pre_mem_write, Vg_CoreSignal, tid, "signal frame ucontext",
                (Addr)&ucp->uc_regs,
                sizeof(ucp->uc_regs) + sizeof(ucp->uc_sigmask) );
      ucp->uc_regs = &ucp->uc_mcontext;
      ucp->uc_sigmask = tst->sig_mask;
      VG_TRACK( post_mem_write, Vg_CoreSignal, tid, 
                (Addr)&ucp->uc_regs,
                sizeof(ucp->uc_regs) + sizeof(ucp->uc_sigmask) );

      stack_mcontext(&ucp->uc_mcontext, tst, True, faultaddr);
      priv = &frame->priv;

      SET_SIGNAL_GPR(tid, 4, (Addr) &frame->siginfo);
      SET_SIGNAL_GPR(tid, 5, (Addr) ucp);
      
      SET_SIGNAL_GPR(tid, 6, (Addr) &frame->siginfo);

   } else {
      
      struct nonrt_sigframe *frame = (struct nonrt_sigframe *) sp;
      struct vki_sigcontext *scp = &frame->sigcontext;

      VG_TRACK( pre_mem_write, Vg_CoreSignal, tid, "signal frame sigcontext",
                (Addr)&scp->_unused[3], sizeof(*scp) - 3 * sizeof(UInt) );
      scp->signal = sigNo;
      scp->handler = (Addr) handler;
      scp->oldmask = tst->sig_mask.sig[0];
      scp->_unused[3] = tst->sig_mask.sig[1];
      VG_TRACK( post_mem_write, Vg_CoreSignal, tid,
                (Addr)&scp->_unused[3], sizeof(*scp) - 3 * sizeof(UInt) );

      stack_mcontext(&frame->mcontext, tst, False, faultaddr);
      priv = &frame->priv;

      SET_SIGNAL_GPR(tid, 4, (Addr) scp);
   }

   priv->magicPI       = 0x31415927;
   priv->sigNo_private = sigNo;
   priv->vex_shadow1   = tst->arch.vex_shadow1;
   priv->vex_shadow2   = tst->arch.vex_shadow2;

   SET_SIGNAL_GPR(tid, 1, sp);
   SET_SIGNAL_GPR(tid, 3, sigNo);
   tst->arch.vex.guest_CIA = (Addr) handler;


   if (0)
      VG_(printf)("pushed signal frame; %%R1 now = %#lx, "
                  "next %%CIA = %#x, status=%d\n",
		  sp, tst->arch.vex.guest_CIA, tst->status);
}









void VG_(sigframe_destroy)( ThreadId tid, Bool isRT )
{
   ThreadState *tst;
   struct vg_sig_private *priv;
   Addr sp;
   UInt frame_size;
   struct vki_mcontext *mc;
   Int sigNo;
   Bool has_siginfo = isRT;

   vg_assert(VG_(is_valid_tid)(tid));
   tst = VG_(get_ThreadState)(tid);

   
   sp = tst->arch.vex.guest_GPR1;
   vg_assert(VG_IS_16_ALIGNED(sp));

   if (has_siginfo) {
      struct rt_sigframe *frame = (struct rt_sigframe *)sp;
      frame_size = sizeof(*frame);
      mc = &frame->ucontext.uc_mcontext;
      priv = &frame->priv;
      vg_assert(priv->magicPI == 0x31415927);
      tst->sig_mask = frame->ucontext.uc_sigmask;
   } else {
      struct nonrt_sigframe *frame = (struct nonrt_sigframe *)sp;
      frame_size = sizeof(*frame);
      mc = &frame->mcontext;
      priv = &frame->priv;
      vg_assert(priv->magicPI == 0x31415927);
      tst->sig_mask.sig[0] = frame->sigcontext.oldmask;
      tst->sig_mask.sig[1] = frame->sigcontext._unused[3];
   }
   tst->tmp_sig_mask = tst->sig_mask;

   sigNo = priv->sigNo_private;

#  define DO(gpr)  tst->arch.vex.guest_GPR##gpr = mc->mc_gregs[VKI_PT_R0+gpr]
   DO(0);  DO(1);  DO(2);  DO(3);  DO(4);  DO(5);  DO(6);  DO(7);
   DO(8);  DO(9);  DO(10); DO(11); DO(12); DO(13); DO(14); DO(15);
   DO(16); DO(17); DO(18); DO(19); DO(20); DO(21); DO(22); DO(23);
   DO(24); DO(25); DO(26); DO(27); DO(28); DO(29); DO(30); DO(31);
#  undef DO

   tst->arch.vex.guest_CIA = mc->mc_gregs[VKI_PT_NIP];

   
   

   LibVEX_GuestPPC32_put_CR( mc->mc_gregs[VKI_PT_CCR], &tst->arch.vex );

   tst->arch.vex.guest_LR  = mc->mc_gregs[VKI_PT_LNK];
   tst->arch.vex.guest_CTR = mc->mc_gregs[VKI_PT_CTR];
   LibVEX_GuestPPC32_put_XER( mc->mc_gregs[VKI_PT_XER], &tst->arch.vex );

   tst->arch.vex_shadow1 = priv->vex_shadow1;
   tst->arch.vex_shadow2 = priv->vex_shadow2;

   VG_TRACK(die_mem_stack_signal, sp, frame_size);

   if (VG_(clo_trace_signals))
      VG_(message)(Vg_DebugMsg,
                   "vg_pop_signal_frame (thread %d): "
                   "isRT=%d valid magic; EIP=%#x\n",
                   tid, has_siginfo, tst->arch.vex.guest_CIA);

   
   VG_TRACK( post_deliver_signal, tid, sigNo );

}

#endif 

