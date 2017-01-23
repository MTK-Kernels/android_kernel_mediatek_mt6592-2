

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

#include "pub_core_basics.h"
#include "pub_core_libcassert.h"
#include "pub_core_vki.h"
#include "pub_core_vkiscnums.h"
#include "pub_core_syscall.h"


#if defined(VGO_linux)


SysRes VG_(mk_SysRes_x86_linux) ( Int val ) {
   SysRes res;
   res._isError = val >= -4095 && val <= -1;
   if (res._isError) {
      res._val = (UInt)(-val);
   } else {
      res._val = (UInt)val;
   }
   return res;
}

SysRes VG_(mk_SysRes_amd64_linux) ( Long val ) {
   SysRes res;
   res._isError = val >= -4095 && val <= -1;
   if (res._isError) {
      res._val = (ULong)(-val);
   } else {
      res._val = (ULong)val;
   }
   return res;
}

SysRes VG_(mk_SysRes_tilegx_linux) ( Long val ) {
  SysRes res;
  res._isError = val >= -4095 && val <= -1;
  if (res._isError) {
    res._val = (ULong)(-val);
  } else {
    res._val = (ULong)val;
  }
  return res;
}

SysRes VG_(mk_SysRes_ppc32_linux) ( UInt val, UInt cr0so ) {
   SysRes res;
   res._isError = (cr0so & 1) != 0;
   res._val     = val;
   return res;
}

SysRes VG_(mk_SysRes_ppc64_linux) ( ULong val, ULong cr0so ) {
   SysRes res;
   res._isError = (cr0so & 1) != 0;
   res._val     = val;
   return res;
}

SysRes VG_(mk_SysRes_s390x_linux) ( Long val ) {
   SysRes res;
   res._isError = val >= -4095 && val <= -1;
   if (res._isError) {
      res._val = -val;
   } else {
      res._val = val;
   }
   return res;
}

SysRes VG_(mk_SysRes_arm_linux) ( Int val ) {
   SysRes res;
   res._isError = val >= -4095 && val <= -1;
   if (res._isError) {
      res._val = (UInt)(-val);
   } else {
      res._val = (UInt)val;
   }
   return res;
}

SysRes VG_(mk_SysRes_arm64_linux) ( Long val ) {
   SysRes res;
   res._isError = val >= -4095 && val <= -1;
   if (res._isError) {
      res._val = (ULong)(-val);
   } else {
      res._val = (ULong)val;
   }
   return res;
}

#if defined(VGA_mips64) || defined(VGA_mips32)
SysRes VG_(mk_SysRes_mips32_linux) ( UWord v0, UWord v1, UWord a3 ) {
   SysRes res;
   res._isError = (a3 != (UWord)0);
   res._val     = v0;
   res._valEx   = v1;
   return res;
}

SysRes VG_(mk_SysRes_mips64_linux) ( ULong v0, ULong v1, ULong a3 ) {
   SysRes res;
   res._isError = (a3 != (ULong)0);
   res._val     = v0;
   res._valEx   = v1;
   return res;
}
#endif

SysRes VG_(mk_SysRes_Error) ( UWord err ) {
   SysRes r;
#if defined(VGA_mips64) || defined(VGA_mips32)
   r._valEx   = 0;
#endif
   r._isError = True;
   r._val     = err;
   return r;
}

SysRes VG_(mk_SysRes_Success) ( UWord res ) {
   SysRes r;
#if defined(VGA_mips64) || defined(VGA_mips32)
   r._valEx   = 0;
#endif
   r._isError = False;
   r._val     = res;
   return r;
}


#elif defined(VGO_darwin)

