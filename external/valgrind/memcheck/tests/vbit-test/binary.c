
#include <assert.h>
#include <string.h>  
#include "vtest.h"


static vbits_t
and_combine(vbits_t v1, vbits_t v2, value_t val2, int invert_val2)
{
   assert(v1.num_bits == v2.num_bits);

   vbits_t new = { .num_bits = v2.num_bits };

   if (invert_val2) {
      switch (v2.num_bits) {
      case 8:  val2.u8  = ~val2.u8  & 0xff;   break;
      case 16: val2.u16 = ~val2.u16 & 0xffff; break;
      case 32: val2.u32 = ~val2.u32;          break;
      case 64: val2.u64 = ~val2.u64;          break;
      default:
         panic(__func__);
      }
   }

   switch (v2.num_bits) {
   case 8:
      new.bits.u8  = (v1.bits.u8 & ~v2.bits.u8  & val2.u8)  & 0xff;
      break;
   case 16:
      new.bits.u16 = (v1.bits.u16 & ~v2.bits.u16 & val2.u16) & 0xffff;
      break;
   case 32:
      new.bits.u32 = (v1.bits.u32 & ~v2.bits.u32 & val2.u32);
      break;
   case 64:
      new.bits.u64 = (v1.bits.u64 & ~v2.bits.u64 & val2.u64);
      break;
   default:
      panic(__func__);
   }
   return new;
}

static void
check_result_for_binary(const irop_t *op, const test_data_t *data)
{
   const opnd_t *result = &data->result;
   const opnd_t *opnd1  = &data->opnds[0];
   const opnd_t *opnd2  = &data->opnds[1];
   vbits_t expected_vbits;

   
   switch (op->undef_kind) {
   case UNDEF_NONE:
      expected_vbits = defined_vbits(result->vbits.num_bits);
      break;

   case UNDEF_ALL:
      expected_vbits = undefined_vbits(result->vbits.num_bits);
      break;

   case UNDEF_LEFT:
      
      expected_vbits = left_vbits(or_vbits(opnd1->vbits, opnd2->vbits),
                                  result->vbits.num_bits);
      break;

   case UNDEF_SAME:
      assert(opnd1->vbits.num_bits == opnd2->vbits.num_bits);
      assert(opnd1->vbits.num_bits == result->vbits.num_bits);

      
      expected_vbits = or_vbits(opnd1->vbits, opnd2->vbits);
      break;

   case UNDEF_CONCAT:
      assert(opnd1->vbits.num_bits == opnd2->vbits.num_bits);
      assert(result->vbits.num_bits == 2 * opnd1->vbits.num_bits);
      expected_vbits = concat_vbits(opnd1->vbits, opnd2->vbits);
      break;

   case UNDEF_SHL:
      if (! completely_defined_vbits(opnd2->vbits)) {
         expected_vbits = undefined_vbits(result->vbits.num_bits);
      } else {
         assert(opnd2->vbits.num_bits == 8);
         unsigned shift_amount = opnd2->value.u8;
      
         expected_vbits = shl_vbits(opnd1->vbits, shift_amount);
      }
      break;

   case UNDEF_SHR:
      if (! completely_defined_vbits(opnd2->vbits)) {
         expected_vbits = undefined_vbits(result->vbits.num_bits);
      } else {
         assert(opnd2->vbits.num_bits == 8);
         unsigned shift_amount = opnd2->value.u8;
      
         expected_vbits = shr_vbits(opnd1->vbits, shift_amount);
      }
      break;

   case UNDEF_SAR:
      if (! completely_defined_vbits(opnd2->vbits)) {
         expected_vbits = undefined_vbits(result->vbits.num_bits);
      } else {
         assert(opnd2->vbits.num_bits == 8);
         unsigned shift_amount = opnd2->value.u8;
      
         expected_vbits = sar_vbits(opnd1->vbits, shift_amount);
      }
      break;

   case UNDEF_AND: {
      vbits_t term1, term2, term3;
      term1 = and_vbits(opnd1->vbits, opnd2->vbits);
      term2 = and_combine(opnd1->vbits, opnd2->vbits, opnd2->value, 0);
      term3 = and_combine(opnd2->vbits, opnd1->vbits, opnd1->value, 0);
      expected_vbits = or_vbits(term1, or_vbits(term2, term3));
      break;
   }

   case UNDEF_OR: {
      vbits_t term1, term2, term3;
      term1 = and_vbits(opnd1->vbits, opnd2->vbits);
      term2 = and_combine(opnd1->vbits, opnd2->vbits, opnd2->value, 1);
      term3 = and_combine(opnd2->vbits, opnd1->vbits, opnd1->value, 1);
      expected_vbits = or_vbits(term1, or_vbits(term2, term3));
      break;
   }

   case UNDEF_ORD:
      expected_vbits = cmpord_vbits(opnd1->vbits.num_bits,
                                    opnd2->vbits.num_bits);
      break;

   default:
      panic(__func__);
   }

   if (! equal_vbits(result->vbits, expected_vbits))
      complain(op, data, expected_vbits);
}


