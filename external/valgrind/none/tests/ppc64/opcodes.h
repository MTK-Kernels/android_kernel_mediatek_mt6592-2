#ifndef OPCODES_H
#define OPCODES_H



#define X20_ASM_DIRECTIVE ".long"
#define X20_OPCODE_OFFSET "26"
#define X20_TH_OFFSET     "21"
#define X20_RA_OFFSET     "16"
#define X20_RB_OFFSET     "11"
#define X20_XO_OFFSET     "1"
#define X20_RES_OFFSET    "0"

#define X20_ASM(OPCODE, TH, RA, RB, XO, RES)       \
        X20_ASM_DIRECTIVE                  " "     \
        "(" #OPCODE "<<" X20_OPCODE_OFFSET ")" "+" \
        "(" #TH     "<<" X20_TH_OFFSET     ")" "+" \
        "(" #RA     "<<" X20_RA_OFFSET     ")" "+" \
        "(" #RB     "<<" X20_RB_OFFSET     ")" "+" \
        "(" #XO     "<<" X20_XO_OFFSET     ")" "+" \
        "(" #RES    "<<" X20_RES_OFFSET    ")"

#define X20(OPCODE, TH, RA, RB, XO, RES) X20_ASM(OPCODE, TH, RA, RB, XO, RES)


#define DCBT_OPCODE 31
#define DCBT_XO     278
#define DCBT_RES    0
#define DCBT_S(RA, RB, TH) X20(DCBT_OPCODE, TH, RA, RB, DCBT_XO, DCBT_RES)
#define ASM_DCBT(RA, RB, TH) __asm__ __volatile__ (DCBT_S(RA, RB, TH))

#define DCBTST_OPCODE 31
#define DCBTST_XO     246
#define DCBTST_RES    0
#define DCBTST_S(RA, RB, TH) X20(DCBTST_OPCODE, TH, RA, RB, DCBTST_XO, DCBTST_RES)
#define ASM_DCBTST(RA, RB, TH) __asm__ __volatile__ (DCBTST_S(RA, RB, TH))

#endif 