SysRes VG_(mk_SysRes_x86_darwin) ( UChar scclass, Bool isErr,
                                   UInt wHI, UInt wLO )
{
   SysRes res;
   res._wHI  = 0;
   res._wLO  = 0;
   res._mode = 0; 
   vg_assert(isErr == False || isErr == True);
   vg_assert(sizeof(UWord) == sizeof(UInt));
   switch (scclass) {
      case VG_DARWIN_SYSCALL_CLASS_UNIX:
         res._wLO  = wLO;
         res._wHI  = wHI;
         res._mode = isErr ? SysRes_UNIX_ERR : SysRes_UNIX_OK;
         break;
      case VG_DARWIN_SYSCALL_CLASS_MACH:
         vg_assert(!isErr);
         vg_assert(wHI == 0);
         res._wLO  = wLO;
         res._mode = SysRes_MACH;
         break;
      case VG_DARWIN_SYSCALL_CLASS_MDEP:
         vg_assert(!isErr);
         vg_assert(wHI == 0);
         res._wLO  = wLO;
         res._mode = SysRes_MDEP;
         break;
      default:
         vg_assert(0);
   }
   return res;
}

SysRes VG_(mk_SysRes_amd64_darwin) ( UChar scclass, Bool isErr,
                                     ULong wHI, ULong wLO )
{
   SysRes res;
   res._wHI  = 0;
   res._wLO  = 0;
   res._mode = 0; 
   vg_assert(isErr == False || isErr == True);
   vg_assert(sizeof(UWord) == sizeof(ULong));
   switch (scclass) {
      case VG_DARWIN_SYSCALL_CLASS_UNIX:
         res._wLO  = wLO;
         res._wHI  = wHI;
         res._mode = isErr ? SysRes_UNIX_ERR : SysRes_UNIX_OK;
         break;
      case VG_DARWIN_SYSCALL_CLASS_MACH:
         vg_assert(!isErr);
         vg_assert(wHI == 0);
         res._wLO  = wLO;
         res._mode = SysRes_MACH;
         break;
      case VG_DARWIN_SYSCALL_CLASS_MDEP:
         vg_assert(!isErr);
         vg_assert(wHI == 0);
         res._wLO  = wLO;
         res._mode = SysRes_MDEP;
         break;
      default:
         vg_assert(0);
   }
   return res;
}

SysRes VG_(mk_SysRes_Error) ( UWord err ) {
   SysRes r;
   r._wHI  = 0;
   r._wLO  = err;
   r._mode = SysRes_UNIX_ERR;
   return r;
}

SysRes VG_(mk_SysRes_Success) ( UWord res ) {
   SysRes r;
   r._wHI  = 0;
   r._wLO  = res;
   r._mode = SysRes_UNIX_OK;
   return r;
}


#else
#  error "Unknown OS"
#endif



#if defined(VGP_x86_linux)
extern UWord do_syscall_WRK (
          UWord syscall_no, 
          UWord a1, UWord a2, UWord a3,
          UWord a4, UWord a5, UWord a6
       );
asm(
".text\n"
".globl do_syscall_WRK\n"
"do_syscall_WRK:\n"
"	.cfi_startproc\n"
"	push	%esi\n"
"	.cfi_adjust_cfa_offset 4\n"
"	.cfi_offset %esi, -8\n"
"	push	%edi\n"
"	.cfi_adjust_cfa_offset 4\n"
"	.cfi_offset %edi, -12\n"
"	push	%ebx\n"
"	.cfi_adjust_cfa_offset 4\n"
"	.cfi_offset %ebx, -16\n"
"	push	%ebp\n"
"	.cfi_adjust_cfa_offset 4\n"
"	.cfi_offset %ebp, -20\n"
"	movl	16+ 4(%esp),%eax\n"
"	movl	16+ 8(%esp),%ebx\n"
"	movl	16+12(%esp),%ecx\n"
"	movl	16+16(%esp),%edx\n"
"	movl	16+20(%esp),%esi\n"
"	movl	16+24(%esp),%edi\n"
"	movl	16+28(%esp),%ebp\n"
"	int	$0x80\n"
"	popl	%ebp\n"
"	.cfi_adjust_cfa_offset -4\n"
"	.cfi_restore %ebp\n"
"	popl	%ebx\n"
"	.cfi_adjust_cfa_offset -4\n"
"	.cfi_restore %ebx\n"
"	popl	%edi\n"
"	.cfi_adjust_cfa_offset -4\n"
"	.cfi_restore %edi\n"
"	popl	%esi\n"
"	.cfi_adjust_cfa_offset -4\n"
"	.cfi_restore %esi\n"
"	ret\n"
"	.cfi_endproc\n"
".previous\n"
);

