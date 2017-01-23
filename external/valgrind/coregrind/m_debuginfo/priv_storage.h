

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

#ifndef __PRIV_STORAGE_H
#define __PRIV_STORAGE_H

#include "pub_core_basics.h"   
#include "pub_core_xarray.h"   
#include "pub_core_deduppoolalloc.h" 
#include "priv_d3basics.h"     
#include "priv_image.h"        


typedef 
   struct {
      SymAVMAs avmas;    
      const HChar*  pri_name;  
      const HChar** sec_names; 
      
      
      
      
      
      UInt    size;    
      Bool    isText;
      Bool    isIFunc; 
   }
   DiSym;


#define LINENO_OVERFLOW (1 << (sizeof(short) * 8))

#define LINENO_BITS     20
#define LOC_SIZE_BITS  (32 - LINENO_BITS)
#define MAX_LINENO     ((1 << LINENO_BITS) - 1)

#define MAX_LOC_SIZE   ((1 << LOC_SIZE_BITS) - 1)

#define OVERFLOW_DIFFERENCE     (LINENO_OVERFLOW - 5000)

typedef
   struct {
      const HChar* filename;     
      const HChar* dirname;      
   } FnDn;

typedef
   struct {
      
      Addr   addr;               
      
      UShort size:LOC_SIZE_BITS; 
      UInt   lineno:LINENO_BITS; 
   }
   DiLoc;

#define LEVEL_BITS  (32 - LINENO_BITS)
#define MAX_LEVEL     ((1 << LEVEL_BITS) - 1)

typedef
   struct {
      
      Addr   addr_lo;            
      
      Addr   addr_hi;            
      
      const HChar* inlinedfn;    
      
      UInt   fndn_ix;            
      UInt   lineno:LINENO_BITS; 
      UShort level:LEVEL_BITS;   
   }
   DiInlLoc;



#define CFIC_IA_SPREL     ((UChar)1)
#define CFIC_IA_BPREL     ((UChar)2)
#define CFIC_ARM_R13REL   ((UChar)3)
#define CFIC_ARM_R12REL   ((UChar)4)
#define CFIC_ARM_R11REL   ((UChar)5)
#define CFIC_ARM_R7REL    ((UChar)6)
#define CFIC_ARM64_SPREL  ((UChar)7)
#define CFIC_ARM64_X29REL ((UChar)8)
#define CFIC_EXPR         ((UChar)9)  

#define CFIR_UNKNOWN      ((UChar)64)
#define CFIR_SAME         ((UChar)65)
#define CFIR_CFAREL       ((UChar)66)
#define CFIR_MEMCFAREL    ((UChar)67)
#define CFIR_EXPR         ((UChar)68)

#if defined(VGA_x86) || defined(VGA_amd64)
typedef
   struct {
      UChar cfa_how; 
      UChar ra_how;  
      UChar sp_how;  
      UChar bp_how;  
      Int   cfa_off;
      Int   ra_off;
      Int   sp_off;
      Int   bp_off;
   }
   DiCfSI_m;
#elif defined(VGA_arm)
typedef
   struct {
      UChar cfa_how; 
      UChar ra_how;  
      UChar r14_how; 
      UChar r13_how; 
      UChar r12_how; 
      UChar r11_how; 
      UChar r7_how;  
      Int   cfa_off;
      Int   ra_off;
      Int   r14_off;
      Int   r13_off;
      Int   r12_off;
      Int   r11_off;
      Int   r7_off;
      
      
   }
   DiCfSI_m;
#elif defined(VGA_arm64)
typedef
   struct {
      UChar cfa_how; 
      UChar ra_how;  
      UChar sp_how;   
      UChar x30_how;  
      UChar x29_how;  
      Int   cfa_off;
      Int   ra_off;
      Int   sp_off;
      Int   x30_off;
      Int   x29_off;
   }
   DiCfSI_m;
#elif defined(VGA_ppc32) || defined(VGA_ppc64be) || defined(VGA_ppc64le)
typedef
   struct {
      UChar cfa_how; 
      UChar ra_how;  
      Int   cfa_off;
      Int   ra_off;
   }
   DiCfSI_m;
