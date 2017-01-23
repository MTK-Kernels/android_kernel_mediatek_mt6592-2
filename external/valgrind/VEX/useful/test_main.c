

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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "libvex_basictypes.h"
#include "libvex.h"

#include "test_main.h"




__attribute__ ((noreturn))
static
void failure_exit ( void )
{
   fprintf(stdout, "VEX did failure_exit.  Bye.\n");
   exit(1);
}

static
void log_bytes ( const HChar* bytes, SizeT nbytes )
{
   fwrite ( bytes, 1, nbytes, stdout );
}

#define N_LINEBUF 10000
static HChar linebuf[N_LINEBUF];

#define N_ORIGBUF 10000
#define N_TRANSBUF 5000

static UChar origbuf[N_ORIGBUF];
static UChar transbuf[N_TRANSBUF];

static Bool verbose = True;

#if 1 
static
IRSB* mc_instrument ( void* closureV,
                      IRSB* bb_in, VexGuestLayout* layout, 
                      VexGuestExtents* vge,
                      IRType gWordTy, IRType hWordTy );
#endif

static Bool chase_into_not_ok ( void* opaque, Addr dst ) {
   return False;
}
static UInt needs_self_check ( void *closureV, VexRegisterUpdates *pxControl,
                               const VexGuestExtents *vge ) {
   return 0;
}

int main ( int argc, char** argv )
{
   FILE* f;
   Int i;
   UInt u, sum;
   Addr32 orig_addr;
   Int bb_number, n_bbs_done = 0;
   Int orig_nbytes, trans_used;
   VexTranslateResult tres;
   VexControl vcon;
   VexGuestExtents vge;
   VexArchInfo vai_x86, vai_amd64, vai_ppc32, vai_arm, vai_mips32, vai_mips64;
   VexAbiInfo vbi;
   VexTranslateArgs vta;

   if (argc != 2) {
      fprintf(stderr, "usage: vex file.orig\n");
      exit(1);
   }
   f = fopen(argv[1], "r");
   if (!f) {
      fprintf(stderr, "can't open `%s'\n", argv[1]);
      exit(1);
   }

   LibVEX_default_VexControl ( &vcon );
   vcon.iropt_level = 2;
   vcon.guest_max_insns = 60;

   LibVEX_Init ( &failure_exit, &log_bytes, 
                 1,   
                 &vcon );


   while (!feof(f)) {

      __attribute__((unused))
      char* unused1 = fgets(linebuf, N_LINEBUF,f);
      if (linebuf[0] == 0) continue;
      if (linebuf[0] != '.') continue;

      if (n_bbs_done == TEST_N_BBS) break;
      n_bbs_done++;

      
      assert(3 == sscanf(&linebuf[1], " %d %x %d\n", 
                                 & bb_number,
                                 & orig_addr, & orig_nbytes ));
      assert(orig_nbytes >= 1);
      assert(!feof(f));
      __attribute__((unused))
      char* unused2 = fgets(linebuf, N_LINEBUF,f);
      assert(linebuf[0] == '.');

      
      if (verbose)
         printf("============ Basic Block %d, Done %d, "
                "Start %x, nbytes %2d ============", 
                bb_number, n_bbs_done-1, orig_addr, orig_nbytes);

      memset(origbuf, 0, sizeof(origbuf));

      assert(orig_nbytes >= 1 && orig_nbytes <= N_ORIGBUF);
      for (i = 0; i < orig_nbytes; i++) {
         assert(1 == sscanf(&linebuf[2 + 3*i], "%x", &u));
         origbuf[18+ i] = (UChar)u;
      }

      
      LibVEX_default_VexArchInfo(&vai_x86);
      vai_x86.hwcaps = VEX_HWCAPS_X86_MMXEXT | VEX_HWCAPS_X86_SSE1
                       | VEX_HWCAPS_X86_SSE2 | VEX_HWCAPS_X86_SSE3;
      vai_x86.endness = VexEndnessLE;

      LibVEX_default_VexArchInfo(&vai_amd64);
      vai_amd64.hwcaps = 0;
      vai_amd64.endness = VexEndnessLE;

      LibVEX_default_VexArchInfo(&vai_ppc32);
      vai_ppc32.hwcaps = 0;
      vai_ppc32.ppc_icache_line_szB = 128;

      LibVEX_default_VexArchInfo(&vai_arm);
      vai_arm.hwcaps = VEX_HWCAPS_ARM_VFP3 | VEX_HWCAPS_ARM_NEON | 7;

      LibVEX_default_VexArchInfo(&vai_mips32);
      vai_mips32.endness = VexEndnessLE;
      vai_mips32.hwcaps = VEX_PRID_COMP_MIPS;

      LibVEX_default_VexArchInfo(&vai_mips64);
      vai_mips64.endness = VexEndnessLE;

      LibVEX_default_VexAbiInfo(&vbi);
      vbi.guest_stack_redzone_size = 128;

      

      vta.abiinfo_both    = vbi;
      vta.guest_bytes     = &origbuf[18];
      vta.guest_bytes_addr = orig_addr;
      vta.callback_opaque = NULL;
      vta.chase_into_ok   = chase_into_not_ok;
      vta.guest_extents   = &vge;
      vta.host_bytes      = transbuf;
      vta.host_bytes_size = N_TRANSBUF;
      vta.host_bytes_used = &trans_used;

#if 0 
      vta.arch_guest     = VexArchPPC32;
      vta.archinfo_guest = vai_ppc32;
      vta.arch_host      = VexArchPPC32;
      vta.archinfo_host  = vai_ppc32;
#endif
#if 0 
      vta.arch_guest     = VexArchAMD64;
      vta.archinfo_guest = vai_amd64;
      vta.arch_host      = VexArchAMD64;
      vta.archinfo_host  = vai_amd64;
#endif
#if 0 
      vta.arch_guest     = VexArchX86;
      vta.archinfo_guest = vai_x86;
      vta.arch_host      = VexArchX86;
      vta.archinfo_host  = vai_x86;
#endif
#if 1 
      vta.arch_guest     = VexArchX86;
      vta.archinfo_guest = vai_x86;
      vta.arch_host      = VexArchMIPS32;
      vta.archinfo_host  = vai_mips32;
#endif
#if 0 
      vta.arch_guest     = VexArchAMD64;
      vta.archinfo_guest = vai_amd64;
      vta.arch_host      = VexArchMIPS64;
      vta.archinfo_host  = vai_mips64;
#endif
#if 0 
      vta.arch_guest     = VexArchARM;
      vta.archinfo_guest = vai_arm;
      vta.arch_host      = VexArchARM;
      vta.archinfo_host  = vai_arm;
      vta.guest_bytes     = &origbuf[18 +1];
      vta.guest_bytes_addr = (Addr) &origbuf[18 +1];
#endif

#if 1 
      vta.instrument1     = NULL;
      vta.instrument2     = NULL;
#endif
#if 0 
      vta.instrument1     = ac_instrument;
      vta.instrument2     = NULL;
#endif
#if 0 
      vta.instrument1     = mc_instrument;
      vta.instrument2     = NULL;
#endif
      vta.needs_self_check  = needs_self_check;
      vta.preamble_function = NULL;
      vta.traceflags      = TEST_FLAGS;
      vta.addProfInc      = False;
      vta.sigill_diag     = True;

      vta.disp_cp_chain_me_to_slowEP = (void*)0x12345678;
      vta.disp_cp_chain_me_to_fastEP = (void*)0x12345679;
      vta.disp_cp_xindir             = (void*)0x1234567A;
      vta.disp_cp_xassisted          = (void*)0x1234567B;

      vta.finaltidy = NULL;

      for (i = 0; i < TEST_N_ITERS; i++)
         tres = LibVEX_Translate ( &vta );

      if (tres.status != VexTransOK)
         printf("\ntres = %d\n", (Int)tres.status);
      assert(tres.status == VexTransOK);
      assert(tres.n_sc_extents == 0);
      assert(vge.n_used == 1);
      assert((UInt)(vge.len[0]) == orig_nbytes);

      sum = 0;
      for (i = 0; i < trans_used; i++)
         sum += (UInt)transbuf[i];
      printf ( " %6.2f ... %u\n", 
               (double)trans_used / (double)vge.len[0], sum );
   }

   fclose(f);
   printf("\n");
   LibVEX_ShowAllocStats();

   return 0;
}


#if 0 

static
__attribute((noreturn))
void panic ( HChar* s )
{
  printf("\npanic: %s\n", s);
  failure_exit();
}