#elif defined(VGP_amd64_linux)
extern UWord do_syscall_WRK (
          UWord syscall_no, 
          UWord a1, UWord a2, UWord a3,
          UWord a4, UWord a5, UWord a6
       );
asm(
".text\n"
".globl do_syscall_WRK\n"
"do_syscall_WRK:\n"
"	movq	%rdi, %rax\n"
"	movq	%rsi, %rdi\n"
"	movq	%rdx, %rsi\n"
"	movq	%rcx, %rdx\n"
"	movq	%r8,  %r10\n"
"	movq	%r9,  %r8\n"
"	movq    8(%rsp), %r9\n"	 
"	syscall\n"
"	ret\n"
".previous\n"
);

#elif defined(VGP_ppc32_linux)
extern ULong do_syscall_WRK (
          UWord syscall_no, 
          UWord a1, UWord a2, UWord a3,
          UWord a4, UWord a5, UWord a6
       );
asm(
".text\n"
".globl do_syscall_WRK\n"
"do_syscall_WRK:\n"
"        mr      0,3\n"
"        mr      3,4\n"
"        mr      4,5\n"
"        mr      5,6\n"
"        mr      6,7\n"
"        mr      7,8\n"
"        mr      8,9\n"
"        sc\n"                  
"        mfcr    4\n"           
"        rlwinm  4,4,4,31,31\n" 
"        blr\n"                 
".previous\n"
);

#elif defined(VGP_ppc64be_linux)
extern void do_syscall_WRK ( ULong* argblock );
asm(
".align   2\n"
".globl   do_syscall_WRK\n"
".section \".opd\",\"aw\"\n"
".align   3\n"
"do_syscall_WRK:\n"
".quad    .do_syscall_WRK,.TOC.@tocbase,0\n"
".previous\n"
".type    .do_syscall_WRK,@function\n"
".globl   .do_syscall_WRK\n"
".do_syscall_WRK:\n"
"        std  3,-16(1)\n"  
"        ld   8, 48(3)\n"  
"        ld   7, 40(3)\n"  
"        ld   6, 32(3)\n"  
"        ld   5, 24(3)\n"  
"        ld   4, 16(3)\n"  
"        ld   0,  0(3)\n"  
"        ld   3,  8(3)\n"  
"        sc\n"             
"        ld   5,-16(1)\n"  
"        std  3,0(5)\n"    
"        mfcr 3\n"
"        srwi 3,3,28\n"
"        andi. 3,3,1\n"
"        std  3,8(5)\n"    
"        blr\n"
);

#elif defined(VGP_ppc64le_linux)
extern void do_syscall_WRK ( ULong* argblock );
asm(
".align   2\n"
".globl   do_syscall_WRK\n"
".type    do_syscall_WRK,@function\n"
"do_syscall_WRK:\n"
"#if  _CALL_ELF == 2"               "\n"
"0:      addis        2,12,.TOC.-0b@ha\n"
"        addi         2,2,.TOC.-0b@l\n"
"        .localentry do_syscall_WRK, .-do_syscall_WRK\n"
"#endif"                            "\n"
"        std  3,-16(1)\n"  
"        ld   8, 48(3)\n"  
"        ld   7, 40(3)\n"  
"        ld   6, 32(3)\n"  
"        ld   5, 24(3)\n"  
"        ld   4, 16(3)\n"  
"        ld   0,  0(3)\n"  
"        ld   3,  8(3)\n"  
"        sc\n"             
"        ld   5,-16(1)\n"  
"        std  3,0(5)\n"    
"        mfcr 3\n"
"        srwi 3,3,28\n"
"        andi. 3,3,1\n"
"        std  3,8(5)\n"    
"        blr\n"
"        .size do_syscall_WRK, .-do_syscall_WRK\n"
);