#elif defined(VGA_s390x)
typedef
   struct {
      UChar cfa_how; 
      UChar sp_how;  
      UChar ra_how;  
      UChar fp_how;  
      Int   cfa_off;
      Int   sp_off;
      Int   ra_off;
      Int   fp_off;
   }
   DiCfSI_m;
#elif defined(VGA_mips32) || defined(VGA_mips64)
typedef
   struct {
      UChar cfa_how; 
      UChar ra_how;  
      UChar sp_how;  
      UChar fp_how;  
      Int   cfa_off;
      Int   ra_off;
      Int   sp_off;
      Int   fp_off;
   }
   DiCfSI_m;
#elif defined(VGA_tilegx)
typedef
   struct {
      UChar cfa_how; 
      UChar ra_how;  
      UChar sp_how;  
      UChar fp_how;  
      Int   cfa_off;
      Int   ra_off;
      Int   sp_off;
      Int   fp_off;
   }
   DiCfSI_m;
#else
#  error "Unknown arch"
#endif

typedef
   struct {
      Addr  base;
      UInt  len;
      UInt  cfsi_m_ix;
   }
   DiCfSI;

typedef
   enum {
      Cunop_Abs=0x231,
      Cunop_Neg,
      Cunop_Not
   }
   CfiUnop;

typedef
   enum {
      Cbinop_Add=0x321,
      Cbinop_Sub,
      Cbinop_And,
      Cbinop_Mul,
      Cbinop_Shl,
      Cbinop_Shr,
      Cbinop_Eq,
      Cbinop_Ge,
      Cbinop_Gt,
      Cbinop_Le,
      Cbinop_Lt,
      Cbinop_Ne
   }
   CfiBinop;

typedef
   enum {
      Creg_INVALID=0x213,
      Creg_IA_SP,
      Creg_IA_BP,
      Creg_IA_IP,
      Creg_ARM_R13,
      Creg_ARM_R12,
      Creg_ARM_R15,
      Creg_ARM_R14,
      Creg_ARM_R7,
      Creg_ARM64_X30,
      Creg_S390_IA,
      Creg_S390_SP,
      Creg_S390_FP,
      Creg_S390_LR,
      Creg_MIPS_RA,
      Creg_TILEGX_IP,
      Creg_TILEGX_SP,
      Creg_TILEGX_BP,
      Creg_TILEGX_LR
   }
   CfiReg;

typedef
   enum {
      Cex_Undef=0x123,
      Cex_Deref,
      Cex_Const,
      Cex_Unop,
      Cex_Binop,
      Cex_CfiReg,
      Cex_DwReg
   }
   CfiExprTag;

typedef 
   struct {
      CfiExprTag tag;
      union {
         struct {
         } Undef;
         struct {
            Int ixAddr;
         } Deref;
         struct {
            UWord con;
         } Const;
         struct {
            CfiUnop op;
            Int ix;
         } Unop;
         struct {
            CfiBinop op;
            Int ixL;
            Int ixR;
         } Binop;
         struct {
            CfiReg reg;
         } CfiReg;
         struct {
            Int reg;
         } DwReg;
      }
      Cex;
   }
   CfiExpr;

extern Int ML_(CfiExpr_Undef) ( XArray* dst );
extern Int ML_(CfiExpr_Deref) ( XArray* dst, Int ixAddr );
extern Int ML_(CfiExpr_Const) ( XArray* dst, UWord con );
extern Int ML_(CfiExpr_Unop)  ( XArray* dst, CfiUnop op, Int ix );
extern Int ML_(CfiExpr_Binop) ( XArray* dst, CfiBinop op, Int ixL, Int ixR );
extern Int ML_(CfiExpr_CfiReg)( XArray* dst, CfiReg reg );
extern Int ML_(CfiExpr_DwReg) ( XArray* dst, Int reg );

extern void ML_(ppCfiExpr)( const XArray* src, Int ix );


typedef
   struct _FPO_DATA {  
      UInt   ulOffStart; 
      UInt   cbProcSize; 
      UInt   cdwLocals;  
      UShort cdwParams;  
      UChar  cbProlog;   
      UChar  cbRegs :3;  
      UChar  fHasSEH:1;  
      UChar  fUseBP :1;  
      UChar  reserved:1;
      UChar  cbFrame:2;  
   }
   FPO_DATA;

