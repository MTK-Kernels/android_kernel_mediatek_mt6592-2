/* Test program for dwarf location functions.
   Copyright (C) 2013 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>
#include <assert.h>
#include <argp.h>
#include <inttypes.h>
#include <errno.h>
#include ELFUTILS_HEADER(dw)
#include ELFUTILS_HEADER(dwfl)
#include <dwarf.h>
#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "../libdw/known-dwarf.h"

static Dwarf *dw;
Dwarf_CFI *cfi_debug;
Dwarf_CFI *cfi_eh;
Dwarf_Addr cfi_eh_bias;

bool has_frame_base;

static void
print_die (Dwarf_Die *die, const char *what, int indent)
{
  Dwarf_Addr entrypc;
  const char *name = dwarf_diename (die) ?: "<unknown>";
  if (dwarf_entrypc (die, &entrypc) == 0)
    printf ("%*s[%" PRIx64 "] %s '%s'@%" PRIx64 "\n", indent * 2, "",
	    dwarf_dieoffset (die), what, name, entrypc);
  else
    printf ("%*s[%" PRIx64 "] %s '%s'\n", indent * 2, "",
	    dwarf_dieoffset (die), what, name);
}

static const char *
dwarf_encoding_string (unsigned int code)
{
  static const char *const known[] =
    {
#define ONE_KNOWN_DW_ATE(NAME, CODE) [CODE] = #NAME,
      ALL_KNOWN_DW_ATE
#undef ONE_KNOWN_DW_ATE
    };

  if (likely (code < sizeof (known) / sizeof (known[0])))
    return known[code];

  return NULL;
}

static void
print_base_type (Dwarf_Die *base)
{
  assert (dwarf_tag (base) == DW_TAG_base_type);

  Dwarf_Attribute encoding;
  Dwarf_Word enctype;
  if (dwarf_attr (base, DW_AT_encoding, &encoding) == NULL
      || dwarf_formudata (&encoding, &enctype) != 0)
    error (EXIT_FAILURE, 0, "base type without encoding");

  Dwarf_Attribute bsize;
  Dwarf_Word bits;
  if (dwarf_attr (base, DW_AT_byte_size, &bsize) != NULL
      && dwarf_formudata (&bsize, &bits) == 0)
    bits *= 8;
  else if (dwarf_attr (base, DW_AT_bit_size, &bsize) == NULL
	   || dwarf_formudata (&bsize, &bits) != 0)
    error (EXIT_FAILURE, 0, "base type without byte or bit size");

  printf ("{%s,%s,%" PRIu64 "@[%" PRIx64 "]}",
	  dwarf_diename (base),
	  dwarf_encoding_string (enctype),
	  bits,
	  dwarf_dieoffset (base));
}

static const char *
dwarf_opcode_string (unsigned int code)
{
  static const char *const known[] =
    {
#define ONE_KNOWN_DW_OP_DESC(NAME, CODE, DESC) ONE_KNOWN_DW_OP (NAME, CODE)
#define ONE_KNOWN_DW_OP(NAME, CODE) [CODE] = #NAME,
      ALL_KNOWN_DW_OP
#undef ONE_KNOWN_DW_OP
#undef ONE_KNOWN_DW_OP_DESC
    };

  if (likely (code < sizeof (known) / sizeof (known[0])))
    return known[code];

  return NULL;
}

static void print_expr (Dwarf_Attribute *, Dwarf_Op *, Dwarf_Addr);

static void
print_expr_block (Dwarf_Attribute *attr, Dwarf_Op *exprs, int len,
		  Dwarf_Addr addr)
{
  printf ("{");
  for (int i = 0; i < len; i++)
    {
      print_expr (attr, &exprs[i], addr);
      printf ("%s", (i + 1 < len ? ", " : ""));
    }
  printf ("}");
}

static void
print_expr_block_addrs (Dwarf_Attribute *attr,
			Dwarf_Addr begin, Dwarf_Addr end,
			Dwarf_Op *exprs, int len)
{
  printf ("      [%" PRIx64 ",%" PRIx64 ") ", begin, end);
  print_expr_block (attr, exprs, len, begin);
  printf ("\n");
}

static void
print_expr (Dwarf_Attribute *attr, Dwarf_Op *expr, Dwarf_Addr addr)
{
  uint8_t atom = expr->atom;
  const char *opname = dwarf_opcode_string (atom);
  assert (opname != NULL);

  switch (atom)
    {
    case DW_OP_deref:
    case DW_OP_dup:
    case DW_OP_drop:
    case DW_OP_over:
    case DW_OP_swap:
    case DW_OP_rot:
    case DW_OP_xderef:
    case DW_OP_abs:
    case DW_OP_and:
    case DW_OP_div:
    case DW_OP_minus:
    case DW_OP_mod:
    case DW_OP_mul:
    case DW_OP_neg:
    case DW_OP_not:
    case DW_OP_or:
    case DW_OP_plus:
    case DW_OP_shl:
    case DW_OP_shr:
    case DW_OP_shra:
    case DW_OP_xor:
    case DW_OP_eq:
    case DW_OP_ge:
    case DW_OP_gt:
    case DW_OP_le:
    case DW_OP_lt:
    case DW_OP_ne:
    case DW_OP_lit0 ... DW_OP_lit31:
    case DW_OP_reg0 ... DW_OP_reg31:
    case DW_OP_nop:
    case DW_OP_stack_value:
      
      printf ("%s", opname);
      break;

    case DW_OP_form_tls_address:
      printf ("%s", opname);
      break;

    case DW_OP_GNU_push_tls_address:
      printf ("%s", opname);
      break;

    case DW_OP_call_frame_cfa:

      if (attr == NULL)
	error (EXIT_FAILURE, 0, "%s used in CFI", opname);

      printf ("%s ", opname);
      if (cfi_eh == NULL && cfi_debug == NULL)
	error (EXIT_FAILURE, 0, "DW_OP_call_frame_cfa used but no cfi found.");

      Dwarf_Frame *frame;
      if (dwarf_cfi_addrframe (cfi_eh, addr + cfi_eh_bias, &frame) != 0
	  && dwarf_cfi_addrframe (cfi_debug, addr, &frame) != 0)
	error (EXIT_FAILURE, 0, "dwarf_cfi_addrframe 0x%" PRIx64 ": %s",
	       addr, dwarf_errmsg (-1));

      Dwarf_Op *cfa_ops;
      size_t cfa_nops;
      if (dwarf_frame_cfa (frame, &cfa_ops, &cfa_nops) != 0)
	error (EXIT_FAILURE, 0, "dwarf_frame_cfa 0x%" PRIx64 ": %s",
	       addr, dwarf_errmsg (-1));
      if (cfa_nops < 1)
	error (EXIT_FAILURE, 0, "dwarf_frame_cfa no ops");
      print_expr_block (NULL, cfa_ops, cfa_nops, 0);
      free (frame);
      break;

    case DW_OP_push_object_address:
      if (attr == NULL)
	error (EXIT_FAILURE, 0, "%s used in CFI", opname);
      printf ("%s", opname);
      break;

    case DW_OP_addr:
      
      printf ("%s(0x%" PRIx64 ")", opname, (Dwarf_Addr) expr->number);
      break;

    case DW_OP_const1u:
    case DW_OP_const2u:
    case DW_OP_const4u:
    case DW_OP_const8u:
    case DW_OP_constu:
    case DW_OP_pick:
    case DW_OP_plus_uconst:
    case DW_OP_regx:
    case DW_OP_piece:
    case DW_OP_deref_size:
    case DW_OP_xderef_size:
      
      printf ("%s(%" PRIu64 ")", opname, expr->number);
      break;

    case DW_OP_call2:
    case DW_OP_call4:
    case DW_OP_call_ref:
      {
	if (attr == NULL)
	  error (EXIT_FAILURE, 0, "%s used in CFI", opname);

	Dwarf_Attribute call_attr;
	if (dwarf_getlocation_attr (attr, expr, &call_attr) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_attr for %s error %s",
		 opname, dwarf_errmsg (-1));

	Dwarf_Die call_die;
	if (dwarf_getlocation_die (attr, expr, &call_die) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_die for %s error %s",
		 opname, dwarf_errmsg (-1));

	Dwarf_Op *call_ops;
	size_t call_len;
	if (dwarf_getlocation (&call_attr, &call_ops, &call_len) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation for entry: %s",
		 dwarf_errmsg (-1));

	printf ("%s([%" PRIx64 "]) ", opname, dwarf_dieoffset (&call_die));
	print_expr_block (&call_attr, call_ops, call_len, addr);
      }
      break;

    case DW_OP_const1s:
    case DW_OP_const2s:
    case DW_OP_const4s:
    case DW_OP_const8s:
    case DW_OP_consts:
    case DW_OP_skip:
    case DW_OP_bra:
    case DW_OP_breg0 ... DW_OP_breg31:
      
      printf ("%s(%" PRId64 ")", opname, (Dwarf_Sword) expr->number);
      break;

    case DW_OP_fbreg:
      
      if (attr == NULL)
	  error (EXIT_FAILURE, 0, "%s used in CFI", opname);

      if (! has_frame_base)
	error (EXIT_FAILURE, 0, "DW_OP_fbreg used without a frame base");

      printf ("%s(%" PRId64 ")", opname, (Dwarf_Sword) expr->number);
      break;

    case DW_OP_bregx:
      
      printf ("%s(%" PRIu64 ",%" PRId64 ")", opname,
	      expr->number, (Dwarf_Sword) expr->number2);
      break;

    case DW_OP_bit_piece:
      
      printf ("%s(%" PRIu64 ",%" PRIu64 ")", opname,
	      expr->number, expr->number2);
      break;

    case DW_OP_implicit_value:
      
      {
	Dwarf_Attribute const_attr;
	Dwarf_Block block;
	if (dwarf_getlocation_attr (attr, expr, &const_attr) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_attr: %s",
		 dwarf_errmsg (-1));

	if (dwarf_formblock (&const_attr, &block) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_formblock: %s",
		 dwarf_errmsg (-1));

	
	Dwarf_Block block_impl;
	if (dwarf_getlocation_implicit_value (attr, expr, &block_impl) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_implicit_value: %s",
		 dwarf_errmsg (-1));

	assert (expr->number == block.length);
	assert (block.length == block_impl.length);
	printf ("%s(%" PRIu64 "){", opname, block.length);
	for (size_t i = 0; i < block.length; i++)
	  {
	    printf ("%02x", block.data[i]);
	    assert (block.data[i] == block_impl.data[i]);
	  }
	printf("}");
      }
      break;

    case DW_OP_GNU_implicit_pointer:
      {
	if (attr == NULL)
	  error (EXIT_FAILURE, 0, "%s used in CFI", opname);

	Dwarf_Attribute attrval;
	if (dwarf_getlocation_implicit_pointer (attr, expr, &attrval) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_implicit_pointer: %s",
		 dwarf_errmsg (-1));

	
	Dwarf_Attribute attrval2;
	if (dwarf_getlocation_attr (attr, expr, &attrval2) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_attr: %s",
		 dwarf_errmsg (-1));

	assert (dwarf_whatattr (&attrval) == dwarf_whatattr (&attrval2));
	assert (dwarf_whatform (&attrval) == dwarf_whatform (&attrval2));
	
	
	assert (attrval.valp == attrval2.valp);

	Dwarf_Die impl_die;
	if (dwarf_getlocation_die (attr, expr, &impl_die) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_due: %s",
		 dwarf_errmsg (-1));

	printf ("%s([%" PRIx64 "],%" PRId64 ") ", opname,
		dwarf_dieoffset (&impl_die), expr->number2);

	if (dwarf_whatattr (&attrval) == DW_AT_const_value)
	  printf ("<constant value>"); 
	else
	  {
	    
	    Dwarf_Op *exprval;
	    size_t exprval_len;
	    int locs = dwarf_getlocation_addr (&attrval, addr,
					       &exprval, &exprval_len, 1);
	    if (locs == 0)
	      printf ("<no location>"); 
	    else if (locs == 1)
	      print_expr_block (&attrval, exprval, exprval_len, addr);
	    else
	      error (EXIT_FAILURE, 0,
		     "dwarf_getlocation_addr attrval at addr 0x%" PRIx64
		     ", locs (%d): %s", addr, locs, dwarf_errmsg (-1));
	  }
      }
      break;

    case DW_OP_GNU_entry_value:
      {
	Dwarf_Attribute entry_attr;
	if (dwarf_getlocation_attr (attr, expr, &entry_attr) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_attr: %s",
		 dwarf_errmsg (-1));

	Dwarf_Op *entry_ops;
	size_t entry_len;
	if (dwarf_getlocation (&entry_attr, &entry_ops, &entry_len) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation for entry: %s",
		 dwarf_errmsg (-1));

	printf ("%s(%zd) ", opname, entry_len);
	print_expr_block (attr, entry_ops, entry_len, addr);
      }
      break;

    case DW_OP_GNU_parameter_ref:
      {
	Dwarf_Die param;
	if (dwarf_getlocation_die (attr, expr, &param) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_die: %s",
		 dwarf_errmsg (-1));
	
	printf ("%s[%" PRIx64 "]", opname, dwarf_dieoffset (&param));
	assert (expr->number == dwarf_cuoffset (&param));
	assert (dwarf_tag (&param) == DW_TAG_formal_parameter);
      }
      break;

    case DW_OP_GNU_convert:
    case DW_OP_GNU_reinterpret:
      {
	Dwarf_Die type;
	Dwarf_Off off = expr->number;
	if (off != 0)
	  {
	    if (dwarf_getlocation_die (attr, expr, &type) != 0)
	      error (EXIT_FAILURE, 0, "dwarf_getlocation_die: %s",
		     dwarf_errmsg (-1));
	    off = dwarf_dieoffset (&type);
	    assert (expr->number == dwarf_cuoffset (&type));
	    printf ("%s", opname);
	    print_base_type (&type);
	  }
	else
	  printf ("%s[%" PRIu64 "]", opname, off);

      }
      break;

    case DW_OP_GNU_regval_type:
      {
	Dwarf_Die type;
	if (dwarf_getlocation_die (attr, expr, &type) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_die: %s",
		 dwarf_errmsg (-1));
	assert (expr->number2 == dwarf_cuoffset (&type));
	
	printf ("%s(reg%" PRIu64 ")", opname, expr->number);
	print_base_type (&type);
      }
      break;

    case DW_OP_GNU_deref_type:
 
      {
	Dwarf_Die type;
	if (dwarf_getlocation_die (attr, expr, &type) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_die: %s",
		 dwarf_errmsg (-1));
	assert (expr->number2 == dwarf_cuoffset (&type));
	
	printf ("%s(%" PRIu64 ")", opname, expr->number);
	print_base_type (&type);
      }
      break;

    case DW_OP_GNU_const_type:
      {
	Dwarf_Die type;
	if (dwarf_getlocation_die (attr, expr, &type) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_die: %s",
		 dwarf_errmsg (-1));
	assert (expr->number == dwarf_cuoffset (&type));

	Dwarf_Attribute const_attr;
	if (dwarf_getlocation_attr (attr, expr, &const_attr) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_attr for type: %s",
		 dwarf_errmsg (-1));
	  
	Dwarf_Block block;
	if (dwarf_formblock (&const_attr, &block) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_formblock for type: %s",
		 dwarf_errmsg (-1));

	printf ("%s", opname);
	print_base_type (&type);
	printf ("(%" PRIu64 ")[", block.length);
	for (size_t i = 0; i < block.length; i++)
	  printf ("%02x", block.data[i]);
	printf("]");
      }
      break;

    default:
      error (EXIT_FAILURE, 0, "unhandled opcode: DW_OP_%s (0x%x)",
	     opname, atom);
    }
}

static void
print_varlocs (Dwarf_Die *funcdie)
{
  
  
  has_frame_base = dwarf_hasattr (funcdie, DW_AT_frame_base);
  if (has_frame_base)
    {
      Dwarf_Attribute fb_attr;
      if (dwarf_attr (funcdie, DW_AT_frame_base, &fb_attr) == NULL)
	error (EXIT_FAILURE, 0, "dwarf_attr fb: %s", dwarf_errmsg (-1));

      Dwarf_Op *fb_expr;
      size_t fb_exprlen;
      if (dwarf_getlocation (&fb_attr, &fb_expr, &fb_exprlen) == 0)
	{
	  
	  Dwarf_Addr entrypc;
	  if (dwarf_entrypc (funcdie, &entrypc) != 0)
	    error (EXIT_FAILURE, 0, "dwarf_entrypc: %s", dwarf_errmsg (-1));

	  printf ("    frame_base: ");
	  if (entrypc == 0)
	    printf ("XXX zero address"); 
	  else
	    print_expr_block (&fb_attr, fb_expr, fb_exprlen, entrypc);
	  printf ("\n");
	}
      else
	{
	  Dwarf_Addr base, start, end;
	  ptrdiff_t off = 0;
	  printf ("    frame_base:\n");
          while ((off = dwarf_getlocations (&fb_attr, off, &base,
					    &start, &end,
					    &fb_expr, &fb_exprlen)) > 0)
	    {
	      printf ("      (%" PRIx64 ",%" PRIx64 ") ", start, end);
	      print_expr_block (&fb_attr, fb_expr, fb_exprlen, start);
	      printf ("\n");
	    }

	  if (off < 0)
	    error (EXIT_FAILURE, 0, "dwarf_getlocations fb: %s",
		   dwarf_errmsg (-1));
	}
    }
  else if (dwarf_tag (funcdie) == DW_TAG_inlined_subroutine)
    {
      
      
      Dwarf_Die *scopes;
      int n = dwarf_getscopes_die (funcdie, &scopes);
      if (n <= 0)
	error (EXIT_FAILURE, 0, "dwarf_getscopes_die: %s", dwarf_errmsg (-1));

      while (n-- > 0)
	if (dwarf_tag (&scopes[n]) == DW_TAG_subprogram
	    && dwarf_hasattr (&scopes[n], DW_AT_frame_base))
	  {
	    has_frame_base = true;
	    break;
	  }
      free (scopes);
    }

  if (! dwarf_haschildren (funcdie))
    return;

  Dwarf_Die child;
  int res = dwarf_child (funcdie, &child);
  if (res < 0)
    error (EXIT_FAILURE, 0, "dwarf_child: %s", dwarf_errmsg (-1));

  if (res == 1)
    return;

  do
    {
      int tag = dwarf_tag (&child);
      if (tag == DW_TAG_variable || tag == DW_TAG_formal_parameter)
	{
	  const char *what = tag == DW_TAG_variable ? "variable" : "parameter";
	  print_die (&child, what, 2);

	  if (dwarf_hasattr (&child, DW_AT_location))
	    {
	      Dwarf_Attribute attr;
	      if (dwarf_attr (&child, DW_AT_location, &attr) == NULL)
		error (EXIT_FAILURE, 0, "dwarf_attr: %s", dwarf_errmsg (-1));

	      Dwarf_Op *expr;
	      size_t exprlen;
	      if (dwarf_getlocation (&attr, &expr, &exprlen) == 0)
		{
		  
		  
		  ptrdiff_t offset = 0;
		  Dwarf_Addr base, begin, end;
		  do
		    {
		      offset = dwarf_ranges (funcdie, offset, &base,
					     &begin, &end);
		      if (offset < 0)
			error (EXIT_FAILURE, 0, "dwarf_ranges: %s",
			       dwarf_errmsg (-1));

		      if (offset > 0)
			{
			  if (exprlen == 0)
			    printf ("      (%"
				    PRIx64 ",%" PRIx64
				    ") <empty expression>\n", begin, end);
			  else
			    print_expr_block_addrs (&attr, begin, end,
						    expr, exprlen);
			}
		    }
		  while (offset > 0);

		  if (offset < 0)
		    error (EXIT_FAILURE, 0, "dwarf_ranges: %s",
			   dwarf_errmsg (-1));
		}
	      else
		{
		  Dwarf_Addr base, begin, end;
		  ptrdiff_t offset = 0;
		  while ((offset = dwarf_getlocations (&attr, offset,
						       &base, &begin, &end,
						       &expr, &exprlen)) > 0)
		    if (begin >= end)
		      printf ("      (%" PRIx64 ",%" PRIx64
			      ") <empty range>\n", begin, end); 
		    else
		      {
			print_expr_block_addrs (&attr, begin, end,
						expr, exprlen);

			
			
			Dwarf_Op *expraddr;
			size_t expraddr_len;
			int locs = dwarf_getlocation_addr (&attr, begin,
							   &expraddr,
							   &expraddr_len, 1);
			assert (locs == 1);
			locs = dwarf_getlocation_addr (&attr, end - 1,
						       &expraddr,
						       &expraddr_len, 1);
			assert (locs == 1);
		      }

		  if (offset < 0)
		    error (EXIT_FAILURE, 0, "dwarf_getlocations: %s",
			   dwarf_errmsg (-1));
		}
	    }
	  else if (dwarf_hasattr (&child, DW_AT_const_value))
	    {
	      printf ("      <constant value>\n"); 
	    }
	  else
	    {
	      printf ("      <no value>\n");
	    }
	}
    }
  while (dwarf_siblingof (&child, &child) == 0);
}

static int
handle_instance (Dwarf_Die *funcdie, void *arg __attribute__ ((unused)))
{
  print_die (funcdie, "inlined function", 1);
  print_varlocs (funcdie);

  return DWARF_CB_OK;
}

static int
handle_function (Dwarf_Die *funcdie, void *arg __attribute__((unused)))
{
  if (dwarf_func_inline (funcdie) > 0)
    {
      

      
      
      
      
      if (dwarf_func_inline_instances (funcdie, &handle_instance, NULL) != 0)
	error (EXIT_FAILURE, 0, "dwarf_func_inline_instances: %s",
	       dwarf_errmsg (-1));
    }
  else
    {
      
      Dwarf_Addr entrypc;
      if (dwarf_entrypc (funcdie, &entrypc) == 0)
	{
	  print_die (funcdie, "function", 1);
	  print_varlocs (funcdie);
	}
    }

  return DWARF_CB_OK;
}

int
main (int argc, char *argv[])
{
  int remaining;
  Dwfl *dwfl;
  (void) argp_parse (dwfl_standard_argp (), argc, argv, 0, &remaining,
                     &dwfl);
  assert (dwfl != NULL);

  Dwarf_Die *cu = NULL;
  Dwarf_Addr dwbias;
  while ((cu = dwfl_nextcu (dwfl, cu, &dwbias)) != NULL)
    {
      Dwarf_Addr cubase;
      if (dwarf_tag (cu) == DW_TAG_compile_unit
	  && dwarf_lowpc (cu, &cubase) == 0)
	{
	  Dwfl_Module *mod = dwfl_cumodule (cu);
	  Dwarf_Addr modbias;
	  dw = dwfl_module_getdwarf (mod, &modbias);
	  assert (dwbias == modbias);

	  const char *mainfile;
	  const char *modname = dwfl_module_info (mod, NULL,
						  NULL, NULL,
						  NULL, NULL,
						  &mainfile,
						  NULL);
	  if (modname == NULL)
	    error (EXIT_FAILURE, 0, "dwfl_module_info: %s", dwarf_errmsg (-1));

	  const char *name = (modname[0] != '\0'
			      ? modname
			      :  basename (mainfile));
	  printf ("module '%s'\n", name);
	  print_die (cu, "CU", 0);

	  Dwarf_Addr elfbias;
	  Elf *elf = dwfl_module_getelf (mod, &elfbias);

	  
	  cfi_debug = dwarf_getcfi (dw); 
	  cfi_eh = dwarf_getcfi_elf (elf);
	  cfi_eh_bias = dwbias - elfbias;

	  
	  Dwarf_Die cudie;
	  uint8_t offsize;
	  uint8_t addrsize;
	  if (dwarf_diecu (cu, &cudie, &addrsize, &offsize) == NULL)
	    error (EXIT_FAILURE, 0, "dwarf_diecu %s", dwarf_errmsg (-1));

	  if (dwarf_getfuncs (cu, handle_function, NULL, 0) != 0)
	    error (EXIT_FAILURE, 0, "dwarf_getfuncs %s",
		   dwarf_errmsg (-1));
	}
    }

  dwfl_end (dwfl);
  return 0;
}
