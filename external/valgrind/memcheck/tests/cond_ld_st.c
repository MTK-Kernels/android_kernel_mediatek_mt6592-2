

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "../memcheck.h"

typedef  unsigned int  UInt;

typedef  unsigned char  Bool;
#define False ((Bool)0)
#define True  ((Bool)1)

static void make_undef ( void* addr, size_t len )
{
  (void) VALGRIND_MAKE_MEM_UNDEFINED(addr, len);
}

static void make_def ( void* addr, size_t len )
{
  (void) VALGRIND_MAKE_MEM_DEFINED(addr, len);
}

__attribute__((noinline))
UInt do_conditional_load32 ( UInt* src, UInt alt, Bool b )
{
   UInt res;
#  if defined(__linux__) && defined(__arm__)
   __asm__ __volatile__(
     "mov r5, %2"     "\n\t"  
     "tst %3, #0xFF"  "\n\t"  
     "it ne"          "\n\t" 
     "ldrne r5, [%1]" "\n\t"  
     "mov %0, r5"     "\n\t"  
     : "=r"(res)
     : "r"(src), "r"(alt), "r"(b)
     :  "r5","cc","memory"
   );
#  else
   __asm__ __volatile__("" ::: "cc","memory");
   res = b ? *src : alt;
#  endif
   
   
   make_def(&res, sizeof(res));
   return res;
}

__attribute__((noinline))
UInt do_conditional_store32 ( UInt* dst, UInt alt, Bool b )
{
#  if defined(__linux__) && defined(__arm__)
   __asm__ __volatile__(
     "mov r5, %1"     "\n\t"  
     "tst %2, #0xFF"  "\n\t"  
     "it ne"          "\n\t" 
     "strne r5, [%0]" "\n\t"  
     : 
     : "r"(dst), "r"(alt), "r"(b)
     :  "r5","cc","memory"
   );
#  else
   __asm__ __volatile__("" ::: "cc","memory");
   if (b) *dst = alt;
#  endif
   UInt res;
   VALGRIND_DISABLE_ERROR_REPORTING;
   res = *dst;
   VALGRIND_ENABLE_ERROR_REPORTING;
   make_def(&res, sizeof(res));
   return res;
}



typedef  enum { Cond_D1=10, Cond_D0, Cond_U1, Cond_U0 }  Inp_Cond;
typedef  enum { Addr_DV=20, Addr_DI, Addr_UV, Addr_UI }  Inp_Addr;
typedef  enum { Alt_Da=30,  Alt_Ub }                     Inp_Alt;
typedef  enum { Data_Dc=40, Data_Ud }                    Inp_Data;

typedef
   struct { Inp_Cond inp_Cond;  Inp_Addr inp_Addr;
            Inp_Alt  inp_Alt;   Inp_Data inp_Data;
            char res; char defErr_Cond; char defErr_Addr; char addrErr; }
   TestCase;

