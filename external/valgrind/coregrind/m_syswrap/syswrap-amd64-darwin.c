

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2005-2013 Apple Inc.
      Greg Parker  gparker@apple.com

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

#include "config.h"                
#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_threadstate.h"
#include "pub_core_aspacemgr.h"
#include "pub_core_xarray.h"
#include "pub_core_clientstate.h"
#include "pub_core_debuglog.h"
#include "pub_core_debuginfo.h"    
#include "pub_core_transtab.h"     
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcfile.h"
#include "pub_core_libcprint.h"
#include "pub_core_libcproc.h"
#include "pub_core_libcsignal.h"
#include "pub_core_mallocfree.h"
#include "pub_core_options.h"
#include "pub_core_scheduler.h"
#include "pub_core_sigframe.h"      
#include "pub_core_signals.h"
#include "pub_core_syscall.h"
#include "pub_core_syswrap.h"
#include "pub_core_tooliface.h"

#include "priv_types_n_macros.h"
#include "priv_syswrap-generic.h"   
#include "priv_syswrap-darwin.h"    
#include "priv_syswrap-main.h"


#include <mach/mach.h>

static void x86_thread_state64_from_vex(x86_thread_state64_t *mach, 
                                        VexGuestAMD64State *vex)
{
    mach->__rax = vex->guest_RAX;
    mach->__rbx = vex->guest_RBX;
    mach->__rcx = vex->guest_RCX;
    mach->__rdx = vex->guest_RDX;
    mach->__rdi = vex->guest_RDI;
    mach->__rsi = vex->guest_RSI;
    mach->__rbp = vex->guest_RBP;
    mach->__rsp = vex->guest_RSP;
    mach->__rflags = LibVEX_GuestAMD64_get_rflags(vex);
    mach->__rip = vex->guest_RIP;
    mach->__r8  = vex->guest_R8;
    mach->__r9  = vex->guest_R9;
    mach->__r10 = vex->guest_R10;
    mach->__r11 = vex->guest_R11;
    mach->__r12 = vex->guest_R12;
    mach->__r13 = vex->guest_R13;
    mach->__r14 = vex->guest_R14;
    mach->__r15 = vex->guest_R15;
}


static void x86_float_state64_from_vex(x86_float_state64_t *mach, 
                                       VexGuestAMD64State *vex)
{
   
   
   VG_(memcpy)(&mach->__fpu_xmm0,  &vex->guest_YMM0,   sizeof(mach->__fpu_xmm0));
   VG_(memcpy)(&mach->__fpu_xmm1,  &vex->guest_YMM1,   sizeof(mach->__fpu_xmm1));
   VG_(memcpy)(&mach->__fpu_xmm2,  &vex->guest_YMM2,   sizeof(mach->__fpu_xmm2));
   VG_(memcpy)(&mach->__fpu_xmm3,  &vex->guest_YMM3,   sizeof(mach->__fpu_xmm3));
   VG_(memcpy)(&mach->__fpu_xmm4,  &vex->guest_YMM4,   sizeof(mach->__fpu_xmm4));
   VG_(memcpy)(&mach->__fpu_xmm5,  &vex->guest_YMM5,   sizeof(mach->__fpu_xmm5));
   VG_(memcpy)(&mach->__fpu_xmm6,  &vex->guest_YMM6,   sizeof(mach->__fpu_xmm6));
   VG_(memcpy)(&mach->__fpu_xmm7,  &vex->guest_YMM7,   sizeof(mach->__fpu_xmm7));
   VG_(memcpy)(&mach->__fpu_xmm8,  &vex->guest_YMM8,   sizeof(mach->__fpu_xmm8));
   VG_(memcpy)(&mach->__fpu_xmm9,  &vex->guest_YMM9,   sizeof(mach->__fpu_xmm9));
   VG_(memcpy)(&mach->__fpu_xmm10, &vex->guest_YMM10,  sizeof(mach->__fpu_xmm10));
   VG_(memcpy)(&mach->__fpu_xmm11, &vex->guest_YMM11,  sizeof(mach->__fpu_xmm11));
   VG_(memcpy)(&mach->__fpu_xmm12, &vex->guest_YMM12,  sizeof(mach->__fpu_xmm12));
   VG_(memcpy)(&mach->__fpu_xmm13, &vex->guest_YMM13,  sizeof(mach->__fpu_xmm13));
   VG_(memcpy)(&mach->__fpu_xmm14, &vex->guest_YMM14,  sizeof(mach->__fpu_xmm14));
   VG_(memcpy)(&mach->__fpu_xmm15, &vex->guest_YMM15,  sizeof(mach->__fpu_xmm15));
}


