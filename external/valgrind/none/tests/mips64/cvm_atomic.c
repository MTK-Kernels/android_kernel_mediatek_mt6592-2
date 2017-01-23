#include <stdio.h>

#define N 256

unsigned long long reg_val_double[N];

void init_reg_val_double()
{
   unsigned long c = 19650218UL;
   int i;
   reg_val_double[0]= c & 0xffffffffUL;
   for (i = 1; i < N; i++) {
      reg_val_double[i] = (1812433253UL * (reg_val_double[i - 1] ^
                          (reg_val_double[i - 1] >> 30)) + i);
   }
}


unsigned long long reg_val_double_copy[N]; 

void copy_reg_val_double()
{
   int i;
   for (i = 0; i < N; i++) {
      reg_val_double_copy[i] = reg_val_double[i];
   }
}

#define TEST1_32(instruction, offset,mem)                    \
{                                                            \
   unsigned long out = 0;                                    \
   unsigned long res_mem = 0;                                \
   __asm__ volatile(                                         \
     "move         $t0, %2"        "\n\t"                    \
     "move         $t1, %3"        "\n\t"                    \
     "daddu        $t0, $t1, $t0"  "\n\t"                    \
     instruction " $t3, ($t0)"     "\n\t"                    \
     "move         %0,  $t3"       "\n\t"                    \
     "lw           %1,  0($t0)"    "\n\t"                    \
     : "=&r" (out), "=&r"(res_mem)                           \
     : "r" (mem) , "r" (offset)                              \
     : "$12", "$13", "cc", "memory"                          \
     );                                                      \
   printf("%s :: offset: 0x%x, out: 0x%lx, result:0x%lx\n",  \
          instruction, offset, out, res_mem);                \
}

#define TEST1_64(instruction, offset,mem)                     \
{                                                             \
   unsigned long out = 0;                                     \
   unsigned long res_mem = 0;                                 \
   __asm__ volatile(                                          \
     "move         $t0, %2"        "\n\t"                     \
     "move         $t1, %3"        "\n\t"                     \
     "daddu        $t0, $t1, $t0"  "\n\t"                     \
     instruction " $t3, ($t0)"     "\n\t"                     \
     "move         %0,  $t3"       "\n\t"                     \
     "ld           %1,  0($t0)"    "\n\t"                     \
     : "=&r" (out), "=&r"(res_mem)                            \
     : "r" (mem) , "r" (offset)                               \
     : "$12", "$13", "cc", "memory"                           \
     );                                                       \
   printf("%s :: offset: 0x%x, out: 0x%lx, result: 0x%lx\n",  \
          instruction, offset, out, res_mem);                 \
}

#define TEST2(instruction, RSVal, RTVal)                            \
{                                                                   \
   unsigned long out;                                               \
   __asm__ volatile(                                                \
      "move $t1, %1"  "\n\t"                                        \
      "move $t2, %2"  "\n\t"                                        \
      instruction     "\n\t"                                        \
      "move %0, $t3"  "\n\t"                                        \
      : "=&r" (out)                                                 \
      : "r" (RSVal), "r" (RTVal)                                    \
      : "$12", "$13", "cc", "memory"                                \
        );                                                          \
   printf("%s :: rd 0x%lx, rs 0x%llx, rt 0x%llx\n",                 \
          instruction, out, (long long) RSVal, (long long) RTVal);  \
}

#define TEST3(instruction, offset, mem, value)                   \
{                                                                \
    unsigned long out = 0;                                       \
    unsigned long outPre = 0;                                    \
   __asm__ volatile(                                             \
     "move         $t0, %2"        "\n\t"                        \
     "move         $t1, %3"        "\n\t"                        \
     "daddu        $t0, $t1, $t0"  "\n\t"                        \
     "ld           %1,  0($t0)"    "\n\t"                        \
     "move         $t2, %4"        "\n\t"                        \
     instruction " $t2, ($t0)"     "\n\t"                        \
     "ld           %0,  0($t0)"    "\n\t"                        \
     : "=&r" (out), "=&r" (outPre)                               \
     : "r" (mem) , "r" (offset), "r" (value)                     \
     : "$12", "$13", "$14", "cc", "memory"                       \
     );                                                          \
     printf("%s :: value: 0x%llx, memPre: 0x%lx, mem: 0x%lx\n",  \
            instruction, value, outPre, out);                    \
}

#define TEST4_32(instruction, offset, mem)                   \
{                                                            \
    unsigned long out = 0;                                   \
    unsigned long res_mem = 0;                               \
   __asm__ volatile(                                         \
      "move         $t0, %2"          "\n\t"                 \
      "move         $t1, %3"          "\n\t"                 \
      "daddu        $t0, $t0, $t1"    "\n\t"                 \
      instruction " $t3, ($t0), $t1"  "\n\t"                 \
      "move         %0,  $t3"         "\n\t"                 \
      "lw           %1,  0($t0)"      "\n\t"                 \
      : "=&r" (out), "=&r"(res_mem)                          \
      : "r" (mem) , "r" (offset)                             \
      : "$12", "$13", "cc", "memory"                         \
     );                                                      \
   printf("%s :: offset: 0x%x, out: 0x%lx, result:0x%lx\n",  \
          instruction, offset, out, res_mem);                \
}