const TestCase loadCases[64] = {

   
   
   
   

   
   
   { Cond_D1,    Addr_DV,    Alt_Da,    Data_Dc,    'C', 'N', 'N', 'N' }, 
   { Cond_D1,    Addr_DV,    Alt_Da,    Data_Ud,    'D', 'N', 'N', 'N' },
   { Cond_D1,    Addr_DV,    Alt_Ub,    Data_Dc,    'C', 'N', 'N', 'N' },
   { Cond_D1,    Addr_DV,    Alt_Ub,    Data_Ud,    'D', 'N', 'N', 'N' },
   { Cond_D1,    Addr_DI,    Alt_Da,    Data_Dc,    'C', 'N', 'N', 'Y' },
   { Cond_D1,    Addr_DI,    Alt_Da,    Data_Ud,    'D', 'N', 'N', 'Y' },
   { Cond_D1,    Addr_DI,    Alt_Ub,    Data_Dc,    'C', 'N', 'N', 'Y' },
   { Cond_D1,    Addr_DI,    Alt_Ub,    Data_Ud,    'D', 'N', 'N', 'Y' },

   { Cond_D1,    Addr_UV,    Alt_Da,    Data_Dc,    'C', 'N', 'Y', 'N' }, 
   { Cond_D1,    Addr_UV,    Alt_Da,    Data_Ud,    'D', 'N', 'Y', 'N' },
   { Cond_D1,    Addr_UV,    Alt_Ub,    Data_Dc,    'C', 'N', 'Y', 'N' },
   { Cond_D1,    Addr_UV,    Alt_Ub,    Data_Ud,    'D', 'N', 'Y', 'N' },
   { Cond_D1,    Addr_UI,    Alt_Da,    Data_Dc,    'C', 'N', 'Y', 'Y' },
   { Cond_D1,    Addr_UI,    Alt_Da,    Data_Ud,    'D', 'N', 'Y', 'Y' },
   { Cond_D1,    Addr_UI,    Alt_Ub,    Data_Dc,    'C', 'N', 'Y', 'Y' },
   { Cond_D1,    Addr_UI,    Alt_Ub,    Data_Ud,    'D', 'N', 'Y', 'Y' },

   
   
   { Cond_D0,    Addr_DV,    Alt_Da,    Data_Dc,    'A', 'N', 'N', 'N' }, 
   { Cond_D0,    Addr_DV,    Alt_Da,    Data_Ud,    'A', 'N', 'N', 'N' },
   { Cond_D0,    Addr_DV,    Alt_Ub,    Data_Dc,    'B', 'N', 'N', 'N' },
   { Cond_D0,    Addr_DV,    Alt_Ub,    Data_Ud,    'B', 'N', 'N', 'N' },
   { Cond_D0,    Addr_DI,    Alt_Da,    Data_Dc,    'A', 'N', 'N', 'N' },
   { Cond_D0,    Addr_DI,    Alt_Da,    Data_Ud,    'A', 'N', 'N', 'N' },
   { Cond_D0,    Addr_DI,    Alt_Ub,    Data_Dc,    'B', 'N', 'N', 'N' },
   { Cond_D0,    Addr_DI,    Alt_Ub,    Data_Ud,    'B', 'N', 'N', 'N' },

   { Cond_D0,    Addr_UV,    Alt_Da,    Data_Dc,    'A', 'N', 'N', 'N' }, 
   { Cond_D0,    Addr_UV,    Alt_Da,    Data_Ud,    'A', 'N', 'N', 'N' },
   { Cond_D0,    Addr_UV,    Alt_Ub,    Data_Dc,    'B', 'N', 'N', 'N' },
   { Cond_D0,    Addr_UV,    Alt_Ub,    Data_Ud,    'B', 'N', 'N', 'N' },
   { Cond_D0,    Addr_UI,    Alt_Da,    Data_Dc,    'A', 'N', 'N', 'N' },
   { Cond_D0,    Addr_UI,    Alt_Da,    Data_Ud,    'A', 'N', 'N', 'N' },
   { Cond_D0,    Addr_UI,    Alt_Ub,    Data_Dc,    'B', 'N', 'N', 'N' },
   { Cond_D0,    Addr_UI,    Alt_Ub,    Data_Ud,    'B', 'N', 'N', 'N' },

   
   
   
   

   
   
   
   
   { Cond_U1,    Addr_DV,    Alt_Da,    Data_Dc,    'C', 'Y', 'N', 'N' }, 
   { Cond_U1,    Addr_DV,    Alt_Da,    Data_Ud,    'D', 'Y', 'N', 'N' },
   { Cond_U1,    Addr_DV,    Alt_Ub,    Data_Dc,    'C', 'Y', 'N', 'N' },
   { Cond_U1,    Addr_DV,    Alt_Ub,    Data_Ud,    'D', 'Y', 'N', 'N' },
   { Cond_U1,    Addr_DI,    Alt_Da,    Data_Dc,    'C', 'Y', 'N', 'Y' },
   { Cond_U1,    Addr_DI,    Alt_Da,    Data_Ud,    'D', 'Y', 'N', 'Y' },
   { Cond_U1,    Addr_DI,    Alt_Ub,    Data_Dc,    'C', 'Y', 'N', 'Y' },
   { Cond_U1,    Addr_DI,    Alt_Ub,    Data_Ud,    'D', 'Y', 'N', 'Y' },

   { Cond_U1,    Addr_UV,    Alt_Da,    Data_Dc,    'C', 'Y', 'Y', 'N' }, 
   { Cond_U1,    Addr_UV,    Alt_Da,    Data_Ud,    'D', 'Y', 'Y', 'N' },
   { Cond_U1,    Addr_UV,    Alt_Ub,    Data_Dc,    'C', 'Y', 'Y', 'N' },
   { Cond_U1,    Addr_UV,    Alt_Ub,    Data_Ud,    'D', 'Y', 'Y', 'N' },
   { Cond_U1,    Addr_UI,    Alt_Da,    Data_Dc,    'C', 'Y', 'Y', 'Y' },
   { Cond_U1,    Addr_UI,    Alt_Da,    Data_Ud,    'D', 'Y', 'Y', 'Y' },
   { Cond_U1,    Addr_UI,    Alt_Ub,    Data_Dc,    'C', 'Y', 'Y', 'Y' },
   { Cond_U1,    Addr_UI,    Alt_Ub,    Data_Ud,    'D', 'Y', 'Y', 'Y' },

   
   
   
   
   
   { Cond_U0,    Addr_DV,    Alt_Da,    Data_Dc,    'A', 'Y', 'N', 'N' }, 
   { Cond_U0,    Addr_DV,    Alt_Da,    Data_Ud,    'A', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_DV,    Alt_Ub,    Data_Dc,    'B', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_DV,    Alt_Ub,    Data_Ud,    'B', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_DI,    Alt_Da,    Data_Dc,    'A', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_DI,    Alt_Da,    Data_Ud,    'A', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_DI,    Alt_Ub,    Data_Dc,    'B', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_DI,    Alt_Ub,    Data_Ud,    'B', 'Y', 'N', 'N' },

   { Cond_U0,    Addr_UV,    Alt_Da,    Data_Dc,    'A', 'Y', 'N', 'N' }, 
   { Cond_U0,    Addr_UV,    Alt_Da,    Data_Ud,    'A', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_UV,    Alt_Ub,    Data_Dc,    'B', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_UV,    Alt_Ub,    Data_Ud,    'B', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_UI,    Alt_Da,    Data_Dc,    'A', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_UI,    Alt_Da,    Data_Ud,    'A', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_UI,    Alt_Ub,    Data_Dc,    'B', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_UI,    Alt_Ub,    Data_Ud,    'B', 'Y', 'N', 'N' }  
};

