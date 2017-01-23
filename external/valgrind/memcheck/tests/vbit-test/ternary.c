
#include <assert.h>
#include "vtest.h"


static void
check_result_for_ternary(const irop_t *op, const test_data_t *data)
{
   const opnd_t *result = &data->result;
   const opnd_t *opnd1  = &data->opnds[0];
   const opnd_t *opnd2  = &data->opnds[1];
   const opnd_t *opnd3  = &data->opnds[2];
   vbits_t expected_vbits;

   
   switch (op->undef_kind) {
   case UNDEF_ALL:
      expected_vbits = undefined_vbits(result->vbits.num_bits);
      break;

   case UNDEF_SAME:
      
      expected_vbits = or_vbits(or_vbits(opnd1->vbits, opnd2->vbits),
                                opnd3->vbits);
      break;

   default:
      panic(__func__);
   }

   if (! equal_vbits(result->vbits, expected_vbits))
      complain(op, data, expected_vbits);
}


int
test_ternary_op(const irop_t *op, test_data_t *data)
{
   unsigned num_input_bits, i, bitpos;
   opnd_t *opnds = data->opnds;
   int tests_done = 0;

   for (i = 0; i < 3; ++i) {
      num_input_bits = bitsof_irtype(opnds[i].type);

      opnds[0].vbits = defined_vbits(bitsof_irtype(opnds[0].type));
      opnds[1].vbits = defined_vbits(bitsof_irtype(opnds[1].type));
      opnds[2].vbits = defined_vbits(bitsof_irtype(opnds[2].type));

      for (bitpos = 0; bitpos < num_input_bits; ++bitpos) {
         opnds[i].vbits = onehot_vbits(bitpos, bitsof_irtype(opnds[i].type));

         valgrind_execute_test(op, data);

         check_result_for_ternary(op, data);

	 tests_done++;
      }
   }
   return tests_done;
}
