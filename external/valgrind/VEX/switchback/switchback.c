

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../pub/libvex_basictypes.h"
#include "../pub/libvex_guest_x86.h"
#include "../pub/libvex_guest_amd64.h"
#include "../pub/libvex_guest_ppc32.h"
#include "../pub/libvex_guest_ppc64.h"
#include "../pub/libvex_guest_arm64.h"
#include "../pub/libvex.h"
#include "../pub/libvex_trc_values.h"
#include "linker.h"

static ULong n_bbs_done = 0;
static Int   n_translations_made = 0;


#if defined(__i386__)
#  define VexGuestState             VexGuestX86State
#  define LibVEX_Guest_initialise   LibVEX_GuestX86_initialise
#  define VexArch                   VexArchX86
#  define VexSubArch                VexSubArchX86_sse1
#  define GuestPC                   guest_EIP
#  define CacheLineSize             0

#elif defined(__aarch64__) && !defined(__arm__)
#  define VexGuestState             VexGuestARM64State
#  define LibVEX_Guest_initialise   LibVEX_GuestARM64_initialise
#  define VexArch                   VexArchARM64
#  define VexSubArch                VexSubArch_NONE
#  define GuestPC                   guest_PC
#  define CacheLineSize             0

#else
#   error "Unknown arch"
#endif

#define TEST_FLAGS ((1<<7)|(1<<3)|(1<<2)|(1<<1)|(1<<0))
#define DEBUG_TRACE_FLAGS ((0<<7)|(0<<6)|(0<<5)|(0<<4)| \
                           (0<<3)|(0<<2)|(0<<1)|(0<<0))

typedef  unsigned long int  Addr;


ULong gstack[64000] __attribute__((aligned(16)));
VexGuestState gst;
VexControl vcon;

HWord sb_helper1 = 0;
HWord sb_helper2 = 0;
HWord sb_helper3 = 0;

#define N_TRANS_CACHE 1000000
#define N_TRANS_TABLE 10000

ULong trans_cache[N_TRANS_CACHE];
VexGuestExtents trans_table [N_TRANS_TABLE];
ULong*          trans_tableP[N_TRANS_TABLE];

Int trans_cache_used = 0;
Int trans_table_used = 0;

static Bool chase_into_ok ( void* opaque, Addr64 dst ) {
   return False;
}

static UInt needs_self_check ( void* opaque, const VexGuestExtents* vge ) {
   return 0;
}


static HWord serviceFn ( HWord arg1, HWord arg2 )
{
   switch (arg1) {
      case 0: 
         printf("---STOP---\n");
         printf("serviceFn:EXIT\n");
	 printf("%llu bbs simulated\n", n_bbs_done);
	 printf("%d translations made, %d tt bytes\n", 
                n_translations_made, 8*trans_cache_used);
         exit(0);
      case 1: 
         putchar(arg2);
         return 0;
      case 2: 
         return (HWord)malloc(arg2);
      case 3: 
         free((void*)arg2);
         return 0;
      default:
         assert(0);
   }
}