static int 
test_shift(const irop_t *op, test_data_t *data)
{
   unsigned num_input_bits, i;
   opnd_t *opnds = data->opnds;
   int tests_done = 0;

   for (unsigned amount = 0; amount < bitsof_irtype(opnds[0].type); ++amount) {
      opnds[1].value.u8 = amount;

      
      num_input_bits = bitsof_irtype(opnds[0].type);

      for (i = 0; i < num_input_bits; ++i) {
         opnds[0].vbits = onehot_vbits(i, bitsof_irtype(opnds[0].type));
         opnds[1].vbits = defined_vbits(bitsof_irtype(opnds[1].type));
         
         valgrind_execute_test(op, data);
         
         check_result_for_binary(op, data);
         tests_done++;
      }
   }

   

   
   if (op->shift_amount_is_immediate) return tests_done;

   num_input_bits = bitsof_irtype(opnds[1].type);

   for (i = 0; i < num_input_bits; ++i) {
      opnds[0].vbits = defined_vbits(bitsof_irtype(opnds[0].type));
      opnds[1].vbits = onehot_vbits(i, bitsof_irtype(opnds[1].type));

      valgrind_execute_test(op, data);

      check_result_for_binary(op, data);

      tests_done++;
   }
   return tests_done;
}


static value_t
all_bits_zero_value(unsigned num_bits)
{
   value_t val;

   switch (num_bits) {
   case 8:  val.u8  = 0; break;
   case 16: val.u16 = 0; break;
   case 32: val.u32 = 0; break;
   case 64: val.u64 = 0; break;
   default:
      panic(__func__);
   }
   return val;
}


static value_t
all_bits_one_value(unsigned num_bits)
{
   value_t val;

   switch (num_bits) {
   case 8:  val.u8  = 0xff;   break;
   case 16: val.u16 = 0xffff; break;
   case 32: val.u32 = ~0u;    break;
   case 64: val.u64 = ~0ull;  break;
   default:
      panic(__func__);
   }
   return val;
}


static int
test_and(const irop_t *op, test_data_t *data)
{
   unsigned num_input_bits, bitpos;
   opnd_t *opnds = data->opnds;
   int tests_done = 0;


   
   num_input_bits = bitsof_irtype(opnds[0].type);

   for (bitpos = 0; bitpos < num_input_bits; ++bitpos) {
      opnds[0].vbits = onehot_vbits(bitpos, bitsof_irtype(opnds[0].type));
      opnds[1].vbits = defined_vbits(bitsof_irtype(opnds[1].type));
      opnds[1].value = all_bits_zero_value(bitsof_irtype(opnds[1].type));

      valgrind_execute_test(op, data);
         
      check_result_for_binary(op, data);
      tests_done++;
   }
   
   
   num_input_bits = bitsof_irtype(opnds[1].type);

   for (bitpos = 0; bitpos < num_input_bits; ++bitpos) {
      opnds[1].vbits = onehot_vbits(bitpos, bitsof_irtype(opnds[1].type));
      opnds[0].vbits = defined_vbits(bitsof_irtype(opnds[0].type));
      opnds[0].value = all_bits_zero_value(bitsof_irtype(opnds[0].type));

      valgrind_execute_test(op, data);
         
      check_result_for_binary(op, data);
      tests_done++;
   }


   
   num_input_bits = bitsof_irtype(opnds[0].type);

   for (bitpos = 0; bitpos < num_input_bits; ++bitpos) {
      opnds[0].vbits = onehot_vbits(bitpos, bitsof_irtype(opnds[0].type));
      opnds[1].vbits = defined_vbits(bitsof_irtype(opnds[1].type));
      opnds[1].value = all_bits_one_value(bitsof_irtype(opnds[1].type));

      valgrind_execute_test(op, data);
         
      check_result_for_binary(op, data);
      tests_done++;
   }
   
   
   num_input_bits = bitsof_irtype(opnds[1].type);

   for (bitpos = 0; bitpos < num_input_bits; ++bitpos) {
      opnds[1].vbits = onehot_vbits(bitpos, bitsof_irtype(opnds[1].type));
      opnds[0].vbits = defined_vbits(bitsof_irtype(opnds[0].type));
      opnds[0].value = all_bits_one_value(bitsof_irtype(opnds[0].type));

      valgrind_execute_test(op, data);
         
      check_result_for_binary(op, data);
      tests_done++;
   }
   return tests_done;
}