static
IRSB* ac_instrument (IRSB* bb_in, VexGuestLayout* layout, IRType hWordTy )
{
#define INVALID_DATA_SIZE   999999

   Int         i;
   Int         sz;
   IRCallee*   helper;
   IRStmt*    st;
   IRExpr* data;
   IRExpr* addr;
   Bool needSz;

   
   IRSB* bb     = emptyIRSB();
   bb->tyenv    = dopyIRTypeEnv(bb_in->tyenv);
   bb->next     = dopyIRExpr(bb_in->next);
   bb->jumpkind = bb_in->jumpkind;

   
   assert(isIRAtom(bb_in->next));

   for (i = 0; i <  bb_in->stmts_used; i++) {
      st = bb_in->stmts[i];
      if (!st) continue;

      switch (st->tag) {

         case Ist_Tmp:
            data = st->Ist.Tmp.data;
            if (data->tag == Iex_LDle) {
               addr = data->Iex.LDle.addr;
               sz = sizeofIRType(data->Iex.LDle.ty);
               needSz = False;
               switch (sz) {
                  case 4: helper = mkIRCallee(1, "ac_helperc_LOAD4", 
                                                 (void*)0x12345601); break;
                  case 2: helper = mkIRCallee(0, "ac_helperc_LOAD2",
                                                 (void*)0x12345602); break;
                  case 1: helper = mkIRCallee(1, "ac_helperc_LOAD1",
                                                 (void*)0x12345603); break;
                  default: helper = mkIRCallee(0, "ac_helperc_LOADN",
                                                  (void*)0x12345604);
                                                  needSz = True; break;
               }
               if (needSz) {
                  addStmtToIRSB( 
                     bb,
                     IRStmt_Dirty(
                        unsafeIRDirty_0_N( helper->regparms, 
					   helper->name, helper->addr,
                                           mkIRExprVec_2(addr, mkIRExpr_HWord(sz)))
                  ));
               } else {
                  addStmtToIRSB( 
                     bb,
                     IRStmt_Dirty(
                        unsafeIRDirty_0_N( helper->regparms, 
					   helper->name, helper->addr, 
                                           mkIRExprVec_1(addr) )
                  ));
               }
            }
            break;

         case Ist_STle:
            data = st->Ist.STle.data;
            addr = st->Ist.STle.addr;
            assert(isIRAtom(data));
            assert(isIRAtom(addr));
            sz = sizeofIRType(typeOfIRExpr(bb_in->tyenv, data));
            needSz = False;
            switch (sz) {
               case 4: helper = mkIRCallee(1, "ac_helperc_STORE4", 
                                              (void*)0x12345605); break;
               case 2: helper = mkIRCallee(0, "ac_helperc_STORE2", 
                                              (void*)0x12345606); break;
               case 1: helper = mkIRCallee(1, "ac_helperc_STORE1", 
                                              (void*)0x12345607); break;
               default: helper = mkIRCallee(0, "ac_helperc_STOREN", 
                                               (void*)0x12345608);
                                               needSz = True; break;
            }
            if (needSz) {
               addStmtToIRSB( 
                  bb,
                  IRStmt_Dirty(
                     unsafeIRDirty_0_N( helper->regparms, 
    				        helper->name, helper->addr, 
                                        mkIRExprVec_2(addr, mkIRExpr_HWord(sz)))
               ));
            } else {
               addStmtToIRSB( 
                  bb,
                  IRStmt_Dirty(
                     unsafeIRDirty_0_N( helper->regparms,
                                        helper->name, helper->addr, 
                                        mkIRExprVec_1(addr) )
               ));
            }
            break;

         case Ist_Put:
            assert(isIRAtom(st->Ist.Put.data));
            break;

         case Ist_PutI:
            assert(isIRAtom(st->Ist.PutI.ix));
            assert(isIRAtom(st->Ist.PutI.data));
            break;

         case Ist_Exit:
            assert(isIRAtom(st->Ist.Exit.guard));
            break;

         case Ist_Dirty:
            if (st->Ist.Dirty.details->mFx == Ifx_None)
               break;
            goto unhandled;

         default:
         unhandled:
            printf("\n");
            ppIRStmt(st);
            printf("\n");
            panic("addrcheck: unhandled IRStmt");
      }

      addStmtToIRSB( bb, dopyIRStmt(st));
   }

   return bb;
}
#endif 


#if 1 

static
__attribute((noreturn))
void panic ( HChar* s )
{
  printf("\npanic: %s\n", s);
  failure_exit();
}

#define tl_assert(xxx) assert(xxx)
#define VG_(xxxx) xxxx
#define tool_panic(zzz) panic(zzz)
#define MC_(zzzz) MC_##zzzz
#define TL_(zzzz) SK_##zzzz


static void MC_helperc_complain_undef ( void );
static void MC_helperc_LOADV8 ( void );
static void MC_helperc_LOADV4 ( void );
static void MC_helperc_LOADV2 ( void );
static void MC_helperc_LOADV1 ( void );
static void MC_helperc_STOREV8( void );
static void MC_helperc_STOREV4( void );
static void MC_helperc_STOREV2( void );
static void MC_helperc_STOREV1( void );
static void MC_helperc_value_check0_fail( void );
static void MC_helperc_value_check1_fail( void );
static void MC_helperc_value_check4_fail( void );

static void MC_helperc_complain_undef ( void ) { }
static void MC_helperc_LOADV8 ( void ) { }
static void MC_helperc_LOADV4 ( void ) { }
static void MC_helperc_LOADV2 ( void ) { }
static void MC_helperc_LOADV1 ( void ) { }
static void MC_helperc_STOREV8( void ) { }
static void MC_helperc_STOREV4( void ) { }
static void MC_helperc_STOREV2( void ) { }
static void MC_helperc_STOREV1( void ) { }
static void MC_helperc_value_check0_fail( void ) { }
static void MC_helperc_value_check1_fail( void ) { }
static void MC_helperc_value_check4_fail( void ) { }



