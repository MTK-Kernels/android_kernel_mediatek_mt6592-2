

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "tests/malloc.h"

typedef  unsigned char  UChar;

typedef  
   struct { __attribute__((aligned(16))) UChar b[16]; }
   UWord128;

typedef
   struct { UWord128 reg[16]; }
   XMMRegs;

typedef
   struct { UWord128 dqw[5]; }
   Mem;

void pp_UWord128 ( UWord128* w ) {
   int i;
   char buf[3];
   for (i = 15; i >= 0; i--) {
      buf[2] = 0;
      sprintf(buf, "%02x", (unsigned int)w->b[i]);
      assert(buf[2] == 0);
      if (buf[0] == '0') buf[0] = '.';
      if (buf[1] == '0') buf[1] = '.';
      printf("%s", buf);
   }
}

void pp_XMMRegs ( char* who, XMMRegs* regs ) {
   int i;
   printf ("%s (xmms in order [15..0]) {\n", who );
   for (i = 0; i < 16; i++) {
      printf("  %%xmm%2d ", i);
      pp_UWord128( &regs->reg[i] );
      printf("\n");
   }
   printf("}\n");
}

void pp_Mem ( char* who, Mem* mem ) {
   int i;
   printf ("%s (dqws in order [15 .. 0]) {\n", who );
   for (i = 0; i < 5; i++) {
      printf("  [%d]    ", i);
      pp_UWord128( &mem->dqw[i] );
      printf("\n");
   }
   printf("}\n");
}

void xor_UWord128( UWord128* src, UWord128* dst ) {
   int i;
   for (i = 0; i < 16; i++)
      dst->b[i] ^= src->b[i];
}
void xor_XMMRegs ( XMMRegs* src, XMMRegs* dst ) {
   int i;
   for (i = 0; i < 16; i++)
      xor_UWord128( &src->reg[i], &dst->reg[i] );
}

void xor_Mem ( Mem* src, Mem* dst ) {
   int i;
   for (i = 0; i < 5; i++)
      xor_UWord128( &src->dqw[i], &dst->dqw[i] );
}

void setup_regs_mem ( XMMRegs* regs, Mem* mem ) {
   int ctr, i, j;
   ctr = 0;
   for (i = 0; i < 16; i++) {
      for (j = 0; j < 16; j++)
        regs->reg[i].b[j] = 0x51 + (ctr++ % 7);
   }
   for (i = 0; i < 5; i++) {
      for (j = 0; j < 16; j++)
        mem->dqw[i].b[j] = 0x52 + (ctr++ % 13);
   }
}

void before_test ( XMMRegs* regs, Mem* mem ) {
   setup_regs_mem( regs, mem );
}

void after_test ( char* who, XMMRegs* regs, Mem* mem ) {
   XMMRegs rdiff;
   Mem     mdiff;
   char s[128];
   setup_regs_mem( &rdiff, &mdiff );
   xor_XMMRegs( regs, &rdiff );
   xor_Mem( mem, &mdiff );
   sprintf(s, "after \"%s\"", who );
   pp_Mem( s, &mdiff );
   pp_XMMRegs( s, &rdiff );
   printf("\n");
}

#define LOAD_XMMREGS_from_r14       \
   "\tmovupd   0(%%r14),  %%xmm0\n" \
   "\tmovupd  16(%%r14),  %%xmm1\n" \
   "\tmovupd  32(%%r14),  %%xmm2\n" \
   "\tmovupd  48(%%r14),  %%xmm3\n" \
   "\tmovupd  64(%%r14),  %%xmm4\n" \
   "\tmovupd  80(%%r14),  %%xmm5\n" \
   "\tmovupd  96(%%r14),  %%xmm6\n" \
   "\tmovupd 112(%%r14),  %%xmm7\n" \
   "\tmovupd 128(%%r14),  %%xmm8\n" \
   "\tmovupd 144(%%r14),  %%xmm9\n" \
   "\tmovupd 160(%%r14), %%xmm10\n" \
   "\tmovupd 176(%%r14), %%xmm11\n" \
   "\tmovupd 192(%%r14), %%xmm12\n" \
   "\tmovupd 208(%%r14), %%xmm13\n" \
   "\tmovupd 224(%%r14), %%xmm14\n" \
   "\tmovupd 240(%%r14), %%xmm15\n"

#define SAVE_XMMREGS_to_r14         \
   "\tmovupd %%xmm0,    0(%%r14)\n" \
   "\tmovupd %%xmm1,   16(%%r14)\n" \
   "\tmovupd %%xmm2,   32(%%r14)\n" \
   "\tmovupd %%xmm3,   48(%%r14)\n" \
   "\tmovupd %%xmm4,   64(%%r14)\n" \
   "\tmovupd %%xmm5,   80(%%r14)\n" \
   "\tmovupd %%xmm6,   96(%%r14)\n" \
   "\tmovupd %%xmm7,  112(%%r14)\n" \
   "\tmovupd %%xmm8,  128(%%r14)\n" \
   "\tmovupd %%xmm9,  144(%%r14)\n" \
   "\tmovupd %%xmm10, 160(%%r14)\n" \
   "\tmovupd %%xmm11, 176(%%r14)\n" \
   "\tmovupd %%xmm12, 192(%%r14)\n" \
   "\tmovupd %%xmm13, 208(%%r14)\n" \
   "\tmovupd %%xmm14, 224(%%r14)\n" \
   "\tmovupd %%xmm15, 240(%%r14)"

#define XMMREGS \
   "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", \
   "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"

#if 0
   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rx\n"
       "\t.byte 0x\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -x + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "x"
     );
     after_test( "", regs, mem );
   }
