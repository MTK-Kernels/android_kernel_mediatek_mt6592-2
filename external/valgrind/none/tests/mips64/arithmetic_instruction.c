#include <stdio.h>
#include "const.h"
#include "macro_int.h"

typedef enum {
   ADD=0,  ADDI,   ADDIU,  ADDU,
   CLO,    CLZ,    DADD,   DADDI,
   DADDIU, DADDU,  DCLO,   DCLZ,
   DDIV,   DDIVU,  DIV,    DIVU,
   DMULT,  DMULTU, DSUB,   DSUBU,
   MADD,   MADDU,  MSUB,   MSUBU,
   MUL,    MULT,   MULTU,  MOVN,
   MOVZ,   SEB,    SEH,    SLT,
   SLTI,   SLTIU,  SLTU,   SUB,
   SUBU
} arithmetic_op;

int main()
{
   arithmetic_op op;
   int i;
   init_reg_val2();

   for (op = ADD; op <= SUBU; op++) {
      for (i = 0; i < N; i++) {
         switch(op) {
            case ADD:
               TEST1("add $t0, $t1, $t2", reg_val1[i], reg_val1[N-i-1],
                                          t0, t1, t2);
               break;

            case ADDI:
               TEST2("addi $t0, $t1, 0xff",   reg_val1[i], 0xff,   t0, t1);
               TEST2("addi $t2, $t3, 0xffff", reg_val1[i], 0xffff, t2, t3);
               TEST2("addi $a0, $a1, 0x0",    reg_val1[i], 0x0,    a0, a1);
               TEST2("addi $s0, $s1, 0x23",   reg_val1[i], 0x23,   s0, s1);
               break;

            case ADDIU:
               TEST2("addiu $t0, $t1, 0xff",   reg_val1[i], 0xff,   t0, t1);
               TEST2("addiu $t2, $t3, 0xffff", reg_val1[i], 0xffff, t2, t3);
               TEST2("addiu $a0, $a1, 0x0",    reg_val1[i], 0x0,    a0, a1);
               TEST2("addiu $s0, $s1, 0x23",   reg_val1[i], 0x23,   s0, s1);
               break;

            case ADDU:
               TEST1("addu $t0, $t1, $t2", reg_val1[i], reg_val1[N-i-1],
                                           t0, t1, t2);
               break;

            case CLO:
               TEST3("clo $t0, $t1", reg_val1[i], t0, t1);
               break;

            case CLZ:
               TEST3("clz $t0, $t1", reg_val1[i], t0, t1);
               break;

            case DADD:
               TEST1("dadd $t0, $t1, $t2", reg_val1[i], reg_val1[N-i-1],
                                           t0, t1, t2);
               break;

            case DADDI:
               TEST2("daddi $t0, $t1, 0xff",   reg_val1[i], 0xff,   t0, t1);
               TEST2("daddi $t2, $t3, 0xffff", reg_val1[i], 0xffff, t2, t3);
               TEST2("daddi $a0, $a1, 0x0",    reg_val1[i], 0x0,    a0, a1);
               TEST2("daddi $s0, $s1, 0x23",   reg_val1[i], 0x23,   s0, s1);
               TEST2("daddi $t0, $t1, 0xff",   reg_val2[i], 0xff,   t0, t1);
               TEST2("daddi $t2, $t3, 0xffff", reg_val2[i], 0xffff, t2, t3);
               TEST2("daddi $a0, $a1, 0x0",    reg_val2[i], 0x0,    a0, a1);
               TEST2("daddi $s0, $s1, 0x23",   reg_val2[i], 0x23,   s0, s1);
               break;

            case DADDIU:
               TEST2("daddiu $t0, $t1, 0xff",   reg_val1[i], 0xff,   t0, t1);
               TEST2("daddiu $t2, $t3, 0xffff", reg_val1[i], 0xffff, t2, t3);
               TEST2("daddiu $a0, $a1, 0x0",    reg_val1[i], 0x0,    a0, a1);
               TEST2("daddiu $s0, $s1, 0x23",   reg_val1[i], 0x23,   s0, s1);
               TEST2("daddiu $t0, $t1, 0xff",   reg_val2[i], 0xff,   t0, t1);
               TEST2("daddiu $t2, $t3, 0xffff", reg_val2[i], 0xffff, t2, t3);
               TEST2("daddiu $a0, $a1, 0x0",    reg_val2[i], 0x0,    a0, a1);
               TEST2("daddiu $s0, $s1, 0x23",   reg_val2[i], 0x23,   s0, s1);
               break;

            case DADDU:
               TEST1("daddu $t0, $t1, $t2", reg_val1[i], reg_val1[N-i-1],
                                            t0, t1, t2);
               TEST1("daddu $s0, $s1, $s2", reg_val2[i], reg_val2[N-i-1],
                                            s0, s1, s2);
               break;

            case DCLO:
               
               TEST3("dclo $t0, $t1", reg_val1[i], t0, t1);
               TEST3("dclo $v0, $v1", reg_val2[i], v0, v1);
               break;

            case DCLZ:
               
               TEST3("dclz $t0, $t1", reg_val1[i], t0, t1);
               TEST3("dclz $v0, $v1", reg_val2[i], v0, v1);
               break;

            case DDIV:
               if (reg_val1[N-i-1] != 0)
                  TEST4("ddiv $t0, $t1", reg_val1[i], reg_val1[N-i-1], t0, t1);

               if (reg_val2[N-i-1] != 0)
                  TEST4("ddiv $v0, $v1", reg_val2[i], reg_val2[N-i-1], v0, v1);

               break;

            case DDIVU:
               if (reg_val1[N-i-1] != 0)
                  TEST4("ddivu $t0, $t1", reg_val1[i], reg_val1[N-i-1], t0, t1);

               if (reg_val2[N-i-1] != 0)
                  TEST4("ddivu $v0, $v1", reg_val2[i], reg_val2[N-i-1], v0, v1);

               break;

            case DIV:
               if (reg_val1[N-i-1] != 0)
                  TEST4("div $t0, $t1", reg_val1[i], reg_val1[N-i-1], t0, t1);

               break;

            case DIVU:
               if (reg_val1[N-i-1] != 0)
                  TEST4("divu $t0, $t1", reg_val1[i], reg_val1[N-i-1], t0, t1);

               break;

            case DMULT:
               
               TEST4("dmult $t0, $t1", reg_val1[i], reg_val1[N-i-1], t0, t1);
               TEST4("dmult $v0, $v1", reg_val2[i], reg_val2[N-i-1], v0, v1);
               break;

            case DMULTU:
               
               TEST4("dmultu $t0, $t1", reg_val1[i], reg_val1[N-i-1], t0, t1);
               TEST4("dmultu $v0, $v1", reg_val2[i], reg_val2[N-i-1], v0, v1);
               break;

            case DSUB:
               TEST1("dsub $t0, $t1, $t2", reg_val1[i], reg_val1[N-i-1],
                                           t0, t1, t2);
               break;

            case DSUBU:
               TEST1("dsubu $t0, $t1, $t2", reg_val1[i], reg_val1[N-i-1],
                                            t0, t1, t2);
               TEST1("dsubu $s0, $s1, $s2", reg_val2[i], reg_val2[N-i-1],
                                            s0, s1, s2);
               break;

            case MADD:
               TEST5("madd $t0, $t1", reg_val1[i], reg_val1[N-i-1], t0, t1);
               break;

            case MADDU:
               TEST5("maddu $t0, $t1", reg_val1[i], reg_val1[N-i-1], t0, t1);
               break;

            case MSUB:
               TEST5("msub $t0, $t1", reg_val1[i], reg_val1[N-i-1], t0, t1);
               break;

            case MSUBU:
               TEST5("msubu $t0, $t1", reg_val1[i], reg_val1[N-i-1], t0, t1);
               break;

            case MUL:
               TEST1("mul $t0, $t1, $t2", reg_val1[i], reg_val1[N-i-1],
                                          t0, t1, t2);
               break;

            case MULT:
               TEST4("mult $t0, $t1", reg_val1[i], reg_val1[N-i-1], t0, t1);
               break;

            case MULTU:
               TEST4("multu $t0, $t1", reg_val1[i], reg_val1[N-i-1], t0, t1);
               break;

            case MOVN:
               TEST1("movn $t0, $t1, $t2", reg_val1[i], reg_val1[N-i-1],
                                           t0, t1, t2);
               TEST1("movn $s0, $s1, $s2", reg_val2[i], reg_val2[N-i-1],
                                           s0, s1, s2);
               break;

            case MOVZ:
               TEST1("movz $t0, $t1, $t2", reg_val1[i], reg_val1[N-i-1],
                                           t0, t1, t2);
               TEST1("movz $s0, $s1, $s2", reg_val2[i], reg_val2[N-i-1],
                                           s0, s1, s2);
               break;

            case SEB:
#if (__mips==64) && (__mips_isa_rev>=2)
               TEST3("seb $t0, $t1", reg_val1[i], t0, t1);
#endif
               break;

            case SEH:
#if (__mips==64) && (__mips_isa_rev>=2)
               TEST3("seh $t0, $t1", reg_val1[i], t0, t1);
#endif
               break;

            case SLT:
               TEST1("slt $t0, $t1, $t2", reg_val1[i], reg_val1[N-i-1],
                                          t0, t1, t2);
               break;

            case SLTI:
               TEST2("slti $t0, $t1, 0xff",   reg_val1[i], 0xff,   t0, t1);
               TEST2("slti $t2, $t3, 0xffff", reg_val1[i], 0xffff, t2, t3);
               TEST2("slti $a0, $a1, 0x0",    reg_val1[i], 0x0,    a0, a1);
               TEST2("slti $s0, $s1, 0x23",   reg_val1[i], 0x23,   s0, s1);
               TEST2("slti $t0, $t1, 0xff",   reg_val2[i], 0xff,   t0, t1);
               TEST2("slti $t2, $t3, 0xffff", reg_val2[i], 0xffff, t2, t3);
               TEST2("slti $a0, $a1, 0x0",    reg_val2[i], 0x0,    a0, a1);
               TEST2("slti $s0, $s1, 0x23",   reg_val2[i], 0x23,   s0, s1);
               break;

            case SLTIU:
               TEST2("sltiu $t0, $t1, 0xff",   reg_val1[i], 0xff,   t0, t1);
               TEST2("sltiu $t2, $t3, 0xffff", reg_val1[i], 0xffff, t2, t3);
               TEST2("sltiu $a0, $a1, 0x0",    reg_val1[i], 0x0,    a0, a1);
               TEST2("sltiu $s0, $s1, 0x23",   reg_val1[i], 0x23,   s0, s1);
               TEST2("sltiu $t0, $t1, 0xff",   reg_val2[i], 0xff,   t0, t1);
               TEST2("sltiu $t2, $t3, 0xffff", reg_val2[i], 0xffff, t2, t3);
               TEST2("sltiu $a0, $a1, 0x0",    reg_val2[i], 0x0,    a0, a1);
               TEST2("sltiu $s0, $s1, 0x23",   reg_val2[i], 0x23,   s0, s1);
               break;

            case SLTU:
               TEST1("sltu $t0, $t1, $t2", reg_val1[i], reg_val1[N-i-1],
                                           t0, t1, t2);
               TEST1("sltu $s0, $s1, $s2", reg_val2[i], reg_val2[N-i-1],
                                           s0, s1, s2);
               break;

            case SUB:
               if (i < 8 || (i > 15 && i < 22))
                  TEST1("sub $t0, $t1, $t2", reg_val1[i], reg_val1[N-i-1],
                                             t0, t1, t2);
               break;

            case SUBU:
               TEST1("subu $t0, $t1, $t2", reg_val1[i], reg_val1[N-i-1],
                                           t0, t1, t2);
               break;

            default:
               printf("Error!\n");
               break;
         }
      }
   }
   return 0;
}