void thread_state_from_vex(thread_state_t mach_generic, 
                           thread_state_flavor_t flavor, 
                           mach_msg_type_number_t count, 
                           VexGuestArchState *vex_generic)
{
   VexGuestAMD64State *vex = (VexGuestAMD64State *)vex_generic;

   switch (flavor) {
   case x86_THREAD_STATE64:
      vg_assert(count == x86_THREAD_STATE64_COUNT);
      x86_thread_state64_from_vex((x86_thread_state64_t *)mach_generic, vex);
      break;

   case x86_FLOAT_STATE64:
      vg_assert(count == x86_FLOAT_STATE64_COUNT);
      x86_float_state64_from_vex((x86_float_state64_t *)mach_generic, vex);
      break;

   case x86_THREAD_STATE:
      ((x86_float_state_t *)mach_generic)->fsh.flavor = flavor;
      ((x86_float_state_t *)mach_generic)->fsh.count = count;
      x86_thread_state64_from_vex(&((x86_thread_state_t *)mach_generic)->uts.ts64, vex);
      break;

   case x86_FLOAT_STATE:
      ((x86_float_state_t *)mach_generic)->fsh.flavor = flavor;
      ((x86_float_state_t *)mach_generic)->fsh.count = count;
      x86_float_state64_from_vex(&((x86_float_state_t *)mach_generic)->ufs.fs64, vex);
      break;
       
   case x86_EXCEPTION_STATE:
      VG_(printf)("thread_state_from_vex: TODO, want exception state\n");
      vg_assert(0);

   default:
      VG_(printf)("thread_state_from_vex: flavor:%#x\n",  flavor);
      vg_assert(0);
   }
}


static void x86_thread_state64_to_vex(const x86_thread_state64_t *mach, 
                                      VexGuestAMD64State *vex)
{
   LibVEX_GuestAMD64_initialise(vex);
   vex->guest_RAX = mach->__rax;
   vex->guest_RBX = mach->__rbx;
   vex->guest_RCX = mach->__rcx;
   vex->guest_RDX = mach->__rdx;
   vex->guest_RDI = mach->__rdi;
   vex->guest_RSI = mach->__rsi;
   vex->guest_RBP = mach->__rbp;
   vex->guest_RSP = mach->__rsp;
   
   vex->guest_RIP = mach->__rip;
   vex->guest_R8  = mach->__r8;
   vex->guest_R9  = mach->__r9;
   vex->guest_R10 = mach->__r10;
   vex->guest_R11 = mach->__r11;
   vex->guest_R12 = mach->__r12;
   vex->guest_R13 = mach->__r13;
   vex->guest_R14 = mach->__r14;
   vex->guest_R15 = mach->__r15;
}

