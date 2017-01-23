

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2013 Nicholas Nethercote
      njn@valgrind.org

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

#if defined(VGP_x86_linux)


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
#include "priv_syswrap-linux.h"      
#include "priv_syswrap-linux-variants.h" 
#include "priv_syswrap-main.h"



__attribute__((noreturn))
void ML_(call_on_new_stack_0_1) ( Addr stack,
			          Addr retaddr,
			          void (*f)(Word),
                                  Word arg1 );
asm(
".text\n"
".globl vgModuleLocal_call_on_new_stack_0_1\n"
"vgModuleLocal_call_on_new_stack_0_1:\n"
"   movl %esp, %esi\n"     
"   movl 4(%esi), %esp\n"  
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
".previous\n"
);


#define FSZ               "4+4+4+4" 
#define __NR_CLONE        VG_STRINGIFY(__NR_clone)
#define __NR_EXIT         VG_STRINGIFY(__NR_exit)

extern
Int do_syscall_clone_x86_linux ( Word (*fn)(void *), 
                                 void* stack, 
                                 Int   flags, 
                                 void* arg,
                                 Int*  child_tid, 
                                 Int*  parent_tid, 
                                 vki_modify_ldt_t * );
asm(
".text\n"
".globl do_syscall_clone_x86_linux\n"
"do_syscall_clone_x86_linux:\n"
"        push    %ebx\n"
"        push    %edi\n"
"        push    %esi\n"

         
"        movl     4+"FSZ"(%esp), %ecx\n"    
"        movl    12+"FSZ"(%esp), %ebx\n"    
"        movl     0+"FSZ"(%esp), %eax\n"    
"        lea     -8(%ecx), %ecx\n"          
"        movl    %ebx, 4(%ecx)\n"           
"        movl    %eax, 0(%ecx)\n"           

         
"        movl     8+"FSZ"(%esp), %ebx\n"    
"        movl    20+"FSZ"(%esp), %edx\n"    
"        movl    16+"FSZ"(%esp), %edi\n"    
"        movl    24+"FSZ"(%esp), %esi\n"    
"        movl    $"__NR_CLONE", %eax\n"
"        int     $0x80\n"                   
"        testl   %eax, %eax\n"              
"        jnz     1f\n"

         
"        popl    %eax\n"
"        call    *%eax\n"                   

         
"        movl    %eax, %ebx\n"              
"        movl    $"__NR_EXIT", %eax\n"
"        int     $0x80\n"

         
"        ud2\n"

"1:\n"   
"        pop     %esi\n"
"        pop     %edi\n"
"        pop     %ebx\n"
"        ret\n"
".previous\n"
);

#undef FSZ
#undef __NR_CLONE
#undef __NR_EXIT


static void setup_child ( ThreadArchState*, ThreadArchState*, Bool );
static SysRes sys_set_thread_area ( ThreadId, vki_modify_ldt_t* );

static SysRes do_clone ( ThreadId ptid, 
                         UInt flags, Addr esp, 
                         Int* parent_tidptr, 
                         Int* child_tidptr, 
                         vki_modify_ldt_t *tlsinfo)
{
   static const Bool debug = False;

   ThreadId     ctid = VG_(alloc_ThreadState)();
   ThreadState* ptst = VG_(get_ThreadState)(ptid);
   ThreadState* ctst = VG_(get_ThreadState)(ctid);
   UWord*       stack;
   SysRes       res;
   Int          eax;
   vki_sigset_t blockall, savedmask;

   VG_(sigfillset)(&blockall);

   vg_assert(VG_(is_running_thread)(ptid));
   vg_assert(VG_(is_valid_tid)(ctid));

   stack = (UWord*)ML_(allocstack)(ctid);
   if (stack == NULL) {
      res = VG_(mk_SysRes_Error)( VKI_ENOMEM );
      goto out;
   }

   setup_child( &ctst->arch, &ptst->arch, True );

   ctst->arch.vex.guest_EAX = 0;

   if (esp != 0)
      ctst->arch.vex.guest_ESP = esp;

   ctst->os_state.parent = ptid;

   
   ctst->sig_mask     = ptst->sig_mask;
   ctst->tmp_sig_mask = ptst->sig_mask;

   ctst->os_state.threadgroup = ptst->os_state.threadgroup;

   ML_(guess_and_register_stack) (esp, ctst);
   
   vg_assert(VG_(owns_BigLock_LL)(ptid));
   VG_TRACK ( pre_thread_ll_create, ptid, ctid );

   if (flags & VKI_CLONE_SETTLS) {
      if (debug)
	 VG_(printf)("clone child has SETTLS: tls info at %p: idx=%d "
                     "base=%#lx limit=%x; esp=%#x fs=%x gs=%x\n",
		     tlsinfo, tlsinfo->entry_number, 
                     tlsinfo->base_addr, tlsinfo->limit,
		     ptst->arch.vex.guest_ESP,
		     ctst->arch.vex.guest_FS, ctst->arch.vex.guest_GS);
      res = sys_set_thread_area(ctid, tlsinfo);
      if (sr_isError(res))
	 goto out;
   }

   flags &= ~VKI_CLONE_SETTLS;

   
   VG_(sigprocmask)(VKI_SIG_SETMASK, &blockall, &savedmask);

   
   eax = do_syscall_clone_x86_linux(
            ML_(start_thread_NORETURN), stack, flags, &VG_(threads)[ctid],
            child_tidptr, parent_tidptr, NULL
         );
   res = VG_(mk_SysRes_x86_linux)( eax );

   VG_(sigprocmask)(VKI_SIG_SETMASK, &savedmask, NULL);

  out:
   if (sr_isError(res)) {
      
      VG_(cleanup_thread)(&ctst->arch);
      ctst->status = VgTs_Empty;
      
      VG_TRACK( pre_thread_ll_exit, ctid );
   }

   return res;
}





static
void translate_to_hw_format (  vki_modify_ldt_t* inn,
                               VexGuestX86SegDescr* out,
                                        Int oldmode )
{
   UInt entry_1, entry_2;
   vg_assert(8 == sizeof(VexGuestX86SegDescr));

   if (0)
      VG_(printf)("translate_to_hw_format: base %#lx, limit %d\n",
                  inn->base_addr, inn->limit );

   
   if (inn->base_addr == 0 && inn->limit == 0) {
      if (oldmode ||
          (inn->contents == 0      &&
           inn->read_exec_only == 1   &&
           inn->seg_32bit == 0      &&
           inn->limit_in_pages == 0   &&
           inn->seg_not_present == 1   &&
           inn->useable == 0 )) {
         entry_1 = 0;
         entry_2 = 0;
         goto install;
      }
   }

   entry_1 = ((inn->base_addr & 0x0000ffff) << 16) |
             (inn->limit & 0x0ffff);
   entry_2 = (inn->base_addr & 0xff000000) |
             ((inn->base_addr & 0x00ff0000) >> 16) |
             (inn->limit & 0xf0000) |
             ((inn->read_exec_only ^ 1) << 9) |
             (inn->contents << 10) |
             ((inn->seg_not_present ^ 1) << 15) |
             (inn->seg_32bit << 22) |
             (inn->limit_in_pages << 23) |
             0x7000;
   if (!oldmode)
      entry_2 |= (inn->useable << 20);

   
  install:
   out->LdtEnt.Words.word1 = entry_1;
   out->LdtEnt.Words.word2 = entry_2;
}