#define TEST4_64(instruction, offset, mem)                    \
{                                                             \
    unsigned long out = 0;                                    \
    unsigned long res_mem = 0;                                \
   __asm__ volatile(                                          \
      "move         $t0, %2"          "\n\t"                  \
      "move         $t1, %3"          "\n\t"                  \
      "daddu        $t0, $t0,   $t1"  "\n\t"                  \
      instruction " $t3, ($t0), $t1"  "\n\t"                  \
      "move         %0,  $t3"         "\n\t"                  \
      "ld           %1,  0($t0)"      "\n\t"                  \
     : "=&r" (out), "=&r"(res_mem)                            \
     : "r" (mem) , "r" (offset)                               \
     : "$12", "$13", "cc", "memory"                           \
     );                                                       \
   printf("%s :: offset: 0x%x, out: 0x%lx, result: 0x%lx\n",  \
          instruction, offset, out, res_mem);                 \
}

typedef enum {
   BADDU, POP, DPOP, SAA, SAAD, LAA, LAAD, LAW, LAWD, LAI, LAID, LAD, LADD,
   LAS, LASD, LAC, LACD
} cvm_op;

int main()
{
#if (_MIPS_ARCH_OCTEON2)
   init_reg_val_double();
   int i,j;
   cvm_op op;
   for (op = BADDU; op <= LACD; op++) {
      switch(op){
         
         case BADDU: {
            for(i = 4; i < N; i += 4)
               for(j = 4; j < N; j += 4)
                  TEST2("baddu $t3, $t1, $t2", reg_val_double[i],
                                               reg_val_double[j]);
            break;
         }
         case POP: {  
            for(j = 4; j < N; j += 4)
               TEST2("pop $t3, $t1", reg_val_double[j], 0);
            break;
         }
         case DPOP: {  
            for(j = 8; j < N; j += 8)
               TEST2("dpop $t3, $t1", reg_val_double[j], 0);
            break;
         }
         case SAA: {  
            copy_reg_val_double();
            for(j = 4; j < N; j += 4)
               TEST3("saa", j, reg_val_double_copy, reg_val_double[j]);
            break;
         }
         case SAAD: {  
            copy_reg_val_double();
            for(j = 8; j < N; j += 8)
               TEST3("saad", j, reg_val_double_copy, reg_val_double[j]);
            break;
         }
         case LAA: {  
            copy_reg_val_double();
            for(j = 4; j < N; j += 4)
               TEST4_32("laa", j, reg_val_double_copy);
            break;
         }
         case LAAD: {  
            copy_reg_val_double();
            for(j = 8; j < N; j += 8)
               TEST4_64("laad ", j, reg_val_double_copy);
            break;
         }
         case LAW: {  
            copy_reg_val_double();
            for(j = 4; j < N; j += 4)
               TEST4_32("law", j, reg_val_double_copy);
            break;
         }
         case LAWD: {  
            copy_reg_val_double();
            for(j = 8; j < N; j += 8)
               TEST4_64("lawd", j, reg_val_double_copy);
            break;
         }
         case LAI: {  
            copy_reg_val_double();
            for(i = 4; i < N; i += 4)
               TEST1_32("lai", i, reg_val_double_copy);
            break;
         }
         case LAID: {  
            copy_reg_val_double();
            for(i = 8; i < N; i += 8)
              TEST1_64("laid ", i, reg_val_double_copy);
            break;
         }
         case LAD: {  
            copy_reg_val_double();
            for(i = 4; i < N; i += 4)
               TEST1_32("lad", i, reg_val_double_copy);
            break;
         }
         case LADD: {  
            copy_reg_val_double();
            for(i = 8; i < N; i += 8)
               TEST1_64("ladd",i, reg_val_double_copy);
            break;
         }
         case LAS:{   
            copy_reg_val_double();
            for(i = 4; i < N; i += 4)
               TEST1_32("las",i, reg_val_double_copy);
            break;
         }
         case LASD:{  
            copy_reg_val_double();
            for(i = 8; i < N; i += 8)
               TEST1_64("lasd",i, reg_val_double_copy);
            break;
         }
         case LAC: {  
            copy_reg_val_double();
            for(i = 4; i < N; i += 4)
               TEST1_32("lac",i, reg_val_double_copy);
            break;
         }
         case LACD: {  
            copy_reg_val_double();
            for(i = 8; i < N; i += 8)
               TEST1_64("lacd",i, reg_val_double_copy);
            break;
         }
         default:
            printf("Nothing to be executed \n");
      }
   }
#endif
   return 0;
}