static void invalidate_icache(void *ptr, unsigned long nbytes)
{
   
   
   
   // which has the following copyright notice:
   /*
   Copyright 2013, ARM Limited
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
   
   * Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   * Neither the name of ARM Limited nor the names of its contributors may be
     used to endorse or promote products derived from this software without
     specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

   
   UInt cache_type_register;
   
   __asm__ __volatile__ ("mrs %[ctr], ctr_el0" 
                         : [ctr] "=r" (cache_type_register));

   const Int kDCacheLineSizeShift = 16;
   const Int kICacheLineSizeShift = 0;
   const UInt kDCacheLineSizeMask = 0xf << kDCacheLineSizeShift;
   const UInt kICacheLineSizeMask = 0xf << kICacheLineSizeShift;

   
   
   const UInt dcache_line_size_power_of_two =
       (cache_type_register & kDCacheLineSizeMask) >> kDCacheLineSizeShift;
   const UInt icache_line_size_power_of_two =
       (cache_type_register & kICacheLineSizeMask) >> kICacheLineSizeShift;

   const UInt dcache_line_size_ = 1 << dcache_line_size_power_of_two;
   const UInt icache_line_size_ = 1 << icache_line_size_power_of_two;

   Addr start = (Addr)ptr;
   
   Addr dsize = (Addr)dcache_line_size_;
   Addr isize = (Addr)icache_line_size_;

   
   Addr dstart = start & ~(dsize - 1);
   Addr istart = start & ~(isize - 1);
   Addr end    = start + nbytes;

   __asm__ __volatile__ (
     
     "0: \n\t"
     
     
     
     
     
     
     
     "dc cvau, %[dline] \n\t"
     "add %[dline], %[dline], %[dsize] \n\t"
     "cmp %[dline], %[end] \n\t"
     "b.lt 0b \n\t"
     
     
     
     
     
     
     
     
     
     "dsb ish \n\t"
     
     "1: \n\t"
     
     
     
     
     "ic ivau, %[iline] \n\t"
     "add %[iline], %[iline], %[isize] \n\t"
     "cmp %[iline], %[end] \n\t"
     "b.lt 1b \n\t"
     
     
     "dsb ish \n\t"
     
     
     
     "isb \n\t"
     : [dline] "+r" (dstart),
       [iline] "+r" (istart)
     : [dsize] "r" (dsize),
       [isize] "r" (isize),
       [end] "r" (end)
     
     
     : "cc", "memory"
   );

}



#if defined(__i386__)

extern void switchback_asm(void);
asm(
"switchback_asm:\n"
"   movl sb_helper1, %eax\n"  
"   movl  16(%eax), %esp\n"   
"   pushl 56(%eax)\n"         
"   movl sb_helper2, %ebx\n"  
"   pushl %ebx\n"             
"   pushl 0(%eax)\n"          
"   movl 4(%eax), %ecx\n" 
"   movl 8(%eax), %edx\n" 
"   movl 12(%eax), %ebx\n" 
"   movl 20(%eax), %ebp\n"
"   movl 24(%eax), %esi\n"
"   movl 28(%eax), %edi\n"
"   popl %eax\n"
"   popfl\n"
"   ret\n"
);
void switchback ( void )
{
   sb_helper1 = (HWord)&gst;
   sb_helper2 = LibVEX_GuestX86_get_eflags(&gst);
   switchback_asm(); 
}

#elif defined(__aarch64__)

extern void switchback_asm(HWord x0_gst, HWord x1_pstate);
asm(
"switchback_asm:"
"   mrs x30, nzcv"  "\n"
"   and x30, x30, #0xFFFFFFFF0FFFFFFF"  "\n"
"   and x1,  x1,  #0x00000000F0000000"  "\n"
"   orr x30, x30, x1"  "\n"
"   msr nzcv, x30"  "\n"

"   ldr x30, [x0, #16 + 8*37]"  "\n"
"   msr tpidr_el0, x30"  "\n"

"   ldr x30, [x0, #16 + 8*31]"  "\n"
"   mov sp,  x30"  "\n"

"   add x30, x0, #(16 + 8*38 + 16*0)"  "\n"
"   ldr q0,  [x30], #16"   "\n"
"   ldr q1,  [x30], #16"   "\n"
"   ldr q2,  [x30], #16"   "\n"
"   ldr q3,  [x30], #16"   "\n"
"   ldr q4,  [x30], #16"   "\n"
"   ldr q5,  [x30], #16"   "\n"
"   ldr q6,  [x30], #16"   "\n"
"   ldr q7,  [x30], #16"   "\n"
"   ldr q8,  [x30], #16"   "\n"
"   ldr q9,  [x30], #16"   "\n"
"   ldr q10, [x30], #16"   "\n"
"   ldr q11, [x30], #16"   "\n"
"   ldr q12, [x30], #16"   "\n"
"   ldr q13, [x30], #16"   "\n"
"   ldr q14, [x30], #16"   "\n"
"   ldr q15, [x30], #16"   "\n"
"   ldr q16, [x30], #16"   "\n"
"   ldr q17, [x30], #16"   "\n"
"   ldr q18, [x30], #16"   "\n"
"   ldr q19, [x30], #16"   "\n"
"   ldr q20, [x30], #16"   "\n"
"   ldr q21, [x30], #16"   "\n"
"   ldr q22, [x30], #16"   "\n"
"   ldr q23, [x30], #16"   "\n"
"   ldr q24, [x30], #16"   "\n"
"   ldr q25, [x30], #16"   "\n"
"   ldr q26, [x30], #16"   "\n"
"   ldr q27, [x30], #16"   "\n"
"   ldr q28, [x30], #16"   "\n"
"   ldr q29, [x30], #16"   "\n"
"   ldr q30, [x30], #16"   "\n"
"   ldr q31, [x30], #16"   "\n"

"   ldr x30, [x0, #16+8*30]"  "\n"
"   ldr x29, [x0, #16+8*29]"  "\n"
"   ldr x28, [x0, #16+8*28]"  "\n"
"   ldr x27, [x0, #16+8*27]"  "\n"
"   ldr x26, [x0, #16+8*26]"  "\n"
"   ldr x25, [x0, #16+8*25]"  "\n"
"   ldr x24, [x0, #16+8*24]"  "\n"
"   ldr x23, [x0, #16+8*23]"  "\n"
"   ldr x22, [x0, #16+8*22]"  "\n"
"   ldr x21, [x0, #16+8*21]"  "\n"
"   ldr x20, [x0, #16+8*20]"  "\n"
"   ldr x19, [x0, #16+8*19]"  "\n"
"   ldr x18, [x0, #16+8*18]"  "\n"
"   ldr x17, [x0, #16+8*17]"  "\n"
"   ldr x16, [x0, #16+8*16]"  "\n"
"   ldr x15, [x0, #16+8*15]"  "\n"
"   ldr x14, [x0, #16+8*14]"  "\n"
"   ldr x13, [x0, #16+8*13]"  "\n"
"   ldr x12, [x0, #16+8*12]"  "\n"
"   ldr x11, [x0, #16+8*11]"  "\n"
"   ldr x10, [x0, #16+8*10]"  "\n"
"   ldr x9,  [x0, #16+8*9]"   "\n"
"   ldr x8,  [x0, #16+8*8]"   "\n"
"   ldr x7,  [x0, #16+8*7]"   "\n"
"   ldr x6,  [x0, #16+8*6]"   "\n"
"   ldr x5,  [x0, #16+8*5]"   "\n"
"   ldr x4,  [x0, #16+8*4]"   "\n"
"   ldr x3,  [x0, #16+8*3]"   "\n"
"   ldr x2,  [x0, #16+8*2]"   "\n"
"   ldr x1,  [x0, #16+8*1]"   "\n"
"   ldr x0,  [x0, #16+8*0]"   "\n"

"nop_start_point:"            "\n"
"   nop"  "\n" 
"nop_end_point:"              "\n"
);

extern void nop_start_point(void);
extern void nop_end_point(void);

void switchback ( void )
{
  assert(offsetof(VexGuestARM64State, guest_X0)  == 16 + 8*0);
  assert(offsetof(VexGuestARM64State, guest_X30) == 16 + 8*30);
  assert(offsetof(VexGuestARM64State, guest_SP)  == 16 + 8*31);
  assert(offsetof(VexGuestARM64State, guest_TPIDR_EL0) == 16 + 8*37);
  assert(offsetof(VexGuestARM64State, guest_Q0)  == 16 + 8*38 + 16*0);

  HWord arg0 = (HWord)&gst;
  HWord arg1 = LibVEX_GuestARM64_get_nzcv(&gst);


  UChar* sa_start     = (UChar*)&switchback_asm;
  UChar* sa_nop_start = (UChar*)&nop_start_point;
  UChar* sa_end       = (UChar*)&nop_end_point;

  Int i;
  Int nbytes       = sa_end - sa_start;
  Int off_nopstart = sa_nop_start - sa_start;
  if (0)
     printf("nbytes = %d, nopstart = %d\n", nbytes, off_nopstart);

   
   UChar* copy = mymalloc(nbytes);
   assert(copy);
   for (i = 0; i < nbytes; i++)
      copy[i] = sa_start[i];

   UInt* p = (UInt*)(&copy[off_nopstart]);

   Addr addr_of_nop = (Addr)p;
   Addr where_to_go = gst.guest_PC;
   Long   diff = ((Long)where_to_go) - ((Long)addr_of_nop);

   if (0) {
     printf("addr of first nop = 0x%llx\n", addr_of_nop);
     printf("where to go       = 0x%llx\n", where_to_go);
     printf("diff = 0x%llx\n", diff);
   }

   if (diff < -0x8000000LL || diff >= 0x8000000LL) {
     
     printf("hosed -- offset too large\n");
     assert(0);
   }

   
   assert(p[0] == 0xd503201f); 

   
   p[0] = 0x14000000 | ((diff >> 2) & 0x3FFFFFF);

   invalidate_icache( copy, nbytes );

   ( (void(*)(HWord,HWord))copy )(arg0, arg1);
}

#else
# error "Unknown plat"
#endif



static HWord block[2]; 
extern HWord run_translation_asm(void);

extern void disp_chain_assisted(void);

#if defined(__aarch64__)
asm(
"run_translation_asm:"            "\n"
"   stp  x29, x30, [sp, #-16]!"   "\n"
"   stp  x27, x28, [sp, #-16]!"   "\n"
"   stp  x25, x26, [sp, #-16]!"   "\n"
"   stp  x23, x24, [sp, #-16]!"   "\n"
"   stp  x21, x22, [sp, #-16]!"   "\n"
"   stp  x19, x20, [sp, #-16]!"   "\n"
"   stp  x0,  xzr, [sp, #-16]!"   "\n"
"   adrp x0, block"               "\n"
"   add  x0, x0, :lo12:block"     "\n"
"   ldr  x21, [x0, #8]"           "\n"  
"   ldr  x1,  [x0, #0]"           "\n"  
"   br   x1"                 "\n"  

"disp_chain_assisted:"            "\n" 
"   mov  x1, x21" "\n"
    
"   ldp  x0,  xzr, [sp], #16"    "\n"
"   ldp  x19, x20, [sp], #16"    "\n"
"   ldp  x21, x22, [sp], #16"    "\n"
"   ldp  x23, x24, [sp], #16"    "\n"
"   ldp  x25, x26, [sp], #16"    "\n"
"   ldp  x27, x28, [sp], #16"    "\n"
"   ldp  x29, x30, [sp], #16"    "\n"
"   mov  x0, x1"                 "\n"
"   ret"                         "\n"
);

#elif defined(__i386__)

asm(
"run_translation_asm:\n"
"   pushal\n"
"   movl gp, %ebp\n"
"   movl f, %eax\n"
"   call *%eax\n"
"   movl %eax, res\n"
"   popal\n"
"   ret\n"
);

#else
# error "Unknown arch"
#endif


HWord run_translation ( HWord translation )
{
   if (0 && DEBUG_TRACE_FLAGS) {
      printf(" run translation %p\n", (void*)translation );
      printf(" simulated bb: %llu\n", n_bbs_done);
   }
   block[0] = translation;
   block[1] = (HWord)&gst;
   HWord trc = run_translation_asm();
   n_bbs_done ++;
   return trc;
}

HWord find_translation ( Addr guest_addr )
{
   Int i;
   HWord __res;
   if (0)
     printf("find translation %p ... ", (void *)(guest_addr));
   for (i = 0; i < trans_table_used; i++)
     if (trans_table[i].base[0] == guest_addr)
        break;
   if (i == trans_table_used) {
      if (0) printf("none\n");
      return 0; 
   }

   if (i > 2) {
      VexGuestExtents tmpE = trans_table[i-1];
      ULong*          tmpP = trans_tableP[i-1];
      trans_table[i-1]  = trans_table[i];
      trans_tableP[i-1] = trans_tableP[i];
      trans_table[i] = tmpE;
      trans_tableP[i] = tmpP;
      i--;
   }

   __res = (HWord)trans_tableP[i];
   if (0) printf("%p\n", (void*)__res);
   return __res;
}

#define N_TRANSBUF 5000
static UChar transbuf[N_TRANSBUF];
void make_translation ( Addr guest_addr, Bool verbose )
{
   VexTranslateArgs   vta;
   VexTranslateResult tres;
   VexArchInfo vex_archinfo;
   Int trans_used, i, ws_needed;

   memset(&vta, 0, sizeof(vta));
   memset(&tres, 0, sizeof(tres));
   memset(&vex_archinfo, 0, sizeof(vex_archinfo));

   if (trans_table_used >= N_TRANS_TABLE
       || trans_cache_used >= N_TRANS_CACHE-1000) {
      trans_cache_used = 0;
      trans_table_used = 0;
   }

   assert(trans_table_used < N_TRANS_TABLE);
   if (0)
     printf("make translation %p\n", (void *)guest_addr);

   LibVEX_default_VexArchInfo(&vex_archinfo);
   
   

   
   vta.arch_guest       = VexArch;
   vta.archinfo_guest   = vex_archinfo;
   vta.arch_host        = VexArch;
   vta.archinfo_host    = vex_archinfo;
   vta.guest_bytes      = (UChar*)guest_addr;
   vta.guest_bytes_addr = guest_addr;
   vta.chase_into_ok    = chase_into_ok;
   vta.guest_extents    = &trans_table[trans_table_used];
   vta.host_bytes       = transbuf;
   vta.host_bytes_size  = N_TRANSBUF;
   vta.host_bytes_used  = &trans_used;
   vta.instrument1      = NULL;
   vta.instrument2      = NULL;
   vta.needs_self_check = needs_self_check;
   vta.traceflags       = verbose ? TEST_FLAGS : DEBUG_TRACE_FLAGS;

   vta.disp_cp_chain_me_to_slowEP = NULL; 
   vta.disp_cp_chain_me_to_fastEP = NULL; 
   vta.disp_cp_xindir             = NULL; 
   vta.disp_cp_xassisted          = disp_chain_assisted;

   vta.addProfInc       = False;

   tres = LibVEX_Translate ( &vta );

   assert(tres.status == VexTransOK);
   assert(tres.offs_profInc == -1);

   ws_needed = (trans_used+7) / 8;
   assert(ws_needed > 0);
   assert(trans_cache_used + ws_needed < N_TRANS_CACHE);
   n_translations_made++;

   for (i = 0; i < trans_used; i++) {
      HChar* dst = ((HChar*)(&trans_cache[trans_cache_used])) + i;
      HChar* src = (HChar*)(&transbuf[i]);
      *dst = *src;
   }

#if defined(__aarch64__)
   invalidate_icache( &trans_cache[trans_cache_used], trans_used );
#endif

   trans_tableP[trans_table_used] = &trans_cache[trans_cache_used];
   trans_table_used++;
   trans_cache_used += ws_needed;
}


__attribute__((unused))
static Bool overlap ( Addr start, UInt len, VexGuestExtents* vge )
{
   Int i;
   for (i = 0; i < vge->n_used; i++) {
     if (vge->base[i]+vge->len[i] <= start
         || vge->base[i] >= start+len) {
       
     } else {
        return True;
     }
   }
   return False; 
}

static ULong  stopAfter = 0;
static UChar* entryP    = NULL;


__attribute__ ((noreturn))
static
void failure_exit ( void )
{
   fprintf(stdout, "VEX did failure_exit.  Bye.\n");
   fprintf(stdout, "bb counter = %llu\n\n", n_bbs_done);
   exit(1);
}

static
void log_bytes ( HChar* bytes, Int nbytes )
{
   fwrite ( bytes, 1, nbytes, stdout );
   fflush ( stdout );
}


static void run_simulator ( void )
{
   static Addr last_guest = 0;
   Addr  next_guest;
   HWord next_host;
   while (1) {
      next_guest = gst.GuestPC;

      if (0)
         printf("\nnext_guest: 0x%x\n", (UInt)next_guest);

      if (next_guest == (Addr)&serviceFn) {

         
#        if defined(__i386__)
         {
            HWord esp = gst.guest_ESP;
            gst.guest_EIP = *(UInt*)(esp+0);
            gst.guest_EAX = serviceFn( *(UInt*)(esp+4), *(UInt*)(esp+8) );
            gst.guest_ESP = esp+4;
            next_guest = gst.guest_EIP;
         }
#        elif defined(__aarch64__)
         {
            gst.guest_X0 = serviceFn( gst.guest_X0, gst.guest_X1 );
            gst.guest_PC = gst.guest_X30;
            next_guest   = gst.guest_PC;
         }
#        else
#        error "Unknown arch"
#        endif
      }

      next_host = find_translation(next_guest);
      if (next_host == 0) {
         make_translation(next_guest,False);
         next_host = find_translation(next_guest);
         assert(next_host != 0);
      }

      
      if (n_bbs_done == stopAfter) {
         printf("---begin SWITCHBACK at bb:%llu---\n", n_bbs_done);
#if 1
         if (last_guest) {
            printf("\n*** Last run translation (bb:%llu):\n", n_bbs_done-1);
            make_translation(last_guest,True);
         }
#endif
#if 0
         if (next_guest) {
            printf("\n*** Current translation (bb:%llu):\n", n_bbs_done);
            make_translation(next_guest,True);
         }
#endif
         printf("---  end SWITCHBACK at bb:%llu ---\n", n_bbs_done);
         switchback();
         assert(0); 
      }

      last_guest = next_guest;
      HWord trc = run_translation(next_host);
      if (0) printf("------- trc = %lu\n", trc);
      if (trc != VEX_TRC_JMP_BORING) {
        if (1) printf("------- trc = %lu\n", trc);
      }
      assert(trc == VEX_TRC_JMP_BORING);
   }
}


static void usage ( void )
{
   printf("usage: switchback #bbs\n");
   printf("   - begins switchback for basic block #bbs\n");
   printf("   - use -1 for largest possible run without switchback\n\n");
   exit(1);
}


int main ( Int argc, HChar** argv )
{
   if (argc != 2)
      usage();

   stopAfter = (ULong)atoll(argv[1]);

   extern void entry ( void*(*service)(int,int) );
   entryP = (UChar*)&entry;

   if (!entryP) {
      printf("switchback: can't find entry point\n");
      exit(1);
   }

   LibVEX_default_VexControl(&vcon);
   vcon.guest_max_insns=50 - 49;
   vcon.guest_chase_thresh=0;
   vcon.iropt_level=2;

   LibVEX_Init( failure_exit, log_bytes, 1, &vcon );
   LibVEX_Guest_initialise(&gst);
   gst.host_EvC_COUNTER  = 999999999; 
   gst.host_EvC_FAILADDR = 0x5a5a5a5a5a5a5a5a;

#  if defined(__i386__)
   gst.guest_EIP = (UInt)entryP;
   gst.guest_ESP = (UInt)&gstack[32000];
   *(UInt*)(gst.guest_ESP+4) = (UInt)serviceFn;
   *(UInt*)(gst.guest_ESP+0) = 0x12345678;

#  elif defined(__aarch64__)
   gst.guest_PC = (ULong)entryP;
   gst.guest_SP = (ULong)&gstack[32000];
   gst.guest_X0 = (ULong)serviceFn;
   HWord tpidr_el0 = 0;
   __asm__ __volatile__("mrs %0, tpidr_el0" : "=r"(tpidr_el0));
   gst.guest_TPIDR_EL0 = tpidr_el0;

#  else
#  error "Unknown arch"
#  endif

   printf("\n---START---\n");

#if 1
   run_simulator();
#else
   ( (void(*)(HWord(*)(HWord,HWord))) entryP ) (serviceFn);
#endif


   return 0;
}