static VexGuestX86SegDescr* alloc_zeroed_x86_GDT ( void )
{
   Int nbytes = VEX_GUEST_X86_GDT_NENT * sizeof(VexGuestX86SegDescr);
   return VG_(calloc)("di.syswrap-x86.azxG.1", nbytes, 1);
}

static VexGuestX86SegDescr* alloc_zeroed_x86_LDT ( void )
{
   Int nbytes = VEX_GUEST_X86_LDT_NENT * sizeof(VexGuestX86SegDescr);
   return VG_(calloc)("di.syswrap-x86.azxL.1", nbytes, 1);
}

static void free_LDT_or_GDT ( VexGuestX86SegDescr* dt )
{
   vg_assert(dt);
   VG_(free)(dt);
}

static void copy_LDT_from_to ( VexGuestX86SegDescr* src,
                               VexGuestX86SegDescr* dst )
{
   Int i;
   vg_assert(src);
   vg_assert(dst);
   for (i = 0; i < VEX_GUEST_X86_LDT_NENT; i++)
      dst[i] = src[i];
}

static void copy_GDT_from_to ( VexGuestX86SegDescr* src,
                               VexGuestX86SegDescr* dst )
{
   Int i;
   vg_assert(src);
   vg_assert(dst);
   for (i = 0; i < VEX_GUEST_X86_GDT_NENT; i++)
      dst[i] = src[i];
}

static void deallocate_LGDTs_for_thread ( VexGuestX86State* vex )
{
   vg_assert(sizeof(HWord) == sizeof(void*));

   if (0)
      VG_(printf)("deallocate_LGDTs_for_thread: "
                  "ldt = 0x%lx, gdt = 0x%lx\n",
                  vex->guest_LDT, vex->guest_GDT );

   if (vex->guest_LDT != (HWord)NULL) {
      free_LDT_or_GDT( (VexGuestX86SegDescr*)vex->guest_LDT );
      vex->guest_LDT = (HWord)NULL;
   }

   if (vex->guest_GDT != (HWord)NULL) {
      free_LDT_or_GDT( (VexGuestX86SegDescr*)vex->guest_GDT );
      vex->guest_GDT = (HWord)NULL;
   }
}


/*
 * linux/kernel/ldt.c
 *
 * Copyright (C) 1992 Krishna Balasubramanian and Linus Torvalds
 * Copyright (C) 1999 Ingo Molnar <mingo@redhat.com>
 */

static
SysRes read_ldt ( ThreadId tid, UChar* ptr, UInt bytecount )
{
   SysRes res;
   UInt   i, size;
   UChar* ldt;

   if (0)
      VG_(printf)("read_ldt: tid = %d, ptr = %p, bytecount = %d\n",
                  tid, ptr, bytecount );

   vg_assert(sizeof(HWord) == sizeof(VexGuestX86SegDescr*));
   vg_assert(8 == sizeof(VexGuestX86SegDescr));

   ldt = (UChar*)(VG_(threads)[tid].arch.vex.guest_LDT);
   res = VG_(mk_SysRes_Success)( 0 );
   if (ldt == NULL)
      
      goto out;

   size = VEX_GUEST_X86_LDT_NENT * sizeof(VexGuestX86SegDescr);
   if (size > bytecount)
      size = bytecount;

   res = VG_(mk_SysRes_Success)( size );
   for (i = 0; i < size; i++)
      ptr[i] = ldt[i];

  out:
   return res;
}


static
SysRes write_ldt ( ThreadId tid, void* ptr, UInt bytecount, Int oldmode )
{
   SysRes res;
   VexGuestX86SegDescr* ldt;
   vki_modify_ldt_t* ldt_info; 

   if (0)
      VG_(printf)("write_ldt: tid = %d, ptr = %p, "
                  "bytecount = %d, oldmode = %d\n",
                  tid, ptr, bytecount, oldmode );

   vg_assert(8 == sizeof(VexGuestX86SegDescr));
   vg_assert(sizeof(HWord) == sizeof(VexGuestX86SegDescr*));

   ldt      = (VexGuestX86SegDescr*)VG_(threads)[tid].arch.vex.guest_LDT;
   ldt_info = (vki_modify_ldt_t*)ptr;

   res = VG_(mk_SysRes_Error)( VKI_EINVAL );
   if (bytecount != sizeof(vki_modify_ldt_t))
      goto out;

   res = VG_(mk_SysRes_Error)( VKI_EINVAL );
   if (ldt_info->entry_number >= VEX_GUEST_X86_LDT_NENT)
      goto out;
   if (ldt_info->contents == 3) {
      if (oldmode)
         goto out;
      if (ldt_info->seg_not_present == 0)
         goto out;
   }

   if (ldt == NULL) {
      ldt = alloc_zeroed_x86_LDT();
      VG_(threads)[tid].arch.vex.guest_LDT = (HWord)ldt;
   }

   
   translate_to_hw_format ( ldt_info, &ldt[ldt_info->entry_number], oldmode );
   res = VG_(mk_SysRes_Success)( 0 );

  out:
   return res;
}


static SysRes sys_modify_ldt ( ThreadId tid,
                               Int func, void* ptr, UInt bytecount )
{
   SysRes ret = VG_(mk_SysRes_Error)( VKI_ENOSYS );

   switch (func) {
   case 0:
      ret = read_ldt(tid, ptr, bytecount);
      break;
   case 1:
      ret = write_ldt(tid, ptr, bytecount, 1);
      break;
   case 2:
      VG_(unimplemented)("sys_modify_ldt: func == 2");
      
      
      
      break;
   case 0x11:
      ret = write_ldt(tid, ptr, bytecount, 0);
      break;
   }
   return ret;
}


static SysRes sys_set_thread_area ( ThreadId tid, vki_modify_ldt_t* info )
{
   Int                  idx;
   VexGuestX86SegDescr* gdt;

   vg_assert(8 == sizeof(VexGuestX86SegDescr));
   vg_assert(sizeof(HWord) == sizeof(VexGuestX86SegDescr*));

   if (info == NULL)
      return VG_(mk_SysRes_Error)( VKI_EFAULT );

   gdt = (VexGuestX86SegDescr*)VG_(threads)[tid].arch.vex.guest_GDT;

   
   if (!gdt) {
      gdt = alloc_zeroed_x86_GDT();
      VG_(threads)[tid].arch.vex.guest_GDT = (HWord)gdt;
   }

   idx = info->entry_number;

   if (idx == -1) {
      for (idx = 1; idx < VEX_GUEST_X86_GDT_NENT; idx++) {
         if (gdt[idx].LdtEnt.Words.word1 == 0 
             && gdt[idx].LdtEnt.Words.word2 == 0)
            break;
      }

      if (idx == VEX_GUEST_X86_GDT_NENT)
         return VG_(mk_SysRes_Error)( VKI_ESRCH );
   } else if (idx < 0 || idx == 0 || idx >= VEX_GUEST_X86_GDT_NENT) {
      
      return VG_(mk_SysRes_Error)( VKI_EINVAL );
   }

   translate_to_hw_format(info, &gdt[idx], 0);

   VG_TRACK( pre_mem_write, Vg_CoreSysCall, tid,
             "set_thread_area(info->entry)",
             (Addr) & info->entry_number, sizeof(unsigned int) );
   info->entry_number = idx;
   VG_TRACK( post_mem_write, Vg_CoreSysCall, tid,
             (Addr) & info->entry_number, sizeof(unsigned int) );

   return VG_(mk_SysRes_Success)( 0 );
}


