
/*
 * Copyright (c) 1996-2004 Per Fogelstrom, Opsycon AB
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _MIPS64_EXEC_H_
#define _MIPS64_EXEC_H_

#define	__LDPGSZ	4096

#define NATIVE_EXEC_ELF
#define NATIVE_ELFSIZE 64
#define	EXEC_SCRIPT

#ifdef __LP64__
#define	ARCH_ELFSIZE 64
#define DB_ELFSIZE 64
#define ELF_TARG_CLASS  ELFCLASS64
#else
#define	ARCH_ELFSIZE 32
#define DB_ELFSIZE 32
#define ELF_TARG_CLASS  ELFCLASS32
#endif

#if defined(__MIPSEB__)
#define ELF_TARG_DATA		ELFDATA2MSB
#else
#define ELF_TARG_DATA		ELFDATA2LSB
#endif
#define ELF_TARG_MACH		EM_MIPS

#define _NLIST_DO_ELF

#if defined(_LP64)
#define _KERN_DO_ELF64
#if defined(COMPAT_O32)
#define _KERN_DO_ELF
#endif
#else
#define _KERN_DO_ELF
#endif


#define PT_MIPS_REGINFO 0x70000000      

#define DT_MIPS_RLD_VERSION  0x70000001 
#define DT_MIPS_TIME_STAMP   0x70000002 
#define DT_MIPS_ICHECKSUM    0x70000003 
#define DT_MIPS_IVERSION     0x70000004 
#define DT_MIPS_FLAGS        0x70000005 
#define DT_MIPS_BASE_ADDRESS 0x70000006 
#define DT_MIPS_CONFLICT     0x70000008 
#define DT_MIPS_LIBLIST      0x70000009 
#define DT_MIPS_LOCAL_GOTNO  0x7000000a 
#define DT_MIPS_CONFLICTNO   0x7000000b 
#define DT_MIPS_LIBLISTNO    0x70000010 
#define DT_MIPS_SYMTABNO     0x70000011 
#define DT_MIPS_UNREFEXTNO   0x70000012 
#define DT_MIPS_GOTSYM       0x70000013 
#define DT_MIPS_HIPAGENO     0x70000014 
#define DT_MIPS_RLD_MAP      0x70000016 

#define DT_PROCNUM (DT_MIPS_RLD_MAP - DT_LOPROC + 1)

#define EF_MIPS_NOREORDER	0x00000001	
#define EF_MIPS_PIC		0x00000002	
#define EF_MIPS_CPIC		0x00000004	
#define	EF_MIPS_ABI2		0x00000020	
#define	EF_MIPS_32BITMODE	0x00000100	
#define EF_MIPS_ARCH		0xf0000000	
#define	E_MIPS_ARCH_1		0x00000000
#define	E_MIPS_ARCH_2		0x10000000
#define	E_MIPS_ARCH_3		0x20000000
#define	E_MIPS_ARCH_4		0x30000000
#define	EF_MIPS_ABI		0x0000f000	
#define	E_MIPS_ABI_NONE		0x00000000	
#define	E_MIPS_ABI_O32		0x00001000
#define	E_MIPS_ABI_O64		0x00002000
#define	E_MIPS_ABI_EABI32	0x00004000
#define	E_MIPS_ABI_EABI64	0x00004000

#define	SHN_MIPS_ACOMMON	0xff00		
#define	SHN_MIPS_SCOMMON	0xff03		
#define	SHN_MIPS_SUNDEFINED	0xff04		

#define	SHT_MIPS_LIBLIST  0x70000000	
#define	SHT_MIPS_CONFLICT 0x70000002	
#define	SHT_MIPS_GPTAB    0x70000003	
#define	SHT_MIPS_UCODE    0x70000004	
#define	SHT_MIPS_DEBUG    0x70000005	
#define	SHT_MIPS_REGINFO  0x70000006	

#define	SHF_MIPS_GPREL	0x10000000	

#if 0
typedef union {
	struct {
		Elf32_Word gt_current_g_value;	
		Elf32_Word gt_unused;	
	} gt_header;			
	struct {
		Elf32_Word gt_g_value;	
		Elf32_Word gt_bytes;	
	} gt_entry;			
} Elf32_gptab;

typedef struct {
	Elf32_Word	ri_gprmask;	
	Elf32_Word	ri_cprmask[4];	
	Elf32_Sword	ri_gp_value;	
} Elf32_RegInfo;
#endif



#define	R_MIPS_NONE	0	
#define	R_MIPS_16	1	
#define	R_MIPS_32	2	
#define	R_MIPS_REL32	3	
#define	R_MIPS_26	4	
#define	R_MIPS_HI16	5	
#define	R_MIPS_LO16	6	
#define	R_MIPS_GPREL16	7	
#define	R_MIPS_LITERAL	8	
#define	R_MIPS_GOT16	9	
#define	R_MIPS_PC16	10	
#define	R_MIPS_CALL16	11	
#define	R_MIPS_GPREL32	12	

#define	R_MIPS_64	18

#define	R_MIPS_REL32_64	((R_MIPS_64 << 8) | R_MIPS_REL32)


#endif	
