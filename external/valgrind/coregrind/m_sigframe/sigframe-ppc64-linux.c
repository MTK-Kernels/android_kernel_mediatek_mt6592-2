

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

#if defined(VGP_ppc64be_linux) || defined(VGP_ppc64le_linux)

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
   on ppc64-linux.

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







#define TRAMP_SIZE 6 

struct vg_sig_private {
   UInt  magicPI;
   UInt  sigNo_private;
   ULong _unused; 
   VexGuestPPC64State vex_shadow1;
   VexGuestPPC64State vex_shadow2;
};

struct rt_sigframe {
   struct vki_ucontext   uc;
   ULong                 _unused[2];
   UInt                  tramp[TRAMP_SIZE];
   struct vki_siginfo*   pinfo;
   void*                 puc;
   vki_siginfo_t         info;
   struct vg_sig_private priv;
   UChar                 abigap[288];   
};

#define SET_SIGNAL_LR(zztst, zzval)                          \
   do { tst->arch.vex.guest_LR = (zzval);                    \
      VG_TRACK( post_reg_write, Vg_CoreSignal, tst->tid,     \
                offsetof(VexGuestPPC64State,guest_LR),       \
                sizeof(UWord) );                             \
   } while (0)

#define SET_SIGNAL_GPR(zztst, zzn, zzval)                    \
   do { tst->arch.vex.guest_GPR##zzn = (zzval);              \
      VG_TRACK( post_reg_write, Vg_CoreSignal, tst->tid,     \
                offsetof(VexGuestPPC64State,guest_GPR##zzn), \
                sizeof(UWord) );                             \
   } while (0)



void VG_(sigframe_create)( ThreadId tid, 
                           Addr sp_top_of_frame,
                           const vki_siginfo_t *siginfo,
                           const struct vki_ucontext *siguc,
                           void *handler, 
                           UInt flags,
                           const vki_sigset_t *mask,
		           void *restorer )
{
   struct vg_sig_private* priv;
   Addr sp;
   ThreadState* tst;
   Int sigNo = siginfo->si_signo;
    
   struct rt_sigframe* frame;

   
   vg_assert(VG_IS_16_ALIGNED(sizeof(struct vg_sig_private)));
   vg_assert(VG_IS_16_ALIGNED(sizeof(struct rt_sigframe)));

   sp_top_of_frame &= ~0xf;
   sp = sp_top_of_frame - sizeof(struct rt_sigframe);

   tst = VG_(get_ThreadState)(tid);
   if (! ML_(sf_maybe_extend_stack)(tst, sp, sp_top_of_frame - sp, flags))
      return;

   vg_assert(VG_IS_16_ALIGNED(sp));

   frame = (struct rt_sigframe *) sp;

   
   VG_(memset)(frame, 0, sizeof(*frame));

   
   frame->pinfo = &frame->info;
   frame->puc = &frame->uc;

   frame->uc.uc_flags = 0;
   frame->uc.uc_link = 0;
   

   
   VG_TRACK( pre_mem_write, Vg_CoreSignal, tid, "signal handler frame",
             sp, sizeof(UWord) );
   *(Addr *)sp = tst->arch.vex.guest_GPR1;
   VG_TRACK( post_mem_write, Vg_CoreSignal, tid, 
             sp, sizeof(UWord) );


   VG_(memcpy)(&frame->info, siginfo, sizeof(*siginfo));
   VG_TRACK( post_mem_write, Vg_CoreSignal, tid,
             (Addr)&frame->info, sizeof(frame->info) );

   frame->uc.uc_flags = 0;
   frame->uc.uc_link  = 0;
   frame->uc.uc_stack = tst->altstack;
   frame->uc.uc_sigmask = tst->sig_mask;
   VG_TRACK( post_mem_write, Vg_CoreSignal, tid,
             (Addr)(&frame->uc), sizeof(frame->uc) );

#  define DO(gpr)  frame->uc.uc_mcontext.gp_regs[VKI_PT_R0+gpr] \
                      = tst->arch.vex.guest_GPR##gpr 
   DO(0);  DO(1);  DO(2);  DO(3);  DO(4);  DO(5);  DO(6);  DO(7);
   DO(8);  DO(9);  DO(10); DO(11); DO(12); DO(13); DO(14); DO(15);
   DO(16); DO(17); DO(18); DO(19); DO(20); DO(21); DO(22); DO(23);
   DO(24); DO(25); DO(26); DO(27); DO(28); DO(29); DO(30); DO(31);
#  undef DO

   frame->uc.uc_mcontext.gp_regs[VKI_PT_NIP]     = tst->arch.vex.guest_CIA;
#ifdef VGP_ppc64le_linux
   frame->uc.uc_mcontext.gp_regs[VKI_PT_MSR]     = 0xf033;  
#else
   frame->uc.uc_mcontext.gp_regs[VKI_PT_MSR]     = 0xf032;  
#endif
   frame->uc.uc_mcontext.gp_regs[VKI_PT_ORIG_R3] = tst->arch.vex.guest_GPR3;
   frame->uc.uc_mcontext.gp_regs[VKI_PT_CTR]     = tst->arch.vex.guest_CTR;
   frame->uc.uc_mcontext.gp_regs[VKI_PT_LNK]     = tst->arch.vex.guest_LR;
   frame->uc.uc_mcontext.gp_regs[VKI_PT_XER]     = LibVEX_GuestPPC64_get_XER(
                                                      &tst->arch.vex);
   frame->uc.uc_mcontext.gp_regs[VKI_PT_CCR]     = LibVEX_GuestPPC64_get_CR(
                                                      &tst->arch.vex);
   
   
   
   
   

   

   
   frame->tramp[0] = 0; 
   frame->tramp[1] = 0; 
   VG_TRACK(post_mem_write, Vg_CoreSignal, tst->tid,
            (Addr)&frame->tramp, sizeof(frame->tramp));

   
   VG_(discard_translations)( (Addr)&frame->tramp[0], 
                              sizeof(frame->tramp), "stack_mcontext" );   

   
   SET_SIGNAL_LR(tst, (Addr)&VG_(ppc64_linux_SUBST_FOR_rt_sigreturn));

   SET_SIGNAL_GPR(tid, 1, sp);

   
   SET_SIGNAL_GPR(tid, 3, sigNo);
   SET_SIGNAL_GPR(tid, 4, (Addr) &frame->info);
   SET_SIGNAL_GPR(tid, 5, (Addr) &frame->uc);
   
   SET_SIGNAL_GPR(tid, 6, (Addr) &frame->info);

#if defined(VGP_ppc64be_linux)
   SET_SIGNAL_GPR(tid, 2, (Addr) ((ULong*)handler)[1]);
   tst->arch.vex.guest_CIA = (Addr) ((ULong*)handler)[0];
#else
   SET_SIGNAL_GPR(tid, 12, (Addr) handler);
   tst->arch.vex.guest_CIA = (Addr) handler;
#endif
   priv = &frame->priv;
   priv->magicPI       = 0x31415927;
   priv->sigNo_private = sigNo;
   priv->vex_shadow1   = tst->arch.vex_shadow1;
   priv->vex_shadow2   = tst->arch.vex_shadow2;

   if (0)
      VG_(printf)("pushed signal frame; %%R1 now = %#lx, "
                  "next %%CIA = %#llx, status=%d\n",
		  sp, tst->arch.vex.guest_CIA, tst->status);
}



void VG_(sigframe_destroy)( ThreadId tid, Bool isRT )
{
   ThreadState *tst;
   struct vg_sig_private *priv;
   Addr sp;
   UInt frame_size;
   struct rt_sigframe *frame;
   Int sigNo;
   Bool has_siginfo = isRT;

   vg_assert(VG_(is_valid_tid)(tid));
   tst = VG_(get_ThreadState)(tid);

   
   sp = tst->arch.vex.guest_GPR1;
   vg_assert(VG_IS_16_ALIGNED(sp));

   frame = (struct rt_sigframe *)sp;
   frame_size = sizeof(*frame);
   priv = &frame->priv;
   vg_assert(priv->magicPI == 0x31415927);
   tst->sig_mask = frame->uc.uc_sigmask;
   tst->tmp_sig_mask = tst->sig_mask;

   sigNo = priv->sigNo_private;

#  define DO(gpr)  tst->arch.vex.guest_GPR##gpr \
                      = frame->uc.uc_mcontext.gp_regs[VKI_PT_R0+gpr]
   DO(0);  DO(1);  DO(2);  DO(3);  DO(4);  DO(5);  DO(6);  DO(7);
   DO(8);  DO(9);  DO(10); DO(11); DO(12); DO(13); DO(14); DO(15);
   DO(16); DO(17); DO(18); DO(19); DO(20); DO(21); DO(22); DO(23);
   DO(24); DO(25); DO(26); DO(27); DO(28); DO(29); DO(30); DO(31);
#  undef DO

   tst->arch.vex.guest_CIA = frame->uc.uc_mcontext.gp_regs[VKI_PT_NIP];

   LibVEX_GuestPPC64_put_CR( frame->uc.uc_mcontext.gp_regs[VKI_PT_CCR], 
                             &tst->arch.vex );

   tst->arch.vex.guest_LR  = frame->uc.uc_mcontext.gp_regs[VKI_PT_LNK];
   tst->arch.vex.guest_CTR = frame->uc.uc_mcontext.gp_regs[VKI_PT_CTR];
   LibVEX_GuestPPC64_put_XER( frame->uc.uc_mcontext.gp_regs[VKI_PT_XER], 
                              &tst->arch.vex );

   tst->arch.vex_shadow1 = priv->vex_shadow1;
   tst->arch.vex_shadow2 = priv->vex_shadow2;

   VG_TRACK(die_mem_stack_signal, sp, frame_size);

   if (VG_(clo_trace_signals))
      VG_(message)(Vg_DebugMsg,
                   "vg_pop_signal_frame (thread %d): isRT=%d "
                   "valid magic; EIP=%#llx\n",
                   tid, has_siginfo, tst->arch.vex.guest_CIA);

   
   VG_TRACK( post_deliver_signal, tid, sigNo );
}

#endif 