#define PDB_FRAME_FPO  0
#define PDB_FRAME_TRAP 1
#define PDB_FRAME_TSS  2


typedef
   struct {
      Addr    aMin;
      Addr    aMax;
      XArray*  vars;
   }
   DiAddrRange;

typedef
   struct {
      const  HChar* name;  
      UWord  typeR; 
      const GExpr* gexpr; 
      const GExpr* fbGX;  
      UInt   fndn_ix; 
      Int    lineNo;   
   }
   DiVariable;

Word 
ML_(cmp_for_DiAddrRange_range) ( const void* keyV, const void* elemV );




typedef struct
{
   Addr  avma; 
   SizeT size; 
   OffT  foff;
   Bool  rx, rw, ro;  
} DebugInfoMapping;

struct _DebugInfoFSM
{
   HChar*  filename;  
   HChar*  dbgname;   
   XArray* maps;      
   Bool  have_rx_map; 
   Bool  have_rw_map; 
   Bool  have_ro_map; 
};


#define SEGINFO_STRPOOLSIZE (64*1024)


#define N_EHFRAME_SECTS 2



struct _DebugInfo {

   

   struct _DebugInfo* next;   
   Bool               mark;   

   ULong handle;


   Bool trace_symtab; 
   Bool trace_cfi;    
   Bool ddump_syms;   
   Bool ddump_line;   
   Bool ddump_frames; 

   struct _DebugInfoFSM fsm;

   Bool  have_dinfo; 


   
   HChar* soname;

   
   Bool     text_present;
   Addr     text_avma;
   Addr     text_svma;
   SizeT    text_size;
   PtrdiffT text_bias;
   Addr     text_debug_svma;
   PtrdiffT text_debug_bias;
   
   Bool     data_present;
   Addr     data_svma;
   Addr     data_avma;
   SizeT    data_size;
   PtrdiffT data_bias;
   Addr     data_debug_svma;
   PtrdiffT data_debug_bias;
   
   Bool     sdata_present;
   Addr     sdata_svma;
   Addr     sdata_avma;
   SizeT    sdata_size;
   PtrdiffT sdata_bias;
   Addr     sdata_debug_svma;
   PtrdiffT sdata_debug_bias;
   
   Bool     rodata_present;
   Addr     rodata_svma;
   Addr     rodata_avma;
   SizeT    rodata_size;
   PtrdiffT rodata_bias;
   Addr     rodata_debug_svma;
   PtrdiffT rodata_debug_bias;
   
   Bool     bss_present;
   Addr     bss_svma;
   Addr     bss_avma;
   SizeT    bss_size;
   PtrdiffT bss_bias;
   Addr     bss_debug_svma;
   PtrdiffT bss_debug_bias;
   
   Bool     sbss_present;
   Addr     sbss_svma;
   Addr     sbss_avma;
   SizeT    sbss_size;
   PtrdiffT sbss_bias;
   Addr     sbss_debug_svma;
   PtrdiffT sbss_debug_bias;
   
   Bool     exidx_present;
   Addr     exidx_avma;
   Addr     exidx_svma;
   SizeT    exidx_size;
   PtrdiffT exidx_bias;
   
   Bool     extab_present;
   Addr     extab_avma;
   Addr     extab_svma;
   SizeT    extab_size;
   PtrdiffT extab_bias;
   
   Bool   plt_present;
   Addr	  plt_avma;
   SizeT  plt_size;
   
   Bool   got_present;
   Addr   got_avma;
   SizeT  got_size;
   
   Bool   gotplt_present;
   Addr   gotplt_avma;
   SizeT  gotplt_size;
   
   Bool   opd_present;
   Addr   opd_avma;
   SizeT  opd_size;
   UInt   n_ehframe;  
   Addr   ehframe_avma[N_EHFRAME_SECTS];
   SizeT  ehframe_size[N_EHFRAME_SECTS];


   
   DiSym*  symtab;
   UWord   symtab_used;
   UWord   symtab_size;
   