#elif defined(VGP_arm_linux)
extern UWord do_syscall_WRK (
          UWord a1, UWord a2, UWord a3,
          UWord a4, UWord a5, UWord a6,
          UWord syscall_no
       );
asm(
".text\n"
".globl do_syscall_WRK\n"
"do_syscall_WRK:\n"
"         push    {r4, r5, r7}\n"
"         ldr     r4, [sp, #12]\n"
"         ldr     r5, [sp, #16]\n"
"         ldr     r7, [sp, #20]\n"
"         svc     0x0\n"
"         pop     {r4, r5, r7}\n"
"         bx      lr\n"
".previous\n"
);

#elif defined(VGP_arm64_linux)
extern UWord do_syscall_WRK (
          UWord a1, UWord a2, UWord a3,
          UWord a4, UWord a5, UWord a6,
          UWord syscall_no
       );
asm(
".text\n"
".globl do_syscall_WRK\n"
"do_syscall_WRK:\n"
"        mov x8, x6\n"
"        mov x6, 0\n"
"        mov x7, 0\n"
"        svc 0\n"
"        ret\n"
".previous\n"
);

#elif defined(VGP_x86_darwin)


__private_extern__ ULong 
do_syscall_unix_WRK ( UWord a1, UWord a2, UWord a3, 
                      UWord a4, UWord a5, UWord a6, 
                      UWord a7, UWord a8, 
                      UWord syscall_no, 
                      UInt* errflag  );
asm(".private_extern _do_syscall_unix_WRK\n"
    "_do_syscall_unix_WRK:\n"
    "        movl    40(%esp), %ecx   \n"  
    "        movl    $0, (%ecx)       \n"
    "        movl    36(%esp), %eax   \n"
    "        int     $0x80            \n"
    "        jnc     1f               \n"  
    "        movl    40(%esp), %ecx   \n"  
    "        movl    $1, (%ecx)       \n"
    "    1:  ret                      \n"
    );

__private_extern__ UInt 
do_syscall_mach_WRK ( UWord a1, UWord a2, UWord a3, 
                      UWord a4, UWord a5, UWord a6, 
                      UWord a7, UWord a8, 
                      UWord syscall_no  );
asm(".private_extern _do_syscall_mach_WRK\n"
    "_do_syscall_mach_WRK:\n"
    "        movl    36(%esp), %eax   \n"
    "        int     $0x81            \n"
    "        ret                      \n"
    );

__private_extern__ UInt 
do_syscall_mdep_WRK ( UWord a1, UWord a2, UWord a3, 
                      UWord a4, UWord a5, UWord a6, 
                      UWord a7, UWord a8, 
                      UWord syscall_no  );
asm(
    ".private_extern _do_syscall_mdep_WRK\n"
    "_do_syscall_mdep_WRK:\n"
    "        movl    36(%esp), %eax   \n"
    "        int     $0x82            \n"
    "        ret                      \n"
    );


#elif defined(VGP_amd64_darwin)


__private_extern__ UWord 
do_syscall_unix_WRK ( UWord a1, UWord a2, UWord a3, 
                      UWord a4, UWord a5, UWord a6, 
                      UWord a7, UWord a8,           
                      UWord syscall_no,             
                      ULong* errflag,        
                      ULong* res2 );         
asm(".private_extern _do_syscall_unix_WRK\n"
    "_do_syscall_unix_WRK:\n"
    "        movq    %rcx, %r10       \n"  
    "        movq    32(%rsp), %rax   \n"  
    "        movq    $0, (%rax)       \n"
    "        movq    24(%rsp), %rax   \n"  
    "        syscall                  \n"
    "        jnc     1f               \n"  
    "        movq    32(%rsp), %rcx   \n"  
    "        movq    $1, (%rcx)       \n"
    "    1:  movq    40(%rsp), %rcx   \n"  
    "        movq    %rdx, (%rcx)     \n"
    "        retq                     \n"  
    );