static Bool c_Cond_D1, c_Cond_D0, c_Cond_U1, c_Cond_U0;
static UInt *c_Addr_DV, *c_Addr_DI, *c_Addr_UV, *c_Addr_UI;
static UInt c_Alt_Da, c_Alt_Ub;

static void setup_test_data ( Inp_Data inp_Data )
{
   c_Cond_D1 = c_Cond_U1 = True;
   c_Cond_D0 = c_Cond_U0 = False;
   make_undef(&c_Cond_U1, sizeof(c_Cond_U1));
   make_undef(&c_Cond_U0, sizeof(c_Cond_U0));

   c_Addr_DV = c_Addr_UV = malloc(4);
   c_Addr_DI = c_Addr_UI = malloc(4);
   
   UInt testd = inp_Data == Data_Dc ? 0xCCCCCCCC : 0xDDDDDDDD;
   *c_Addr_DV = *c_Addr_DI = testd;
   if (inp_Data == Data_Dc) {
     
   } else {
     make_undef(c_Addr_DV, 4);
     make_undef(c_Addr_DI, 4);
   }

   
   
   free(c_Addr_DI);

   
   make_undef(&c_Addr_UV, sizeof(c_Addr_UV));
   make_undef(&c_Addr_UI, sizeof(c_Addr_UI));

   
   c_Alt_Da = 0xAAAAAAAA;
   c_Alt_Ub = 0xBBBBBBBB;
   make_undef(&c_Alt_Ub, sizeof(c_Alt_Ub));
}

