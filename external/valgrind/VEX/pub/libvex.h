

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

#ifndef __LIBVEX_H
#define __LIBVEX_H


#include "libvex_basictypes.h"
#include "libvex_ir.h"




typedef 
   enum { 
      VexArch_INVALID=0x400,
      VexArchX86, 
      VexArchAMD64, 
      VexArchARM,
      VexArchARM64,
      VexArchPPC32,
      VexArchPPC64,
      VexArchS390X,
      VexArchMIPS32,
      VexArchMIPS64,
      VexArchTILEGX
   }
   VexArch;


typedef
   enum {
      VexEndness_INVALID=0x600, 
      VexEndnessLE,             
      VexEndnessBE              
   }
   VexEndness;



#define VEX_HWCAPS_X86_MMXEXT  (1<<1)  
#define VEX_HWCAPS_X86_SSE1    (1<<2)  
#define VEX_HWCAPS_X86_SSE2    (1<<3)  
#define VEX_HWCAPS_X86_SSE3    (1<<4)  
#define VEX_HWCAPS_X86_LZCNT   (1<<5)  

#define VEX_HWCAPS_AMD64_SSE3   (1<<5)  
#define VEX_HWCAPS_AMD64_CX16   (1<<6)  
#define VEX_HWCAPS_AMD64_LZCNT  (1<<7)  
#define VEX_HWCAPS_AMD64_AVX    (1<<8)  
#define VEX_HWCAPS_AMD64_RDTSCP (1<<9)  
#define VEX_HWCAPS_AMD64_BMI    (1<<10) 
#define VEX_HWCAPS_AMD64_AVX2   (1<<11) 

#define VEX_HWCAPS_PPC32_F     (1<<8)  
#define VEX_HWCAPS_PPC32_V     (1<<9)  
#define VEX_HWCAPS_PPC32_FX    (1<<10) 
#define VEX_HWCAPS_PPC32_GX    (1<<11) 
#define VEX_HWCAPS_PPC32_VX    (1<<12) 
#define VEX_HWCAPS_PPC32_DFP   (1<<17) 
#define VEX_HWCAPS_PPC32_ISA2_07   (1<<19) 

#define VEX_HWCAPS_PPC64_V     (1<<13) 
#define VEX_HWCAPS_PPC64_FX    (1<<14) 
#define VEX_HWCAPS_PPC64_GX    (1<<15) 
#define VEX_HWCAPS_PPC64_VX    (1<<16) 
#define VEX_HWCAPS_PPC64_DFP   (1<<18) 
#define VEX_HWCAPS_PPC64_ISA2_07   (1<<20) 


#define VEX_S390X_MODEL_Z900     0
#define VEX_S390X_MODEL_Z800     1
#define VEX_S390X_MODEL_Z990     2
#define VEX_S390X_MODEL_Z890     3
#define VEX_S390X_MODEL_Z9_EC    4
#define VEX_S390X_MODEL_Z9_BC    5
#define VEX_S390X_MODEL_Z10_EC   6
#define VEX_S390X_MODEL_Z10_BC   7
#define VEX_S390X_MODEL_Z196     8
#define VEX_S390X_MODEL_Z114     9
#define VEX_S390X_MODEL_ZEC12    10
#define VEX_S390X_MODEL_ZBC12    11
#define VEX_S390X_MODEL_Z13      12
#define VEX_S390X_MODEL_UNKNOWN  13     
#define VEX_S390X_MODEL_MASK     0x3F

#define VEX_HWCAPS_S390X_LDISP (1<<6)   
#define VEX_HWCAPS_S390X_EIMM  (1<<7)   
#define VEX_HWCAPS_S390X_GIE   (1<<8)   
#define VEX_HWCAPS_S390X_DFP   (1<<9)   
#define VEX_HWCAPS_S390X_FGX   (1<<10)  
#define VEX_HWCAPS_S390X_ETF2  (1<<11)  
#define VEX_HWCAPS_S390X_STFLE (1<<12)  
#define VEX_HWCAPS_S390X_ETF3  (1<<13)  
#define VEX_HWCAPS_S390X_STCKF (1<<14)  
#define VEX_HWCAPS_S390X_FPEXT (1<<15)  
#define VEX_HWCAPS_S390X_LSC   (1<<16)  
#define VEX_HWCAPS_S390X_PFPO  (1<<17)  

#define VEX_HWCAPS_S390X_ALL   (VEX_HWCAPS_S390X_LDISP | \
                                VEX_HWCAPS_S390X_EIMM  | \
                                VEX_HWCAPS_S390X_GIE   | \
                                VEX_HWCAPS_S390X_DFP   | \
                                VEX_HWCAPS_S390X_FGX   | \
                                VEX_HWCAPS_S390X_STFLE | \
                                VEX_HWCAPS_S390X_STCKF | \
                                VEX_HWCAPS_S390X_FPEXT | \
                                VEX_HWCAPS_S390X_LSC   | \
                                VEX_HWCAPS_S390X_ETF3  | \
                                VEX_HWCAPS_S390X_ETF2  | \
                                VEX_HWCAPS_S390X_PFPO)