static SysRes sys_get_thread_area ( ThreadId tid, vki_modify_ldt_t* info )
{
   Int idx;
   VexGuestX86SegDescr* gdt;

   vg_assert(sizeof(HWord) == sizeof(VexGuestX86SegDescr*));
   vg_assert(8 == sizeof(VexGuestX86SegDescr));

   if (info == NULL)
      return VG_(mk_SysRes_Error)( VKI_EFAULT );

   idx = info->entry_number;

   if (idx < 0 || idx >= VEX_GUEST_X86_GDT_NENT)
      return VG_(mk_SysRes_Error)( VKI_EINVAL );

   gdt = (VexGuestX86SegDescr*)VG_(threads)[tid].arch.vex.guest_GDT;

   
   if (!gdt) {
      gdt = alloc_zeroed_x86_GDT();
      VG_(threads)[tid].arch.vex.guest_GDT = (HWord)gdt;
   }

   info->base_addr = ( gdt[idx].LdtEnt.Bits.BaseHi << 24 ) |
                     ( gdt[idx].LdtEnt.Bits.BaseMid << 16 ) |
                     gdt[idx].LdtEnt.Bits.BaseLow;
   info->limit = ( gdt[idx].LdtEnt.Bits.LimitHi << 16 ) |
                   gdt[idx].LdtEnt.Bits.LimitLow;
   info->seg_32bit = gdt[idx].LdtEnt.Bits.Default_Big;
   info->contents = ( gdt[idx].LdtEnt.Bits.Type >> 2 ) & 0x3;
   info->read_exec_only = ( gdt[idx].LdtEnt.Bits.Type & 0x1 ) ^ 0x1;
   info->limit_in_pages = gdt[idx].LdtEnt.Bits.Granularity;
   info->seg_not_present = gdt[idx].LdtEnt.Bits.Pres ^ 0x1;
   info->useable = gdt[idx].LdtEnt.Bits.Sys;
   info->reserved = 0;

   return VG_(mk_SysRes_Success)( 0 );
}


void VG_(cleanup_thread) ( ThreadArchState* arch )
{
   
   
   deallocate_LGDTs_for_thread( &arch->vex );
}  


static void setup_child (  ThreadArchState *child, 
                            ThreadArchState *parent,
                          Bool inherit_parents_GDT )
{
   
   child->vex = parent->vex;
   child->vex_shadow1 = parent->vex_shadow1;
   child->vex_shadow2 = parent->vex_shadow2;

   
   if (parent->vex.guest_LDT == (HWord)NULL) {
      
      child->vex.guest_LDT = (HWord)NULL;
   } else {
      
      child->vex.guest_LDT = (HWord)alloc_zeroed_x86_LDT();
      copy_LDT_from_to( (VexGuestX86SegDescr*)parent->vex.guest_LDT, 
                        (VexGuestX86SegDescr*)child->vex.guest_LDT );
   }

   child->vex.guest_GDT = (HWord)NULL;

   if (inherit_parents_GDT && parent->vex.guest_GDT != (HWord)NULL) {
      child->vex.guest_GDT = (HWord)alloc_zeroed_x86_GDT();
      copy_GDT_from_to( (VexGuestX86SegDescr*)parent->vex.guest_GDT,
                        (VexGuestX86SegDescr*)child->vex.guest_GDT );
   }
}  



#define PRE(name)       DEFN_PRE_TEMPLATE(x86_linux, name)
#define POST(name)      DEFN_POST_TEMPLATE(x86_linux, name)

DECL_TEMPLATE(x86_linux, sys_stat64);
DECL_TEMPLATE(x86_linux, sys_fstatat64);
DECL_TEMPLATE(x86_linux, sys_fstat64);
DECL_TEMPLATE(x86_linux, sys_lstat64);
DECL_TEMPLATE(x86_linux, sys_clone);
DECL_TEMPLATE(x86_linux, old_mmap);
DECL_TEMPLATE(x86_linux, sys_mmap2);
DECL_TEMPLATE(x86_linux, sys_sigreturn);
DECL_TEMPLATE(x86_linux, sys_rt_sigreturn);
DECL_TEMPLATE(x86_linux, sys_modify_ldt);
DECL_TEMPLATE(x86_linux, sys_set_thread_area);
DECL_TEMPLATE(x86_linux, sys_get_thread_area);
DECL_TEMPLATE(x86_linux, sys_ptrace);
DECL_TEMPLATE(x86_linux, sys_sigsuspend);
DECL_TEMPLATE(x86_linux, old_select);
DECL_TEMPLATE(x86_linux, sys_vm86old);
DECL_TEMPLATE(x86_linux, sys_vm86);
DECL_TEMPLATE(x86_linux, sys_syscall223);

PRE(old_select)
{
   PRE_REG_READ1(long, "old_select", struct sel_arg_struct *, args);
   PRE_MEM_READ( "old_select(args)", ARG1, 5*sizeof(UWord) );
   *flags |= SfMayBlock;
   {
      UInt* arg_struct = (UInt*)ARG1;
      UInt a1, a2, a3, a4, a5;

      a1 = arg_struct[0];
      a2 = arg_struct[1];
      a3 = arg_struct[2];
      a4 = arg_struct[3];
      a5 = arg_struct[4];

      PRINT("old_select ( %d, %#x, %#x, %#x, %#x )", a1,a2,a3,a4,a5);
      if (a2 != (Addr)NULL)
         PRE_MEM_READ( "old_select(readfds)",   a2, a1/8  );
      if (a3 != (Addr)NULL)
         PRE_MEM_READ( "old_select(writefds)",  a3, a1/8  );
      if (a4 != (Addr)NULL)
         PRE_MEM_READ( "old_select(exceptfds)", a4, a1/8  );
      if (a5 != (Addr)NULL)
         PRE_MEM_READ( "old_select(timeout)", a5, sizeof(struct vki_timeval) );
   }
}

