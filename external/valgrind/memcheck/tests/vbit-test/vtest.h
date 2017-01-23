
#ifndef VTEST_H
#define VTEST_H


#include <stdint.h>   
#include "libvex.h"   
#include "vbits.h"    



typedef enum {
   
   UNDEF_NONE,

   
   UNDEF_ALL,

   
   
   UNDEF_SAME,

   
   
   
   
   UNDEF_TRUNC,

   
   
   
   
   UNDEF_ZEXT,

   
   
   
   
   UNDEF_SEXT,

   
   
   UNDEF_LEFT,    

   UNDEF_CONCAT,  
   UNDEF_UPPER,   
   UNDEF_SHL,     
   UNDEF_SHR,     
   UNDEF_SAR,     
   UNDEF_OR,      
   UNDEF_AND,     

   UNDEF_ORD,     

   
   UNDEF_UNKNOWN
} undef_t;


typedef struct {
   IROp op;
   const char *name;
   undef_t     undef_kind;
   int         shift_amount_is_immediate;
   
   unsigned    s390x  : 1;
   unsigned    amd64  : 1;
   unsigned    ppc32  : 1;
   unsigned    ppc64  : 1;
   unsigned    arm    : 1;
   unsigned    arm64  : 1;
   unsigned    x86    : 1;
   unsigned    mips32 : 1;
   unsigned    mips64 : 1;
   unsigned    tilegx : 1;
} irop_t;


#define MAX_OPERANDS 4

typedef struct {
   IRType  type;
   vbits_t vbits;
   value_t value;
} opnd_t;


typedef struct {
   opnd_t result;
   opnd_t opnds[MAX_OPERANDS];
   unsigned rounding_mode;
} test_data_t;


irop_t *get_irop(IROp);
int  is_floating_point_op_with_rounding_mode(IROp);
int  get_num_operands(IROp);

void print_opnd(FILE *, const opnd_t *);

int test_unary_op(const irop_t *, test_data_t *);
int test_binary_op(const irop_t *, test_data_t *);
int test_ternary_op(const irop_t *, test_data_t *);
int test_qernary_op(const irop_t *, test_data_t *);

void valgrind_vex_init_for_iri(IRICB *);
void valgrind_execute_test(const irop_t *, test_data_t *);

IRICB new_iricb(const irop_t *, test_data_t *);

void panic(const char *) __attribute__((noreturn));
void complain(const irop_t *, const test_data_t *, vbits_t expected);

unsigned sizeof_irtype(IRType);
void typeof_primop(IROp, IRType *t_dst, IRType *t_arg1, IRType *t_arg2, 
                   IRType *t_arg3, IRType *t_arg4);

static __inline__ unsigned bitsof_irtype(IRType type)
{
   return type == Ity_I1 ? 1 : sizeof_irtype(type) * 8;
}


extern int verbose;

#endif 