__private_extern__ UWord 
do_syscall_mach_WRK ( UWord a1, UWord a2, UWord a3, 
                      UWord a4, UWord a5, UWord a6, 
                      UWord a7, UWord a8,           
                      UWord syscall_no );           
asm(".private_extern _do_syscall_mach_WRK\n"
    "_do_syscall_mach_WRK:\n"
    "        movq    %rcx, %r10       \n"  
    "        movq    24(%rsp), %rax   \n"  
    "        syscall                  \n"
    "        retq                     \n"
    );

#elif defined(VGP_s390x_linux)

static UWord do_syscall_WRK (
   UWord syscall_no,
   UWord arg1, UWord arg2, UWord arg3,
   UWord arg4, UWord arg5, UWord arg6
   )
{
   register UWord __arg1 asm("2") = arg1;
   register UWord __arg2 asm("3") = arg2;
   register UWord __arg3 asm("4") = arg3;
   register UWord __arg4 asm("5") = arg4;
   register UWord __arg5 asm("6") = arg5;
   register UWord __arg6 asm("7") = arg6;
   register ULong __svcres asm("2");

   __asm__ __volatile__ (
                 "lgr %%r1,%1\n\t"
                 "svc 0\n\t"
		: "=d" (__svcres)
		: "a" (syscall_no),
		  "0" (__arg1),
		  "d" (__arg2),
		  "d" (__arg3),
		  "d" (__arg4),
		  "d" (__arg5),
		  "d" (__arg6)
		: "1", "cc", "memory");

   return (UWord) (__svcres);
}

#elif defined(VGP_mips32_linux)
extern int do_syscall_WRK (
          int a1, int a2, int a3,
          int a4, int a5, int a6, int syscall_no, UWord *err,
          UWord *valHi, UWord* valLo
       );
asm(
".globl do_syscall_WRK\n"
".ent do_syscall_WRK\n"
".text\n"
"do_syscall_WRK:\n"   
"   lw $2, 24($29)\n"    
"   syscall\n"
"   lw $8, 28($29)\n" 
"   sw $7, ($8)\n"
"   lw $8, 32($29)\n" 
"   sw $3, ($8)\n"   
"   lw $8, 36($29)\n" 
"   sw $2, ($8)\n"   
"   jr $31\n"
"   nop\n"
".previous\n"
".end do_syscall_WRK\n"
);

#elif defined(VGP_mips64_linux)
extern UWord do_syscall_WRK ( UWord a1, UWord a2, UWord a3, UWord a4, UWord a5,
                              UWord a6, UWord syscall_no, ULong* V1_val );
asm (
".text\n"
".globl do_syscall_WRK\n"
"do_syscall_WRK:\n"
"   daddiu $29, $29, -8\n"
"   sd $11, 0($29)\n"
"   move $2, $10\n"
"   syscall\n"
"   ld $11, 0($29)\n"
"   daddiu $29, $29, 8\n"
"   sd $3, 0($11)\n"  
"   sd $7, 8($11)\n"  
"   jr $31\n"
".previous\n"
);

#elif defined(VGP_tilegx_linux)
extern UWord do_syscall_WRK (
          UWord syscall_no, 
          UWord a1, UWord a2, UWord a3,
          UWord a4, UWord a5, UWord a6
       );
asm(
    ".text\n"
    "do_syscall_WRK:\n"
    "move  r10, r0\n"
    "move  r0,  r1\n"
    "move  r1,  r2\n"
    "move  r2,  r3\n"
    "move  r3,  r4\n"
    "move  r4,  r5\n"
    "move  r5,  r6\n"
    "swint1\n"
    "jrp   lr\n"
    ".previous\n"
    );

#else
#  error Unknown platform
#endif



