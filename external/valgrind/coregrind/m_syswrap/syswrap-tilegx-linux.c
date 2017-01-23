

/*
  This file is part of Valgrind, a dynamic binary instrumentation
  framework.

  Copyright (C) 2010-2013 Tilera Corp.

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


#if defined(VGP_tilegx_linux)
#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_vkiscnums.h"
#include "pub_core_threadstate.h"
#include "pub_core_aspacemgr.h"
#include "pub_core_debuglog.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcprint.h"
#include "pub_core_libcproc.h"
#include "pub_core_libcsignal.h"
#include "pub_core_options.h"
#include "pub_core_scheduler.h"
#include "pub_core_sigframe.h"     
#include "pub_core_signals.h"
#include "pub_core_syscall.h"
#include "pub_core_syswrap.h"
#include "pub_core_tooliface.h"
#include "pub_core_stacks.h"        
#include "pub_core_transtab.h"      
#include "priv_types_n_macros.h"
#include "priv_syswrap-generic.h"   
#include "priv_syswrap-linux.h"     
#include "priv_syswrap-main.h"

#include "pub_core_debuginfo.h"     
#include "pub_core_xarray.h"
#include "pub_core_clientstate.h"   
#include "pub_core_errormgr.h"
#include "pub_core_libcfile.h"
#include "pub_core_machine.h"       
#include "pub_core_mallocfree.h"
#include "pub_core_stacktrace.h"    
#include "pub_core_ume.h"

#include "config.h"


__attribute__ ((noreturn))
void ML_(call_on_new_stack_0_1) (Addr stack, Addr retaddr,
                                 void (*f) (Word), Word arg1);
                                
                                
                                
                                
     asm (
       ".text\n"
       ".globl vgModuleLocal_call_on_new_stack_0_1\n"
       "vgModuleLocal_call_on_new_stack_0_1:\n"
       "  {\n"
       "   move sp, r0\n\t"
       "   move r51, r2\n\t"
       "  }\n"
       "  {\n"
       "   move r0, r3\n\t"
       "   move r1, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r2, zero\n\t"
       "   move r3, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r4, zero\n\t"
       "   move r5, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r6, zero\n\t"
       "   move r7, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r8, zero\n\t"
       "   move r9, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r10, zero\n\t"
       "   move r11, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r12, zero\n\t"
       "   move r13, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r14, zero\n\t"
       "   move r15, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r16, zero\n\t"
       "   move r17, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r18, zero\n\t"
       "   move r19, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r20, zero\n\t"
       "   move r21, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r22, zero\n\t"
       "   move r23, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r24, zero\n\t"
       "   move r25, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r26, zero\n\t"
       "   move r27, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r28, zero\n\t"
       "   move r29, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r30, zero\n\t"
       "   move r31, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r32, zero\n\t"
       "   move r33, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r34, zero\n\t"
       "   move r35, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r36, zero\n\t"
       "   move r37, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r38, zero\n\t"
       "   move r39, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r40, zero\n\t"
       "   move r41, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r42, zero\n\t"
       "   move r43, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r44, zero\n\t"
       "   move r45, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r46, zero\n\t"
       "   move r47, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r48, zero\n\t"
       "   move r49, zero\n\t"
       "  }\n"
       "  {\n"
       "   move r50, zero\n\t"
       "   jr      r51\n\t"
       "  }\n"
       "   ill \n"    
          );
#define __NR_CLONE        VG_STRINGIFY(__NR_clone)
#define __NR_EXIT         VG_STRINGIFY(__NR_exit)

Long do_syscall_clone_tilegx_linux ( Word (*fn) (void *),  
                                     void *stack,          
                                     Long flags,           
                                     void *arg,            
                                     Long * child_tid,     
                                     Long * parent_tid,    
                                     Long   tls );         
     asm (
       ".text\n"
       "   .globl   do_syscall_clone_tilegx_linux\n"
       "   do_syscall_clone_tilegx_linux:\n"
       "   beqz  r0, .Linvalid\n"
       "   beqz  r1, .Linvalid\n"
       "   {\n"
       "    st    sp, r29; "       
       "    addli sp, sp, -32\n"   
       "   }\n"

       "    move  r29, sp; "       
       "    st    r29, lr\n"       

       "    addi  r29, r29, 8\n"
       "   {\n"
       "    st    r29, r10\n"      
       
       "    addi  r1, r1, -32\n"   
       "   }\n"
       
       "   { st  r1, r0; addi r1, r1, 8 }\n"
       
       "   { st  r1, r3; addi r1, r1, 8 }\n"
       
       "   { st  r1, r2; addi r1, r1, -16 }\n"

       "   {\n"
       
       "    move r0,  r2\n"   
       
       "    move r2,  r5\n"   
       "   }\n"
       "   {\n"
       "    move r3,  r4\n"   
       "    move r4,  r6\n"   
       "   }\n"
       "   moveli r10, " __NR_CLONE "\n"
       "   swint1\n"

       "   beqz  r0, .Lchild\n"
       "   move r29, sp\n"
       "   ld   lr, r29\n"        
       "   addi r29, r29, 8\n"
       "   {\n"
       "    ld   r10,  r29\n"      
       "    addi sp, sp, 32\n"
       "   }\n"
       "   ld   r29, sp\n"
       "   jrp  lr\n"

       ".Lchild:"
       "   move r2, sp\n"
       "   {\n"
       "    ld   r3, r2\n"
       "    addi r2, r2, 8\n"
       "   }\n"
       "   ld   r0, r2\n"
       "   jalr r3\n"
       "   moveli r10, " __NR_EXIT "\n"
       "   swint1\n"

       ".Linvalid:"
       "  { movei r1, 22; jrp lr }\n"
          );

#undef __NR_CLONE
#undef __NR_EXIT

static void setup_child ( ThreadArchState *, ThreadArchState * );
static SysRes sys_set_tls ( ThreadId tid, Addr tlsptr );
static SysRes do_clone ( ThreadId ptid,
                         Long flags, Addr sp,
                         Long * parent_tidptr,
                         Long * child_tidptr,
                         Addr child_tls )
{
  const Bool debug = False;
  ThreadId ctid = VG_ (alloc_ThreadState) ();
  ThreadState * ptst = VG_ (get_ThreadState) (ptid);
  ThreadState * ctst = VG_ (get_ThreadState) (ctid);
  Long ret = 0;
  Long * stack;
  SysRes res;
  vki_sigset_t blockall, savedmask;

  VG_ (sigfillset) (&blockall);
  vg_assert (VG_ (is_running_thread) (ptid));
  vg_assert (VG_ (is_valid_tid) (ctid));
  stack = (Long *) ML_ (allocstack) (ctid);
  if (stack == NULL) {
    res = VG_ (mk_SysRes_Error) (VKI_ENOMEM);
    goto out;
  }
  setup_child (&ctst->arch, &ptst->arch);

  
  ctst->arch.vex.guest_r0 = 0;
  ctst->arch.vex.guest_r3 = 0;
  if (sp != 0)
    ctst->arch.vex.guest_r54 = sp;

  ctst->os_state.parent = ptid;
  ctst->sig_mask = ptst->sig_mask;
  ctst->tmp_sig_mask = ptst->sig_mask;


  ctst->os_state.threadgroup = ptst->os_state.threadgroup;
  ML_(guess_and_register_stack) (sp, ctst);

  VG_TRACK (pre_thread_ll_create, ptid, ctid);
  if (flags & VKI_CLONE_SETTLS) {
    if (debug)
      VG_(printf)("clone child has SETTLS: tls at %#lx\n", child_tls);
    ctst->arch.vex.guest_r53 = child_tls;
    res = sys_set_tls(ctid, child_tls);
    if (sr_isError(res))
      goto out;
  }

  flags &= ~VKI_CLONE_SETTLS;
  VG_ (sigprocmask) (VKI_SIG_SETMASK, &blockall, &savedmask);
  
  ret = do_syscall_clone_tilegx_linux (ML_ (start_thread_NORETURN),
                                       stack, flags, &VG_ (threads)[ctid],
                                       child_tidptr, parent_tidptr,
                                       (Long)NULL );

  
  if (debug)
    VG_(printf)("ret: 0x%lx\n", ret);

  res = VG_(mk_SysRes_tilegx_linux) ( ret);

  VG_ (sigprocmask) (VKI_SIG_SETMASK, &savedmask, NULL);

 out:
  if (sr_isError (res)) {
    VG_(cleanup_thread) (&ctst->arch);
    ctst->status = VgTs_Empty;
    VG_TRACK (pre_thread_ll_exit, ctid);
  }
  ptst->arch.vex.guest_r0 = 0;

  return res;
}

extern Addr do_brk ( Addr newbrk );

extern
SysRes do_mremap( Addr old_addr, SizeT old_len,
                  Addr new_addr, SizeT new_len,
                  UWord flags, ThreadId tid );

extern Bool linux_kernel_2_6_22(void);


void
VG_ (cleanup_thread) ( ThreadArchState * arch ) { }

void
setup_child (  ThreadArchState * child,
               ThreadArchState * parent )
{
  
  child->vex = parent->vex;
  child->vex_shadow1 = parent->vex_shadow1;
  child->vex_shadow2 = parent->vex_shadow2;
}

SysRes sys_set_tls ( ThreadId tid, Addr tlsptr )
{
  VG_(threads)[tid].arch.vex.guest_r53 = tlsptr;
  return VG_(mk_SysRes_Success)( 0 );
}


#define PRE(name)       DEFN_PRE_TEMPLATE(tilegx_linux, name)
#define POST(name)      DEFN_POST_TEMPLATE(tilegx_linux, name)


DECL_TEMPLATE (tilegx_linux, sys_clone);
DECL_TEMPLATE (tilegx_linux, sys_rt_sigreturn);
DECL_TEMPLATE (tilegx_linux, sys_socket);
DECL_TEMPLATE (tilegx_linux, sys_setsockopt);
DECL_TEMPLATE (tilegx_linux, sys_getsockopt);
DECL_TEMPLATE (tilegx_linux, sys_connect);
DECL_TEMPLATE (tilegx_linux, sys_accept);
DECL_TEMPLATE (tilegx_linux, sys_accept4);
DECL_TEMPLATE (tilegx_linux, sys_sendto);
DECL_TEMPLATE (tilegx_linux, sys_recvfrom);
DECL_TEMPLATE (tilegx_linux, sys_sendmsg);
DECL_TEMPLATE (tilegx_linux, sys_recvmsg);
DECL_TEMPLATE (tilegx_linux, sys_shutdown);
DECL_TEMPLATE (tilegx_linux, sys_bind);
DECL_TEMPLATE (tilegx_linux, sys_listen);
DECL_TEMPLATE (tilegx_linux, sys_getsockname);
DECL_TEMPLATE (tilegx_linux, sys_getpeername);
DECL_TEMPLATE (tilegx_linux, sys_socketpair);
DECL_TEMPLATE (tilegx_linux, sys_semget);
DECL_TEMPLATE (tilegx_linux, sys_semop);
DECL_TEMPLATE (tilegx_linux, sys_semtimedop);
DECL_TEMPLATE (tilegx_linux, sys_semctl);
DECL_TEMPLATE (tilegx_linux, sys_msgget);
DECL_TEMPLATE (tilegx_linux, sys_msgrcv);
DECL_TEMPLATE (tilegx_linux, sys_msgsnd);
DECL_TEMPLATE (tilegx_linux, sys_msgctl);
DECL_TEMPLATE (tilegx_linux, sys_shmget);
DECL_TEMPLATE (tilegx_linux, wrap_sys_shmat);
DECL_TEMPLATE (tilegx_linux, sys_shmdt);
DECL_TEMPLATE (tilegx_linux, sys_shmdt);
DECL_TEMPLATE (tilegx_linux, sys_shmctl);
DECL_TEMPLATE (tilegx_linux, sys_arch_prctl);
DECL_TEMPLATE (tilegx_linux, sys_ptrace);
DECL_TEMPLATE (tilegx_linux, sys_fadvise64);
DECL_TEMPLATE (tilegx_linux, sys_mmap);
DECL_TEMPLATE (tilegx_linux, sys_syscall184);
DECL_TEMPLATE (tilegx_linux, sys_cacheflush);
DECL_TEMPLATE (tilegx_linux, sys_set_dataplane);

PRE(sys_clone)
{
  ULong cloneflags;

  PRINT("sys_clone ( %lx, %#lx, %#lx, %#lx, %#lx )",ARG1,ARG2,ARG3,ARG4,ARG5);
  PRE_REG_READ5(int, "clone",
                unsigned long, flags,
                void *, child_stack,
                int *, parent_tidptr,
                int *, child_tidptr,
                void *, tlsaddr);

  if (ARG1 & VKI_CLONE_PARENT_SETTID) {
    PRE_MEM_WRITE("clone(parent_tidptr)", ARG3, sizeof(Int));
    if (!VG_(am_is_valid_for_client)(ARG3, sizeof(Int), VKI_PROT_WRITE)) {
      SET_STATUS_Failure( VKI_EFAULT );
      return;
    }
  }
  if (ARG1 & (VKI_CLONE_CHILD_SETTID | VKI_CLONE_CHILD_CLEARTID)) {
    PRE_MEM_WRITE("clone(child_tidptr)", ARG4, sizeof(Int));
    if (!VG_(am_is_valid_for_client)(ARG4, sizeof(Int), VKI_PROT_WRITE)) {
      SET_STATUS_Failure( VKI_EFAULT );
      return;
    }
  }

  cloneflags = ARG1;

  if (!ML_(client_signal_OK)(ARG1 & VKI_CSIGNAL)) {
    SET_STATUS_Failure( VKI_EINVAL );
    return;
  }

  
  switch (cloneflags & (VKI_CLONE_VM | VKI_CLONE_FS
                        | VKI_CLONE_FILES | VKI_CLONE_VFORK)) {
  case VKI_CLONE_VM | VKI_CLONE_FS | VKI_CLONE_FILES:
    
    SET_STATUS_from_SysRes(
      do_clone(tid,
               ARG1,          
               (Addr)ARG2,    
               (Long *)ARG3,  
               (Long *)ARG4,  
               (Addr)ARG5));  
    break;

  case VKI_CLONE_VFORK | VKI_CLONE_VM: 
    
    cloneflags &= ~(VKI_CLONE_VFORK | VKI_CLONE_VM);

  case 0: 
    SET_STATUS_from_SysRes(
      ML_(do_fork_clone)(tid,
                         cloneflags,      
                         (Int *)ARG3,     
                         (Int *)ARG4));   
    break;

  default:
    
    VG_(message)(Vg_UserMsg,
                 "Unsupported clone() flags: 0x%lx\n", ARG1);
    VG_(message)(Vg_UserMsg,
                 "\n");
    VG_(message)(Vg_UserMsg,
                 "The only supported clone() uses are:\n");
    VG_(message)(Vg_UserMsg,
                 " - via a threads library (LinuxThreads or NPTL)\n");
    VG_(message)(Vg_UserMsg,
                 " - via the implementation of fork or vfork\n");
    VG_(unimplemented)
      ("Valgrind does not support general clone().");
  }

  if (SUCCESS) {
    if (ARG1 & VKI_CLONE_PARENT_SETTID)
      POST_MEM_WRITE(ARG3, sizeof(Int));
    if (ARG1 & (VKI_CLONE_CHILD_SETTID | VKI_CLONE_CHILD_CLEARTID))
      POST_MEM_WRITE(ARG4, sizeof(Int));

    *flags |= SfYieldAfter;
  }
}

PRE(sys_rt_sigreturn)
{

  ThreadState* tst;
  PRINT("sys_rt_sigreturn ( )");

  vg_assert(VG_(is_valid_tid)(tid));
  vg_assert(tid >= 1 && tid < VG_N_THREADS);
  vg_assert(VG_(is_running_thread)(tid));

  tst = VG_(get_ThreadState)(tid);
  tst->arch.vex.guest_r54 -= sizeof(Addr);

  ML_(fixup_guest_state_to_restart_syscall)(&tst->arch);

  VG_(sigframe_destroy)(tid, True);

  *flags |= SfNoWriteResult;
  SET_STATUS_Success(0);

  
  *flags |= SfPollAfter;
}

PRE(sys_arch_prctl)
{
  PRINT( "arch_prctl ( %ld, %lx )", ARG1, ARG2 );

  vg_assert(VG_(is_valid_tid)(tid));
  vg_assert(tid >= 1 && tid < VG_N_THREADS);
  vg_assert(VG_(is_running_thread)(tid));

  I_die_here;
}

PRE(sys_ptrace)
{
  PRINT("sys_ptrace ( %ld, %ld, %#lx, %#lx )", ARG1,ARG2,ARG3,ARG4);
  PRE_REG_READ4(int, "ptrace",
                long, request, long, pid, long, addr, long, data);
  switch (ARG1) {
  case VKI_PTRACE_PEEKTEXT:
  case VKI_PTRACE_PEEKDATA:
  case VKI_PTRACE_PEEKUSR:
    PRE_MEM_WRITE( "ptrace(peek)", ARG4,
                   sizeof (long));
    break;
  case VKI_PTRACE_GETREGS:
    PRE_MEM_WRITE( "ptrace(getregs)", ARG4,
                   sizeof (struct vki_user_regs_struct));
    break;
#if 0 
  case VKI_PTRACE_GETFPREGS:
    PRE_MEM_WRITE( "ptrace(getfpregs)", ARG4,
                   sizeof (struct vki_user_i387_struct));
    break;
#endif
  case VKI_PTRACE_SETREGS:
    PRE_MEM_READ( "ptrace(setregs)", ARG4,
                  sizeof (struct vki_user_regs_struct));
    break;
#if 0 
  case VKI_PTRACE_SETFPREGS:
    PRE_MEM_READ( "ptrace(setfpregs)", ARG4,
                  sizeof (struct vki_user_i387_struct));
    break;
#endif
  case VKI_PTRACE_GETEVENTMSG:
    PRE_MEM_WRITE( "ptrace(geteventmsg)", ARG4, sizeof(unsigned long));
    break;
  case VKI_PTRACE_GETSIGINFO:
    PRE_MEM_WRITE( "ptrace(getsiginfo)", ARG4, sizeof(vki_siginfo_t));
    break;
  case VKI_PTRACE_SETSIGINFO:
    PRE_MEM_READ( "ptrace(setsiginfo)", ARG4, sizeof(vki_siginfo_t));
    break;
  default:
    break;
  }
}

POST(sys_ptrace)
{
  switch (ARG1) {
  case VKI_PTRACE_PEEKTEXT:
  case VKI_PTRACE_PEEKDATA:
  case VKI_PTRACE_PEEKUSR:
    POST_MEM_WRITE( ARG4, sizeof (long));
    break;
  case VKI_PTRACE_GETREGS:
    POST_MEM_WRITE( ARG4, sizeof (struct vki_user_regs_struct));
    break;
#if 0 
  case VKI_PTRACE_GETFPREGS:
    POST_MEM_WRITE( ARG4, sizeof (struct vki_user_i387_struct));
    break;
#endif
  case VKI_PTRACE_GETEVENTMSG:
    POST_MEM_WRITE( ARG4, sizeof(unsigned long));
    break;
  case VKI_PTRACE_GETSIGINFO:
    POST_MEM_WRITE( ARG4, sizeof(vki_siginfo_t));
    break;
  default:
    break;
  }
}

PRE(sys_socket)
{
  PRINT("sys_socket ( %ld, %ld, %ld )",ARG1,ARG2,ARG3);
  PRE_REG_READ3(long, "socket", int, domain, int, type, int, protocol);
}
POST(sys_socket)
{
  SysRes r;
  vg_assert(SUCCESS);
  r = ML_(generic_POST_sys_socket)(tid, VG_(mk_SysRes_Success)(RES));
  SET_STATUS_from_SysRes(r);
}

PRE(sys_setsockopt)
{
  PRINT("sys_setsockopt ( %ld, %ld, %ld, %#lx, %ld )",ARG1,ARG2,ARG3,ARG4,ARG5);
  PRE_REG_READ5(long, "setsockopt",
                int, s, int, level, int, optname,
                const void *, optval, int, optlen);
  ML_(generic_PRE_sys_setsockopt)(tid, ARG1,ARG2,ARG3,ARG4,ARG5);
}

PRE(sys_getsockopt)
{
  PRINT("sys_getsockopt ( %ld, %ld, %ld, %#lx, %#lx )",ARG1,ARG2,ARG3,ARG4,ARG5);
  PRE_REG_READ5(long, "getsockopt",
                int, s, int, level, int, optname,
                void *, optval, int, *optlen);
  ML_(linux_PRE_sys_getsockopt)(tid, ARG1,ARG2,ARG3,ARG4,ARG5);
}
POST(sys_getsockopt)
{
  vg_assert(SUCCESS);
  ML_(linux_POST_sys_getsockopt)(tid, VG_(mk_SysRes_Success)(RES),
                                 ARG1,ARG2,ARG3,ARG4,ARG5);
}

PRE(sys_connect)
{
  *flags |= SfMayBlock;
  PRINT("sys_connect ( %ld, %#lx, %ld )",ARG1,ARG2,ARG3);
  PRE_REG_READ3(long, "connect",
                int, sockfd, struct sockaddr *, serv_addr, int, addrlen);
  ML_(generic_PRE_sys_connect)(tid, ARG1,ARG2,ARG3);
}

PRE(sys_accept)
{
  *flags |= SfMayBlock;
  PRINT("sys_accept ( %ld, %#lx, %ld )",ARG1,ARG2,ARG3);
  PRE_REG_READ3(long, "accept",
                int, s, struct sockaddr *, addr, int, *addrlen);
  ML_(generic_PRE_sys_accept)(tid, ARG1,ARG2,ARG3);
}
POST(sys_accept)
{
  SysRes r;
  vg_assert(SUCCESS);
  r = ML_(generic_POST_sys_accept)(tid, VG_(mk_SysRes_Success)(RES),
                                   ARG1,ARG2,ARG3);
  SET_STATUS_from_SysRes(r);
}

PRE(sys_accept4)
{
  *flags |= SfMayBlock;
  PRINT("sys_accept4 ( %ld, %#lx, %ld, %ld )",ARG1,ARG2,ARG3,ARG4);
  PRE_REG_READ4(long, "accept4",
                int, s, struct sockaddr *, addr, int, *addrlen, int, flags);
  ML_(generic_PRE_sys_accept)(tid, ARG1,ARG2,ARG3);
}
POST(sys_accept4)
{
  SysRes r;
  vg_assert(SUCCESS);
  r = ML_(generic_POST_sys_accept)(tid, VG_(mk_SysRes_Success)(RES),
                                   ARG1,ARG2,ARG3);
  SET_STATUS_from_SysRes(r);
}

PRE(sys_sendto)
{
  *flags |= SfMayBlock;
  PRINT("sys_sendto ( %ld, %#lx, %ld, %lu, %#lx, %ld )",ARG1,ARG2,ARG3,
        ARG4,ARG5,ARG6);
  PRE_REG_READ6(long, "sendto",
                int, s, const void *, msg, int, len,
                unsigned int, flags,
                const struct sockaddr *, to, int, tolen);
  ML_(generic_PRE_sys_sendto)(tid, ARG1,ARG2,ARG3,ARG4,ARG5,ARG6);
}

PRE(sys_recvfrom)
{
  *flags |= SfMayBlock;
  PRINT("sys_recvfrom ( %ld, %#lx, %ld, %lu, %#lx, %#lx )",ARG1,ARG2,ARG3,
        ARG4,ARG5,ARG6);
  PRE_REG_READ6(long, "recvfrom",
                int, s, void *, buf, int, len, unsigned int, flags,
                struct sockaddr *, from, int *, fromlen);
  ML_(generic_PRE_sys_recvfrom)(tid, ARG1,ARG2,ARG3,ARG4,ARG5,ARG6);
}
POST(sys_recvfrom)
{
  vg_assert(SUCCESS);
  ML_(generic_POST_sys_recvfrom)(tid, VG_(mk_SysRes_Success)(RES),
                                 ARG1,ARG2,ARG3,ARG4,ARG5,ARG6);
}

PRE(sys_sendmsg)
{
  *flags |= SfMayBlock;
  PRINT("sys_sendmsg ( %ld, %#lx, %ld )",ARG1,ARG2,ARG3);
  PRE_REG_READ3(long, "sendmsg",
                int, s, const struct msghdr *, msg, int, flags);
  ML_(generic_PRE_sys_sendmsg)(tid, "msg", ARG2);
}

PRE(sys_recvmsg)
{
  *flags |= SfMayBlock;
  PRINT("sys_recvmsg ( %ld, %#lx, %ld )",ARG1,ARG2,ARG3);
  PRE_REG_READ3(long, "recvmsg", int, s, struct msghdr *, msg, int, flags);
  ML_(generic_PRE_sys_recvmsg)(tid, "msg", (struct vki_msghdr *) ARG2);
}

POST(sys_recvmsg)
{
  ML_(generic_POST_sys_recvmsg)(tid, "msg", (struct vki_msghdr *)ARG2, RES);
}

PRE(sys_shutdown)
{
  *flags |= SfMayBlock;
  PRINT("sys_shutdown ( %ld, %ld )",ARG1,ARG2);
  PRE_REG_READ2(int, "shutdown", int, s, int, how);
}

PRE(sys_bind)
{
  PRINT("sys_bind ( %ld, %#lx, %ld )",ARG1,ARG2,ARG3);
  PRE_REG_READ3(long, "bind",
                int, sockfd, struct sockaddr *, my_addr, int, addrlen);
  ML_(generic_PRE_sys_bind)(tid, ARG1,ARG2,ARG3);
}

PRE(sys_listen)
{
  PRINT("sys_listen ( %ld, %ld )",ARG1,ARG2);
  PRE_REG_READ2(long, "listen", int, s, int, backlog);
}

PRE(sys_getsockname)
{
  PRINT("sys_getsockname ( %ld, %#lx, %#lx )",ARG1,ARG2,ARG3);
  PRE_REG_READ3(long, "getsockname",
                int, s, struct sockaddr *, name, int *, namelen);
  ML_(generic_PRE_sys_getsockname)(tid, ARG1,ARG2,ARG3);
}
POST(sys_getsockname)
{
  vg_assert(SUCCESS);
  ML_(generic_POST_sys_getsockname)(tid, VG_(mk_SysRes_Success)(RES),
                                    ARG1,ARG2,ARG3);
}

PRE(sys_getpeername)
{
  PRINT("sys_getpeername ( %ld, %#lx, %#lx )",ARG1,ARG2,ARG3);
  PRE_REG_READ3(long, "getpeername",
                int, s, struct sockaddr *, name, int *, namelen);
  ML_(generic_PRE_sys_getpeername)(tid, ARG1,ARG2,ARG3);
}
POST(sys_getpeername)
{
  vg_assert(SUCCESS);
  ML_(generic_POST_sys_getpeername)(tid, VG_(mk_SysRes_Success)(RES),
                                    ARG1,ARG2,ARG3);
}

PRE(sys_socketpair)
{
  PRINT("sys_socketpair ( %ld, %ld, %ld, %#lx )",ARG1,ARG2,ARG3,ARG4);
  PRE_REG_READ4(long, "socketpair",
                int, d, int, type, int, protocol, int*, sv);
  ML_(generic_PRE_sys_socketpair)(tid, ARG1,ARG2,ARG3,ARG4);
}
POST(sys_socketpair)
{
  vg_assert(SUCCESS);
  ML_(generic_POST_sys_socketpair)(tid, VG_(mk_SysRes_Success)(RES),
                                   ARG1,ARG2,ARG3,ARG4);
}

PRE(sys_semget)
{
  PRINT("sys_semget ( %ld, %ld, %ld )",ARG1,ARG2,ARG3);
  PRE_REG_READ3(long, "semget", vki_key_t, key, int, nsems, int, semflg);
}

PRE(sys_semop)
{
  *flags |= SfMayBlock;
  PRINT("sys_semop ( %ld, %#lx, %lu )",ARG1,ARG2,ARG3);
  PRE_REG_READ3(long, "semop",
                int, semid, struct sembuf *, sops, unsigned, nsoops);
  ML_(generic_PRE_sys_semop)(tid, ARG1,ARG2,ARG3);
}

PRE(sys_semtimedop)
{
  *flags |= SfMayBlock;
  PRINT("sys_semtimedop ( %ld, %#lx, %lu, %#lx )",ARG1,ARG2,ARG3,ARG4);
  PRE_REG_READ4(long, "semtimedop",
                int, semid, struct sembuf *, sops, unsigned, nsoops,
                struct timespec *, timeout);
  ML_(generic_PRE_sys_semtimedop)(tid, ARG1,ARG2,ARG3,ARG4);
}

PRE(sys_semctl)
{
  switch (ARG3 & ~VKI_IPC_64) {
  case VKI_IPC_INFO:
  case VKI_SEM_INFO:
    PRINT("sys_semctl ( %ld, %ld, %ld, %#lx )",ARG1,ARG2,ARG3,ARG4);
    PRE_REG_READ4(long, "semctl",
                  int, semid, int, semnum, int, cmd, struct seminfo *, arg);
    break;
  case VKI_IPC_STAT:
  case VKI_SEM_STAT:
  case VKI_IPC_SET:
    PRINT("sys_semctl ( %ld, %ld, %ld, %#lx )",ARG1,ARG2,ARG3,ARG4);
    PRE_REG_READ4(long, "semctl",
                  int, semid, int, semnum, int, cmd, struct semid_ds *, arg);
    break;
  case VKI_GETALL:
  case VKI_SETALL:
    PRINT("sys_semctl ( %ld, %ld, %ld, %#lx )",ARG1,ARG2,ARG3,ARG4);
    PRE_REG_READ4(long, "semctl",
                  int, semid, int, semnum, int, cmd, unsigned short *, arg);
    break;
  default:
    PRINT("sys_semctl ( %ld, %ld, %ld )",ARG1,ARG2,ARG3);
    PRE_REG_READ3(long, "semctl",
                  int, semid, int, semnum, int, cmd);
    break;
  }
  ML_(generic_PRE_sys_semctl)(tid, ARG1,ARG2,ARG3|VKI_IPC_64,ARG4);
}
POST(sys_semctl)
{
  ML_(generic_POST_sys_semctl)(tid, RES,ARG1,ARG2,ARG3|VKI_IPC_64,ARG4);
}

PRE(sys_msgget)
{
  PRINT("sys_msgget ( %ld, %ld )",ARG1,ARG2);
  PRE_REG_READ2(long, "msgget", vki_key_t, key, int, msgflg);
}

PRE(sys_msgsnd)
{
  PRINT("sys_msgsnd ( %ld, %#lx, %ld, %ld )",ARG1,ARG2,ARG3,ARG4);
  PRE_REG_READ4(long, "msgsnd",
                int, msqid, struct msgbuf *, msgp, vki_size_t, msgsz,
                int, msgflg);
  ML_(linux_PRE_sys_msgsnd)(tid, ARG1,ARG2,ARG3,ARG4);
  if ((ARG4 & VKI_IPC_NOWAIT) == 0)
    *flags |= SfMayBlock;
}

PRE(sys_msgrcv)
{
  PRINT("sys_msgrcv ( %ld, %#lx, %ld, %ld, %ld )",ARG1,ARG2,ARG3,ARG4,ARG5);
  PRE_REG_READ5(long, "msgrcv",
                int, msqid, struct msgbuf *, msgp, vki_size_t, msgsz,
                long, msgytp, int, msgflg);
  ML_(linux_PRE_sys_msgrcv)(tid, ARG1,ARG2,ARG3,ARG4,ARG5);
  if ((ARG4 & VKI_IPC_NOWAIT) == 0)
    *flags |= SfMayBlock;
}
POST(sys_msgrcv)
{
  ML_(linux_POST_sys_msgrcv)(tid, RES,ARG1,ARG2,ARG3,ARG4,ARG5);
}

PRE(sys_msgctl)
{
  PRINT("sys_msgctl ( %ld, %ld, %#lx )",ARG1,ARG2,ARG3);
  PRE_REG_READ3(long, "msgctl",
                int, msqid, int, cmd, struct msqid_ds *, buf);
  ML_(linux_PRE_sys_msgctl)(tid, ARG1,ARG2,ARG3);
}
POST(sys_msgctl)
{
  ML_(linux_POST_sys_msgctl)(tid, RES,ARG1,ARG2,ARG3);
}

PRE(sys_shmget)
{
  PRINT("sys_shmget ( %ld, %ld, %ld )",ARG1,ARG2,ARG3);
  PRE_REG_READ3(long, "shmget", vki_key_t, key, vki_size_t, size, int, shmflg);
}

PRE(wrap_sys_shmat)
{
  UWord arg2tmp;
  PRINT("wrap_sys_shmat ( %ld, %#lx, %ld )",ARG1,ARG2,ARG3);
  PRE_REG_READ3(long, "shmat",
                int, shmid, const void *, shmaddr, int, shmflg);
  arg2tmp = ML_(generic_PRE_sys_shmat)(tid, ARG1,ARG2,ARG3);
  if (arg2tmp == 0)
    SET_STATUS_Failure( VKI_EINVAL );
  else
    ARG2 = arg2tmp;  
}
POST(wrap_sys_shmat)
{
  ML_(generic_POST_sys_shmat)(tid, RES,ARG1,ARG2,ARG3);
}

PRE(sys_shmdt)
{
  PRINT("sys_shmdt ( %#lx )",ARG1);
  PRE_REG_READ1(long, "shmdt", const void *, shmaddr);
  if (!ML_(generic_PRE_sys_shmdt)(tid, ARG1))
    SET_STATUS_Failure( VKI_EINVAL );
}
POST(sys_shmdt)
{
  ML_(generic_POST_sys_shmdt)(tid, RES,ARG1);
}

PRE(sys_shmctl)
{
  PRINT("sys_shmctl ( %ld, %ld, %#lx )",ARG1,ARG2,ARG3);
  PRE_REG_READ3(long, "shmctl",
                int, shmid, int, cmd, struct shmid_ds *, buf);
  ML_(generic_PRE_sys_shmctl)(tid, ARG1,ARG2|VKI_IPC_64,ARG3);
}
POST(sys_shmctl)
{
  ML_(generic_POST_sys_shmctl)(tid, RES,ARG1,ARG2|VKI_IPC_64,ARG3);
}

PRE(sys_fadvise64)
{
  PRINT("sys_fadvise64 ( %ld, %ld, %lu, %ld )", ARG1,ARG2,ARG3,ARG4);
  PRE_REG_READ4(long, "fadvise64",
                int, fd, vki_loff_t, offset, vki_size_t, len, int, advice);
}

PRE(sys_mmap)
{
  SysRes r;

  PRINT("sys_mmap ( %#lx, %llu, %ld, %ld, %d, %ld )",
        ARG1, (ULong)ARG2, ARG3, ARG4, (Int)ARG5, ARG6 );
  PRE_REG_READ6(long, "mmap",
                unsigned long, start, unsigned long, length,
                unsigned long, prot,  unsigned long, flags,
                unsigned long, fd,    unsigned long, offset);

  r = ML_(generic_PRE_sys_mmap)( tid, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6 );
  SET_STATUS_from_SysRes(r);
}


PRE(sys_cacheflush)
{
   PRINT("cacheflush (%lx, %lx, %lx)", ARG1, ARG2, ARG3);
   PRE_REG_READ3(long, "cacheflush", unsigned long, addr,
                 int, nbytes, int, cache);
   VG_ (discard_translations) ((Addr)ARG1, (ULong) ARG2,
                               "PRE(sys_cacheflush)");
   SET_STATUS_Success(0);
}

PRE(sys_set_dataplane)
{
  *flags |= SfMayBlock;
  PRINT("sys_set_dataplane ( %ld )", ARG1);
  PRE_REG_READ1(long, "set_dataplane", unsigned long, flag);
}

#undef PRE
#undef POST



#define PLAX_(const, name)    WRAPPER_ENTRY_X_(tilegx_linux, const, name)
#define PLAXY(const, name)    WRAPPER_ENTRY_XY(tilegx_linux, const, name)


static SyscallTableEntry syscall_table[] = {

  LINXY(__NR_io_setup,          sys_io_setup),             
  LINX_(__NR_io_destroy,        sys_io_destroy),           
  LINX_(__NR_io_submit,         sys_io_submit),            
  LINXY(__NR_io_cancel,         sys_io_cancel),            
  LINXY(__NR_io_getevents,      sys_io_getevents),         
  LINX_(__NR_setxattr,          sys_setxattr),             
  LINX_(__NR_lsetxattr,         sys_lsetxattr),            
  LINX_(__NR_fsetxattr,         sys_fsetxattr),            
  LINXY(__NR_getxattr,          sys_getxattr),             
  LINXY(__NR_lgetxattr,         sys_lgetxattr),            
  LINXY(__NR_fgetxattr,         sys_fgetxattr),            
  LINXY(__NR_listxattr,         sys_listxattr),            
  LINXY(__NR_llistxattr,        sys_llistxattr),           
  LINXY(__NR_flistxattr,        sys_flistxattr),           
  LINX_(__NR_removexattr,       sys_removexattr),          
  LINX_(__NR_lremovexattr,      sys_lremovexattr),         
  LINX_(__NR_fremovexattr,      sys_fremovexattr),         
  GENXY(__NR_getcwd,            sys_getcwd),               
  LINXY(__NR_lookup_dcookie,    sys_lookup_dcookie),       
  LINX_(__NR_eventfd2,          sys_eventfd2),             
  LINXY(__NR_epoll_create1,     sys_epoll_create1),        
  LINX_(__NR_epoll_ctl,         sys_epoll_ctl),            
  LINXY(__NR_epoll_pwait,       sys_epoll_pwait),          
  GENXY(__NR_dup,               sys_dup),                  
  GENXY(__NR_dup2,              sys_dup2),                 
  LINXY(__NR_dup3,              sys_dup3),                 
  LINXY(__NR_fcntl,             sys_fcntl),                
  LINXY(__NR_inotify_init1,     sys_inotify_init1),        
  LINX_(__NR_inotify_add_watch, sys_inotify_add_watch),    
  LINX_(__NR_inotify_rm_watch,  sys_inotify_rm_watch),     
  LINXY(__NR_ioctl,             sys_ioctl),                
  LINX_(__NR_ioprio_set,        sys_ioprio_set),           
  LINX_(__NR_ioprio_get,        sys_ioprio_get),           
  GENX_(__NR_flock,             sys_flock),                
  LINX_(__NR_mknodat,           sys_mknodat),              
  LINX_(__NR_mkdirat,           sys_mkdirat),              
  LINX_(__NR_unlinkat,          sys_unlinkat),             
  LINX_(__NR_symlinkat,         sys_symlinkat),            
  LINX_(__NR_linkat,            sys_linkat),               
  LINX_(__NR_renameat,          sys_renameat),             
  LINX_(__NR_umount2,           sys_umount),               
  LINX_(__NR_mount,             sys_mount),                

  GENXY(__NR_statfs,            sys_statfs),               
  GENXY(__NR_fstatfs,           sys_fstatfs),              
  GENX_(__NR_truncate,          sys_truncate),             
  GENX_(__NR_ftruncate,         sys_ftruncate),            
  LINX_(__NR_fallocate,         sys_fallocate),            
  LINX_(__NR_faccessat,         sys_faccessat),            
  GENX_(__NR_chdir,             sys_chdir),                
  GENX_(__NR_fchdir,            sys_fchdir),               
  GENX_(__NR_chroot,            sys_chroot),               
  GENX_(__NR_fchmod,            sys_fchmod),               
  LINX_(__NR_fchmodat,          sys_fchmodat),             
  LINX_(__NR_fchownat,          sys_fchownat),             
  GENX_(__NR_fchown,            sys_fchown),               
  LINXY(__NR_openat,            sys_openat),               
  GENXY(__NR_close,             sys_close),                
  LINX_(__NR_vhangup,           sys_vhangup),              
  LINXY(__NR_pipe2,             sys_pipe2),                
  LINX_(__NR_quotactl,          sys_quotactl),             
  GENXY(__NR_getdents64,        sys_getdents64),           
  LINX_(__NR_lseek,             sys_lseek),                
  GENXY(__NR_read,              sys_read),                 
  GENX_(__NR_write,             sys_write),                
  GENXY(__NR_readv,             sys_readv),                
  GENX_(__NR_writev,            sys_writev),               
  GENXY(__NR_pread64,           sys_pread64),              
  GENX_(__NR_pwrite64,          sys_pwrite64),             
  LINXY(__NR_preadv,            sys_preadv),               
  LINX_(__NR_pwritev,           sys_pwritev),              
  LINXY(__NR_sendfile,          sys_sendfile),             
  LINX_(__NR_pselect6,          sys_pselect6),             
  LINXY(__NR_ppoll,             sys_ppoll),                
  LINXY(__NR_signalfd4,         sys_signalfd4),            
  LINX_(__NR_splice,            sys_splice),               
  LINX_(__NR_readlinkat,        sys_readlinkat),           
  LINXY(__NR3264_fstatat,       sys_newfstatat),           
  GENXY(__NR_fstat,             sys_newfstat),             
  GENX_(__NR_sync,              sys_sync),                 
  GENX_(__NR_fsync,             sys_fsync),                
  GENX_(__NR_fdatasync,         sys_fdatasync),            
  LINX_(__NR_sync_file_range,   sys_sync_file_range),      
  LINXY(__NR_timerfd_create,    sys_timerfd_create),       
  LINXY(__NR_timerfd_settime,   sys_timerfd_settime),      
  LINXY(__NR_timerfd_gettime,   sys_timerfd_gettime),      
  LINX_(__NR_utimensat,         sys_utimensat),            

  LINXY(__NR_capget,            sys_capget),               
  LINX_(__NR_capset,            sys_capset),               
  LINX_(__NR_personality,       sys_personality),          
  GENX_(__NR_exit,              sys_exit),                 
  LINX_(__NR_exit_group,        sys_exit_group),           
  LINXY(__NR_waitid,            sys_waitid),               
  LINX_(__NR_set_tid_address,   sys_set_tid_address),      
  LINXY(__NR_futex,             sys_futex),                
  LINX_(__NR_set_robust_list,   sys_set_robust_list),      
  LINXY(__NR_get_robust_list,   sys_get_robust_list),      
  GENXY(__NR_nanosleep,         sys_nanosleep),            
  GENXY(__NR_getitimer,         sys_getitimer),            
  GENXY(__NR_setitimer,         sys_setitimer),            
  LINX_(__NR_init_module,       sys_init_module),          
  LINX_(__NR_delete_module,     sys_delete_module),        
  LINXY(__NR_timer_create,      sys_timer_create),         
  LINXY(__NR_timer_gettime,     sys_timer_gettime),        
  LINX_(__NR_timer_getoverrun,  sys_timer_getoverrun),     
  LINXY(__NR_timer_settime,     sys_timer_settime),        
  LINX_(__NR_timer_delete,      sys_timer_delete),         
  LINX_(__NR_clock_settime,     sys_clock_settime),        
  LINXY(__NR_clock_gettime,     sys_clock_gettime),        
  LINXY(__NR_clock_getres,      sys_clock_getres),         
  LINXY(__NR_clock_nanosleep,   sys_clock_nanosleep),      
  LINXY(__NR_syslog,            sys_syslog),               
  PLAXY(__NR_ptrace,            sys_ptrace),               
  LINXY(__NR_sched_setparam,          sys_sched_setparam), 
  LINX_(__NR_sched_setscheduler,      sys_sched_setscheduler),     
  LINX_(__NR_sched_getscheduler,      sys_sched_getscheduler),     
  LINXY(__NR_sched_getparam,          sys_sched_getparam), 
  LINX_(__NR_sched_setaffinity, sys_sched_setaffinity),    
  LINXY(__NR_sched_getaffinity, sys_sched_getaffinity),    
  LINX_(__NR_sched_yield,       sys_sched_yield),          
  LINX_(__NR_sched_get_priority_max,  sys_sched_get_priority_max), 
  LINX_(__NR_sched_get_priority_min,  sys_sched_get_priority_min), 
  LINXY(__NR_sched_rr_get_interval,   sys_sched_rr_get_interval),  

  GENX_(__NR_kill,              sys_kill),                 
  LINXY(__NR_tkill,             sys_tkill),                
  LINXY(__NR_tgkill,            sys_tgkill),               
  GENXY(__NR_sigaltstack,       sys_sigaltstack),          
  LINX_(__NR_rt_sigsuspend,     sys_rt_sigsuspend),        
  LINXY(__NR_rt_sigaction,      sys_rt_sigaction),         
  LINXY(__NR_rt_sigprocmask,    sys_rt_sigprocmask),       
  LINXY(__NR_rt_sigpending,     sys_rt_sigpending),        
  LINXY(__NR_rt_sigtimedwait,   sys_rt_sigtimedwait),      
  LINXY(__NR_rt_sigqueueinfo,   sys_rt_sigqueueinfo),      
  PLAX_(__NR_rt_sigreturn,      sys_rt_sigreturn),         
  GENX_(__NR_setpriority,             sys_setpriority),    
  GENX_(__NR_getpriority,             sys_getpriority),    

  GENX_(__NR_setregid,          sys_setregid),             
  GENX_(__NR_setgid,            sys_setgid),               
  GENX_(__NR_setreuid,          sys_setreuid),             
  GENX_(__NR_setuid,            sys_setuid),               
  LINX_(__NR_setresuid,         sys_setresuid),            
  LINXY(__NR_getresuid,         sys_getresuid),            
  LINX_(__NR_setresgid,         sys_setresgid),            
  LINXY(__NR_getresgid,         sys_getresgid),            
  LINX_(__NR_setfsuid,          sys_setfsuid),             
  LINX_(__NR_setfsgid,          sys_setfsgid),             
  GENXY(__NR_times,             sys_times),                
  GENX_(__NR_setpgid,           sys_setpgid),              
  GENX_(__NR_getpgid,           sys_getpgid),              
  GENX_(__NR_getsid,            sys_getsid),               
  GENX_(__NR_setsid,            sys_setsid),               
  GENXY(__NR_getgroups,         sys_getgroups),            
  GENX_(__NR_setgroups,         sys_setgroups),            
  GENXY(__NR_uname,             sys_newuname),             
  GENXY(__NR_getrlimit,         sys_getrlimit),            
  GENX_(__NR_setrlimit,         sys_setrlimit),            
  GENXY(__NR_getrusage,         sys_getrusage),            
  GENX_(__NR_umask,             sys_umask),                
  LINXY(__NR_prctl,             sys_prctl),                

  GENXY(__NR_gettimeofday,      sys_gettimeofday),         
  GENX_(__NR_settimeofday,      sys_settimeofday),         
  LINXY(__NR_adjtimex,          sys_adjtimex),             
  GENX_(__NR_getpid,            sys_getpid),               
  GENX_(__NR_getppid,           sys_getppid),              
  GENX_(__NR_getuid,            sys_getuid),               
  GENX_(__NR_geteuid,           sys_geteuid),              
  GENX_(__NR_getgid,            sys_getgid),               
  GENX_(__NR_getegid,           sys_getegid),              
  LINX_(__NR_gettid,            sys_gettid),               
  LINXY(__NR_sysinfo,           sys_sysinfo),              
  LINXY(__NR_mq_open,           sys_mq_open),              
  LINX_(__NR_mq_unlink,         sys_mq_unlink),            
  LINX_(__NR_mq_timedsend,      sys_mq_timedsend),         
  LINXY(__NR_mq_timedreceive,   sys_mq_timedreceive),      
  LINX_(__NR_mq_notify,         sys_mq_notify),            
  LINXY(__NR_mq_getsetattr,     sys_mq_getsetattr),        
  PLAX_(__NR_msgget,            sys_msgget),               
  PLAXY(__NR_msgctl,            sys_msgctl),               
  PLAXY(__NR_msgrcv,            sys_msgrcv),               
  PLAX_(__NR_msgsnd,            sys_msgsnd),               
  PLAX_(__NR_semget,            sys_semget),               
  PLAXY(__NR_semctl,            sys_semctl),               
  PLAX_(__NR_semtimedop,        sys_semtimedop),           
  PLAX_(__NR_semop,             sys_semop),                
  PLAX_(__NR_shmget,            sys_shmget),               
  PLAXY(__NR_shmat,             wrap_sys_shmat),           
  PLAXY(__NR_shmctl,            sys_shmctl),               
  PLAXY(__NR_shmdt,             sys_shmdt),                
  PLAXY(__NR_socket,            sys_socket),               
  PLAXY(__NR_socketpair,        sys_socketpair),           
  PLAX_(__NR_bind,              sys_bind),                 
  PLAX_(__NR_listen,            sys_listen),               
  PLAXY(__NR_accept,            sys_accept),               
  PLAX_(__NR_connect,           sys_connect),              
  PLAXY(__NR_getsockname,       sys_getsockname),          
  PLAXY(__NR_getpeername,       sys_getpeername),          
  PLAX_(__NR_sendto,            sys_sendto),               
  PLAXY(__NR_recvfrom,          sys_recvfrom),             
  PLAX_(__NR_setsockopt,        sys_setsockopt),           
  PLAXY(__NR_getsockopt,        sys_getsockopt),           
  PLAX_(__NR_shutdown,          sys_shutdown),             
  PLAX_(__NR_sendmsg,           sys_sendmsg),              
  PLAXY(__NR_recvmsg,           sys_recvmsg),              
  LINX_(__NR_readahead,         sys_readahead),            
  GENX_(__NR_brk,               sys_brk),                  
  GENXY(__NR_munmap,            sys_munmap),               
  GENX_(__NR_mremap,            sys_mremap),               
  LINX_(__NR_add_key,           sys_add_key),              
  LINX_(__NR_request_key,       sys_request_key),          
  LINXY(__NR_keyctl,            sys_keyctl),               
  PLAX_(__NR_clone,             sys_clone),                
  GENX_(__NR_execve,            sys_execve),               
  PLAX_(__NR_mmap,              sys_mmap),                 
  GENXY(__NR_mprotect,          sys_mprotect),             
  GENX_(__NR_msync,             sys_msync),                
  GENX_(__NR_mlock,                   sys_mlock),          
  GENX_(__NR_munlock,           sys_munlock),              
  GENX_(__NR_mlockall,          sys_mlockall),             
  LINX_(__NR_munlockall,        sys_munlockall),           
  GENX_(__NR_mincore,           sys_mincore),              
  GENX_(__NR_madvise,           sys_madvise),              

  LINX_(__NR_mbind,             sys_mbind),                
  LINXY(__NR_get_mempolicy,     sys_get_mempolicy),        
  LINX_(__NR_set_mempolicy,     sys_set_mempolicy),        

  LINXY(__NR_rt_tgsigqueueinfo, sys_rt_tgsigqueueinfo),    

  PLAXY(__NR_accept4,           sys_accept4),              

  PLAX_(__NR_cacheflush,        sys_cacheflush),           
  PLAX_(__NR_set_dataplane,     sys_set_dataplane),        

  GENXY(__NR_wait4,             sys_wait4),                
};

SyscallTableEntry* ML_(get_linux_syscall_entry) ( UInt sysno )
{
  const UInt syscall_table_size
    = sizeof(syscall_table) / sizeof(syscall_table[0]);

  
  if (sysno < syscall_table_size) {
    SyscallTableEntry* sys = &syscall_table[sysno];
    if (sys->before == NULL)
      return NULL; 
    else
      return sys;
  }
  

  
  return NULL;
}

#endif 