#endif

int main ( void )
{
   XMMRegs* regs;
   Mem*     mem;
   regs = memalign16(sizeof(XMMRegs) + 16);
   mem  = memalign16(sizeof(Mem) + 16);

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%r8\n"
       "\t.byte 0x66,0x49,0x0f,0x58,0x48,0x00\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "r8"
     );
     after_test( "rex.WB addpd  0x0(%r8),%xmm1", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rdi\n"
       "\t.byte 0xf2,0x48,0x0f,0x58,0x27\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rdi"
     );
     after_test( "rex.W addsd  (%rdi),%xmm4", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rdx\n"
       "\t.byte 0x66,0x48,0x0f,0x28,0x0a\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rdx"
     );
     after_test( "rex.W movapd (%rdx),%xmm1", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rdx\n"
       "\t.byte 0x66,0x48,0x0f,0x29,0x0a\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rdx"
     );
     after_test( "rex.W movapd %xmm1,(%rdx)", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rdx\n"
       "\t.byte 0x48,0x0f,0x28,0x42,0x30\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0x30 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rdx"
     );
     after_test( "movaps 0x30(%rdx),%xmm0", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%r8\n"
       "\t.byte 0x49,0x0f,0x29,0x48,0x00\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "r8"
     );
     after_test( "rex.WB movaps %xmm1,0x0(%r8)", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rdx\n"
       "\t.byte 0xf2,0x48,0x0f,0x12,0x2a\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rdx"
     );
     after_test( "movddup (%rdx),%xmm5", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rsi\n"
       "\t.byte 0x66,0x48,0x0f,0x16,0x06\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rsi"
     );
     after_test( "rex.W movhpd (%rsi),%xmm0", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rdi\n"
       "\t.byte 0x66,0x48,0x0f,0x17,0x07\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rdi"
     );
     after_test( "rex.W movhpd %xmm0,(%rdi)", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rsi\n"
       "\t.byte 0x48,0x0f,0x16,0x36\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rsi"
     );
     after_test( "rex.W movhps (%rsi),%xmm6", regs, mem );
   }
   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%r11\n"
       "\t.byte 0x49,0x0F,0x17,0x03\n" 
       SAVE_XMMREGS_to_r14
         :  :  "r"(regs), "r"( 0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "r11"
     );
     after_test( "rex.WB movhps %xmm0,(%r11)", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rdx\n"
       "\t.byte 0x66,0x48,0x0f,0x12,0x4a,0x00\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rdx"
     );
     after_test( "rex.W movlpd 0x0(%rdx),%xmm1", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rax\n"
       "\t.byte 0x66,0x48,0x0f,0x13,0x30\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rax"
     );
     after_test( "rex.W movlpd %xmm6,(%rax)", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rdi\n"
       "\t.byte 0x48,0x0f,0x12,0x07\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rdi"
     );
     after_test( "rex.W movlps (%rdi),%xmm0", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%r10\n"
       "\t.byte 0x49,0x0f,0x13,0x02\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "r10"
     );
     after_test( "rex.WB movlps %xmm0,(%r10)", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rax\n"
       "\t.byte 0xf3,0x48,0x0f,0x7e,0x00\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rax"
     );
     after_test( "rex.W movq (%rax),%xmm0", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rax\n"
       "\t.byte 0x66,0x48,0x0f,0xd6,0x00\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rax"
     );
     after_test( "rex.W movq %xmm0,(%rax)", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rcx\n"
       "\t.byte 0xf2,0x48,0x0f,0x10,0x11\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rcx"
     );
     after_test( "rex.W movsd (%rcx),%xmm2", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rdi\n"
       "\t.byte 0xf2,0x48,0x0f,0x11,0x3f\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rdi"
     );
     after_test( "rex.W movsd %xmm7,(%rdi)", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rsi\n"
       "\t.byte 0xf3,0x48,0x0f,0x10,0x5e,0x04\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0x4 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rsi"
     );
     after_test( "rex.W movss 0x4(%rsi),%xmm3", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rdi\n"
       "\t.byte 0x66,0x48,0x0f,0x11,0x07\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rdi"
     );
     after_test( "rex.W movupd %xmm0,(%rdi)", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rcx\n"
       "\t.byte 0x66,0x48,0x0f,0x59,0x61,0x00\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rcx"
     );
     after_test( "rex.W mulpd 0x0(%rcx),%xmm4", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%rdi\n"
       "\t.byte 0xf2,0x48,0x0f,0x59,0x1f\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( -0 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "rdi"
     );
     after_test( "rex.W mulsd (%rdi),%xmm3", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%r10\n"
       "\txorq %%rsi, %%rsi\n"
       "\t.byte 0x49,0x0f,0x18,0x4c,0xf2,0xa0\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( - -0x60 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "r10","rsi"
     );
     after_test( "rex.WB prefetcht0 -0x60(%r10,%rsi,8)", regs, mem );
   }

   
   {
     before_test( regs, mem );
     __asm__ __volatile__(
         "movq %0, %%r14\n"
       "\tmovq %1, %%r15\n"
       LOAD_XMMREGS_from_r14
       "\tmovq %%r15, %%r13\n"
       "\t.byte 0xf2,0x49,0x0f,0x5c,0x4d,0xf8\n"
       SAVE_XMMREGS_to_r14
          :  :  "r"(regs), "r"( - -0x8 + (char*)&mem->dqw[2] )
                    :  "r14","r15","memory", XMMREGS,
                                "r13"
     );
     after_test( "rex.WB subsd  -0x8(%r13),%xmm1", regs, mem );
   }

   free(regs);
   free(mem);
   return 0;
}