SysRes VG_(do_syscall) ( UWord sysno, UWord a1, UWord a2, UWord a3,
                                      UWord a4, UWord a5, UWord a6,
                                      UWord a7, UWord a8 )
{
#  if defined(VGP_x86_linux)
   UWord val = do_syscall_WRK(sysno,a1,a2,a3,a4,a5,a6);
   return VG_(mk_SysRes_x86_linux)( val );

#  elif defined(VGP_amd64_linux)
   UWord val = do_syscall_WRK(sysno,a1,a2,a3,a4,a5,a6);
   return VG_(mk_SysRes_amd64_linux)( val );

#  elif defined(VGP_ppc32_linux)
   ULong ret     = do_syscall_WRK(sysno,a1,a2,a3,a4,a5,a6);
   UInt  val     = (UInt)(ret>>32);
   UInt  cr0so   = (UInt)(ret);
   return VG_(mk_SysRes_ppc32_linux)( val, cr0so );

#  elif defined(VGP_ppc64be_linux) || defined(VGP_ppc64le_linux)
   ULong argblock[7];
   argblock[0] = sysno;
   argblock[1] = a1;
   argblock[2] = a2;
   argblock[3] = a3;
   argblock[4] = a4;
   argblock[5] = a5;
   argblock[6] = a6;
   do_syscall_WRK( &argblock[0] );
   return VG_(mk_SysRes_ppc64_linux)( argblock[0], argblock[1] );

#  elif defined(VGP_arm_linux)
   UWord val = do_syscall_WRK(a1,a2,a3,a4,a5,a6,sysno);
   return VG_(mk_SysRes_arm_linux)( val );

#  elif defined(VGP_arm64_linux)
   UWord val = do_syscall_WRK(a1,a2,a3,a4,a5,a6,sysno);
   return VG_(mk_SysRes_arm64_linux)( val );

#  elif defined(VGP_x86_darwin)
   UInt  wLO = 0, wHI = 0, err = 0;
   ULong u64;
   UChar scclass = VG_DARWIN_SYSNO_CLASS(sysno);
   switch (scclass) {
      case VG_DARWIN_SYSCALL_CLASS_UNIX:
         u64 = do_syscall_unix_WRK(a1,a2,a3,a4,a5,a6,a7,a8,
                                   VG_DARWIN_SYSNO_FOR_KERNEL(sysno), &err);
         wLO = (UInt)u64;
         wHI = (UInt)(u64 >> 32);
         break;
      case VG_DARWIN_SYSCALL_CLASS_MACH:
         wLO = do_syscall_mach_WRK(a1,a2,a3,a4,a5,a6,a7,a8, 
                                   VG_DARWIN_SYSNO_FOR_KERNEL(sysno));
         err = 0;
         break;
      case VG_DARWIN_SYSCALL_CLASS_MDEP:
         wLO = do_syscall_mdep_WRK(a1,a2,a3,a4,a5,a6,a7,a8, 
                                   VG_DARWIN_SYSNO_FOR_KERNEL(sysno));
         err = 0;
         break;
      default:
         vg_assert(0);
         break;
   }
   return VG_(mk_SysRes_x86_darwin)( scclass, err ? True : False, wHI, wLO );

#  elif defined(VGP_amd64_darwin)
   ULong wLO = 0, wHI = 0, err = 0;
   UChar scclass = VG_DARWIN_SYSNO_CLASS(sysno);
   switch (scclass) {
      case VG_DARWIN_SYSCALL_CLASS_UNIX:
         wLO = do_syscall_unix_WRK(a1,a2,a3,a4,a5,a6,a7,a8,
                                   VG_DARWIN_SYSNO_FOR_KERNEL(sysno), &err, &wHI);
         break;
      case VG_DARWIN_SYSCALL_CLASS_MACH:
      case VG_DARWIN_SYSCALL_CLASS_MDEP:
         wLO = do_syscall_mach_WRK(a1,a2,a3,a4,a5,a6,a7,a8, 
                                   VG_DARWIN_SYSNO_FOR_KERNEL(sysno));
         err = 0;
         break;
      default:
         vg_assert(0);
         break;
   }
   return VG_(mk_SysRes_amd64_darwin)( scclass, err ? True : False, wHI, wLO );
  
#elif defined(VGP_s390x_linux)
   UWord val;

   if (sysno == __NR_mmap) {
     ULong argbuf[6];

     argbuf[0] = a1;
     argbuf[1] = a2;
     argbuf[2] = a3;
     argbuf[3] = a4;
     argbuf[4] = a5;
     argbuf[5] = a6;
     val = do_syscall_WRK(sysno,(UWord)&argbuf[0],0,0,0,0,0);
   } else {
     val = do_syscall_WRK(sysno,a1,a2,a3,a4,a5,a6);
   }

   return VG_(mk_SysRes_s390x_linux)( val );

#elif defined(VGP_mips32_linux)
   UWord err   = 0;
   UWord valHi = 0;
   UWord valLo = 0;
   (void) do_syscall_WRK(a1,a2,a3,a4,a5,a6, sysno,&err,&valHi,&valLo);
   return VG_(mk_SysRes_mips32_linux)( valLo, valHi, (ULong)err );

#elif defined(VGP_mips64_linux)
   ULong v1_a3[2];
   v1_a3[0] = 0xFF00;
   v1_a3[1] = 0xFF00;
   ULong V0 = do_syscall_WRK(a1,a2,a3,a4,a5,a6,sysno,v1_a3);
   ULong V1 = (ULong)v1_a3[0];
   ULong A3 = (ULong)v1_a3[1];
   return VG_(mk_SysRes_mips64_linux)( V0, V1, A3 );

#  elif defined(VGP_tilegx_linux)
   UWord val = do_syscall_WRK(sysno,a1,a2,a3,a4,a5,a6);

   return VG_(mk_SysRes_tilegx_linux)( val );

#else
#  error Unknown platform
#endif
}