static int
test_or(const irop_t *op, test_data_t *data)
{
   unsigned num_input_bits, bitpos;
   opnd_t *opnds = data->opnds;
   int tests_done = 0;


   
   num_input_bits = bitsof_irtype(opnds[0].type);

   opnds[0].vbits = defined_vbits(bitsof_irtype(opnds[0].type));
   opnds[1].vbits = defined_vbits(bitsof_irtype(opnds[1].type));
   opnds[1].value = all_bits_one_value(bitsof_irtype(opnds[1].type));

   for (bitpos = 0; bitpos < num_input_bits; ++bitpos) {
      opnds[0].vbits = onehot_vbits(bitpos, bitsof_irtype(opnds[0].type));

      valgrind_execute_test(op, data);
         
      check_result_for_binary(op, data);
      tests_done++;
   }
   
   
   num_input_bits = bitsof_irtype(opnds[1].type);

   opnds[0].vbits = defined_vbits(bitsof_irtype(opnds[0].type));
   opnds[1].vbits = defined_vbits(bitsof_irtype(opnds[1].type));
   opnds[0].value = all_bits_one_value(bitsof_irtype(opnds[0].type));

   for (bitpos = 0; bitpos < num_input_bits; ++bitpos) {
      opnds[1].vbits = onehot_vbits(bitpos, bitsof_irtype(opnds[1].type));

      valgrind_execute_test(op, data);
         
      check_result_for_binary(op, data);
      tests_done++;
   }


   
   num_input_bits = bitsof_irtype(opnds[0].type);

   opnds[0].vbits = defined_vbits(bitsof_irtype(opnds[0].type));
   opnds[1].vbits = defined_vbits(bitsof_irtype(opnds[1].type));
   opnds[1].value = all_bits_zero_value(bitsof_irtype(opnds[1].type));

   for (bitpos = 0; bitpos < num_input_bits; ++bitpos) {
      opnds[0].vbits = onehot_vbits(bitpos, bitsof_irtype(opnds[0].type));

      valgrind_execute_test(op, data);
         
      check_result_for_binary(op, data);
      tests_done++;
   }
   
   
   num_input_bits = bitsof_irtype(opnds[1].type);

   opnds[0].vbits = defined_vbits(bitsof_irtype(opnds[0].type));
   opnds[1].vbits = defined_vbits(bitsof_irtype(opnds[1].type));
   opnds[0].value = all_bits_zero_value(bitsof_irtype(opnds[0].type));

   for (bitpos = 0; bitpos < num_input_bits; ++bitpos) {
      opnds[1].vbits = onehot_vbits(bitpos, bitsof_irtype(opnds[1].type));

      valgrind_execute_test(op, data);
         
      check_result_for_binary(op, data);
      tests_done++;
   }
   return tests_done;
}


int
test_binary_op(const irop_t *op, test_data_t *data)
{
   unsigned num_input_bits, i, bitpos;
   opnd_t *opnds = data->opnds;
   int tests_done = 0;

   
   switch (op->undef_kind) {
   case UNDEF_SHL:
   case UNDEF_SHR:
   case UNDEF_SAR:
      return test_shift(op, data);

   case UNDEF_AND:
      return test_and(op, data);

   case UNDEF_OR:
      return test_or(op, data);

   default:
      break;
   }

   for (i = 0; i < 2; ++i) {

      if (i == 1 && op->shift_amount_is_immediate) break;

      num_input_bits = bitsof_irtype(opnds[i].type);
      opnds[0].vbits = defined_vbits(bitsof_irtype(opnds[0].type));
      opnds[1].vbits = defined_vbits(bitsof_irtype(opnds[1].type));

      memset(&opnds[1].value, 0xff, sizeof opnds[1].value);

      if (op->shift_amount_is_immediate)
         opnds[1].value.u8 = 1;

      for (bitpos = 0; bitpos < num_input_bits; ++bitpos) {
         opnds[i].vbits = onehot_vbits(bitpos, bitsof_irtype(opnds[i].type));

         valgrind_execute_test(op, data);

         check_result_for_binary(op, data);

         tests_done++;
      }
   }
   return tests_done;
}