static void do_test_case ( int caseNo, Bool isLoad, const TestCase* lc )
{
   fprintf(stderr,
           "\n-----------------------------------------------------------\n");
   fprintf(stderr, "%s CASE %d\n", isLoad ? "LOAD" : "STORE", caseNo);
   
   assert(Cond_D1 <= lc->inp_Cond && lc->inp_Cond <= Cond_U0);
   assert(Addr_DV <= lc->inp_Addr && lc->inp_Addr <= Addr_UI);
   assert(lc->inp_Alt == Alt_Da || lc->inp_Alt == Alt_Ub);
   assert(lc->inp_Data == Data_Dc || lc->inp_Data == Data_Ud);
   assert('A' <= lc->res && lc->res <= 'D');
   assert(lc->defErr_Cond == 'Y' || lc->defErr_Cond == 'N');
   assert(lc->defErr_Addr == 'Y' || lc->defErr_Addr == 'N');
   assert(lc->addrErr     == 'Y' || lc->addrErr     == 'N');
   
   setup_test_data(lc->inp_Data);

   
   
   Bool i_Cond;
   UInt* i_Addr;
   UInt i_Alt;
   switch (lc->inp_Cond) {
     case Cond_D1: i_Cond = c_Cond_D1; break;
     case Cond_D0: i_Cond = c_Cond_D0; break;
     case Cond_U1: i_Cond = c_Cond_U1; break;
     case Cond_U0: i_Cond = c_Cond_U0; break;
     default: assert(0);
   }
   switch (lc->inp_Addr) {
     case Addr_DV: i_Addr = c_Addr_DV; break;
     case Addr_DI: i_Addr = c_Addr_DI; break;
     case Addr_UV: i_Addr = c_Addr_UV; break;
     case Addr_UI: i_Addr = c_Addr_UI; break;
     default: assert(0);
   }
   switch (lc->inp_Alt) {
     case Alt_Da: i_Alt = c_Alt_Da; break;
     case Alt_Ub: i_Alt = c_Alt_Ub; break;
     default: assert(0);
   }

   
   UInt n_errs_exp
     = (lc->defErr_Cond == 'Y' ? 1 : 0) + (lc->defErr_Addr == 'Y' ? 1 : 0)
       + (lc->addrErr == 'Y' ? 1 : 0);

   UInt n_errs_act = VALGRIND_COUNT_ERRORS;
   UInt res_act;
   if (isLoad) {
      res_act = do_conditional_load32(i_Addr, i_Alt, i_Cond);
   } else {
      res_act = do_conditional_store32(i_Addr, i_Alt, i_Cond);
   }
   n_errs_act = VALGRIND_COUNT_ERRORS - n_errs_act;

   if (n_errs_act == n_errs_exp) {
      fprintf(stderr, "PASS: %u errors\n", n_errs_act);
   } else {
      fprintf(stderr, "FAIL: %u errors expected, %u actual\n",
              n_errs_exp, n_errs_act);
   }

   
   UInt res_exp = 0;
   switch (lc->res) {
      case 'A': res_exp = 0xAAAAAAAA; break;
      case 'B': res_exp = 0xBBBBBBBB; break;
      case 'C': res_exp = 0xCCCCCCCC; break;
      case 'D': res_exp = 0xDDDDDDDD; break;
      default: assert(0);
   }

   if (res_act == res_exp) {
      fprintf(stderr, "PASS: correct result\n");
   } else {
      fprintf(stderr, "FAIL: result: %08x expected, %08x actual\n",
              res_exp, res_act);
   }

   free(c_Addr_DV);
}


void do_test_case_steer ( void (*fn)(int,Bool,const TestCase*),
                          int i, Bool isLd, const TestCase* tc )
{
   __asm__ __volatile__("");
   if (i == 0) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 1) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 2) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 3) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 4) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 5) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 6) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 7) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 8) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 9) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 10) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 11) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 12) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 13) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 14) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 15) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 16) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 17) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 18) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 19) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 20) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 21) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 22) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 23) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 24) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 25) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 26) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 27) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 28) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 29) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 30) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 31) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 32) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 33) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 34) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 35) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 36) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 37) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 38) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 39) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 40) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 41) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 42) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 43) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 44) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 45) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 46) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 47) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 48) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 49) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 50) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 51) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 52) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 53) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 54) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 55) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 56) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 57) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 58) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 59) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 60) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 61) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 62) { fn(i,isLd,tc); return; };
   __asm__ __volatile__("");
   if (i == 63) { fn(i,isLd,tc); return; };
   assert(0);
}