PRE(sys_clone)
{
   UInt cloneflags;
   Bool badarg = False;

   PRINT("sys_clone ( %lx, %#lx, %#lx, %#lx, %#lx )",ARG1,ARG2,ARG3,ARG4,ARG5);
   PRE_REG_READ2(int, "clone",
                 unsigned long, flags,
                 void *, child_stack);

   if (ARG1 & VKI_CLONE_PARENT_SETTID) {
      if (VG_(tdict).track_pre_reg_read) {
         PRA3("clone", int *, parent_tidptr);
      }
      PRE_MEM_WRITE("clone(parent_tidptr)", ARG3, sizeof(Int));
      if (!VG_(am_is_valid_for_client)(ARG3, sizeof(Int), 
                                             VKI_PROT_WRITE)) {
         badarg = True;
      }
   }
   if (ARG1 & VKI_CLONE_SETTLS) {
      if (VG_(tdict).track_pre_reg_read) {
         PRA4("clone", vki_modify_ldt_t *, tlsinfo);
      }
      PRE_MEM_READ("clone(tlsinfo)", ARG4, sizeof(vki_modify_ldt_t));
      if (!VG_(am_is_valid_for_client)(ARG4, sizeof(vki_modify_ldt_t), 
                                             VKI_PROT_READ)) {
         badarg = True;
      }
   }
   if (ARG1 & (VKI_CLONE_CHILD_SETTID | VKI_CLONE_CHILD_CLEARTID)) {
      if (VG_(tdict).track_pre_reg_read) {
         PRA5("clone", int *, child_tidptr);
      }
      PRE_MEM_WRITE("clone(child_tidptr)", ARG5, sizeof(Int));
      if (!VG_(am_is_valid_for_client)(ARG5, sizeof(Int), 
                                             VKI_PROT_WRITE)) {
         badarg = True;
      }
   }

   if (badarg) {
      SET_STATUS_Failure( VKI_EFAULT );
      return;
   }

   cloneflags = ARG1;

   if (!ML_(client_signal_OK)(ARG1 & VKI_CSIGNAL)) {
      SET_STATUS_Failure( VKI_EINVAL );
      return;
   }

   if (
        1 ||
          (cloneflags == 0x100011 || cloneflags == 0x1200011
                                  || cloneflags == 0x7D0F00
                                  || cloneflags == 0x790F00
                                  || cloneflags == 0x3D0F00
                                  || cloneflags == 0x410F00
                                  || cloneflags == 0xF00
                                  || cloneflags == 0xF21)) {
     
   }
   else {
      
      goto reject;
   }

   
   switch (cloneflags & (VKI_CLONE_VM | VKI_CLONE_FS 
                         | VKI_CLONE_FILES | VKI_CLONE_VFORK)) {
   case VKI_CLONE_VM | VKI_CLONE_FS | VKI_CLONE_FILES:
      
      SET_STATUS_from_SysRes(
         do_clone(tid,
                  ARG1,         
                  (Addr)ARG2,   
                  (Int *)ARG3,  
                  (Int *)ARG5,  
                  (vki_modify_ldt_t *)ARG4)); 
      break;

   case VKI_CLONE_VFORK | VKI_CLONE_VM: 
      
      cloneflags &= ~(VKI_CLONE_VFORK | VKI_CLONE_VM);

   case 0: 
      SET_STATUS_from_SysRes(
         ML_(do_fork_clone)(tid,
                       cloneflags,      
                       (Int *)ARG3,     
                       (Int *)ARG5));   
      break;

   default:
   reject:
      
      VG_(message)(Vg_UserMsg, "\n");
      VG_(message)(Vg_UserMsg, "Unsupported clone() flags: 0x%lx\n", ARG1);
      VG_(message)(Vg_UserMsg, "\n");
      VG_(message)(Vg_UserMsg, "The only supported clone() uses are:\n");
      VG_(message)(Vg_UserMsg, " - via a threads library (LinuxThreads or NPTL)\n");
      VG_(message)(Vg_UserMsg, " - via the implementation of fork or vfork\n");
      VG_(message)(Vg_UserMsg, " - for the Quadrics Elan3 user-space driver\n");
      VG_(unimplemented)
         ("Valgrind does not support general clone().");
   }

   if (SUCCESS) {
      if (ARG1 & VKI_CLONE_PARENT_SETTID)
         POST_MEM_WRITE(ARG3, sizeof(Int));
      if (ARG1 & (VKI_CLONE_CHILD_SETTID | VKI_CLONE_CHILD_CLEARTID))
         POST_MEM_WRITE(ARG5, sizeof(Int));

      *flags |= SfYieldAfter;
   }
}

PRE(sys_sigreturn)
{

   ThreadState* tst;
   PRINT("sys_sigreturn ( )");

   vg_assert(VG_(is_valid_tid)(tid));
   vg_assert(tid >= 1 && tid < VG_N_THREADS);
   vg_assert(VG_(is_running_thread)(tid));

   tst = VG_(get_ThreadState)(tid);
   tst->arch.vex.guest_ESP -= sizeof(Addr)+sizeof(Word);
   

   ML_(fixup_guest_state_to_restart_syscall)(&tst->arch);

   
   VG_(sigframe_destroy)(tid, False);

   *flags |= SfNoWriteResult;
   SET_STATUS_Success(0);

   
   *flags |= SfPollAfter;
}

PRE(sys_rt_sigreturn)
{

   ThreadState* tst;
   PRINT("sys_rt_sigreturn ( )");

   vg_assert(VG_(is_valid_tid)(tid));
   vg_assert(tid >= 1 && tid < VG_N_THREADS);
   vg_assert(VG_(is_running_thread)(tid));

   tst = VG_(get_ThreadState)(tid);
   tst->arch.vex.guest_ESP -= sizeof(Addr);
   

   ML_(fixup_guest_state_to_restart_syscall)(&tst->arch);

   
   VG_(sigframe_destroy)(tid, True);

   *flags |= SfNoWriteResult;
   SET_STATUS_Success(0);

   
   *flags |= SfPollAfter;
}

PRE(sys_modify_ldt)
{
   PRINT("sys_modify_ldt ( %ld, %#lx, %ld )", ARG1,ARG2,ARG3);
   PRE_REG_READ3(int, "modify_ldt", int, func, void *, ptr,
                 unsigned long, bytecount);
   
   if (ARG1 == 0) {
      
      PRE_MEM_WRITE( "modify_ldt(ptr)", ARG2, ARG3 );
   }
   if (ARG1 == 1 || ARG1 == 0x11) {
      
      PRE_MEM_READ( "modify_ldt(ptr)", ARG2, sizeof(vki_modify_ldt_t) );
   }
   
   SET_STATUS_from_SysRes( sys_modify_ldt( tid, ARG1, (void*)ARG2, ARG3 ) );

   if (ARG1 == 0 && SUCCESS && RES > 0) {
      POST_MEM_WRITE( ARG2, RES );
   }
}

PRE(sys_set_thread_area)
{
   PRINT("sys_set_thread_area ( %#lx )", ARG1);
   PRE_REG_READ1(int, "set_thread_area", struct user_desc *, u_info)
   PRE_MEM_READ( "set_thread_area(u_info)", ARG1, sizeof(vki_modify_ldt_t) );

   
   SET_STATUS_from_SysRes( sys_set_thread_area( tid, (void *)ARG1 ) );
}