   DiLoc*  loctab;
   UInt    sizeof_fndn_ix;  
   void*   loctab_fndn_ix;  
   UWord   loctab_used;
   UWord   loctab_size;
   DiInlLoc* inltab;
   UWord   inltab_used;
   UWord   inltab_size;
   SizeT   maxinl_codesz;

   Addr* cfsi_base;
   UInt  sizeof_cfsi_m_ix; 
   void* cfsi_m_ix; 

   DiCfSI* cfsi_rd; 
                                   
   UWord   cfsi_used;
   UWord   cfsi_size;

   DedupPoolAlloc *cfsi_m_pool;
   Addr    cfsi_minavma;
   Addr    cfsi_maxavma;
   XArray* cfsi_exprs; 

   FPO_DATA* fpo;
   UWord     fpo_size;
   Addr      fpo_minavma;
   Addr      fpo_maxavma;
   Addr      fpo_base_avma;

   DedupPoolAlloc *strpool;

   DedupPoolAlloc *fndnpool;

   XArray* varinfo;


   XArray*  admin_tyents;

   
   XArray* admin_gexprs;

   DebugInfoMapping* last_rx_map;
};



extern void ML_(addSym) ( struct _DebugInfo* di, DiSym* sym );

extern UInt ML_(addFnDn) (struct _DebugInfo* di,
                          const HChar* filename, 
                          const HChar* dirname);  

extern const HChar* ML_(fndn_ix2filename) (const DebugInfo* di,
                                           UInt fndn_ix);

extern const HChar* ML_(fndn_ix2dirname) (const DebugInfo* di,
                                          UInt fndn_ix);

extern UInt ML_(fndn_ix) (const DebugInfo* di, Word locno);

extern
void ML_(addLineInfo) ( struct _DebugInfo* di, 
                        UInt fndn_ix,
                        Addr this, Addr next, Int lineno, Int entry);

extern
void ML_(addInlInfo) ( struct _DebugInfo* di, 
                       Addr addr_lo, Addr addr_hi,
                       const HChar* inlinedfn,
                       UInt fndn_ix,
                       Int lineno, UShort level);

extern void ML_(addDiCfSI) ( struct _DebugInfo* di, 
                             Addr base, UInt len, DiCfSI_m* cfsi_m );

DiCfSI_m* ML_(get_cfsi_m) (const DebugInfo* di, UInt pos);

extern const HChar* ML_(addStr) ( DebugInfo* di, const HChar* str, Int len );

extern const HChar* ML_(addStrFromCursor)( DebugInfo* di, DiCursor c );

extern void ML_(addVar)( struct _DebugInfo* di,
                         Int    level,
                         Addr   aMin,
                         Addr   aMax,
                         const  HChar* name,
                         UWord  typeR, 
                         const GExpr* gexpr,
                         const GExpr* fbGX, 
                         UInt   fndn_ix, 
                         Int    lineNo, 
                         Bool   show );

extern void ML_(canonicaliseTables) ( struct _DebugInfo* di );

extern void ML_(canonicaliseCFI) ( struct _DebugInfo* di );

extern void ML_(finish_CFSI_arrays) ( struct _DebugInfo* di );


extern Word ML_(search_one_symtab) ( const DebugInfo* di, Addr ptr,
                                     Bool match_anywhere_in_sym,
                                     Bool findText );

extern Word ML_(search_one_loctab) ( const DebugInfo* di, Addr ptr );

extern Word ML_(search_one_cfitab) ( const DebugInfo* di, Addr ptr );

extern Word ML_(search_one_fpotab) ( const DebugInfo* di, Addr ptr );

extern DebugInfoMapping* ML_(find_rx_mapping) ( DebugInfo* di,
                                                Addr lo, Addr hi );


extern 
void ML_(symerr) ( const DebugInfo* di, Bool serious, const HChar* msg );

extern void ML_(ppSym) ( Int idx, const DiSym* sym );

extern void ML_(ppDiCfSI) ( const XArray*  exprs,
                            Addr base, UInt len,
                            const DiCfSI_m* si_m );


#define TRACE_SYMTAB_ENABLED (di->trace_symtab)
#define TRACE_SYMTAB(format, args...) \
   if (TRACE_SYMTAB_ENABLED) { VG_(printf)(format, ## args); }


#endif 