/*
   This file is part of MemCheck, a heavyweight Valgrind tool for
   detecting memory errors.

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




struct _MCEnv;

static IRType  shadowType ( IRType ty );
static IRExpr* expr2vbits ( struct _MCEnv* mce, IRExpr* e );



typedef
   struct _MCEnv {
      
      IRSB* bb;

      IRTemp* tmpMap;
      Int     n_originalTmps; 

      VexGuestLayout* layout;
      IRType hWordTy;
   }
   MCEnv;


static IRTemp findShadowTmp ( MCEnv* mce, IRTemp orig )
{
   tl_assert(orig < mce->n_originalTmps);
   if (mce->tmpMap[orig] == IRTemp_INVALID) {
      mce->tmpMap[orig] 
         = newIRTemp(mce->bb->tyenv, 
                     shadowType(mce->bb->tyenv->types[orig]));
   }
   return mce->tmpMap[orig];
}

static void newShadowTmp ( MCEnv* mce, IRTemp orig )
{
   tl_assert(orig < mce->n_originalTmps);
   mce->tmpMap[orig] 
      = newIRTemp(mce->bb->tyenv, 
                  shadowType(mce->bb->tyenv->types[orig]));
}




typedef  IRExpr  IRAtom;

static Bool isOriginalAtom ( MCEnv* mce, IRAtom* a1 )
{
   if (a1->tag == Iex_Const)
      return True;
   if (a1->tag == Iex_RdTmp && a1->Iex.RdTmp.tmp < mce->n_originalTmps)
      return True;
   return False;
}

static Bool isShadowAtom ( MCEnv* mce, IRAtom* a1 )
{
   if (a1->tag == Iex_Const)
      return True;
   if (a1->tag == Iex_RdTmp && a1->Iex.RdTmp.tmp >= mce->n_originalTmps)
      return True;
   return False;
}

static Bool sameKindedAtoms ( IRAtom* a1, IRAtom* a2 )
{
   if (a1->tag == Iex_RdTmp && a1->tag == Iex_RdTmp)
      return True;
   if (a1->tag == Iex_Const && a1->tag == Iex_Const)
      return True;
   return False;
}




static IRType shadowType ( IRType ty )
{
   switch (ty) {
      case Ity_I1:
      case Ity_I8:
      case Ity_I16:
      case Ity_I32: 
      case Ity_I64:  return ty;
      case Ity_F32:  return Ity_I32;
      case Ity_F64:  return Ity_I64;
      case Ity_V128: return Ity_V128;
      default: ppIRType(ty); 
               VG_(tool_panic)("memcheck:shadowType");
   }
}

static IRExpr* definedOfType ( IRType ty ) {
   switch (ty) {
      case Ity_I1:   return IRExpr_Const(IRConst_U1(False));
      case Ity_I8:   return IRExpr_Const(IRConst_U8(0));
      case Ity_I16:  return IRExpr_Const(IRConst_U16(0));
      case Ity_I32:  return IRExpr_Const(IRConst_U32(0));
      case Ity_I64:  return IRExpr_Const(IRConst_U64(0));
      case Ity_V128: return IRExpr_Const(IRConst_V128(0x0000));
      default:      VG_(tool_panic)("memcheck:definedOfType");
   }
}



#define assign(_bb,_tmp,_expr)   \
   addStmtToIRSB((_bb), IRStmt_WrTmp((_tmp),(_expr)))

#define stmt(_bb,_stmt)    \
   addStmtToIRSB((_bb), (_stmt))

#define binop(_op, _arg1, _arg2) IRExpr_Binop((_op),(_arg1),(_arg2))
#define unop(_op, _arg)          IRExpr_Unop((_op),(_arg))
#define mkU8(_n)                 IRExpr_Const(IRConst_U8(_n))
#define mkU16(_n)                IRExpr_Const(IRConst_U16(_n))
#define mkU32(_n)                IRExpr_Const(IRConst_U32(_n))
#define mkU64(_n)                IRExpr_Const(IRConst_U64(_n))
#define mkV128(_n)               IRExpr_Const(IRConst_V128(_n))
#define mkexpr(_tmp)             IRExpr_RdTmp((_tmp))

static IRAtom* assignNew ( MCEnv* mce, IRType ty, IRExpr* e ) {
   IRTemp t = newIRTemp(mce->bb->tyenv, ty);
   assign(mce->bb, t, e);
   return mkexpr(t);
}




static IRAtom* mkDifD8 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew(mce, Ity_I8, binop(Iop_And8, a1, a2));
}

static IRAtom* mkDifD16 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew(mce, Ity_I16, binop(Iop_And16, a1, a2));
}

static IRAtom* mkDifD32 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew(mce, Ity_I32, binop(Iop_And32, a1, a2));
}

static IRAtom* mkDifD64 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew(mce, Ity_I64, binop(Iop_And64, a1, a2));
}

static IRAtom* mkDifDV128 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew(mce, Ity_V128, binop(Iop_AndV128, a1, a2));
}


static IRAtom* mkUifU8 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew(mce, Ity_I8, binop(Iop_Or8, a1, a2));
}

static IRAtom* mkUifU16 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew(mce, Ity_I16, binop(Iop_Or16, a1, a2));
}

static IRAtom* mkUifU32 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew(mce, Ity_I32, binop(Iop_Or32, a1, a2));
}

static IRAtom* mkUifU64 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew(mce, Ity_I64, binop(Iop_Or64, a1, a2));
}

static IRAtom* mkUifUV128 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew(mce, Ity_V128, binop(Iop_OrV128, a1, a2));
}

static IRAtom* mkUifU ( MCEnv* mce, IRType vty, IRAtom* a1, IRAtom* a2 ) {
   switch (vty) {
      case Ity_I8:   return mkUifU8(mce, a1, a2);
      case Ity_I16:  return mkUifU16(mce, a1, a2);
      case Ity_I32:  return mkUifU32(mce, a1, a2);
      case Ity_I64:  return mkUifU64(mce, a1, a2);
      case Ity_V128: return mkUifUV128(mce, a1, a2);
      default:
         VG_(printf)("\n"); ppIRType(vty); VG_(printf)("\n");
         VG_(tool_panic)("memcheck:mkUifU");
   }
}


static IRAtom* mkLeft8 ( MCEnv* mce, IRAtom* a1 ) {
   tl_assert(isShadowAtom(mce,a1));
   
   return assignNew(mce, Ity_I8, 
                    binop(Iop_Or8, a1, 
                          assignNew(mce, Ity_I8,
                                    
                                    binop(Iop_Sub8, mkU8(0), a1) )));
}

static IRAtom* mkLeft16 ( MCEnv* mce, IRAtom* a1 ) {
   tl_assert(isShadowAtom(mce,a1));
   
   return assignNew(mce, Ity_I16, 
                    binop(Iop_Or16, a1, 
                          assignNew(mce, Ity_I16,
                                    
                                    binop(Iop_Sub16, mkU16(0), a1) )));
}

static IRAtom* mkLeft32 ( MCEnv* mce, IRAtom* a1 ) {
   tl_assert(isShadowAtom(mce,a1));
   
   return assignNew(mce, Ity_I32, 
                    binop(Iop_Or32, a1, 
                          assignNew(mce, Ity_I32,
                                    
                                    binop(Iop_Sub32, mkU32(0), a1) )));
}


static IRAtom* mkImproveAND8 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew(mce, Ity_I8, binop(Iop_Or8, data, vbits));
}

static IRAtom* mkImproveAND16 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew(mce, Ity_I16, binop(Iop_Or16, data, vbits));
}

static IRAtom* mkImproveAND32 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew(mce, Ity_I32, binop(Iop_Or32, data, vbits));
}

static IRAtom* mkImproveAND64 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew(mce, Ity_I64, binop(Iop_Or64, data, vbits));
}

static IRAtom* mkImproveANDV128 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew(mce, Ity_V128, binop(Iop_OrV128, data, vbits));
}

static IRAtom* mkImproveOR8 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew(
             mce, Ity_I8, 
             binop(Iop_Or8, 
                   assignNew(mce, Ity_I8, unop(Iop_Not8, data)), 
                   vbits) );
}

static IRAtom* mkImproveOR16 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew(
             mce, Ity_I16, 
             binop(Iop_Or16, 
                   assignNew(mce, Ity_I16, unop(Iop_Not16, data)), 
                   vbits) );
}

static IRAtom* mkImproveOR32 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew(
             mce, Ity_I32, 
             binop(Iop_Or32, 
                   assignNew(mce, Ity_I32, unop(Iop_Not32, data)), 
                   vbits) );
}

static IRAtom* mkImproveOR64 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew(
             mce, Ity_I64, 
             binop(Iop_Or64, 
                   assignNew(mce, Ity_I64, unop(Iop_Not64, data)), 
                   vbits) );
}

static IRAtom* mkImproveORV128 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew(
             mce, Ity_V128, 
             binop(Iop_OrV128, 
                   assignNew(mce, Ity_V128, unop(Iop_NotV128, data)), 
                   vbits) );
}


static IRAtom* mkPCastTo( MCEnv* mce, IRType dst_ty, IRAtom* vbits ) 
{
   IRType  ty;
   IRAtom* tmp1;
   
   
   tl_assert(isShadowAtom(mce,vbits));
   ty   = typeOfIRExpr(mce->bb->tyenv, vbits);
   tmp1 = NULL;
   switch (ty) {
      case Ity_I1:
         tmp1 = vbits;
         break;
      case Ity_I8: 
         tmp1 = assignNew(mce, Ity_I1, binop(Iop_CmpNE8, vbits, mkU8(0)));
         break;
      case Ity_I16: 
         tmp1 = assignNew(mce, Ity_I1, binop(Iop_CmpNE16, vbits, mkU16(0)));
         break;
      case Ity_I32: 
         tmp1 = assignNew(mce, Ity_I1, binop(Iop_CmpNE32, vbits, mkU32(0)));
         break;
      case Ity_I64: 
         tmp1 = assignNew(mce, Ity_I1, binop(Iop_CmpNE64, vbits, mkU64(0)));
         break;
      default:
         VG_(tool_panic)("mkPCastTo(1)");
   }
   tl_assert(tmp1);
   
   switch (dst_ty) {
      case Ity_I1:
         return tmp1;
      case Ity_I8: 
         return assignNew(mce, Ity_I8, unop(Iop_1Sto8, tmp1));
      case Ity_I16: 
         return assignNew(mce, Ity_I16, unop(Iop_1Sto16, tmp1));
      case Ity_I32: 
         return assignNew(mce, Ity_I32, unop(Iop_1Sto32, tmp1));
      case Ity_I64: 
         return assignNew(mce, Ity_I64, unop(Iop_1Sto64, tmp1));
      case Ity_V128:
         tmp1 = assignNew(mce, Ity_I64,  unop(Iop_1Sto64, tmp1));
         tmp1 = assignNew(mce, Ity_V128, binop(Iop_64HLtoV128, tmp1, tmp1));
         return tmp1;
      default: 
         ppIRType(dst_ty);
         VG_(tool_panic)("mkPCastTo(2)");
   }
}




static void setHelperAnns ( MCEnv* mce, IRDirty* di ) {
   di->nFxState = 2;
   di->fxState[0].fx     = Ifx_Read;
   di->fxState[0].offset = mce->layout->offset_SP;
   di->fxState[0].size   = mce->layout->sizeof_SP;
   di->fxState[1].fx     = Ifx_Read;
   di->fxState[1].offset = mce->layout->offset_IP;
   di->fxState[1].size   = mce->layout->sizeof_IP;
}


static void complainIfUndefined ( MCEnv* mce, IRAtom* atom )
{
   IRAtom*  vatom;
   IRType   ty;
   Int      sz;
   IRDirty* di;
   IRAtom*  cond;

   tl_assert(isOriginalAtom(mce, atom));
   vatom = expr2vbits( mce, atom );
   tl_assert(isShadowAtom(mce, vatom));
   tl_assert(sameKindedAtoms(atom, vatom));

   ty = typeOfIRExpr(mce->bb->tyenv, vatom);

   
   sz = ty==Ity_I1 ? 0 : sizeofIRType(ty);

   cond = mkPCastTo( mce, Ity_I1, vatom );
   

   switch (sz) {
      case 0:
         di = unsafeIRDirty_0_N( 0, 
                                 "MC_(helperc_value_check0_fail)",
                                 &MC_(helperc_value_check0_fail),
                                 mkIRExprVec_0() 
                               );
         break;
      case 1:
         di = unsafeIRDirty_0_N( 0, 
                                 "MC_(helperc_value_check1_fail)",
                                 &MC_(helperc_value_check1_fail),
                                 mkIRExprVec_0() 
                               );
         break;
      case 4:
         di = unsafeIRDirty_0_N( 0, 
                                 "MC_(helperc_value_check4_fail)",
                                 &MC_(helperc_value_check4_fail),
                                 mkIRExprVec_0() 
                               );
         break;
      default:
         di = unsafeIRDirty_0_N( 1, 
                                 "MC_(helperc_complain_undef)",
                                 &MC_(helperc_complain_undef),
                                 mkIRExprVec_1( mkIRExpr_HWord( sz ))
                               );
         break;
   }
   di->guard = cond;
   setHelperAnns( mce, di );
   stmt( mce->bb, IRStmt_Dirty(di));

   tl_assert(isIRAtom(vatom));
   
   if (vatom->tag == Iex_RdTmp) {
      tl_assert(atom->tag == Iex_RdTmp);
      newShadowTmp(mce, atom->Iex.RdTmp.tmp);
      assign(mce->bb, findShadowTmp(mce, atom->Iex.RdTmp.tmp), 
                      definedOfType(ty));
   }
}



static Bool isAlwaysDefd ( MCEnv* mce, Int offset, Int size )
{
   Int minoffD, maxoffD, i;
   Int minoff = offset;
   Int maxoff = minoff + size - 1;
   tl_assert((minoff & ~0xFFFF) == 0);
   tl_assert((maxoff & ~0xFFFF) == 0);

   for (i = 0; i < mce->layout->n_alwaysDefd; i++) {
      minoffD = mce->layout->alwaysDefd[i].offset;
      maxoffD = minoffD + mce->layout->alwaysDefd[i].size - 1;
      tl_assert((minoffD & ~0xFFFF) == 0);
      tl_assert((maxoffD & ~0xFFFF) == 0);

      if (maxoff < minoffD || maxoffD < minoff)
         continue; 
      if (minoff >= minoffD && maxoff <= maxoffD)
         return True; 

      VG_(tool_panic)("memcheck:isAlwaysDefd:partial overlap");
   }
   return False; 
}


static
void do_shadow_PUT ( MCEnv* mce,  Int offset, 
                     IRAtom* atom, IRAtom* vatom )
{
   IRType ty;
   if (atom) {
      tl_assert(!vatom);
      tl_assert(isOriginalAtom(mce, atom));
      vatom = expr2vbits( mce, atom );
   } else {
      tl_assert(vatom);
      tl_assert(isShadowAtom(mce, vatom));
   }

   ty = typeOfIRExpr(mce->bb->tyenv, vatom);
   tl_assert(ty != Ity_I1);
   if (isAlwaysDefd(mce, offset, sizeofIRType(ty))) {
      
      
      
   } else {
      
      stmt( mce->bb, IRStmt_Put( offset + mce->layout->total_sizeB, vatom ) );
   }
}


static
void do_shadow_PUTI ( MCEnv* mce, 
                      IRRegArray* descr, IRAtom* ix, Int bias, IRAtom* atom )
{
   IRAtom* vatom;
   IRType  ty, tyS;
   Int     arrSize;;

   tl_assert(isOriginalAtom(mce,atom));
   vatom = expr2vbits( mce, atom );
   tl_assert(sameKindedAtoms(atom, vatom));
   ty   = descr->elemTy;
   tyS  = shadowType(ty);
   arrSize = descr->nElems * sizeofIRType(ty);
   tl_assert(ty != Ity_I1);
   tl_assert(isOriginalAtom(mce,ix));
   complainIfUndefined(mce,ix);
   if (isAlwaysDefd(mce, descr->base, arrSize)) {
      
      
      
   } else {
      IRRegArray* new_descr 
         = mkIRRegArray( descr->base + mce->layout->total_sizeB, 
                      tyS, descr->nElems);
      stmt( mce->bb, IRStmt_PutI( mkIRPutI( new_descr, ix, bias, vatom ) ));
   }
}


static 
IRExpr* shadow_GET ( MCEnv* mce, Int offset, IRType ty )
{
   IRType tyS = shadowType(ty);
   tl_assert(ty != Ity_I1);
   if (isAlwaysDefd(mce, offset, sizeofIRType(ty))) {
      
      return definedOfType(tyS);
   } else {
      return IRExpr_Get( offset + mce->layout->total_sizeB, tyS );
   }
}


static
IRExpr* shadow_GETI ( MCEnv* mce, IRRegArray* descr, IRAtom* ix, Int bias )
{
   IRType ty   = descr->elemTy;
   IRType tyS  = shadowType(ty);
   Int arrSize = descr->nElems * sizeofIRType(ty);
   tl_assert(ty != Ity_I1);
   tl_assert(isOriginalAtom(mce,ix));
   complainIfUndefined(mce,ix);
   if (isAlwaysDefd(mce, descr->base, arrSize)) {
      
      return definedOfType(tyS);
   } else {
      IRRegArray* new_descr 
         = mkIRRegArray( descr->base + mce->layout->total_sizeB, 
                      tyS, descr->nElems);
      return IRExpr_GetI( new_descr, ix, bias );
   }
}



static
IRAtom* mkLazy2 ( MCEnv* mce, IRType finalVty, IRAtom* va1, IRAtom* va2 )
{
   
   IRAtom* at;
   tl_assert(isShadowAtom(mce,va1));
   tl_assert(isShadowAtom(mce,va2));
   at = mkPCastTo(mce, Ity_I32, va1);
   at = mkUifU(mce, Ity_I32, at, mkPCastTo(mce, Ity_I32, va2));
   at = mkPCastTo(mce, finalVty, at);
   return at;
}


static
IRAtom* mkLazyN ( MCEnv* mce, 
                  IRAtom** exprvec, IRType finalVtype, IRCallee* cee )
{
   Int i;
   IRAtom* here;
   IRAtom* curr = definedOfType(Ity_I32);
   for (i = 0; exprvec[i]; i++) {
      tl_assert(i < 32);
      tl_assert(isOriginalAtom(mce, exprvec[i]));
      if (cee->mcx_mask & (1<<i)) {
         if (0) VG_(printf)("excluding %s(%d)\n", cee->name, i);
      } else {
         here = mkPCastTo( mce, Ity_I32, expr2vbits(mce, exprvec[i]) );
         curr = mkUifU32(mce, here, curr);
      }
   }
   return mkPCastTo(mce, finalVtype, curr );
}



static
__attribute__((unused))
IRAtom* expensiveAdd32 ( MCEnv* mce, IRAtom* qaa, IRAtom* qbb, 
                                     IRAtom* aa,  IRAtom* bb )
{
   IRAtom *a_min, *b_min, *a_max, *b_max;
   IRType ty;
   IROp   opAND, opOR, opXOR, opNOT, opADD;

   tl_assert(isShadowAtom(mce,qaa));
   tl_assert(isShadowAtom(mce,qbb));
   tl_assert(isOriginalAtom(mce,aa));
   tl_assert(isOriginalAtom(mce,bb));
   tl_assert(sameKindedAtoms(qaa,aa));
   tl_assert(sameKindedAtoms(qbb,bb));

   ty    = Ity_I32;
   opAND = Iop_And32;
   opOR  = Iop_Or32;
   opXOR = Iop_Xor32;
   opNOT = Iop_Not32;
   opADD = Iop_Add32;

   
   a_min = assignNew(mce,ty, 
                     binop(opAND, aa,
                                  assignNew(mce,ty, unop(opNOT, qaa))));

   
   b_min = assignNew(mce,ty, 
                     binop(opAND, bb,
                                  assignNew(mce,ty, unop(opNOT, qbb))));

   
   a_max = assignNew(mce,ty, binop(opOR, aa, qaa));

   
   b_max = assignNew(mce,ty, binop(opOR, bb, qbb));

   
   return
   assignNew(mce,ty,
      binop( opOR,
             assignNew(mce,ty, binop(opOR, qaa, qbb)),
             assignNew(mce,ty, 
                binop(opXOR, assignNew(mce,ty, binop(opADD, a_min, b_min)),
                             assignNew(mce,ty, binop(opADD, a_max, b_max))
                )
             )
      )
   );
}




static IRAtom* mkPCast8x16 ( MCEnv* mce, IRAtom* at )
{
   return assignNew(mce, Ity_V128, unop(Iop_CmpNEZ8x16, at));
}

static IRAtom* mkPCast16x8 ( MCEnv* mce, IRAtom* at )
{
   return assignNew(mce, Ity_V128, unop(Iop_CmpNEZ16x8, at));
}

static IRAtom* mkPCast32x4 ( MCEnv* mce, IRAtom* at )
{
   return assignNew(mce, Ity_V128, unop(Iop_CmpNEZ32x4, at));
}

static IRAtom* mkPCast64x2 ( MCEnv* mce, IRAtom* at )
{
   return assignNew(mce, Ity_V128, unop(Iop_CmpNEZ64x2, at));
}




static
IRAtom* binary32Fx4 ( MCEnv* mce, IRAtom* vatomX, IRAtom* vatomY )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   tl_assert(isShadowAtom(mce, vatomY));
   at = mkUifUV128(mce, vatomX, vatomY);
   at = assignNew(mce, Ity_V128, mkPCast32x4(mce, at));
   return at;
}

static
IRAtom* unary32Fx4 ( MCEnv* mce, IRAtom* vatomX )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   at = assignNew(mce, Ity_V128, mkPCast32x4(mce, vatomX));
   return at;
}

static
IRAtom* binary32F0x4 ( MCEnv* mce, IRAtom* vatomX, IRAtom* vatomY )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   tl_assert(isShadowAtom(mce, vatomY));
   at = mkUifUV128(mce, vatomX, vatomY);
   at = assignNew(mce, Ity_I32, unop(Iop_V128to32, at));
   at = mkPCastTo(mce, Ity_I32, at);
   at = assignNew(mce, Ity_V128, binop(Iop_SetV128lo32, vatomX, at));
   return at;
}

static
IRAtom* unary32F0x4 ( MCEnv* mce, IRAtom* vatomX )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   at = assignNew(mce, Ity_I32, unop(Iop_V128to32, vatomX));
   at = mkPCastTo(mce, Ity_I32, at);
   at = assignNew(mce, Ity_V128, binop(Iop_SetV128lo32, vatomX, at));
   return at;
}


static
IRAtom* binary64Fx2 ( MCEnv* mce, IRAtom* vatomX, IRAtom* vatomY )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   tl_assert(isShadowAtom(mce, vatomY));
   at = mkUifUV128(mce, vatomX, vatomY);
   at = assignNew(mce, Ity_V128, mkPCast64x2(mce, at));
   return at;
}

static
IRAtom* unary64Fx2 ( MCEnv* mce, IRAtom* vatomX )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   at = assignNew(mce, Ity_V128, mkPCast64x2(mce, vatomX));
   return at;
}

static
IRAtom* binary64F0x2 ( MCEnv* mce, IRAtom* vatomX, IRAtom* vatomY )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   tl_assert(isShadowAtom(mce, vatomY));
   at = mkUifUV128(mce, vatomX, vatomY);
   at = assignNew(mce, Ity_I64, unop(Iop_V128to64, at));
   at = mkPCastTo(mce, Ity_I64, at);
   at = assignNew(mce, Ity_V128, binop(Iop_SetV128lo64, vatomX, at));
   return at;
}

static
IRAtom* unary64F0x2 ( MCEnv* mce, IRAtom* vatomX )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   at = assignNew(mce, Ity_I64, unop(Iop_V128to64, vatomX));
   at = mkPCastTo(mce, Ity_I64, at);
   at = assignNew(mce, Ity_V128, binop(Iop_SetV128lo64, vatomX, at));
   return at;
}


static
IRAtom* vectorNarrowV128 ( MCEnv* mce, IROp narrow_op, 
                          IRAtom* vatom1, IRAtom* vatom2)
{
   IRAtom *at1, *at2, *at3;
   IRAtom* (*pcast)( MCEnv*, IRAtom* );
   switch (narrow_op) {
      case Iop_QNarrowBin32Sto16Sx8: pcast = mkPCast32x4; break;
      case Iop_QNarrowBin16Sto8Sx16: pcast = mkPCast16x8; break;
      case Iop_QNarrowBin16Sto8Ux16: pcast = mkPCast16x8; break;
      default: VG_(tool_panic)("vectorNarrowV128");
   }
   tl_assert(isShadowAtom(mce,vatom1));
   tl_assert(isShadowAtom(mce,vatom2));
   at1 = assignNew(mce, Ity_V128, pcast(mce, vatom1));
   at2 = assignNew(mce, Ity_V128, pcast(mce, vatom2));
   at3 = assignNew(mce, Ity_V128, binop(narrow_op, at1, at2));
   return at3;
}



static
IRAtom* binary8Ix16 ( MCEnv* mce, IRAtom* vatom1, IRAtom* vatom2 )
{
   IRAtom* at;
   at = mkUifUV128(mce, vatom1, vatom2);
   at = mkPCast8x16(mce, at);
   return at;   
}

static
IRAtom* binary16Ix8 ( MCEnv* mce, IRAtom* vatom1, IRAtom* vatom2 )
{
   IRAtom* at;
   at = mkUifUV128(mce, vatom1, vatom2);
   at = mkPCast16x8(mce, at);
   return at;   
}

static
IRAtom* binary32Ix4 ( MCEnv* mce, IRAtom* vatom1, IRAtom* vatom2 )
{
   IRAtom* at;
   at = mkUifUV128(mce, vatom1, vatom2);
   at = mkPCast32x4(mce, at);
   return at;   
}

static
IRAtom* binary64Ix2 ( MCEnv* mce, IRAtom* vatom1, IRAtom* vatom2 )
{
   IRAtom* at;
   at = mkUifUV128(mce, vatom1, vatom2);
   at = mkPCast64x2(mce, at);
   return at;   
}



static 
IRAtom* expr2vbits_Binop ( MCEnv* mce,
                           IROp op,
                           IRAtom* atom1, IRAtom* atom2 )
{
   IRType  and_or_ty;
   IRAtom* (*uifu)    (MCEnv*, IRAtom*, IRAtom*);
   IRAtom* (*difd)    (MCEnv*, IRAtom*, IRAtom*);
   IRAtom* (*improve) (MCEnv*, IRAtom*, IRAtom*);

   IRAtom* vatom1 = expr2vbits( mce, atom1 );
   IRAtom* vatom2 = expr2vbits( mce, atom2 );

   tl_assert(isOriginalAtom(mce,atom1));
   tl_assert(isOriginalAtom(mce,atom2));
   tl_assert(isShadowAtom(mce,vatom1));
   tl_assert(isShadowAtom(mce,vatom2));
   tl_assert(sameKindedAtoms(atom1,vatom1));
   tl_assert(sameKindedAtoms(atom2,vatom2));
   switch (op) {

      

      case Iop_ShrN16x8:
      case Iop_ShrN32x4:
      case Iop_ShrN64x2:
      case Iop_SarN16x8:
      case Iop_SarN32x4:
      case Iop_ShlN16x8:
      case Iop_ShlN32x4:
      case Iop_ShlN64x2:
         
         complainIfUndefined(mce, atom2);
         return assignNew(mce, Ity_V128, binop(op, vatom1, atom2));

      case Iop_QSub8Ux16:
      case Iop_QSub8Sx16:
      case Iop_Sub8x16:
      case Iop_Min8Ux16:
      case Iop_Max8Ux16:
      case Iop_CmpGT8Sx16:
      case Iop_CmpEQ8x16:
      case Iop_Avg8Ux16:
      case Iop_QAdd8Ux16:
      case Iop_QAdd8Sx16:
      case Iop_Add8x16:
         return binary8Ix16(mce, vatom1, vatom2);

      case Iop_QSub16Ux8:
      case Iop_QSub16Sx8:
      case Iop_Sub16x8:
      case Iop_Mul16x8:
      case Iop_MulHi16Sx8:
      case Iop_MulHi16Ux8:
      case Iop_Min16Sx8:
      case Iop_Max16Sx8:
      case Iop_CmpGT16Sx8:
      case Iop_CmpEQ16x8:
      case Iop_Avg16Ux8:
      case Iop_QAdd16Ux8:
      case Iop_QAdd16Sx8:
      case Iop_Add16x8:
         return binary16Ix8(mce, vatom1, vatom2);

      case Iop_Sub32x4:
      case Iop_QSub32Sx4:
      case Iop_QSub32Ux4:
      case Iop_CmpGT32Sx4:
      case Iop_CmpEQ32x4:
      case Iop_Add32x4:
      case Iop_QAdd32Ux4:
      case Iop_QAdd32Sx4:
         return binary32Ix4(mce, vatom1, vatom2);

      case Iop_Sub64x2:
      case Iop_QSub64Ux2:
      case Iop_QSub64Sx2:
      case Iop_Add64x2:
      case Iop_QAdd64Ux2:
      case Iop_QAdd64Sx2:
         return binary64Ix2(mce, vatom1, vatom2);

      case Iop_QNarrowBin32Sto16Sx8:
      case Iop_QNarrowBin16Sto8Sx16:
      case Iop_QNarrowBin16Sto8Ux16:
         return vectorNarrowV128(mce, op, vatom1, vatom2);

      case Iop_Sub64Fx2:
      case Iop_Mul64Fx2:
      case Iop_Min64Fx2:
      case Iop_Max64Fx2:
      case Iop_Div64Fx2:
      case Iop_CmpLT64Fx2:
      case Iop_CmpLE64Fx2:
      case Iop_CmpEQ64Fx2:
      case Iop_Add64Fx2:
         return binary64Fx2(mce, vatom1, vatom2);      

      case Iop_Sub64F0x2:
      case Iop_Mul64F0x2:
      case Iop_Min64F0x2:
      case Iop_Max64F0x2:
      case Iop_Div64F0x2:
      case Iop_CmpLT64F0x2:
      case Iop_CmpLE64F0x2:
      case Iop_CmpEQ64F0x2:
      case Iop_Add64F0x2:
         return binary64F0x2(mce, vatom1, vatom2);      

      

      case Iop_Sub32Fx4:
      case Iop_Mul32Fx4:
      case Iop_Min32Fx4:
      case Iop_Max32Fx4:
      case Iop_Div32Fx4:
      case Iop_CmpLT32Fx4:
      case Iop_CmpLE32Fx4:
      case Iop_CmpEQ32Fx4:
      case Iop_Add32Fx4:
         return binary32Fx4(mce, vatom1, vatom2);      

      case Iop_Sub32F0x4:
      case Iop_Mul32F0x4:
      case Iop_Min32F0x4:
      case Iop_Max32F0x4:
      case Iop_Div32F0x4:
      case Iop_CmpLT32F0x4:
      case Iop_CmpLE32F0x4:
      case Iop_CmpEQ32F0x4:
      case Iop_Add32F0x4:
         return binary32F0x4(mce, vatom1, vatom2);      

      
      case Iop_SetV128lo32:
      case Iop_SetV128lo64:
      case Iop_64HLtoV128:
      case Iop_InterleaveLO64x2:
      case Iop_InterleaveLO32x4:
      case Iop_InterleaveLO16x8:
      case Iop_InterleaveLO8x16:
      case Iop_InterleaveHI64x2:
      case Iop_InterleaveHI32x4:
      case Iop_InterleaveHI16x8:
      case Iop_InterleaveHI8x16:
         return assignNew(mce, Ity_V128, binop(op, vatom1, vatom2));

      

         
      case Iop_F64toI64S:
      case Iop_I64StoF64:
         return mkLazy2(mce, Ity_I64, vatom1, vatom2);

      case Iop_PRemC3210F64: case Iop_PRem1C3210F64:
         
      case Iop_F64toI32S:
      case Iop_F64toF32:
         
         return mkLazy2(mce, Ity_I32, vatom1, vatom2);

      case Iop_F64toI16S:
         
         return mkLazy2(mce, Ity_I16, vatom1, vatom2);

      case Iop_ScaleF64:
      case Iop_Yl2xF64:
      case Iop_Yl2xp1F64:
      case Iop_PRemF64:
      case Iop_AtanF64:
      case Iop_AddF64:
      case Iop_DivF64:
      case Iop_SubF64:
      case Iop_MulF64:
         return mkLazy2(mce, Ity_I64, vatom1, vatom2);

      case Iop_CmpF64:
         return mkLazy2(mce, Ity_I32, vatom1, vatom2);

      

      case Iop_DivModU64to32:
      case Iop_DivModS64to32:
         return mkLazy2(mce, Ity_I64, vatom1, vatom2);

      case Iop_16HLto32:
         return assignNew(mce, Ity_I32, binop(op, vatom1, vatom2));
      case Iop_32HLto64:
         return assignNew(mce, Ity_I64, binop(op, vatom1, vatom2));

      case Iop_MullS32:
      case Iop_MullU32: {
         IRAtom* vLo32 = mkLeft32(mce, mkUifU32(mce, vatom1,vatom2));
         IRAtom* vHi32 = mkPCastTo(mce, Ity_I32, vLo32);
         return assignNew(mce, Ity_I64, binop(Iop_32HLto64, vHi32, vLo32));
      }

      case Iop_MullS16:
      case Iop_MullU16: {
         IRAtom* vLo16 = mkLeft16(mce, mkUifU16(mce, vatom1,vatom2));
         IRAtom* vHi16 = mkPCastTo(mce, Ity_I16, vLo16);
         return assignNew(mce, Ity_I32, binop(Iop_16HLto32, vHi16, vLo16));
      }

      case Iop_MullS8:
      case Iop_MullU8: {
         IRAtom* vLo8 = mkLeft8(mce, mkUifU8(mce, vatom1,vatom2));
         IRAtom* vHi8 = mkPCastTo(mce, Ity_I8, vLo8);
         return assignNew(mce, Ity_I16, binop(Iop_8HLto16, vHi8, vLo8));
      }

      case Iop_Add32:
#        if 0
         return expensiveAdd32(mce, vatom1,vatom2, atom1,atom2);
#        endif
      case Iop_Sub32:
      case Iop_Mul32:
         return mkLeft32(mce, mkUifU32(mce, vatom1,vatom2));

      case Iop_Mul16:
      case Iop_Add16:
      case Iop_Sub16:
         return mkLeft16(mce, mkUifU16(mce, vatom1,vatom2));

      case Iop_Sub8:
      case Iop_Add8:
         return mkLeft8(mce, mkUifU8(mce, vatom1,vatom2));

      case Iop_CmpLE32S: case Iop_CmpLE32U: 
      case Iop_CmpLT32U: case Iop_CmpLT32S:
      case Iop_CmpEQ32: case Iop_CmpNE32:
         return mkPCastTo(mce, Ity_I1, mkUifU32(mce, vatom1,vatom2));

      case Iop_CmpEQ16: case Iop_CmpNE16:
         return mkPCastTo(mce, Ity_I1, mkUifU16(mce, vatom1,vatom2));

      case Iop_CmpEQ8: case Iop_CmpNE8:
         return mkPCastTo(mce, Ity_I1, mkUifU8(mce, vatom1,vatom2));

      case Iop_Shl32: case Iop_Shr32: case Iop_Sar32:
         complainIfUndefined(mce, atom2);
         return assignNew(mce, Ity_I32, binop(op, vatom1, atom2));

      case Iop_Shl16: case Iop_Shr16: case Iop_Sar16:
         
         complainIfUndefined(mce, atom2);
         return assignNew(mce, Ity_I16, binop(op, vatom1, atom2));

      case Iop_Shl8: case Iop_Shr8:
         
         complainIfUndefined(mce, atom2);
         return assignNew(mce, Ity_I8, binop(op, vatom1, atom2));

      case Iop_Shl64: case Iop_Shr64: 
         
         complainIfUndefined(mce, atom2);
         return assignNew(mce, Ity_I64, binop(op, vatom1, atom2));

      case Iop_AndV128:
         uifu = mkUifUV128; difd = mkDifDV128; 
         and_or_ty = Ity_V128; improve = mkImproveANDV128; goto do_And_Or;
      case Iop_And64:
         uifu = mkUifU64; difd = mkDifD64; 
         and_or_ty = Ity_I64; improve = mkImproveAND64; goto do_And_Or;
      case Iop_And32:
         uifu = mkUifU32; difd = mkDifD32; 
         and_or_ty = Ity_I32; improve = mkImproveAND32; goto do_And_Or;
      case Iop_And16:
         uifu = mkUifU16; difd = mkDifD16; 
         and_or_ty = Ity_I16; improve = mkImproveAND16; goto do_And_Or;
      case Iop_And8:
         uifu = mkUifU8; difd = mkDifD8; 
         and_or_ty = Ity_I8; improve = mkImproveAND8; goto do_And_Or;

      case Iop_OrV128:
         uifu = mkUifUV128; difd = mkDifDV128; 
         and_or_ty = Ity_V128; improve = mkImproveORV128; goto do_And_Or;
      case Iop_Or64:
         uifu = mkUifU64; difd = mkDifD64; 
         and_or_ty = Ity_I64; improve = mkImproveOR64; goto do_And_Or;
      case Iop_Or32:
         uifu = mkUifU32; difd = mkDifD32; 
         and_or_ty = Ity_I32; improve = mkImproveOR32; goto do_And_Or;
      case Iop_Or16:
         uifu = mkUifU16; difd = mkDifD16; 
         and_or_ty = Ity_I16; improve = mkImproveOR16; goto do_And_Or;
      case Iop_Or8:
         uifu = mkUifU8; difd = mkDifD8; 
         and_or_ty = Ity_I8; improve = mkImproveOR8; goto do_And_Or;

      do_And_Or:
         return
         assignNew(
            mce, 
            and_or_ty,
            difd(mce, uifu(mce, vatom1, vatom2),
                      difd(mce, improve(mce, atom1, vatom1),
                                improve(mce, atom2, vatom2) ) ) );

      case Iop_Xor8:
         return mkUifU8(mce, vatom1, vatom2);
      case Iop_Xor16:
         return mkUifU16(mce, vatom1, vatom2);
      case Iop_Xor32:
         return mkUifU32(mce, vatom1, vatom2);
      case Iop_Xor64:
         return mkUifU64(mce, vatom1, vatom2);
      case Iop_XorV128:
         return mkUifUV128(mce, vatom1, vatom2);

      default:
         ppIROp(op);
         VG_(tool_panic)("memcheck:expr2vbits_Binop");
   }
}


static 
IRExpr* expr2vbits_Unop ( MCEnv* mce, IROp op, IRAtom* atom )
{
   IRAtom* vatom = expr2vbits( mce, atom );
   tl_assert(isOriginalAtom(mce,atom));
   switch (op) {

      case Iop_Sqrt64Fx2:
         return unary64Fx2(mce, vatom);

      case Iop_Sqrt64F0x2:
         return unary64F0x2(mce, vatom);

      case Iop_Sqrt32Fx4:
      case Iop_RecipEst32Fx4:
         return unary32Fx4(mce, vatom);

      case Iop_Sqrt32F0x4:
      case Iop_RSqrtEst32F0x4:
      case Iop_RecipEst32F0x4:
         return unary32F0x4(mce, vatom);

      case Iop_32UtoV128:
      case Iop_64UtoV128:
         return assignNew(mce, Ity_V128, unop(op, vatom));

      case Iop_F32toF64: 
      case Iop_I32StoF64:
      case Iop_NegF64:
      case Iop_SinF64:
      case Iop_CosF64:
      case Iop_TanF64:
      case Iop_SqrtF64:
      case Iop_AbsF64:
      case Iop_2xm1F64:
         return mkPCastTo(mce, Ity_I64, vatom);

      case Iop_Clz32:
      case Iop_Ctz32:
         return mkPCastTo(mce, Ity_I32, vatom);

      case Iop_32Sto64:
      case Iop_32Uto64:
      case Iop_V128to64:
      case Iop_V128HIto64:
         return assignNew(mce, Ity_I64, unop(op, vatom));

      case Iop_64to32:
      case Iop_64HIto32:
      case Iop_1Uto32:
      case Iop_8Uto32:
      case Iop_16Uto32:
      case Iop_16Sto32:
      case Iop_8Sto32:
         return assignNew(mce, Ity_I32, unop(op, vatom));

      case Iop_8Sto16:
      case Iop_8Uto16:
      case Iop_32to16:
      case Iop_32HIto16:
         return assignNew(mce, Ity_I16, unop(op, vatom));

      case Iop_1Uto8:
      case Iop_16to8:
      case Iop_32to8:
         return assignNew(mce, Ity_I8, unop(op, vatom));

      case Iop_32to1:
         return assignNew(mce, Ity_I1, unop(Iop_32to1, vatom));

      case Iop_ReinterpF64asI64:
      case Iop_ReinterpI64asF64:
      case Iop_ReinterpI32asF32:
      case Iop_NotV128:
      case Iop_Not64:
      case Iop_Not32:
      case Iop_Not16:
      case Iop_Not8:
      case Iop_Not1:
         return vatom;

      default:
         ppIROp(op);
         VG_(tool_panic)("memcheck:expr2vbits_Unop");
   }
}


static
IRAtom* expr2vbits_LDle_WRK ( MCEnv* mce, IRType ty, IRAtom* addr, UInt bias )
{
   void*    helper;
   HChar*   hname;
   IRDirty* di;
   IRTemp   datavbits;
   IRAtom*  addrAct;

   tl_assert(isOriginalAtom(mce,addr));

   complainIfUndefined( mce, addr );

   ty = shadowType(ty);
   switch (ty) {
      case Ity_I64: helper = &MC_(helperc_LOADV8);
                    hname = "MC_(helperc_LOADV8)";
                    break;
      case Ity_I32: helper = &MC_(helperc_LOADV4);
                    hname = "MC_(helperc_LOADV4)";
                    break;
      case Ity_I16: helper = &MC_(helperc_LOADV2);
                    hname = "MC_(helperc_LOADV2)";
                    break;
      case Ity_I8:  helper = &MC_(helperc_LOADV1);
                    hname = "MC_(helperc_LOADV1)";
                    break;
      default:      ppIRType(ty);
                    VG_(tool_panic)("memcheck:do_shadow_LDle");
   }

   
   if (bias == 0) {
      addrAct = addr;
   } else {
      IROp    mkAdd;
      IRAtom* eBias;
      IRType  tyAddr  = mce->hWordTy;
      tl_assert( tyAddr == Ity_I32 || tyAddr == Ity_I64 );
      mkAdd   = tyAddr==Ity_I32 ? Iop_Add32 : Iop_Add64;
      eBias   = tyAddr==Ity_I32 ? mkU32(bias) : mkU64(bias);
      addrAct = assignNew(mce, tyAddr, binop(mkAdd, addr, eBias) );
   }

   datavbits = newIRTemp(mce->bb->tyenv, ty);
   di = unsafeIRDirty_1_N( datavbits, 
                           1, hname, helper, 
                           mkIRExprVec_1( addrAct ));
   setHelperAnns( mce, di );
   stmt( mce->bb, IRStmt_Dirty(di) );

   return mkexpr(datavbits);
}


static
IRAtom* expr2vbits_LDle ( MCEnv* mce, IRType ty, IRAtom* addr, UInt bias )
{
   IRAtom *v64hi, *v64lo;
   switch (shadowType(ty)) {
      case Ity_I8: 
      case Ity_I16: 
      case Ity_I32: 
      case Ity_I64:
         return expr2vbits_LDle_WRK(mce, ty, addr, bias);
      case Ity_V128:
         v64lo = expr2vbits_LDle_WRK(mce, Ity_I64, addr, bias);
         v64hi = expr2vbits_LDle_WRK(mce, Ity_I64, addr, bias+8);
         return assignNew( mce, 
                           Ity_V128, 
                           binop(Iop_64HLtoV128, v64hi, v64lo));
      default:
         VG_(tool_panic)("expr2vbits_LDle");
   }
}


static
IRAtom* expr2vbits_ITE ( MCEnv* mce, 
                         IRAtom* cond, IRAtom* iftrue, IRAtom* iffalse )
{
   IRAtom *vbitsC, *vbits0, *vbits1;
   IRType ty;
   tl_assert(isOriginalAtom(mce, cond));
   tl_assert(isOriginalAtom(mce, iftrue));
   tl_assert(isOriginalAtom(mce, iffalse));

   vbitsC = expr2vbits(mce, cond);
   vbits0 = expr2vbits(mce, iffalse);
   vbits1 = expr2vbits(mce, iftrue);
   ty = typeOfIRExpr(mce->bb->tyenv, vbits0);

   return
      mkUifU(mce, ty, assignNew(mce, ty, IRExpr_ITE(cond, vbits1, vbits0)),
                      mkPCastTo(mce, ty, vbitsC) );
}      


static
IRExpr* expr2vbits ( MCEnv* mce, IRExpr* e )
{
   switch (e->tag) {

      case Iex_Get:
         return shadow_GET( mce, e->Iex.Get.offset, e->Iex.Get.ty );

      case Iex_GetI:
         return shadow_GETI( mce, e->Iex.GetI.descr, 
                                  e->Iex.GetI.ix, e->Iex.GetI.bias );

      case Iex_RdTmp:
         return IRExpr_RdTmp( findShadowTmp(mce, e->Iex.RdTmp.tmp) );

      case Iex_Const:
         return definedOfType(shadowType(typeOfIRExpr(mce->bb->tyenv, e)));

      case Iex_Binop:
         return expr2vbits_Binop(
                   mce,
                   e->Iex.Binop.op,
                   e->Iex.Binop.arg1, e->Iex.Binop.arg2
                );

      case Iex_Unop:
         return expr2vbits_Unop( mce, e->Iex.Unop.op, e->Iex.Unop.arg );

      case Iex_Load:
         return expr2vbits_LDle( mce, e->Iex.Load.ty, 
                                      e->Iex.Load.addr, 0 );

      case Iex_CCall:
         return mkLazyN( mce, e->Iex.CCall.args, 
                              e->Iex.CCall.retty,
                              e->Iex.CCall.cee );

      case Iex_ITE:
         return expr2vbits_ITE( mce, e->Iex.ITE.cond, e->Iex.ITE.iftrue, 
                                e->Iex.ITE.iffalse);

      default: 
         VG_(printf)("\n");
         ppIRExpr(e);
         VG_(printf)("\n");
         VG_(tool_panic)("memcheck: expr2vbits");
   }
}



static
IRExpr* zwidenToHostWord ( MCEnv* mce, IRAtom* vatom )
{
   IRType ty, tyH;

   
   tl_assert(isShadowAtom(mce,vatom));

   ty  = typeOfIRExpr(mce->bb->tyenv, vatom);
   tyH = mce->hWordTy;

   if (tyH == Ity_I32) {
      switch (ty) {
         case Ity_I32: return vatom;
         case Ity_I16: return assignNew(mce, tyH, unop(Iop_16Uto32, vatom));
         case Ity_I8:  return assignNew(mce, tyH, unop(Iop_8Uto32, vatom));
         default:      goto unhandled;
      }
   } else {
      goto unhandled;
   }
  unhandled:
   VG_(printf)("\nty = "); ppIRType(ty); VG_(printf)("\n");
   VG_(tool_panic)("zwidenToHostWord");
}



static 
void do_shadow_STle ( MCEnv* mce, 
                      IRAtom* addr, UInt bias,
                      IRAtom* data, IRAtom* vdata )
{
   IROp     mkAdd;
   IRType   ty, tyAddr;
   IRDirty  *di, *diLo64, *diHi64;
   IRAtom   *addrAct, *addrLo64, *addrHi64;
   IRAtom   *vdataLo64, *vdataHi64;
   IRAtom   *eBias, *eBias0, *eBias8;
   void*    helper = NULL;
   HChar*   hname = NULL;

   tyAddr = mce->hWordTy;
   mkAdd  = tyAddr==Ity_I32 ? Iop_Add32 : Iop_Add64;
   tl_assert( tyAddr == Ity_I32 || tyAddr == Ity_I64 );

   di = diLo64 = diHi64 = NULL;
   eBias = eBias0 = eBias8 = NULL;
   addrAct = addrLo64 = addrHi64 = NULL;
   vdataLo64 = vdataHi64 = NULL;

   if (data) {
      tl_assert(!vdata);
      tl_assert(isOriginalAtom(mce, data));
      tl_assert(bias == 0);
      vdata = expr2vbits( mce, data );
   } else {
      tl_assert(vdata);
   }

   tl_assert(isOriginalAtom(mce,addr));
   tl_assert(isShadowAtom(mce,vdata));

   ty = typeOfIRExpr(mce->bb->tyenv, vdata);

   complainIfUndefined( mce, addr );

   switch (ty) {
      case Ity_V128: 
      case Ity_I64: helper = &MC_(helperc_STOREV8);
                    hname = "MC_(helperc_STOREV8)";
                    break;
      case Ity_I32: helper = &MC_(helperc_STOREV4);
                    hname = "MC_(helperc_STOREV4)";
                    break;
      case Ity_I16: helper = &MC_(helperc_STOREV2);
                    hname = "MC_(helperc_STOREV2)";
                    break;
      case Ity_I8:  helper = &MC_(helperc_STOREV1);
                    hname = "MC_(helperc_STOREV1)";
                    break;
      default:      VG_(tool_panic)("memcheck:do_shadow_STle");
   }

   if (ty == Ity_V128) {

      
      
      eBias0    = tyAddr==Ity_I32 ? mkU32(bias)   : mkU64(bias);
      addrLo64  = assignNew(mce, tyAddr, binop(mkAdd, addr, eBias0) );
      vdataLo64 = assignNew(mce, Ity_I64, unop(Iop_V128to64, vdata));
      diLo64    = unsafeIRDirty_0_N( 
                     1, hname, helper, 
                     mkIRExprVec_2( addrLo64, vdataLo64 ));

      eBias8    = tyAddr==Ity_I32 ? mkU32(bias+8) : mkU64(bias+8);
      addrHi64  = assignNew(mce, tyAddr, binop(mkAdd, addr, eBias8) );
      vdataHi64 = assignNew(mce, Ity_I64, unop(Iop_V128HIto64, vdata));
      diHi64    = unsafeIRDirty_0_N( 
                     1, hname, helper, 
                     mkIRExprVec_2( addrHi64, vdataHi64 ));

      setHelperAnns( mce, diLo64 );
      setHelperAnns( mce, diHi64 );
      stmt( mce->bb, IRStmt_Dirty(diLo64) );
      stmt( mce->bb, IRStmt_Dirty(diHi64) );

   } else {

      
      
      if (bias == 0) {
         addrAct = addr;
      } else {
         eBias   = tyAddr==Ity_I32 ? mkU32(bias) : mkU64(bias);
         addrAct = assignNew(mce, tyAddr, binop(mkAdd, addr, eBias) );
      }

      if (ty == Ity_I64) {
         di = unsafeIRDirty_0_N( 
                 1, hname, helper, 
                 mkIRExprVec_2( addrAct, vdata ));
      } else {
         di = unsafeIRDirty_0_N( 
                 2, hname, helper, 
                 mkIRExprVec_2( addrAct,
                                zwidenToHostWord( mce, vdata )));
      }
      setHelperAnns( mce, di );
      stmt( mce->bb, IRStmt_Dirty(di) );
   }

}



static IRType szToITy ( Int n )
{
   switch (n) {
      case 1: return Ity_I8;
      case 2: return Ity_I16;
      case 4: return Ity_I32;
      case 8: return Ity_I64;
      default: VG_(tool_panic)("szToITy(memcheck)");
   }
}

static
void do_shadow_Dirty ( MCEnv* mce, IRDirty* d )
{
   Int     i, n, offset, toDo, gSz, gOff;
   IRAtom  *src, *here, *curr;
   IRType  tyAddr, tySrc, tyDst;
   IRTemp  dst;

   
   complainIfUndefined(mce, d->guard);

   
   curr = definedOfType(Ity_I32);

   
   for (i = 0; d->args[i]; i++) {
      if (d->cee->mcx_mask & (1<<i)) {
         
      } else {
         here = mkPCastTo( mce, Ity_I32, expr2vbits(mce, d->args[i]) );
         curr = mkUifU32(mce, here, curr);
      }
   }

   
   for (i = 0; i < d->nFxState; i++) {
      tl_assert(d->fxState[i].fx != Ifx_None);
      if (d->fxState[i].fx == Ifx_Write)
         continue;

      
      if (isAlwaysDefd(mce, d->fxState[i].offset, d->fxState[i].size )) {
         if (0)
         VG_(printf)("memcheck: Dirty gst: ignored off %d, sz %d\n",
                     d->fxState[i].offset, d->fxState[i].size );
         continue;
      }

      gSz  = d->fxState[i].size;
      gOff = d->fxState[i].offset;
      tl_assert(gSz > 0);
      while (True) {
         if (gSz == 0) break;
         n = gSz <= 8 ? gSz : 8;
         tySrc = szToITy( n );
         src   = assignNew( mce, tySrc, 
                            shadow_GET(mce, gOff, tySrc ) );
         here = mkPCastTo( mce, Ity_I32, src );
         curr = mkUifU32(mce, here, curr);
         gSz -= n;
         gOff += n;
      }

   }

   tyAddr = Ity_INVALID;

   if (d->mFx != Ifx_None) {
      tl_assert(d->mAddr);
      complainIfUndefined(mce, d->mAddr);

      tyAddr = typeOfIRExpr(mce->bb->tyenv, d->mAddr);
      tl_assert(tyAddr == Ity_I32 || tyAddr == Ity_I64);
      tl_assert(tyAddr == mce->hWordTy); 
   }

   
   if (d->mFx == Ifx_Read || d->mFx == Ifx_Modify) {
      offset = 0;
      toDo   = d->mSize;
      
      while (toDo >= 4) {
         here = mkPCastTo( 
                   mce, Ity_I32,
                   expr2vbits_LDle ( mce, Ity_I32, 
                                     d->mAddr, d->mSize - toDo )
                );
         curr = mkUifU32(mce, here, curr);
         toDo -= 4;
      }
      
      while (toDo >= 2) {
         here = mkPCastTo( 
                   mce, Ity_I32,
                   expr2vbits_LDle ( mce, Ity_I16, 
                                     d->mAddr, d->mSize - toDo )
                );
         curr = mkUifU32(mce, here, curr);
         toDo -= 2;
      }
      tl_assert(toDo == 0); 
   }


   
   if (d->tmp != IRTemp_INVALID) {
      dst   = findShadowTmp(mce, d->tmp);
      tyDst = typeOfIRTemp(mce->bb->tyenv, d->tmp);
      assign( mce->bb, dst, mkPCastTo( mce, tyDst, curr) );
   }

   
   for (i = 0; i < d->nFxState; i++) {
      tl_assert(d->fxState[i].fx != Ifx_None);
      if (d->fxState[i].fx == Ifx_Read)
         continue;
      
      if (isAlwaysDefd(mce, d->fxState[i].offset, d->fxState[i].size ))
         continue;
      /* This state element is written or modified.  So we need to
         consider it.  If larger than 8 bytes, deal with it in 8-byte
         chunks. */
      gSz  = d->fxState[i].size;
      gOff = d->fxState[i].offset;
      tl_assert(gSz > 0);
      while (True) {
         if (gSz == 0) break;
         n = gSz <= 8 ? gSz : 8;
         tyDst = szToITy( n );
         do_shadow_PUT( mce, gOff,
                             NULL, 
                             mkPCastTo( mce, tyDst, curr ) );
         gSz -= n;
         gOff += n;
      }
   }

   
   if (d->mFx == Ifx_Write || d->mFx == Ifx_Modify) {
      offset = 0;
      toDo   = d->mSize;
      
      while (toDo >= 4) {
         do_shadow_STle( mce, d->mAddr, d->mSize - toDo,
                         NULL, 
                         mkPCastTo( mce, Ity_I32, curr ) );
         toDo -= 4;
      }
      
      while (toDo >= 2) {
         do_shadow_STle( mce, d->mAddr, d->mSize - toDo,
                         NULL, 
                         mkPCastTo( mce, Ity_I16, curr ) );
         toDo -= 2;
      }
      tl_assert(toDo == 0); 
   }

}



