
#include <stdio.h>
#include <stdlib.h>

#define HAVE_SSE2 1





typedef   union {
   long long            q;   
   unsigned long long   uq;   
   int                  d[2];   
   unsigned int         ud[2];   
   short                w[4];   
   unsigned short       uw[4];   
   char                 b[8];   
   unsigned char        ub[8];   
} mmx_t;


inline extern int
mmx_ok(void)
{
   register int ok = 0;

   __asm__ __volatile__ (
      
      "movl $1, %%eax\n\t"
      "cpuid\n\t"
      "movl %%edx, %0"
      : "=a" (ok)
      : 
   );
   return((ok & 0x800000) == 0x800000);
}


#ifdef   MMX_TRACE


#include <stdio.h>

#define   mmx_m2r(op, mem, reg) \
   { \
      mmx_t mmx_trace; \
      mmx_trace = (mem); \
      fprintf(stderr, #op "_m2r(" #mem "=0x%016llx, ", mmx_trace.q); \
      __asm__ __volatile__ ("movq %%" #reg ", %0" \
                  : "=X" (mmx_trace) \
                  :  ); \
      fprintf(stderr, #reg "=0x%016llx) => ", mmx_trace.q); \
      __asm__ __volatile__ (#op " %0, %%" #reg \
                  :  \
                  : "X" (mem)); \
      __asm__ __volatile__ ("movq %%" #reg ", %0" \
                  : "=X" (mmx_trace) \
                  :  ); \
      fprintf(stderr, #reg "=0x%016llx\n", mmx_trace.q); \
   }

#define   mmx_r2m(op, reg, mem) \
   { \
      mmx_t mmx_trace; \
      __asm__ __volatile__ ("movq %%" #reg ", %0" \
                  : "=X" (mmx_trace) \
                  :  ); \
      fprintf(stderr, #op "_r2m(" #reg "=0x%016llx, ", mmx_trace.q); \
      mmx_trace = (mem); \
      fprintf(stderr, #mem "=0x%016llx) => ", mmx_trace.q); \
      __asm__ __volatile__ (#op " %%" #reg ", %0" \
                  : "=X" (mem) \
                  :  ); \
      mmx_trace = (mem); \
      fprintf(stderr, #mem "=0x%016llx\n", mmx_trace.q); \
   }

#define   mmx_r2r(op, regs, regd) \
   { \
      mmx_t mmx_trace; \
      __asm__ __volatile__ ("movq %%" #regs ", %0" \
                  : "=X" (mmx_trace) \
                  :  ); \
      fprintf(stderr, #op "_r2r(" #regs "=0x%016llx, ", mmx_trace.q); \
      __asm__ __volatile__ ("movq %%" #regd ", %0" \
                  : "=X" (mmx_trace) \
                  :  ); \
      fprintf(stderr, #regd "=0x%016llx) => ", mmx_trace.q); \
      __asm__ __volatile__ (#op " %" #regs ", %" #regd); \
      __asm__ __volatile__ ("movq %%" #regd ", %0" \
                  : "=X" (mmx_trace) \
                  :  ); \
      fprintf(stderr, #regd "=0x%016llx\n", mmx_trace.q); \
   }

#define   mmx_m2m(op, mems, memd) \
   { \
      mmx_t mmx_trace; \
      mmx_trace = (mems); \
      fprintf(stderr, #op "_m2m(" #mems "=0x%016llx, ", mmx_trace.q); \
      mmx_trace = (memd); \
      fprintf(stderr, #memd "=0x%016llx) => ", mmx_trace.q); \
      __asm__ __volatile__ ("movq %0, %%mm0\n\t" \
                  #op " %1, %%mm0\n\t" \
                  "movq %%mm0, %0" \
                  : "=X" (memd) \
                  : "X" (mems)); \
      mmx_trace = (memd); \
      fprintf(stderr, #memd "=0x%016llx\n", mmx_trace.q); \
   }

#else


#define   mmx_m2r(op, mem, reg) \
   __asm__ __volatile__ (#op " %0, %%" #reg \
               :  \
               : "X" (mem))

#define   mmx_r2m(op, reg, mem) \
   __asm__ __volatile__ (#op " %%" #reg ", %0" \
               : "=X" (mem) \
               :  )

#define   mmx_r2r(op, regs, regd) \
   __asm__ __volatile__ (#op " %" #regs ", %" #regd)

#define   mmx_m2m(op, mems, memd) \
   __asm__ __volatile__ ("movq %0, %%mm0\n\t" \
               #op " %1, %%mm0\n\t" \
               "movq %%mm0, %0" \
               : "=X" (memd) \
               : "X" (mems))

#endif


#define   movq_m2r(var, reg)     mmx_m2r(movq, var, reg)
#define   movq_r2m(reg, var)     mmx_r2m(movq, reg, var)
#define   movq_r2r(regs, regd)   mmx_r2r(movq, regs, regd)
#define   movq(vars, vard) \
   __asm__ __volatile__ ("movq %1, %%mm0\n\t" \
               "movq %%mm0, %0" \
               : "=X" (vard) \
               : "X" (vars))


#define   movd_m2r(var, reg)     mmx_m2r(movd, var, reg)
#define   movd_r2m(reg, var)     mmx_r2m(movd, reg, var)
#define   movd_r2r(regs, regd)   mmx_r2r(movd, regs, regd)
#define   movd(vars, vard) \
   __asm__ __volatile__ ("movd %1, %%mm0\n\t" \
               "movd %%mm0, %0" \
               : "=X" (vard) \
               : "X" (vars))


#define   paddd_m2r(var, reg)     mmx_m2r(paddd, var, reg)
#define   paddd_r2r(regs, regd)   mmx_r2r(paddd, regs, regd)
#define   paddd(vars, vard)       mmx_m2m(paddd, vars, vard)

#define   paddw_m2r(var, reg)     mmx_m2r(paddw, var, reg)
#define   paddw_r2r(regs, regd)   mmx_r2r(paddw, regs, regd)
#define   paddw(vars, vard)       mmx_m2m(paddw, vars, vard)

#define   paddb_m2r(var, reg)     mmx_m2r(paddb, var, reg)
#define   paddb_r2r(regs, regd)   mmx_r2r(paddb, regs, regd)
#define   paddb(vars, vard)       mmx_m2m(paddb, vars, vard)


#define   paddsw_m2r(var, reg)     mmx_m2r(paddsw, var, reg)
#define   paddsw_r2r(regs, regd)   mmx_r2r(paddsw, regs, regd)
#define   paddsw(vars, vard)       mmx_m2m(paddsw, vars, vard)

#define   paddsb_m2r(var, reg)     mmx_m2r(paddsb, var, reg)
#define   paddsb_r2r(regs, regd)   mmx_r2r(paddsb, regs, regd)
#define   paddsb(vars, vard)       mmx_m2m(paddsb, vars, vard)


#define   paddusw_m2r(var, reg)     mmx_m2r(paddusw, var, reg)
#define   paddusw_r2r(regs, regd)   mmx_r2r(paddusw, regs, regd)
#define   paddusw(vars, vard)       mmx_m2m(paddusw, vars, vard)

#define   paddusb_m2r(var, reg)     mmx_m2r(paddusb, var, reg)
#define   paddusb_r2r(regs, regd)   mmx_r2r(paddusb, regs, regd)
#define   paddusb(vars, vard)       mmx_m2m(paddusb, vars, vard)


#define   psubd_m2r(var, reg)     mmx_m2r(psubd, var, reg)
#define   psubd_r2r(regs, regd)   mmx_r2r(psubd, regs, regd)
#define   psubd(vars, vard)       mmx_m2m(psubd, vars, vard)

#define   psubw_m2r(var, reg)     mmx_m2r(psubw, var, reg)
#define   psubw_r2r(regs, regd)   mmx_r2r(psubw, regs, regd)
#define   psubw(vars, vard)       mmx_m2m(psubw, vars, vard)

#define   psubb_m2r(var, reg)     mmx_m2r(psubb, var, reg)
#define   psubb_r2r(regs, regd)   mmx_r2r(psubb, regs, regd)
#define   psubb(vars, vard)       mmx_m2m(psubb, vars, vard)


#define   psubsw_m2r(var, reg)     mmx_m2r(psubsw, var, reg)
#define   psubsw_r2r(regs, regd)   mmx_r2r(psubsw, regs, regd)
#define   psubsw(vars, vard)       mmx_m2m(psubsw, vars, vard)

#define   psubsb_m2r(var, reg)     mmx_m2r(psubsb, var, reg)
#define   psubsb_r2r(regs, regd)   mmx_r2r(psubsb, regs, regd)
#define   psubsb(vars, vard)       mmx_m2m(psubsb, vars, vard)


#define   psubusw_m2r(var, reg)     mmx_m2r(psubusw, var, reg)
#define   psubusw_r2r(regs, regd)   mmx_r2r(psubusw, regs, regd)
#define   psubusw(vars, vard)       mmx_m2m(psubusw, vars, vard)

#define   psubusb_m2r(var, reg)     mmx_m2r(psubusb, var, reg)
#define   psubusb_r2r(regs, regd)   mmx_r2r(psubusb, regs, regd)
#define   psubusb(vars, vard)       mmx_m2m(psubusb, vars, vard)


#define   pmullw_m2r(var, reg)     mmx_m2r(pmullw, var, reg)
#define   pmullw_r2r(regs, regd)   mmx_r2r(pmullw, regs, regd)
#define   pmullw(vars, vard)       mmx_m2m(pmullw, vars, vard)


#define   pmulhw_m2r(var, reg)     mmx_m2r(pmulhw, var, reg)
#define   pmulhw_r2r(regs, regd)   mmx_r2r(pmulhw, regs, regd)
#define   pmulhw(vars, vard)       mmx_m2m(pmulhw, vars, vard)


#define   pmaddwd_m2r(var, reg)     mmx_m2r(pmaddwd, var, reg)
#define   pmaddwd_r2r(regs, regd)   mmx_r2r(pmaddwd, regs, regd)
#define   pmaddwd(vars, vard)       mmx_m2m(pmaddwd, vars, vard)


#define   pand_m2r(var, reg)     mmx_m2r(pand, var, reg)
#define   pand_r2r(regs, regd)   mmx_r2r(pand, regs, regd)
#define   pand(vars, vard)       mmx_m2m(pand, vars, vard)


#define   pandn_m2r(var, reg)     mmx_m2r(pandn, var, reg)
#define   pandn_r2r(regs, regd)   mmx_r2r(pandn, regs, regd)
#define   pandn(vars, vard)       mmx_m2m(pandn, vars, vard)


#define   por_m2r(var, reg)     mmx_m2r(por, var, reg)
#define   por_r2r(regs, regd)   mmx_r2r(por, regs, regd)
#define   por(vars, vard)       mmx_m2m(por, vars, vard)


#define   pxor_m2r(var, reg)     mmx_m2r(pxor, var, reg)
#define   pxor_r2r(regs, regd)   mmx_r2r(pxor, regs, regd)
#define   pxor(vars, vard)       mmx_m2m(pxor, vars, vard)


#define   pcmpeqd_m2r(var, reg)     mmx_m2r(pcmpeqd, var, reg)
#define   pcmpeqd_r2r(regs, regd)   mmx_r2r(pcmpeqd, regs, regd)
#define   pcmpeqd(vars, vard)       mmx_m2m(pcmpeqd, vars, vard)

#define   pcmpeqw_m2r(var, reg)     mmx_m2r(pcmpeqw, var, reg)
#define   pcmpeqw_r2r(regs, regd)   mmx_r2r(pcmpeqw, regs, regd)
#define   pcmpeqw(vars, vard)       mmx_m2m(pcmpeqw, vars, vard)

#define   pcmpeqb_m2r(var, reg)     mmx_m2r(pcmpeqb, var, reg)
#define   pcmpeqb_r2r(regs, regd)   mmx_r2r(pcmpeqb, regs, regd)
#define   pcmpeqb(vars, vard)       mmx_m2m(pcmpeqb, vars, vard)


#define   pcmpgtd_m2r(var, reg)   mmx_m2r(pcmpgtd, var, reg)
#define   pcmpgtd_r2r(regs, regd)   mmx_r2r(pcmpgtd, regs, regd)
#define   pcmpgtd(vars, vard)   mmx_m2m(pcmpgtd, vars, vard)

#define   pcmpgtw_m2r(var, reg)   mmx_m2r(pcmpgtw, var, reg)
#define   pcmpgtw_r2r(regs, regd)   mmx_r2r(pcmpgtw, regs, regd)
#define   pcmpgtw(vars, vard)   mmx_m2m(pcmpgtw, vars, vard)

#define   pcmpgtb_m2r(var, reg)   mmx_m2r(pcmpgtb, var, reg)
#define   pcmpgtb_r2r(regs, regd)   mmx_r2r(pcmpgtb, regs, regd)
#define   pcmpgtb(vars, vard)   mmx_m2m(pcmpgtb, vars, vard)


#define   psllq_m2r(var, reg)   mmx_m2r(psllq, var, reg)
#define   psllq_r2r(regs, regd)   mmx_r2r(psllq, regs, regd)
#define   psllq(vars, vard)   mmx_m2m(psllq, vars, vard)

#define   pslld_m2r(var, reg)   mmx_m2r(pslld, var, reg)
#define   pslld_r2r(regs, regd)   mmx_r2r(pslld, regs, regd)
#define   pslld(vars, vard)   mmx_m2m(pslld, vars, vard)

#define   psllw_m2r(var, reg)   mmx_m2r(psllw, var, reg)
#define   psllw_r2r(regs, regd)   mmx_r2r(psllw, regs, regd)
#define   psllw(vars, vard)   mmx_m2m(psllw, vars, vard)


#define   psrlq_m2r(var, reg)   mmx_m2r(psrlq, var, reg)
#define   psrlq_r2r(regs, regd)   mmx_r2r(psrlq, regs, regd)
#define   psrlq(vars, vard)   mmx_m2m(psrlq, vars, vard)

#define   psrld_m2r(var, reg)   mmx_m2r(psrld, var, reg)
#define   psrld_r2r(regs, regd)   mmx_r2r(psrld, regs, regd)
#define   psrld(vars, vard)   mmx_m2m(psrld, vars, vard)

#define   psrlw_m2r(var, reg)   mmx_m2r(psrlw, var, reg)
#define   psrlw_r2r(regs, regd)   mmx_r2r(psrlw, regs, regd)
#define   psrlw(vars, vard)   mmx_m2m(psrlw, vars, vard)


#define   psrad_m2r(var, reg)   mmx_m2r(psrad, var, reg)
#define   psrad_r2r(regs, regd)   mmx_r2r(psrad, regs, regd)
#define   psrad(vars, vard)   mmx_m2m(psrad, vars, vard)

#define   psraw_m2r(var, reg)   mmx_m2r(psraw, var, reg)
#define   psraw_r2r(regs, regd)   mmx_r2r(psraw, regs, regd)
#define   psraw(vars, vard)   mmx_m2m(psraw, vars, vard)


#define   packssdw_m2r(var, reg)   mmx_m2r(packssdw, var, reg)
#define   packssdw_r2r(regs, regd) mmx_r2r(packssdw, regs, regd)
#define   packssdw(vars, vard)   mmx_m2m(packssdw, vars, vard)

#define   packsswb_m2r(var, reg)   mmx_m2r(packsswb, var, reg)
#define   packsswb_r2r(regs, regd) mmx_r2r(packsswb, regs, regd)
#define   packsswb(vars, vard)   mmx_m2m(packsswb, vars, vard)


#define   packuswb_m2r(var, reg)   mmx_m2r(packuswb, var, reg)
#define   packuswb_r2r(regs, regd) mmx_r2r(packuswb, regs, regd)
#define   packuswb(vars, vard)   mmx_m2m(packuswb, vars, vard)


#define   punpckldq_m2r(var, reg)   mmx_m2r(punpckldq, var, reg)
#define   punpckldq_r2r(regs, regd) mmx_r2r(punpckldq, regs, regd)
#define   punpckldq(vars, vard)   mmx_m2m(punpckldq, vars, vard)

#define   punpcklwd_m2r(var, reg)   mmx_m2r(punpcklwd, var, reg)
#define   punpcklwd_r2r(regs, regd) mmx_r2r(punpcklwd, regs, regd)
#define   punpcklwd(vars, vard)   mmx_m2m(punpcklwd, vars, vard)

#define   punpcklbw_m2r(var, reg)   mmx_m2r(punpcklbw, var, reg)
#define   punpcklbw_r2r(regs, regd) mmx_r2r(punpcklbw, regs, regd)
#define   punpcklbw(vars, vard)   mmx_m2m(punpcklbw, vars, vard)


#define   punpckhdq_m2r(var, reg)   mmx_m2r(punpckhdq, var, reg)
#define   punpckhdq_r2r(regs, regd) mmx_r2r(punpckhdq, regs, regd)
#define   punpckhdq(vars, vard)   mmx_m2m(punpckhdq, vars, vard)

#define   punpckhwd_m2r(var, reg)   mmx_m2r(punpckhwd, var, reg)
#define   punpckhwd_r2r(regs, regd) mmx_r2r(punpckhwd, regs, regd)
#define   punpckhwd(vars, vard)   mmx_m2m(punpckhwd, vars, vard)

#define   punpckhbw_m2r(var, reg)   mmx_m2r(punpckhbw, var, reg)
#define   punpckhbw_r2r(regs, regd) mmx_r2r(punpckhbw, regs, regd)
#define   punpckhbw(vars, vard)   mmx_m2m(punpckhbw, vars, vard)


#define   paddq_m2r(var, reg)     mmx_m2r(paddq, var, reg)
#define   paddq_r2r(regs, regd)   mmx_r2r(paddq, regs, regd)
#define   paddq(vars, vard)       mmx_m2m(paddq, vars, vard)

#define   psubq_m2r(var, reg)     mmx_m2r(psubq, var, reg)
#define   psubq_r2r(regs, regd)   mmx_r2r(psubq, regs, regd)
#define   psubq(vars, vard)       mmx_m2m(psubq, vars, vard)



#ifdef   MMX_TRACE

#define   emms() \
   { \
      fprintf(stderr, "emms()\n"); \
      __asm__ __volatile__ ("emms"); \
   }

#else

#define   emms()         __asm__ __volatile__ ("emms")

#endif

void mkRand( mmx_t* mm )
{
  mm->uw[0] = 0xFFFF & (random() >> 7);
  mm->uw[1] = 0xFFFF & (random() >> 7);
  mm->uw[2] = 0xFFFF & (random() >> 7);
  mm->uw[3] = 0xFFFF & (random() >> 7);
}



int main( void )
{
  int i;
  
   mmx_t ma;
   mmx_t mb;
   mmx_t ma0, mb0;
   movq_r2r(mm0, mm1);


   


#define do_test(_name, _operation) \
   for (i = 0; i < 25000; i++) {                                 \
      mkRand(&ma);                                               \
      mkRand(&mb);                                               \
      ma0 = ma; mb0 = mb;                                        \
      _operation;                                                \
      fprintf(stdout, "%s ( %016llx, %016llx ) -> %016llx\n",    \
                     _name, ma0.q, mb0.q, mb.q);                 \
      fflush(stdout);                                            \
   }


   {
     do_test("paddd", paddd(ma,mb));
     do_test("paddw", paddw(ma,mb));
     do_test("paddb", paddb(ma,mb));

     do_test("paddsw", paddsw(ma,mb));
     do_test("paddsb", paddsb(ma,mb));

     do_test("paddusw", paddusw(ma,mb));
     do_test("paddusb", paddusb(ma,mb));

     do_test("psubd", psubd(ma,mb));
     do_test("psubw", psubw(ma,mb));
     do_test("psubb", psubb(ma,mb));

     do_test("psubsw", psubsw(ma,mb));
     do_test("psubsb", psubsb(ma,mb));

     do_test("psubusw", psubusw(ma,mb));
     do_test("psubusb", psubusb(ma,mb));

     do_test("pmulhw", pmulhw(ma,mb));
     do_test("pmullw", pmullw(ma,mb));

     do_test("pmaddwd", pmaddwd(ma,mb));

     do_test("pcmpeqd", pcmpeqd(ma,mb));
     do_test("pcmpeqw", pcmpeqw(ma,mb));
     do_test("pcmpeqb", pcmpeqb(ma,mb));

     do_test("pcmpgtd", pcmpgtd(ma,mb));
     do_test("pcmpgtw", pcmpgtw(ma,mb));
     do_test("pcmpgtb", pcmpgtb(ma,mb));

     do_test("packssdw", packssdw(ma,mb));
     do_test("packsswb", packsswb(ma,mb));
     do_test("packuswb", packuswb(ma,mb));

     do_test("punpckhdq", punpckhdq(ma,mb));
     do_test("punpckhwd", punpckhwd(ma,mb));
     do_test("punpckhbw", punpckhbw(ma,mb));

     do_test("punpckldq", punpckldq(ma,mb));
     do_test("punpcklwd", punpcklwd(ma,mb));
     do_test("punpcklbw", punpcklbw(ma,mb));

     do_test("pand", pand(ma,mb));
     do_test("pandn", pandn(ma,mb));
     do_test("por", por(ma,mb));
     do_test("pxor", pxor(ma,mb));

     do_test("psllq", psllq(ma,mb));
     do_test("pslld", pslld(ma,mb));
     do_test("psllw", psllw(ma,mb));

     do_test("psrlq", psrlq(ma,mb));
     do_test("psrld", psrld(ma,mb));
     do_test("psrlw", psrlw(ma,mb));

     do_test("psrad", psrad(ma,mb));
     do_test("psraw", psraw(ma,mb));

#if HAVE_SSE2
     do_test("paddq", paddq(ma,mb));
     do_test("psubq", psubq(ma,mb));
#endif

     emms();
   }

   
   exit(0);
}