PRE(sys_get_thread_area)
{
   PRINT("sys_get_thread_area ( %#lx )", ARG1);
   PRE_REG_READ1(int, "get_thread_area", struct user_desc *, u_info)
   PRE_MEM_WRITE( "get_thread_area(u_info)", ARG1, sizeof(vki_modify_ldt_t) );

   
   SET_STATUS_from_SysRes( sys_get_thread_area( tid, (void *)ARG1 ) );

   if (SUCCESS) {
      POST_MEM_WRITE( ARG1, sizeof(vki_modify_ldt_t) );
   }
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
   case VKI_PTRACE_GETFPREGS:
      PRE_MEM_WRITE( "ptrace(getfpregs)", ARG4, 
		     sizeof (struct vki_user_i387_struct));
      break;
   case VKI_PTRACE_GETFPXREGS:
      PRE_MEM_WRITE( "ptrace(getfpxregs)", ARG4, 
                     sizeof(struct vki_user_fxsr_struct) );
      break;
   case VKI_PTRACE_GET_THREAD_AREA:
      PRE_MEM_WRITE( "ptrace(get_thread_area)", ARG4, 
                     sizeof(struct vki_user_desc) );
      break;
   case VKI_PTRACE_SETREGS:
      PRE_MEM_READ( "ptrace(setregs)", ARG4, 
		     sizeof (struct vki_user_regs_struct));
      break;
   case VKI_PTRACE_SETFPREGS:
      PRE_MEM_READ( "ptrace(setfpregs)", ARG4, 
		     sizeof (struct vki_user_i387_struct));
      break;
   case VKI_PTRACE_SETFPXREGS:
      PRE_MEM_READ( "ptrace(setfpxregs)", ARG4, 
                     sizeof(struct vki_user_fxsr_struct) );
      break;
   case VKI_PTRACE_SET_THREAD_AREA:
      PRE_MEM_READ( "ptrace(set_thread_area)", ARG4, 
                     sizeof(struct vki_user_desc) );
      break;
   case VKI_PTRACE_GETEVENTMSG:
      PRE_MEM_WRITE( "ptrace(geteventmsg)", ARG4, sizeof(unsigned long));
      break;
   case VKI_PTRACE_GETSIGINFO:
      PRE_MEM_WRITE( "ptrace(getsiginfo)", ARG4, sizeof(vki_siginfo_t));
      break;
   case VKI_PTRACE_SETSIGINFO:
      PRE_MEM_READ( "ptrace(setsiginfo)", ARG4, sizeof(vki_siginfo_t));
      break;
   case VKI_PTRACE_GETREGSET:
      ML_(linux_PRE_getregset)(tid, ARG3, ARG4);
      break;
   case VKI_PTRACE_SETREGSET:
      ML_(linux_PRE_setregset)(tid, ARG3, ARG4);
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
   case VKI_PTRACE_GETFPREGS:
      POST_MEM_WRITE( ARG4, sizeof (struct vki_user_i387_struct));
      break;
   case VKI_PTRACE_GETFPXREGS:
      POST_MEM_WRITE( ARG4, sizeof(struct vki_user_fxsr_struct) );
      break;
   case VKI_PTRACE_GET_THREAD_AREA:
      POST_MEM_WRITE( ARG4, sizeof(struct vki_user_desc) );
      break;
   case VKI_PTRACE_GETEVENTMSG:
      POST_MEM_WRITE( ARG4, sizeof(unsigned long));
      break;
   case VKI_PTRACE_GETSIGINFO:
      POST_MEM_WRITE( ARG4, sizeof(vki_siginfo_t));
      break;
   case VKI_PTRACE_GETREGSET:
      ML_(linux_POST_getregset)(tid, ARG3, ARG4);
      break;
   default:
      break;
   }
}

PRE(old_mmap)
{
   UWord a1, a2, a3, a4, a5, a6;
   SysRes r;

   UWord* args = (UWord*)ARG1;
   PRE_REG_READ1(long, "old_mmap", struct mmap_arg_struct *, args);
   PRE_MEM_READ( "old_mmap(args)", (Addr)args, 6*sizeof(UWord) );

   a1 = args[1-1];
   a2 = args[2-1];
   a3 = args[3-1];
   a4 = args[4-1];
   a5 = args[5-1];
   a6 = args[6-1];

   PRINT("old_mmap ( %#lx, %llu, %ld, %ld, %ld, %ld )",
         a1, (ULong)a2, a3, a4, a5, a6 );

   r = ML_(generic_PRE_sys_mmap)( tid, a1, a2, a3, a4, a5, (Off64T)a6 );
   SET_STATUS_from_SysRes(r);
}

PRE(sys_mmap2)
{
   SysRes r;

   
   
   
   
   
   
   vg_assert(VKI_PAGE_SIZE == 4096);
   PRINT("sys_mmap2 ( %#lx, %llu, %ld, %ld, %ld, %ld )",
         ARG1, (ULong)ARG2, ARG3, ARG4, ARG5, ARG6 );
   PRE_REG_READ6(long, "mmap2",
                 unsigned long, start, unsigned long, length,
                 unsigned long, prot,  unsigned long, flags,
                 unsigned long, fd,    unsigned long, offset);

   r = ML_(generic_PRE_sys_mmap)( tid, ARG1, ARG2, ARG3, ARG4, ARG5, 
                                       4096 * (Off64T)ARG6 );
   SET_STATUS_from_SysRes(r);
}

PRE(sys_lstat64)
{
   PRINT("sys_lstat64 ( %#lx(%s), %#lx )",ARG1,(char*)ARG1,ARG2);
   PRE_REG_READ2(long, "lstat64", char *, file_name, struct stat64 *, buf);
   PRE_MEM_RASCIIZ( "lstat64(file_name)", ARG1 );
   PRE_MEM_WRITE( "lstat64(buf)", ARG2, sizeof(struct vki_stat64) );
}

POST(sys_lstat64)
{
   vg_assert(SUCCESS);
   if (RES == 0) {
      POST_MEM_WRITE( ARG2, sizeof(struct vki_stat64) );
   }
}

PRE(sys_stat64)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   PRINT("sys_stat64 ( %#lx(%s), %#lx )",ARG1,(char*)ARG1,ARG2);
   PRE_REG_READ2(long, "stat64", char *, file_name, struct stat64 *, buf);
   PRE_MEM_RASCIIZ( "stat64(file_name)", ARG1 );
   PRE_MEM_WRITE( "stat64(buf)", ARG2, sizeof(struct vki_stat64) );
}

POST(sys_stat64)
{
   POST_MEM_WRITE( ARG2, sizeof(struct vki_stat64) );
}

PRE(sys_fstatat64)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   PRINT("sys_fstatat64 ( %ld, %#lx(%s), %#lx )",ARG1,ARG2,(char*)ARG2,ARG3);
   PRE_REG_READ3(long, "fstatat64",
                 int, dfd, char *, file_name, struct stat64 *, buf);
   PRE_MEM_RASCIIZ( "fstatat64(file_name)", ARG2 );
   PRE_MEM_WRITE( "fstatat64(buf)", ARG3, sizeof(struct vki_stat64) );
}

POST(sys_fstatat64)
{
   POST_MEM_WRITE( ARG3, sizeof(struct vki_stat64) );
}

PRE(sys_fstat64)
{
   PRINT("sys_fstat64 ( %ld, %#lx )",ARG1,ARG2);
   PRE_REG_READ2(long, "fstat64", unsigned long, fd, struct stat64 *, buf);
   PRE_MEM_WRITE( "fstat64(buf)", ARG2, sizeof(struct vki_stat64) );
}

POST(sys_fstat64)
{
   POST_MEM_WRITE( ARG2, sizeof(struct vki_stat64) );
}

PRE(sys_sigsuspend)
{
   *flags |= SfMayBlock;
   PRINT("sys_sigsuspend ( %ld, %ld, %ld )", ARG1,ARG2,ARG3 );
   PRE_REG_READ3(int, "sigsuspend",
                 int, history0, int, history1,
                 vki_old_sigset_t, mask);
}

PRE(sys_vm86old)
{
   PRINT("sys_vm86old ( %#lx )", ARG1);
   PRE_REG_READ1(int, "vm86old", struct vm86_struct *, info);
   PRE_MEM_WRITE( "vm86old(info)", ARG1, sizeof(struct vki_vm86_struct));
}

POST(sys_vm86old)
{
   POST_MEM_WRITE( ARG1, sizeof(struct vki_vm86_struct));
}

PRE(sys_vm86)
{
   PRINT("sys_vm86 ( %ld, %#lx )", ARG1,ARG2);
   PRE_REG_READ2(int, "vm86", unsigned long, fn, struct vm86plus_struct *, v86);
   if (ARG1 == VKI_VM86_ENTER || ARG1 == VKI_VM86_ENTER_NO_BYPASS)
      PRE_MEM_WRITE( "vm86(v86)", ARG2, sizeof(struct vki_vm86plus_struct));
}

POST(sys_vm86)
{
   if (ARG1 == VKI_VM86_ENTER || ARG1 == VKI_VM86_ENTER_NO_BYPASS)
      POST_MEM_WRITE( ARG2, sizeof(struct vki_vm86plus_struct));
}



