

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

#if defined(VGP_x86_darwin)

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
#include "pub_core_signals.h"
#include "pub_core_syscall.h"
#include "pub_core_syswrap.h"
#include "pub_core_tooliface.h"

#include "priv_types_n_macros.h"
#include "priv_syswrap-generic.h"   
#include "priv_syswrap-darwin.h"    
#include "priv_syswrap-main.h"


#include <mach/mach.h>

static void x86_thread_state32_from_vex(i386_thread_state_t *mach, 
                                        VexGuestX86State *vex)
{
    mach->__eax = vex->guest_EAX;
    mach->__ebx = vex->guest_EBX;
    mach->__ecx = vex->guest_ECX;
    mach->__edx = vex->guest_EDX;
    mach->__edi = vex->guest_EDI;
    mach->__esi = vex->guest_ESI;
    mach->__ebp = vex->guest_EBP;
    mach->__esp = vex->guest_ESP;
    mach->__ss = vex->guest_SS;
    mach->__eflags = LibVEX_GuestX86_get_eflags(vex);
    mach->__eip = vex->guest_EIP;
    mach->__cs = vex->guest_CS;
    mach->__ds = vex->guest_DS;
    mach->__es = vex->guest_ES;
    mach->__fs = vex->guest_FS;
    mach->__gs = vex->guest_GS;
}


static void x86_float_state32_from_vex(i386_float_state_t *mach, 
                                       VexGuestX86State *vex)
{
   

   VG_(memcpy)(&mach->__fpu_xmm0, &vex->guest_XMM0, 8 * sizeof(mach->__fpu_xmm0));
}


void thread_state_from_vex(thread_state_t mach_generic, 
                           thread_state_flavor_t flavor, 
                           mach_msg_type_number_t count, 
                           VexGuestArchState *vex_generic)
{
   VexGuestX86State *vex = (VexGuestX86State *)vex_generic;

   switch (flavor) {
   case i386_THREAD_STATE:
      vg_assert(count == i386_THREAD_STATE_COUNT);
      x86_thread_state32_from_vex((i386_thread_state_t *)mach_generic, vex);
      break;

   case i386_FLOAT_STATE:
      vg_assert(count == i386_FLOAT_STATE_COUNT);
      x86_float_state32_from_vex((i386_float_state_t *)mach_generic, vex);
      break;
       
   default:
      vg_assert(0);
   }
}


static void x86_thread_state32_to_vex(const i386_thread_state_t *mach, 
                                      VexGuestX86State *vex)
{
   LibVEX_GuestX86_initialise(vex);
   vex->guest_EAX = mach->__eax;
   vex->guest_EBX = mach->__ebx;
   vex->guest_ECX = mach->__ecx;
   vex->guest_EDX = mach->__edx;
   vex->guest_EDI = mach->__edi;
   vex->guest_ESI = mach->__esi;
   vex->guest_EBP = mach->__ebp;
   vex->guest_ESP = mach->__esp;
   vex->guest_SS = mach->__ss;
   
   vex->guest_EIP = mach->__eip;
   vex->guest_CS = mach->__cs;
   vex->guest_DS = mach->__ds;
   vex->guest_ES = mach->__es;
   vex->guest_FS = mach->__fs;
   vex->guest_GS = mach->__gs;
}

static void x86_float_state32_to_vex(const i386_float_state_t *mach, 
                                     VexGuestX86State *vex)
{
   

   VG_(memcpy)(&vex->guest_XMM0, &mach->__fpu_xmm0, 8 * sizeof(mach->__fpu_xmm0));
}