static Bool isBogusAtom ( IRAtom* at )
{
   ULong n = 0;
   IRConst* con;
   tl_assert(isIRAtom(at));
   if (at->tag == Iex_RdTmp)
      return False;
   tl_assert(at->tag == Iex_Const);
   con = at->Iex.Const.con;
   switch (con->tag) {
      case Ico_U8:  n = (ULong)con->Ico.U8; break;
      case Ico_U16: n = (ULong)con->Ico.U16; break;
      case Ico_U32: n = (ULong)con->Ico.U32; break;
      case Ico_U64: n = (ULong)con->Ico.U64; break;
      default: ppIRExpr(at); tl_assert(0);
   }
   
   return (n == 0xFEFEFEFF
           || n == 0x80808080
           || n == 0x1010101
           || n == 1010100);
}

__attribute__((unused))
static Bool checkForBogusLiterals (  IRStmt* st )
{
   Int     i;
   IRExpr* e;
   switch (st->tag) {
      case Ist_WrTmp:
         e = st->Ist.WrTmp.data;
         switch (e->tag) {
            case Iex_Get:
            case Iex_RdTmp:
               return False;
            case Iex_Unop: 
               return isBogusAtom(e->Iex.Unop.arg);
            case Iex_Binop: 
               return isBogusAtom(e->Iex.Binop.arg1)
                      || isBogusAtom(e->Iex.Binop.arg2);
            case Iex_ITE:
               return isBogusAtom(e->Iex.ITE.cond)
                      || isBogusAtom(e->Iex.ITE.iftrue)
                      || isBogusAtom(e->Iex.ITE.iffalse);
            case Iex_Load: 
               return isBogusAtom(e->Iex.Load.addr);
            case Iex_CCall:
               for (i = 0; e->Iex.CCall.args[i]; i++)
                  if (isBogusAtom(e->Iex.CCall.args[i]))
                     return True;
               return False;
            default: 
               goto unhandled;
         }
      case Ist_Put:
         return isBogusAtom(st->Ist.Put.data);
      case Ist_Store:
         return isBogusAtom(st->Ist.Store.addr) 
                || isBogusAtom(st->Ist.Store.data);
      case Ist_Exit:
         return isBogusAtom(st->Ist.Exit.guard);
      default: 
      unhandled:
         ppIRStmt(st);
         VG_(tool_panic)("hasBogusLiterals");
   }
}