static void x86_float_state64_to_vex(const x86_float_state64_t *mach, 
                                     VexGuestAMD64State *vex)
{
   
   
   VG_(memcpy)(&vex->guest_YMM0,  &mach->__fpu_xmm0,  sizeof(mach->__fpu_xmm0));
   VG_(memcpy)(&vex->guest_YMM1,  &mach->__fpu_xmm1,  sizeof(mach->__fpu_xmm1));
   VG_(memcpy)(&vex->guest_YMM2,  &mach->__fpu_xmm2,  sizeof(mach->__fpu_xmm2));
   VG_(memcpy)(&vex->guest_YMM3,  &mach->__fpu_xmm3,  sizeof(mach->__fpu_xmm3));
   VG_(memcpy)(&vex->guest_YMM4,  &mach->__fpu_xmm4,  sizeof(mach->__fpu_xmm4));
   VG_(memcpy)(&vex->guest_YMM5,  &mach->__fpu_xmm5,  sizeof(mach->__fpu_xmm5));
   VG_(memcpy)(&vex->guest_YMM6,  &mach->__fpu_xmm6,  sizeof(mach->__fpu_xmm6));
   VG_(memcpy)(&vex->guest_YMM7,  &mach->__fpu_xmm7,  sizeof(mach->__fpu_xmm7));
   VG_(memcpy)(&vex->guest_YMM8,  &mach->__fpu_xmm8,  sizeof(mach->__fpu_xmm8));
   VG_(memcpy)(&vex->guest_YMM9,  &mach->__fpu_xmm9,  sizeof(mach->__fpu_xmm9));
   VG_(memcpy)(&vex->guest_YMM10, &mach->__fpu_xmm10, sizeof(mach->__fpu_xmm10));
   VG_(memcpy)(&vex->guest_YMM11, &mach->__fpu_xmm11, sizeof(mach->__fpu_xmm11));
   VG_(memcpy)(&vex->guest_YMM12, &mach->__fpu_xmm12, sizeof(mach->__fpu_xmm12));
   VG_(memcpy)(&vex->guest_YMM13, &mach->__fpu_xmm13, sizeof(mach->__fpu_xmm13));
   VG_(memcpy)(&vex->guest_YMM14, &mach->__fpu_xmm14, sizeof(mach->__fpu_xmm14));
   VG_(memcpy)(&vex->guest_YMM15, &mach->__fpu_xmm15, sizeof(mach->__fpu_xmm15));
}


void thread_state_to_vex(const thread_state_t mach_generic, 
                         thread_state_flavor_t flavor, 
                         mach_msg_type_number_t count, 
                         VexGuestArchState *vex_generic)
{
   VexGuestAMD64State *vex = (VexGuestAMD64State *)vex_generic;
   
   switch(flavor) {
   case x86_THREAD_STATE64:
      vg_assert(count == x86_THREAD_STATE64_COUNT);
      x86_thread_state64_to_vex((const x86_thread_state64_t*)mach_generic,vex);
      break;
   case x86_FLOAT_STATE64:
      vg_assert(count == x86_FLOAT_STATE64_COUNT);
      x86_float_state64_to_vex((const x86_float_state64_t*)mach_generic,vex);
      break;

   default:
      vg_assert(0);
      break;
   }
}


ThreadState *build_thread(const thread_state_t state, 
                          thread_state_flavor_t flavor, 
                          mach_msg_type_number_t count)
{
   ThreadId tid = VG_(alloc_ThreadState)();
   ThreadState *tst = VG_(get_ThreadState)(tid);
    
   vg_assert(flavor == x86_THREAD_STATE64);
   vg_assert(count == x86_THREAD_STATE64_COUNT);

   

   thread_state_to_vex(state, flavor, count, &tst->arch.vex);

   I_die_here;
   

   find_stack_segment(tid, tst->arch.vex.guest_RSP);

   return tst;
}


void hijack_thread_state(thread_state_t mach_generic, 
                         thread_state_flavor_t flavor, 
                         mach_msg_type_number_t count, 
                         ThreadState *tst)
{
   x86_thread_state64_t *mach = (x86_thread_state64_t *)mach_generic;
   char *stack;

   vg_assert(flavor == x86_THREAD_STATE64);
   vg_assert(count == x86_THREAD_STATE64_COUNT);

   stack = (char *)allocstack(tst->tid);
   stack -= 64+320;                       
   memset(stack, 0, 64+320);              
   *(uintptr_t *)stack = 0;               

   mach->__rdi = (uintptr_t)tst;          
   mach->__rip = (uintptr_t)&start_thread_NORETURN;
   mach->__rsp = (uintptr_t)stack;
}


__attribute__((noreturn))
void call_on_new_stack_0_1 ( Addr stack,
			     Addr retaddr,
			     void (*f)(Word),
                             Word arg1 );