void thread_state_to_vex(const thread_state_t mach_generic, 
                         thread_state_flavor_t flavor, 
                         mach_msg_type_number_t count, 
                         VexGuestArchState *vex_generic)
{
   VexGuestX86State *vex = (VexGuestX86State *)vex_generic;
   
   switch(flavor) {
   case i386_THREAD_STATE:
      vg_assert(count == i386_THREAD_STATE_COUNT);
      x86_thread_state32_to_vex((const i386_thread_state_t*)mach_generic,vex);
      break;
   case i386_FLOAT_STATE:
      vg_assert(count == i386_FLOAT_STATE_COUNT);
      x86_float_state32_to_vex((const i386_float_state_t*)mach_generic,vex);
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
    
   vg_assert(flavor == i386_THREAD_STATE);
   vg_assert(count == i386_THREAD_STATE_COUNT);

   

   thread_state_to_vex(state, flavor, count, &tst->arch.vex);

   I_die_here;
   

   find_stack_segment(tid, tst->arch.vex.guest_ESP);

   return tst;
}


void hijack_thread_state(thread_state_t mach_generic, 
                         thread_state_flavor_t flavor, 
                         mach_msg_type_number_t count, 
                         ThreadState *tst)
{
   i386_thread_state_t *mach = (i386_thread_state_t *)mach_generic;
   char *stack;

   vg_assert(flavor == i386_THREAD_STATE);
   vg_assert(count == i386_THREAD_STATE_COUNT);

   stack = (char *)allocstack(tst->tid);
   stack -= 64+320;                       
   memset(stack, 0, 64+320);              
   *(uintptr_t *)stack = (uintptr_t)tst;  
   stack -= sizeof(uintptr_t);
   *(uintptr_t *)stack = 0;               

   mach->__eip = (uintptr_t)&start_thread_NORETURN;
   mach->__esp = (uintptr_t)stack;
}


__attribute__((noreturn))
void call_on_new_stack_0_1 ( Addr stack,
			     Addr retaddr,
			     void (*f)(Word),
                             Word arg1 );
asm(
".globl _call_on_new_stack_0_1\n"
"_call_on_new_stack_0_1:\n"
"   movl %esp, %esi\n"     
"   movl 4(%esi), %esp\n"  
"   pushl $0\n"            
"   pushl $0\n"            
"   pushl $0\n"            
"   pushl 16(%esi)\n"      
"   pushl  8(%esi)\n"      
"   pushl 12(%esi)\n"      
"   movl $0, %eax\n"       
"   movl $0, %ebx\n"
"   movl $0, %ecx\n"
"   movl $0, %edx\n"
"   movl $0, %esi\n"
"   movl $0, %edi\n"
"   movl $0, %ebp\n"
"   ret\n"                 
"   ud2\n"                 
);


asm(
".globl _pthread_hijack_asm\n"
"_pthread_hijack_asm:\n"
"   movl %esp,%ebp\n"
"   push $0\n"    
"   push %ebp\n"  
"   push %esi\n"  
"   push %edi\n"  
"   push %edx\n"  
"   push %ecx\n"  
"   push %ebx\n"  
"   push %eax\n"  
"   push $0\n"    
"   jmp _pthread_hijack\n"
    );



void pthread_hijack(Addr self, Addr kport, Addr func, Addr func_arg, 
                    Addr stacksize, Addr flags, Addr sp)
{
   vki_sigset_t blockall;
   ThreadState *tst = (ThreadState *)func_arg;
   VexGuestX86State *vex = &tst->arch.vex;

   

   
   
   semaphore_wait(tst->os_state.child_go);

   VG_(sigfillset)(&blockall);
   VG_(sigprocmask)(VKI_SIG_SETMASK, &blockall, NULL);

   
   
   
   
   LibVEX_GuestX86_initialise(vex);
   vex->guest_EIP = pthread_starter;
   vex->guest_EAX = self;
   vex->guest_EBX = kport;
   vex->guest_ECX = func;
   vex->guest_EDX = tst->os_state.func_arg;
   vex->guest_EDI = stacksize;
   vex->guest_ESI = flags;
   vex->guest_ESP = sp;

   
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
"   movl %esp,%ebp\n"
"   push $0\n"    
"   push $0\n"    
"   push %ebp\n"  
"   push %edi\n"  
"   push %edx\n"  
"   push %ecx\n"  
"   push %ebx\n"  
"   push %eax\n"  
"   push $0\n"    
"   jmp _wqthread_hijack\n"
    );


void wqthread_hijack(Addr self, Addr kport, Addr stackaddr, Addr workitem, 
                     Int reuse, Addr sp)
{
   ThreadState *tst;
   VexGuestX86State *vex;
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

#     if DARWIN_VERS <= DARWIN_10_6
      UWord magic_delta = 0;
#     elif DARWIN_VERS == DARWIN_10_7 || DARWIN_VERS == DARWIN_10_8
      UWord magic_delta = 0x48;
#     elif DARWIN_VERS == DARWIN_10_9 || DARWIN_VERS == DARWIN_10_10
      UWord magic_delta = 0xB0;
#     else
#       error "magic_delta: to be computed on new OS version"
        
#     endif

      
      
      
      
      ThreadId tid = VG_(lwpid_to_vgtid)(kport);
      vg_assert(VG_(is_valid_tid)(tid));
      vg_assert(mach_thread_self() == kport);

      tst = VG_(get_ThreadState)(tid);

      if (0) VG_(printf)("wqthread_hijack reuse %s: tid %d, tst %p, "
                         "tst->os_state.pthread %#lx, self %#lx\n",
                         tst->os_state.pthread == self ? "SAME" : "DIFF",
                         tid, tst, tst->os_state.pthread, self);

      vex = &tst->arch.vex;
      vg_assert(tst->os_state.pthread - magic_delta == self);
   }
   else {
      
      tst = VG_(get_ThreadState)(VG_(alloc_ThreadState)());        
      vex = &tst->arch.vex;
      allocstack(tst->tid);
      LibVEX_GuestX86_initialise(vex);
   }
        
   
   
   
   vex->guest_EIP = wqthread_starter;
   vex->guest_EAX = self;
   vex->guest_EBX = kport;
   vex->guest_ECX = stackaddr;
   vex->guest_EDX = workitem;
   vex->guest_EDI = reuse;
   vex->guest_ESI = 0;
   vex->guest_ESP = sp;

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