PRE(sys_syscall223)
{
   Int err;


   if (!KernelVariantiS(KernelVariant_bproc, VG_(clo_kernel_variant))) {
      PRINT("non-existent syscall! (syscall 223)");
      PRE_REG_READ0(long, "ni_syscall(223)");
      SET_STATUS_Failure( VKI_ENOSYS );
      return;
   }

   err = ML_(linux_variant_PRE_sys_bproc)( ARG1, ARG2, ARG3, 
                                           ARG4, ARG5, ARG6 );
   if (err) {
      SET_STATUS_Failure( err );
      return;
   }
   
   *flags |= SfMayBlock; 
}

POST(sys_syscall223)
{
   ML_(linux_variant_POST_sys_bproc)( ARG1, ARG2, ARG3, 
                                      ARG4, ARG5, ARG6 );
}

#undef PRE
#undef POST



#define PLAX_(sysno, name)    WRAPPER_ENTRY_X_(x86_linux, sysno, name) 
#define PLAXY(sysno, name)    WRAPPER_ENTRY_XY(x86_linux, sysno, name)



static SyscallTableEntry syscall_table[] = {
   GENX_(__NR_exit,              sys_exit),           
   GENX_(__NR_fork,              sys_fork),           
   GENXY(__NR_read,              sys_read),           
   GENX_(__NR_write,             sys_write),          

   GENXY(__NR_open,              sys_open),           
   GENXY(__NR_close,             sys_close),          
   GENXY(__NR_waitpid,           sys_waitpid),        
   GENXY(__NR_creat,             sys_creat),          
   GENX_(__NR_link,              sys_link),           

   GENX_(__NR_unlink,            sys_unlink),         
   GENX_(__NR_execve,            sys_execve),         
   GENX_(__NR_chdir,             sys_chdir),          
   GENXY(__NR_time,              sys_time),           
   GENX_(__NR_mknod,             sys_mknod),          

   GENX_(__NR_chmod,             sys_chmod),          
   GENX_(__NR_break,             sys_ni_syscall),     
   LINX_(__NR_lseek,             sys_lseek),          

   GENX_(__NR_getpid,            sys_getpid),         
   LINX_(__NR_mount,             sys_mount),          
   LINX_(__NR_umount,            sys_oldumount),      
   LINX_(__NR_setuid,            sys_setuid16),       
   LINX_(__NR_getuid,            sys_getuid16),       

   LINX_(__NR_stime,             sys_stime),          
   PLAXY(__NR_ptrace,            sys_ptrace),         
   GENX_(__NR_alarm,             sys_alarm),          
   GENX_(__NR_pause,             sys_pause),          

   LINX_(__NR_utime,             sys_utime),          
   GENX_(__NR_stty,              sys_ni_syscall),     
   GENX_(__NR_gtty,              sys_ni_syscall),     
   GENX_(__NR_access,            sys_access),         
   GENX_(__NR_nice,              sys_nice),           

   GENX_(__NR_ftime,             sys_ni_syscall),     
   GENX_(__NR_sync,              sys_sync),           
   GENX_(__NR_kill,              sys_kill),           
   GENX_(__NR_rename,            sys_rename),         
   GENX_(__NR_mkdir,             sys_mkdir),          

   GENX_(__NR_rmdir,             sys_rmdir),          
   GENXY(__NR_dup,               sys_dup),            
   LINXY(__NR_pipe,              sys_pipe),           
   GENXY(__NR_times,             sys_times),          
   GENX_(__NR_prof,              sys_ni_syscall),     
   GENX_(__NR_brk,               sys_brk),            
   LINX_(__NR_setgid,            sys_setgid16),       
   LINX_(__NR_getgid,            sys_getgid16),       
   LINX_(__NR_geteuid,           sys_geteuid16),      

   LINX_(__NR_getegid,           sys_getegid16),      
   GENX_(__NR_acct,              sys_acct),           
   LINX_(__NR_umount2,           sys_umount),         
   GENX_(__NR_lock,              sys_ni_syscall),     
   LINXY(__NR_ioctl,             sys_ioctl),          

   LINXY(__NR_fcntl,             sys_fcntl),          
   GENX_(__NR_mpx,               sys_ni_syscall),     
   GENX_(__NR_setpgid,           sys_setpgid),        
   GENX_(__NR_ulimit,            sys_ni_syscall),     
   GENX_(__NR_umask,             sys_umask),          
   GENX_(__NR_chroot,            sys_chroot),         
   GENXY(__NR_dup2,              sys_dup2),           
   GENX_(__NR_getppid,           sys_getppid),        

   GENX_(__NR_getpgrp,           sys_getpgrp),        
   GENX_(__NR_setsid,            sys_setsid),         
   LINXY(__NR_sigaction,         sys_sigaction),      
   LINX_(__NR_setreuid,          sys_setreuid16),     
   LINX_(__NR_setregid,          sys_setregid16),     
   PLAX_(__NR_sigsuspend,        sys_sigsuspend),     
   LINXY(__NR_sigpending,        sys_sigpending),     
   GENX_(__NR_sethostname,       sys_sethostname),    
   GENX_(__NR_setrlimit,         sys_setrlimit),      
   GENXY(__NR_getrlimit,         sys_old_getrlimit),  
   GENXY(__NR_getrusage,         sys_getrusage),      
   GENXY(__NR_gettimeofday,      sys_gettimeofday),   
   GENX_(__NR_settimeofday,      sys_settimeofday),   

   LINXY(__NR_getgroups,         sys_getgroups16),    
   LINX_(__NR_setgroups,         sys_setgroups16),    
   PLAX_(__NR_select,            old_select),         
   GENX_(__NR_symlink,           sys_symlink),        
   GENX_(__NR_readlink,          sys_readlink),       
   PLAX_(__NR_mmap,              old_mmap),           
   GENXY(__NR_munmap,            sys_munmap),         
   GENX_(__NR_truncate,          sys_truncate),       
   GENX_(__NR_ftruncate,         sys_ftruncate),      
   GENX_(__NR_fchmod,            sys_fchmod),         

   LINX_(__NR_fchown,            sys_fchown16),       
   GENX_(__NR_getpriority,       sys_getpriority),    
   GENX_(__NR_setpriority,       sys_setpriority),    
   GENX_(__NR_profil,            sys_ni_syscall),     
   GENXY(__NR_statfs,            sys_statfs),         

   GENXY(__NR_fstatfs,           sys_fstatfs),        
   LINX_(__NR_ioperm,            sys_ioperm),         
   LINXY(__NR_socketcall,        sys_socketcall),     
   LINXY(__NR_syslog,            sys_syslog),         
   GENXY(__NR_setitimer,         sys_setitimer),      

   GENXY(__NR_getitimer,         sys_getitimer),      
   GENXY(__NR_stat,              sys_newstat),        
   GENXY(__NR_lstat,             sys_newlstat),       
   GENXY(__NR_fstat,             sys_newfstat),       
   GENX_(__NR_iopl,              sys_iopl),           
   LINX_(__NR_vhangup,           sys_vhangup),        
   GENX_(__NR_idle,              sys_ni_syscall),     
   PLAXY(__NR_vm86old,           sys_vm86old),        
   GENXY(__NR_wait4,             sys_wait4),          
   LINXY(__NR_sysinfo,           sys_sysinfo),        
   LINXY(__NR_ipc,               sys_ipc),            
   GENX_(__NR_fsync,             sys_fsync),          
   PLAX_(__NR_sigreturn,         sys_sigreturn),      

   PLAX_(__NR_clone,             sys_clone),          
   GENXY(__NR_uname,             sys_newuname),       
   PLAX_(__NR_modify_ldt,        sys_modify_ldt),     
   LINXY(__NR_adjtimex,          sys_adjtimex),       

   GENXY(__NR_mprotect,          sys_mprotect),       
   LINXY(__NR_sigprocmask,       sys_sigprocmask),    
   GENX_(__NR_create_module,     sys_ni_syscall),     
   LINX_(__NR_init_module,       sys_init_module),    
   LINX_(__NR_delete_module,     sys_delete_module),  
   GENX_(__NR_get_kernel_syms,   sys_ni_syscall),     
   LINX_(__NR_quotactl,          sys_quotactl),       
   GENX_(__NR_getpgid,           sys_getpgid),        
   GENX_(__NR_fchdir,            sys_fchdir),         
   LINX_(__NR_personality,       sys_personality),    
   GENX_(__NR_afs_syscall,       sys_ni_syscall),     
   LINX_(__NR_setfsuid,          sys_setfsuid16),     
   LINX_(__NR_setfsgid,          sys_setfsgid16),     
 
   LINXY(__NR__llseek,           sys_llseek),         
   GENXY(__NR_getdents,          sys_getdents),       
   GENX_(__NR__newselect,        sys_select),         
   GENX_(__NR_flock,             sys_flock),          
   GENX_(__NR_msync,             sys_msync),          

   GENXY(__NR_readv,             sys_readv),          
   GENX_(__NR_writev,            sys_writev),         
   GENX_(__NR_getsid,            sys_getsid),         
   GENX_(__NR_fdatasync,         sys_fdatasync),      
   LINXY(__NR__sysctl,           sys_sysctl),         

   GENX_(__NR_mlock,             sys_mlock),          
   GENX_(__NR_munlock,           sys_munlock),        
   GENX_(__NR_mlockall,          sys_mlockall),       
   LINX_(__NR_munlockall,        sys_munlockall),     
   LINXY(__NR_sched_setparam,    sys_sched_setparam), 

   LINXY(__NR_sched_getparam,         sys_sched_getparam),        
   LINX_(__NR_sched_setscheduler,     sys_sched_setscheduler),    
   LINX_(__NR_sched_getscheduler,     sys_sched_getscheduler),    
   LINX_(__NR_sched_yield,            sys_sched_yield),           
   LINX_(__NR_sched_get_priority_max, sys_sched_get_priority_max),

   LINX_(__NR_sched_get_priority_min, sys_sched_get_priority_min),
   LINXY(__NR_sched_rr_get_interval,  sys_sched_rr_get_interval), 
   GENXY(__NR_nanosleep,         sys_nanosleep),      
   GENX_(__NR_mremap,            sys_mremap),         
   LINX_(__NR_setresuid,         sys_setresuid16),    

   LINXY(__NR_getresuid,         sys_getresuid16),    
   PLAXY(__NR_vm86,              sys_vm86),           
   GENX_(__NR_query_module,      sys_ni_syscall),     
   GENXY(__NR_poll,              sys_poll),           
   LINX_(__NR_setresgid,         sys_setresgid16),    
   LINXY(__NR_getresgid,         sys_getresgid16),    
   LINXY(__NR_prctl,             sys_prctl),          
   PLAX_(__NR_rt_sigreturn,      sys_rt_sigreturn),   
   LINXY(__NR_rt_sigaction,      sys_rt_sigaction),   

   LINXY(__NR_rt_sigprocmask,    sys_rt_sigprocmask), 
   LINXY(__NR_rt_sigpending,     sys_rt_sigpending),  
   LINXY(__NR_rt_sigtimedwait,   sys_rt_sigtimedwait),
   LINXY(__NR_rt_sigqueueinfo,   sys_rt_sigqueueinfo),
   LINX_(__NR_rt_sigsuspend,     sys_rt_sigsuspend),  

   GENXY(__NR_pread64,           sys_pread64),        
   GENX_(__NR_pwrite64,          sys_pwrite64),       
   LINX_(__NR_chown,             sys_chown16),        
   GENXY(__NR_getcwd,            sys_getcwd),         
   LINXY(__NR_capget,            sys_capget),         

   LINX_(__NR_capset,            sys_capset),         
   GENXY(__NR_sigaltstack,       sys_sigaltstack),    
   LINXY(__NR_sendfile,          sys_sendfile),       
   GENXY(__NR_getpmsg,           sys_getpmsg),        
   GENX_(__NR_putpmsg,           sys_putpmsg),        

   
   GENX_(__NR_vfork,             sys_fork),           
   GENXY(__NR_ugetrlimit,        sys_getrlimit),      
   PLAX_(__NR_mmap2,             sys_mmap2),          
   GENX_(__NR_truncate64,        sys_truncate64),     
   GENX_(__NR_ftruncate64,       sys_ftruncate64),    
   
   PLAXY(__NR_stat64,            sys_stat64),         
   PLAXY(__NR_lstat64,           sys_lstat64),        
   PLAXY(__NR_fstat64,           sys_fstat64),        
   GENX_(__NR_lchown32,          sys_lchown),         
   GENX_(__NR_getuid32,          sys_getuid),         

   GENX_(__NR_getgid32,          sys_getgid),         
   GENX_(__NR_geteuid32,         sys_geteuid),        
   GENX_(__NR_getegid32,         sys_getegid),        
   GENX_(__NR_setreuid32,        sys_setreuid),       
   GENX_(__NR_setregid32,        sys_setregid),       

   GENXY(__NR_getgroups32,       sys_getgroups),      
   GENX_(__NR_setgroups32,       sys_setgroups),      
   GENX_(__NR_fchown32,          sys_fchown),         
   LINX_(__NR_setresuid32,       sys_setresuid),      
   LINXY(__NR_getresuid32,       sys_getresuid),      

   LINX_(__NR_setresgid32,       sys_setresgid),      
   LINXY(__NR_getresgid32,       sys_getresgid),      
   GENX_(__NR_chown32,           sys_chown),          
   GENX_(__NR_setuid32,          sys_setuid),         
   GENX_(__NR_setgid32,          sys_setgid),         

   LINX_(__NR_setfsuid32,        sys_setfsuid),       
   LINX_(__NR_setfsgid32,        sys_setfsgid),       
   LINX_(__NR_pivot_root,        sys_pivot_root),     
   GENXY(__NR_mincore,           sys_mincore),        
   GENX_(__NR_madvise,           sys_madvise),        

   GENXY(__NR_getdents64,        sys_getdents64),     
   LINXY(__NR_fcntl64,           sys_fcntl64),        
   GENX_(222,                    sys_ni_syscall),     
   PLAXY(223,                    sys_syscall223),     
   LINX_(__NR_gettid,            sys_gettid),         

   LINX_(__NR_readahead,         sys_readahead),      
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
   LINXY(__NR_tkill,             sys_tkill),          
   LINXY(__NR_sendfile64,        sys_sendfile64),     

   LINXY(__NR_futex,             sys_futex),             
   LINX_(__NR_sched_setaffinity, sys_sched_setaffinity), 
   LINXY(__NR_sched_getaffinity, sys_sched_getaffinity), 
   PLAX_(__NR_set_thread_area,   sys_set_thread_area),   
   PLAX_(__NR_get_thread_area,   sys_get_thread_area),   

   LINXY(__NR_io_setup,          sys_io_setup),       
   LINX_(__NR_io_destroy,        sys_io_destroy),     
   LINXY(__NR_io_getevents,      sys_io_getevents),   
   LINX_(__NR_io_submit,         sys_io_submit),      
   LINXY(__NR_io_cancel,         sys_io_cancel),      

   LINX_(__NR_fadvise64,         sys_fadvise64),      
   GENX_(251,                    sys_ni_syscall),     
   LINX_(__NR_exit_group,        sys_exit_group),     
   LINXY(__NR_lookup_dcookie,    sys_lookup_dcookie), 
   LINXY(__NR_epoll_create,      sys_epoll_create),   

   LINX_(__NR_epoll_ctl,         sys_epoll_ctl),         
   LINXY(__NR_epoll_wait,        sys_epoll_wait),        
   LINX_(__NR_set_tid_address,   sys_set_tid_address),   
   LINXY(__NR_timer_create,      sys_timer_create),      

   LINXY(__NR_timer_settime,     sys_timer_settime),  
   LINXY(__NR_timer_gettime,     sys_timer_gettime),  
   LINX_(__NR_timer_getoverrun,  sys_timer_getoverrun),
   LINX_(__NR_timer_delete,      sys_timer_delete),   
   LINX_(__NR_clock_settime,     sys_clock_settime),  

   LINXY(__NR_clock_gettime,     sys_clock_gettime),  
   LINXY(__NR_clock_getres,      sys_clock_getres),   
   LINXY(__NR_clock_nanosleep,   sys_clock_nanosleep),
   GENXY(__NR_statfs64,          sys_statfs64),       
   GENXY(__NR_fstatfs64,         sys_fstatfs64),      

   LINX_(__NR_tgkill,            sys_tgkill),         
   GENX_(__NR_utimes,            sys_utimes),         
   LINX_(__NR_fadvise64_64,      sys_fadvise64_64),   
   GENX_(__NR_vserver,           sys_ni_syscall),     
   LINX_(__NR_mbind,             sys_mbind),          

   LINXY(__NR_get_mempolicy,     sys_get_mempolicy),  
   LINX_(__NR_set_mempolicy,     sys_set_mempolicy),  
   LINXY(__NR_mq_open,           sys_mq_open),        
   LINX_(__NR_mq_unlink,         sys_mq_unlink),      
   LINX_(__NR_mq_timedsend,      sys_mq_timedsend),   

   LINXY(__NR_mq_timedreceive,   sys_mq_timedreceive),
   LINX_(__NR_mq_notify,         sys_mq_notify),      
   LINXY(__NR_mq_getsetattr,     sys_mq_getsetattr),  
   GENX_(__NR_sys_kexec_load,    sys_ni_syscall),     
   LINXY(__NR_waitid,            sys_waitid),         

   GENX_(285,                    sys_ni_syscall),     
   LINX_(__NR_add_key,           sys_add_key),        
   LINX_(__NR_request_key,       sys_request_key),    
   LINXY(__NR_keyctl,            sys_keyctl),         
   LINX_(__NR_ioprio_set,        sys_ioprio_set),     

   LINX_(__NR_ioprio_get,        sys_ioprio_get),     
   LINX_(__NR_inotify_init,	 sys_inotify_init),   
   LINX_(__NR_inotify_add_watch, sys_inotify_add_watch), 
   LINX_(__NR_inotify_rm_watch,	 sys_inotify_rm_watch), 

   LINXY(__NR_openat,		 sys_openat),           
   LINX_(__NR_mkdirat,		 sys_mkdirat),          
   LINX_(__NR_mknodat,		 sys_mknodat),          
   LINX_(__NR_fchownat,		 sys_fchownat),         
   LINX_(__NR_futimesat,	 sys_futimesat),        

   PLAXY(__NR_fstatat64,	 sys_fstatat64),        
   LINX_(__NR_unlinkat,		 sys_unlinkat),         
   LINX_(__NR_renameat,		 sys_renameat),         
   LINX_(__NR_linkat,		 sys_linkat),           
   LINX_(__NR_symlinkat,	 sys_symlinkat),        

   LINX_(__NR_readlinkat,	 sys_readlinkat),       
   LINX_(__NR_fchmodat,		 sys_fchmodat),         
   LINX_(__NR_faccessat,	 sys_faccessat),        
   LINX_(__NR_pselect6,		 sys_pselect6),         
   LINXY(__NR_ppoll,		 sys_ppoll),            

   LINX_(__NR_unshare,		 sys_unshare),          
   LINX_(__NR_set_robust_list,	 sys_set_robust_list),  
   LINXY(__NR_get_robust_list,	 sys_get_robust_list),  
   LINX_(__NR_splice,            sys_splice),           
   LINX_(__NR_sync_file_range,   sys_sync_file_range),  

   LINX_(__NR_tee,               sys_tee),              
   LINXY(__NR_vmsplice,          sys_vmsplice),         
   LINXY(__NR_move_pages,        sys_move_pages),       
   LINXY(__NR_getcpu,            sys_getcpu),           
   LINXY(__NR_epoll_pwait,       sys_epoll_pwait),      

   LINX_(__NR_utimensat,         sys_utimensat),        
   LINXY(__NR_signalfd,          sys_signalfd),         
   LINXY(__NR_timerfd_create,    sys_timerfd_create),   
   LINXY(__NR_eventfd,           sys_eventfd),          
   LINX_(__NR_fallocate,         sys_fallocate),        

   LINXY(__NR_timerfd_settime,   sys_timerfd_settime),  
   LINXY(__NR_timerfd_gettime,   sys_timerfd_gettime),  
   LINXY(__NR_signalfd4,         sys_signalfd4),        
   LINXY(__NR_eventfd2,          sys_eventfd2),         
   LINXY(__NR_epoll_create1,     sys_epoll_create1),     

   LINXY(__NR_dup3,              sys_dup3),             
   LINXY(__NR_pipe2,             sys_pipe2),            
   LINXY(__NR_inotify_init1,     sys_inotify_init1),    
   LINXY(__NR_preadv,            sys_preadv),           
   LINX_(__NR_pwritev,           sys_pwritev),          

   LINXY(__NR_rt_tgsigqueueinfo, sys_rt_tgsigqueueinfo),
   LINXY(__NR_perf_event_open,   sys_perf_event_open),  
   LINXY(__NR_recvmmsg,          sys_recvmmsg),         
   LINXY(__NR_fanotify_init,     sys_fanotify_init),    
   LINX_(__NR_fanotify_mark,     sys_fanotify_mark),    

   LINXY(__NR_prlimit64,         sys_prlimit64),        
   LINXY(__NR_name_to_handle_at, sys_name_to_handle_at),
   LINXY(__NR_open_by_handle_at, sys_open_by_handle_at),
   LINXY(__NR_clock_adjtime,     sys_clock_adjtime),    
   LINX_(__NR_syncfs,            sys_syncfs),           

   LINXY(__NR_sendmmsg,          sys_sendmmsg),         
   LINXY(__NR_process_vm_readv,  sys_process_vm_readv), 
   LINX_(__NR_process_vm_writev, sys_process_vm_writev),
   LINX_(__NR_kcmp,              sys_kcmp),             


   LINXY(__NR_getrandom,         sys_getrandom),        
   LINXY(__NR_memfd_create,      sys_memfd_create)      
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