asm(
".globl _call_on_new_stack_0_1\n"
"_call_on_new_stack_0_1:\n"
"   movq  %rsp, %rbp\n"     
"   movq  %rdi, %rsp\n"     
"   movq  %rcx, %rdi\n"     
"   pushq %rsi\n"           
"   pushq %rdx\n"           
"   movq $0, %rax\n"        
"   movq $0, %rbx\n"
"   movq $0, %rcx\n"
"   movq $0, %rdx\n"
"   movq $0, %rsi\n"
"   movq $0, %rbp\n"
"   movq $0, %r8\n"
"   movq $0, %r9\n"
"   movq $0, %r10\n"
"   movq $0, %r11\n"
"   movq $0, %r12\n"
"   movq $0, %r13\n"
"   movq $0, %r14\n"
"   movq $0, %r15\n"
"   ret\n"                 
"   ud2\n"                 
);

asm(
".globl _pthread_hijack_asm\n"
"_pthread_hijack_asm:\n"
"   movq %rsp,%rbp\n"
"   push $0\n"    
"   push %rbp\n"  
                  
"   push $0\n"    
"   jmp _pthread_hijack\n"
);



void pthread_hijack(Addr self, Addr kport, Addr func, Addr func_arg, 
                    Addr stacksize, Addr flags, Addr sp)
{
   vki_sigset_t blockall;
   ThreadState *tst = (ThreadState *)func_arg;
   VexGuestAMD64State *vex = &tst->arch.vex;

   

   
   
   semaphore_wait(tst->os_state.child_go);

   VG_(sigfillset)(&blockall);
   VG_(sigprocmask)(VKI_SIG_SETMASK, &blockall, NULL);

   
   
   
   LibVEX_GuestAMD64_initialise(vex);
   vex->guest_RIP = pthread_starter;
   vex->guest_RDI = self;
   vex->guest_RSI = kport;
   vex->guest_RDX = func;
   vex->guest_RCX = tst->os_state.func_arg;
   vex->guest_R8  = stacksize;
   vex->guest_R9  = flags;
   vex->guest_RSP = sp;

   
   tst->os_state.pthread = self;
   tst->os_state.lwpid = kport;
   record_named_port(tst->tid, kport, MACH_PORT_RIGHT_SEND, "thread-%p");

   if ((flags & 0x01000000) == 0) {
      
      Addr stack = VG_PGROUNDUP(sp) - stacksize;
      tst->client_stack_highest_byte = stack+stacksize-1;
      tst->client_stack_szB = stacksize;

      
      ML_(notify_core_and_tool_of_mmap)(
            stack+stacksize, pthread_structsize, 
            VKI_PROT_READ|VKI_PROT_WRITE, VKI_MAP_PRIVATE, -1, 0);
      
      ML_(notify_core_and_tool_of_mmap)(
            stack, stacksize, 
            VKI_PROT_READ|VKI_PROT_WRITE, VKI_MAP_PRIVATE, -1, 0);
      
      ML_(notify_core_and_tool_of_mmap)(
            stack-VKI_PAGE_SIZE, VKI_PAGE_SIZE,
            0, VKI_MAP_PRIVATE, -1, 0);
   } else {
      
      find_stack_segment(tst->tid, sp);
   }
   ML_(sync_mappings)("after", "pthread_hijack", 0);

   
   
   

   
   
   semaphore_signal(tst->os_state.child_done);
   

   
   call_on_new_stack_0_1(tst->os_state.valgrind_stack_init_SP, 0, 
                         start_thread_NORETURN, (Word)tst);

   
   vg_assert(0);
}



asm(
".globl _wqthread_hijack_asm\n"
"_wqthread_hijack_asm:\n"
"   movq %rsp,%r9\n"  
                      
"   push $0\n"        
"   jmp _wqthread_hijack\n"
);