const HChar* VG_(strerror) ( UWord errnum )
{
   switch (errnum) {
   case VKI_EPERM:       return "Operation not permitted";
   case VKI_ENOENT:      return "No such file or directory";
   case VKI_ESRCH:       return "No such process";
   case VKI_EINTR:       return "Interrupted system call";
   case VKI_EIO:         return "Input/output error";
   case VKI_ENXIO:       return "No such device or address";
   case VKI_E2BIG:       return "Argument list too long";
   case VKI_ENOEXEC:     return "Exec format error";
   case VKI_EBADF:       return "Bad file descriptor";
   case VKI_ECHILD:      return "No child processes";
   case VKI_EAGAIN:      return "Resource temporarily unavailable";
   case VKI_ENOMEM:      return "Cannot allocate memory";
   case VKI_EACCES:      return "Permission denied";
   case VKI_EFAULT:      return "Bad address";
   case VKI_ENOTBLK:     return "Block device required";
   case VKI_EBUSY:       return "Device or resource busy";
   case VKI_EEXIST:      return "File exists";
   case VKI_EXDEV:       return "Invalid cross-device link";
   case VKI_ENODEV:      return "No such device";
   case VKI_ENOTDIR:     return "Not a directory";
   case VKI_EISDIR:      return "Is a directory";
   case VKI_EINVAL:      return "Invalid argument";
   case VKI_ENFILE:      return "Too many open files in system";
   case VKI_EMFILE:      return "Too many open files";
   case VKI_ENOTTY:      return "Inappropriate ioctl for device";
   case VKI_ETXTBSY:     return "Text file busy";
   case VKI_EFBIG:       return "File too large";
   case VKI_ENOSPC:      return "No space left on device";
   case VKI_ESPIPE:      return "Illegal seek";
   case VKI_EROFS:       return "Read-only file system";
   case VKI_EMLINK:      return "Too many links";
   case VKI_EPIPE:       return "Broken pipe";
   case VKI_EDOM:        return "Numerical argument out of domain";
   case VKI_ERANGE:      return "Numerical result out of range";

   case VKI_ENOSYS:      return "Function not implemented";
   case VKI_EOVERFLOW:   return "Value too large for defined data type";
#     if defined(VKI_ERESTARTSYS)
      case VKI_ERESTARTSYS: return "ERESTARTSYS";
#     endif
   default:              return "VG_(strerror): unknown error";
   }
}