#define VEX_HWCAPS_S390X(x)  ((x) & ~VEX_S390X_MODEL_MASK)
#define VEX_S390X_MODEL(x)   ((x) &  VEX_S390X_MODEL_MASK)

#define VEX_HWCAPS_TILEGX_BASE (1<<16)  

#define VEX_HWCAPS_ARM_VFP    (1<<6)  
#define VEX_HWCAPS_ARM_VFP2   (1<<7)  
#define VEX_HWCAPS_ARM_VFP3   (1<<8)  
#define VEX_HWCAPS_ARM_NEON   (1<<16) 

#define VEX_ARM_ARCHLEVEL(x) ((x) & 0x3f)



#define VEX_PRID_COMP_MIPS      0x00010000
#define VEX_PRID_COMP_BROADCOM  0x00020000
#define VEX_PRID_COMP_NETLOGIC  0x000C0000
#define VEX_PRID_COMP_CAVIUM    0x000D0000

#define VEX_PRID_IMP_34K        0x9500
#define VEX_PRID_IMP_74K        0x9700

#define VEX_PRID_CPU_32FPR      0x00000040

#define VEX_MIPS_COMP_ID(x) ((x) & 0x00FF0000)
#define VEX_MIPS_PROC_ID(x) ((x) & 0x0000FF00)
#define VEX_MIPS_REV(x) ((x) & 0x000000FF)
#define VEX_MIPS_PROC_DSP2(x) ((VEX_MIPS_COMP_ID(x) == VEX_PRID_COMP_MIPS) && \
                               (VEX_MIPS_PROC_ID(x) == VEX_PRID_IMP_74K))
#define VEX_MIPS_PROC_DSP(x)  (VEX_MIPS_PROC_DSP2(x) || \
                               ((VEX_MIPS_COMP_ID(x) == VEX_PRID_COMP_MIPS) && \
                               (VEX_MIPS_PROC_ID(x) == VEX_PRID_IMP_34K)))


extern const HChar* LibVEX_ppVexArch    ( VexArch );
extern const HChar* LibVEX_ppVexEndness ( VexEndness endness );
extern const HChar* LibVEX_ppVexHwCaps  ( VexArch, UInt );


typedef enum {
   DATA_CACHE=0x500,
   INSN_CACHE,
   UNIFIED_CACHE
} VexCacheKind;

typedef struct {
   VexCacheKind kind;
   UInt level;         
   UInt sizeB;         
   UInt line_sizeB;    
   UInt assoc;         
   Bool is_trace_cache;  
} VexCache;

#define VEX_CACHE_INIT(_kind, _level, _size, _line_size, _assoc)         \
         ({ (VexCache) { .kind = _kind, .level = _level, .sizeB = _size, \
               .line_sizeB = _line_size, .assoc = _assoc, \
               .is_trace_cache = False }; })

typedef struct {
   UInt num_levels;
   UInt num_caches;
   VexCache *caches;
   Bool icaches_maintain_coherence;
} VexCacheInfo;



typedef
   struct {
      
      UInt         hwcaps;
      VexEndness   endness;
      VexCacheInfo hwcache_info;
      
      Int ppc_icache_line_szB;
      UInt ppc_dcbz_szB;
      UInt ppc_dcbzl_szB; 
      UInt arm64_dMinLine_lg2_szB;
      UInt arm64_iMinLine_lg2_szB;
   }
   VexArchInfo;

extern 
void LibVEX_default_VexArchInfo ( VexArchInfo* vai );



typedef
   struct {
      Int guest_stack_redzone_size;

      Bool guest_amd64_assume_fs_is_const;

      Bool guest_amd64_assume_gs_is_const;

      Bool guest_ppc_zap_RZ_at_blr;

      Bool (*guest_ppc_zap_RZ_at_bl)(Addr);

      Bool host_ppc_calls_use_fndescrs;
   }
   VexAbiInfo;

extern 
void LibVEX_default_VexAbiInfo ( VexAbiInfo* vbi );




typedef
   enum {
      VexRegUpd_INVALID=0x700,
      VexRegUpdSpAtMemAccess,
      VexRegUpdUnwindregsAtMemAccess,
      VexRegUpdAllregsAtMemAccess,
      VexRegUpdAllregsAtEachInsn
   }
   VexRegisterUpdates;


typedef
   struct {
      
      Int iropt_verbosity;
      Int iropt_level;
      VexRegisterUpdates iropt_register_updates_default;
      Int iropt_unroll_thresh;
      Int guest_max_insns;
      Int guest_chase_thresh;
      Bool guest_chase_cond;
   }
   VexControl;