void wqthread_hijack(Addr self, Addr kport, Addr stackaddr, Addr workitem, 
                     Int reuse, Addr sp)
{
   ThreadState *tst;
   VexGuestAMD64State *vex;
   Addr stack;
   SizeT stacksize;
   vki_sigset_t blockall;

   VG_(acquire_BigLock_LL)("wqthread_hijack");

   if (0) VG_(printf)(
             "wqthread_hijack: self %#lx, kport %#lx, "
	     "stackaddr %#lx, workitem %#lx, reuse/flags %x, sp %#lx\n", 
	     self, kport, stackaddr, workitem, reuse, sp);

   VG_(sigfillset)(&blockall);
   VG_(sigprocmask)(VKI_SIG_SETMASK, &blockall, NULL);

#  if DARWIN_VERS <= DARWIN_10_7
   Bool is_reuse = reuse != 0;
#  elif DARWIN_VERS == DARWIN_10_8 || DARWIN_VERS == DARWIN_10_9 || DARWIN_VERS == DARWIN_10_10
   Bool is_reuse = (reuse & 0x20000 ) != 0;
#  else
#    error "Unsupported Darwin version"
#  endif

   if (is_reuse) {

#      if DARWIN_VERS <= DARWIN_10_6
       UWord magic_delta = 0;
#      elif DARWIN_VERS == DARWIN_10_7 || DARWIN_VERS == DARWIN_10_8
       UWord magic_delta = 0x60;
#      elif DARWIN_VERS == DARWIN_10_9 || DARWIN_VERS == DARWIN_10_10
       UWord magic_delta = 0xE0;
#      else
#        error "magic_delta: to be computed on new OS version"
         
#      endif

       
       
       
       
       ThreadId tid = VG_(lwpid_to_vgtid)(kport);
       vg_assert(VG_(is_valid_tid)(tid));
       vg_assert(mach_thread_self() == kport);

       tst = VG_(get_ThreadState)(tid);

       if (0) VG_(printf)("wqthread_hijack reuse %s: tid %d, tst %p, "
                          "tst->os_state.pthread %#lx\n",
                          tst->os_state.pthread == self ? "SAME" : "DIFF",
                          tid, tst, tst->os_state.pthread);

       vex = &tst->arch.vex;
       vg_assert(tst->os_state.pthread - magic_delta == self);
   }
   else {
       
       tst = VG_(get_ThreadState)(VG_(alloc_ThreadState)());        
       vex = &tst->arch.vex;
       allocstack(tst->tid);
       LibVEX_GuestAMD64_initialise(vex);
   }
       
   
   
   
   vex->guest_RIP = wqthread_starter;
   vex->guest_RDI = self;
   vex->guest_RSI = kport;
   vex->guest_RDX = stackaddr;
   vex->guest_RCX = workitem;
   vex->guest_R8  = reuse;
   vex->guest_R9  = 0;
   vex->guest_RSP = sp;

   stacksize = 512*1024;  
   stack = VG_PGROUNDUP(sp) - stacksize;

   if (is_reuse) {
      
      

      VG_(release_BigLock_LL)("wqthread_hijack(1)");
      ML_(wqthread_continue_NORETURN)(tst->tid);
   } 
   else {
      
      tst->os_state.pthread = self;
      tst->os_state.lwpid = kport;
      record_named_port(tst->tid, kport, MACH_PORT_RIGHT_SEND, "wqthread-%p");
      
      
      tst->client_stack_highest_byte = stack+stacksize-1;
      tst->client_stack_szB = stacksize;

      
      
      
      ML_(notify_core_and_tool_of_mmap)(
            stack+stacksize, pthread_structsize, 
            VKI_PROT_READ|VKI_PROT_WRITE, VKI_MAP_PRIVATE, -1, 0);
      
      
      ML_(notify_core_and_tool_of_mmap)(
            stack, stacksize, 
            VKI_PROT_READ|VKI_PROT_WRITE, VKI_MAP_PRIVATE, -1, 0);
      
      
      ML_(notify_core_and_tool_of_mmap)(
            stack-VKI_PAGE_SIZE, VKI_PAGE_SIZE,
            0, VKI_MAP_PRIVATE, -1, 0);

      ML_(sync_mappings)("after", "wqthread_hijack", 0);

      
      VG_(release_BigLock_LL)("wqthread_hijack(2)");
      call_on_new_stack_0_1(tst->os_state.valgrind_stack_init_SP, 0, 
                            start_thread_NORETURN, (Word)tst);
   }

   
   vg_assert(0);
}

#endif 