IRSB* mc_instrument ( void* closureV,
                      IRSB* bb_in, VexGuestLayout* layout, 
                      VexGuestExtents* vge,
                      IRType gWordTy, IRType hWordTy )
{
   Bool verboze = False; 

   

   Int i, j, first_stmt;
   IRStmt* st;
   MCEnv mce;

   
   IRSB* bb     = emptyIRSB();
   bb->tyenv    = deepCopyIRTypeEnv(bb_in->tyenv);
   bb->next     = deepCopyIRExpr(bb_in->next);
   bb->jumpkind = bb_in->jumpkind;

   mce.bb             = bb;
   mce.layout         = layout;
   mce.n_originalTmps = bb->tyenv->types_used;
   mce.hWordTy        = hWordTy;
   mce.tmpMap         = LibVEX_Alloc(mce.n_originalTmps * sizeof(IRTemp));
   for (i = 0; i < mce.n_originalTmps; i++)
      mce.tmpMap[i] = IRTemp_INVALID;

   

   for (i = 0; i <  bb_in->stmts_used; i++) {
      st = bb_in->stmts[i];
      if (!st) continue;

      tl_assert(isFlatIRStmt(st));

      first_stmt = bb->stmts_used;

      if (verboze) {
         ppIRStmt(st);
         VG_(printf)("\n\n");
      }

      switch (st->tag) {

         case Ist_WrTmp:
            assign( bb, findShadowTmp(&mce, st->Ist.WrTmp.tmp), 
                        expr2vbits( &mce, st->Ist.WrTmp.data) );
            break;

         case Ist_Put:
            do_shadow_PUT( &mce, 
                           st->Ist.Put.offset,
                           st->Ist.Put.data,
                           NULL  );
            break;

         case Ist_PutI:
            do_shadow_PUTI( &mce, 
                            st->Ist.PutI.details->descr,
                            st->Ist.PutI.details->ix,
                            st->Ist.PutI.details->bias,
                            st->Ist.PutI.details->data );
            break;

         case Ist_Store:
            do_shadow_STle( &mce, st->Ist.Store.addr, 0,
                                  st->Ist.Store.data,
                                  NULL  );
            break;

         case Ist_Exit:
            
               complainIfUndefined( &mce, st->Ist.Exit.guard );
            break;

         case Ist_Dirty:
            do_shadow_Dirty( &mce, st->Ist.Dirty.details );
            break;

         case Ist_IMark:
         case Ist_NoOp:
            break;

         default:
            VG_(printf)("\n");
            ppIRStmt(st);
            VG_(printf)("\n");
            VG_(tool_panic)("memcheck: unhandled IRStmt");

      } 

      if (verboze) {
         for (j = first_stmt; j < bb->stmts_used; j++) {
            VG_(printf)("   ");
            ppIRStmt(bb->stmts[j]);
            VG_(printf)("\n");
         }
         VG_(printf)("\n");
      }

      addStmtToIRSB(bb, st);

   }

   
   first_stmt = bb->stmts_used;

   if (verboze) {
      VG_(printf)("bb->next = ");
      ppIRExpr(bb->next);
      VG_(printf)("\n\n");
   }

   complainIfUndefined( &mce, bb->next );

   if (verboze) {
      for (j = first_stmt; j < bb->stmts_used; j++) {
         VG_(printf)("   ");
         ppIRStmt(bb->stmts[j]);
         VG_(printf)("\n");
      }
      VG_(printf)("\n");
   }

   return bb;
}
#endif 