extern 
void LibVEX_default_VexControl (  VexControl* vcon );



extern void* LibVEX_Alloc ( SizeT nbytes );

extern void LibVEX_ShowAllocStats ( void );




#define VEXGLO_N_ALWAYSDEFD  24

typedef
   struct {
      Int total_sizeB;
      
      Int offset_SP;
      Int sizeof_SP; 
      
      Int offset_FP;
      Int sizeof_FP; 
      
      Int offset_IP;
      Int sizeof_IP; 
      Int n_alwaysDefd;
      struct {
         Int offset;
         Int size;
      } alwaysDefd[VEXGLO_N_ALWAYSDEFD];
   }
   VexGuestLayout;


#define LibVEX_N_SPILL_BYTES 4096

#define LibVEX_GUEST_STATE_ALIGN 16



extern void LibVEX_Init (

   
#  if __cplusplus == 1 && __GNUC__ && __GNUC__ <= 3
#  else
   __attribute__ ((noreturn))
#  endif
   void (*failure_exit) ( void ),

   
   void (*log_bytes) ( const HChar*, SizeT nbytes ),

   
   Int debuglevel,

   
   const VexControl* vcon
);



typedef
   struct {
      
      enum { VexTransOK=0x800,
             VexTransAccessFail, VexTransOutputFull } status;
      
      UInt n_sc_extents;
      Int offs_profInc;
      UInt n_guest_instrs;
   }
   VexTranslateResult;


typedef
   struct {
      Addr   base[3];
      UShort len[3];
      UShort n_used;
   }
   VexGuestExtents;


typedef
   struct {
      VexArch      arch_guest;
      VexArchInfo  archinfo_guest;
      VexArch      arch_host;
      VexArchInfo  archinfo_host;
      VexAbiInfo   abiinfo_both;

      void*   callback_opaque;

      
      
      const UChar*  guest_bytes;
      Addr    guest_bytes_addr;

      Bool    (*chase_into_ok) ( void*, Addr );

      
      VexGuestExtents* guest_extents;

      
      UChar*  host_bytes;
      Int     host_bytes_size;
      
      Int*    host_bytes_used;

      IRSB*   (*instrument1) ( void*, 
                               IRSB*, 
                               const VexGuestLayout*, 
                               const VexGuestExtents*,
                               const VexArchInfo*,
                               IRType gWordTy, IRType hWordTy );
      IRSB*   (*instrument2) ( void*, 
                               IRSB*, 
                               const VexGuestLayout*, 
                               const VexGuestExtents*,
                               const VexArchInfo*,
                               IRType gWordTy, IRType hWordTy );

      IRSB* (*finaltidy) ( IRSB* );

      UInt (*needs_self_check)( void*,
                                VexRegisterUpdates* pxControl,
                                const VexGuestExtents* );

      Bool    (*preamble_function)(void*, IRSB*);

      
      Int     traceflags;

      
      Bool    sigill_diag;

      Bool    addProfInc;

      const void* disp_cp_chain_me_to_slowEP;
      const void* disp_cp_chain_me_to_fastEP;
      const void* disp_cp_xindir;
      const void* disp_cp_xassisted;
   }
   VexTranslateArgs;


extern 
VexTranslateResult LibVEX_Translate ( VexTranslateArgs* );




typedef
   struct {
      HWord start;
      HWord len;     
   }
   VexInvalRange;

extern
VexInvalRange LibVEX_Chain ( VexArch     arch_host,
                             VexEndness  endhess_host,
                             void*       place_to_chain,
                             const void* disp_cp_chain_me_EXPECTED,
                             const void* place_to_jump_to );

extern
VexInvalRange LibVEX_UnChain ( VexArch     arch_host,
                               VexEndness  endness_host,
                               void*       place_to_unchain,
                               const void* place_to_jump_to_EXPECTED,
                               const void* disp_cp_chain_me );

extern
Int LibVEX_evCheckSzB ( VexArch arch_host );


extern
VexInvalRange LibVEX_PatchProfInc ( VexArch      arch_host,
                                    VexEndness   endness_host,
                                    void*        place_to_patch,
                                    const ULong* location_of_counter );



extern void LibVEX_ShowStats ( void );



#define NO_ROUNDING_MODE (~0u)

typedef 
   struct {
      IROp  op;        
      HWord result;    
      HWord opnd1;     
      HWord opnd2;     
      HWord opnd3;     
      HWord opnd4;     
      IRType t_result; 
      IRType t_opnd1;  
      IRType t_opnd2;  
      IRType t_opnd3;  
      IRType t_opnd4;  
      UInt  rounding_mode;
      UInt  num_operands; 
      Bool  shift_amount_is_immediate;
   }
   IRICB;

extern void LibVEX_InitIRI ( const IRICB * );


#endif 