const TestCase storeCases[64] = {

   
   
   
   

   
   
   { Cond_D1,    Addr_DV,    Alt_Da,    Data_Dc,    'A', 'N', 'N', 'N' }, 
   { Cond_D1,    Addr_DV,    Alt_Da,    Data_Ud,    'A', 'N', 'N', 'N' },
   { Cond_D1,    Addr_DV,    Alt_Ub,    Data_Dc,    'B', 'N', 'N', 'N' },
   { Cond_D1,    Addr_DV,    Alt_Ub,    Data_Ud,    'B', 'N', 'N', 'N' },
   { Cond_D1,    Addr_DI,    Alt_Da,    Data_Dc,    'A', 'N', 'N', 'Y' },
   { Cond_D1,    Addr_DI,    Alt_Da,    Data_Ud,    'A', 'N', 'N', 'Y' },
   { Cond_D1,    Addr_DI,    Alt_Ub,    Data_Dc,    'B', 'N', 'N', 'Y' },
   { Cond_D1,    Addr_DI,    Alt_Ub,    Data_Ud,    'B', 'N', 'N', 'Y' },

   { Cond_D1,    Addr_UV,    Alt_Da,    Data_Dc,    'A', 'N', 'Y', 'N' }, 
   { Cond_D1,    Addr_UV,    Alt_Da,    Data_Ud,    'A', 'N', 'Y', 'N' },
   { Cond_D1,    Addr_UV,    Alt_Ub,    Data_Dc,    'B', 'N', 'Y', 'N' },
   { Cond_D1,    Addr_UV,    Alt_Ub,    Data_Ud,    'B', 'N', 'Y', 'N' },
   { Cond_D1,    Addr_UI,    Alt_Da,    Data_Dc,    'A', 'N', 'Y', 'Y' },
   { Cond_D1,    Addr_UI,    Alt_Da,    Data_Ud,    'A', 'N', 'Y', 'Y' },
   { Cond_D1,    Addr_UI,    Alt_Ub,    Data_Dc,    'B', 'N', 'Y', 'Y' },
   { Cond_D1,    Addr_UI,    Alt_Ub,    Data_Ud,    'B', 'N', 'Y', 'Y' },

   
   
   { Cond_D0,    Addr_DV,    Alt_Da,    Data_Dc,    'C', 'N', 'N', 'N' }, 
   { Cond_D0,    Addr_DV,    Alt_Da,    Data_Ud,    'D', 'N', 'N', 'N' },
   { Cond_D0,    Addr_DV,    Alt_Ub,    Data_Dc,    'C', 'N', 'N', 'N' },
   { Cond_D0,    Addr_DV,    Alt_Ub,    Data_Ud,    'D', 'N', 'N', 'N' },
   { Cond_D0,    Addr_DI,    Alt_Da,    Data_Dc,    'C', 'N', 'N', 'N' },
   { Cond_D0,    Addr_DI,    Alt_Da,    Data_Ud,    'D', 'N', 'N', 'N' },
   { Cond_D0,    Addr_DI,    Alt_Ub,    Data_Dc,    'C', 'N', 'N', 'N' },
   { Cond_D0,    Addr_DI,    Alt_Ub,    Data_Ud,    'D', 'N', 'N', 'N' },

   { Cond_D0,    Addr_UV,    Alt_Da,    Data_Dc,    'C', 'N', 'N', 'N' }, 
   { Cond_D0,    Addr_UV,    Alt_Da,    Data_Ud,    'D', 'N', 'N', 'N' },
   { Cond_D0,    Addr_UV,    Alt_Ub,    Data_Dc,    'C', 'N', 'N', 'N' },
   { Cond_D0,    Addr_UV,    Alt_Ub,    Data_Ud,    'D', 'N', 'N', 'N' },
   { Cond_D0,    Addr_UI,    Alt_Da,    Data_Dc,    'C', 'N', 'N', 'N' },
   { Cond_D0,    Addr_UI,    Alt_Da,    Data_Ud,    'D', 'N', 'N', 'N' },
   { Cond_D0,    Addr_UI,    Alt_Ub,    Data_Dc,    'C', 'N', 'N', 'N' },
   { Cond_D0,    Addr_UI,    Alt_Ub,    Data_Ud,    'D', 'N', 'N', 'N' },

   
   
   
   

   
   
   
   
   { Cond_U1,    Addr_DV,    Alt_Da,    Data_Dc,    'A', 'Y', 'N', 'N' }, 
   { Cond_U1,    Addr_DV,    Alt_Da,    Data_Ud,    'A', 'Y', 'N', 'N' },
   { Cond_U1,    Addr_DV,    Alt_Ub,    Data_Dc,    'B', 'Y', 'N', 'N' },
   { Cond_U1,    Addr_DV,    Alt_Ub,    Data_Ud,    'B', 'Y', 'N', 'N' },
   { Cond_U1,    Addr_DI,    Alt_Da,    Data_Dc,    'A', 'Y', 'N', 'Y' },
   { Cond_U1,    Addr_DI,    Alt_Da,    Data_Ud,    'A', 'Y', 'N', 'Y' },
   { Cond_U1,    Addr_DI,    Alt_Ub,    Data_Dc,    'B', 'Y', 'N', 'Y' },
   { Cond_U1,    Addr_DI,    Alt_Ub,    Data_Ud,    'B', 'Y', 'N', 'Y' },

   { Cond_U1,    Addr_UV,    Alt_Da,    Data_Dc,    'A', 'Y', 'Y', 'N' }, 
   { Cond_U1,    Addr_UV,    Alt_Da,    Data_Ud,    'A', 'Y', 'Y', 'N' },
   { Cond_U1,    Addr_UV,    Alt_Ub,    Data_Dc,    'B', 'Y', 'Y', 'N' },
   { Cond_U1,    Addr_UV,    Alt_Ub,    Data_Ud,    'B', 'Y', 'Y', 'N' },
   { Cond_U1,    Addr_UI,    Alt_Da,    Data_Dc,    'A', 'Y', 'Y', 'Y' },
   { Cond_U1,    Addr_UI,    Alt_Da,    Data_Ud,    'A', 'Y', 'Y', 'Y' },
   { Cond_U1,    Addr_UI,    Alt_Ub,    Data_Dc,    'B', 'Y', 'Y', 'Y' },
   { Cond_U1,    Addr_UI,    Alt_Ub,    Data_Ud,    'B', 'Y', 'Y', 'Y' },

   
   
   
   
   
   { Cond_U0,    Addr_DV,    Alt_Da,    Data_Dc,    'C', 'Y', 'N', 'N' }, 
   { Cond_U0,    Addr_DV,    Alt_Da,    Data_Ud,    'D', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_DV,    Alt_Ub,    Data_Dc,    'C', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_DV,    Alt_Ub,    Data_Ud,    'D', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_DI,    Alt_Da,    Data_Dc,    'C', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_DI,    Alt_Da,    Data_Ud,    'D', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_DI,    Alt_Ub,    Data_Dc,    'C', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_DI,    Alt_Ub,    Data_Ud,    'D', 'Y', 'N', 'N' },

   { Cond_U0,    Addr_UV,    Alt_Da,    Data_Dc,    'C', 'Y', 'N', 'N' }, 
   { Cond_U0,    Addr_UV,    Alt_Da,    Data_Ud,    'D', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_UV,    Alt_Ub,    Data_Dc,    'C', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_UV,    Alt_Ub,    Data_Ud,    'D', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_UI,    Alt_Da,    Data_Dc,    'C', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_UI,    Alt_Da,    Data_Ud,    'D', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_UI,    Alt_Ub,    Data_Dc,    'C', 'Y', 'N', 'N' },
   { Cond_U0,    Addr_UI,    Alt_Ub,    Data_Ud,    'D', 'Y', 'N', 'N' }  
};

void usage ( char* pname )
{
  fprintf(stderr, "usage: %s [loads|stores]\n", pname);
  exit(1);
}

int main ( int argc, char** argv )
{
   UInt i, nCases;

   if (argc != 2) usage(argv[0]);

   Bool doLoad = False;
   if (0 == strcmp(argv[1], "loads")) {
     doLoad = True;
   }
   else if (0 == strcmp(argv[1], "stores")) {
     doLoad = False;
   }
   else usage(argv[0]);

   if (doLoad) {
     nCases = sizeof(loadCases) / sizeof(loadCases[0]);
     assert(nCases == 64);
     for (i = 0; i < nCases; i++)
       do_test_case_steer( do_test_case, i, True, &loadCases[i] );
   } else {
     nCases = sizeof(storeCases) / sizeof(storeCases[0]);
     assert(nCases == 64);
     for (i = 0; i < nCases; i++)
       do_test_case_steer( do_test_case, i, False, &storeCases[i] );
   }

   return 0;
}
