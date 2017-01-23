/* Get macro information.
   Copyright (C) 2002-2009, 2014 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <dwarf.h>
#include <search.h>
#include <stdlib.h>
#include <string.h>

#include <libdwP.h>

static int
get_offset_from (Dwarf_Die *die, int name, Dwarf_Word *retp)
{
  
  Dwarf_Attribute attr;
  if (INTUSE(dwarf_attr) (die, name, &attr) == NULL)
    return -1;

  
  return INTUSE(dwarf_formudata) (&attr, retp);
}

static int
macro_op_compare (const void *p1, const void *p2)
{
  const Dwarf_Macro_Op_Table *t1 = (const Dwarf_Macro_Op_Table *) p1;
  const Dwarf_Macro_Op_Table *t2 = (const Dwarf_Macro_Op_Table *) p2;

  if (t1->offset < t2->offset)
    return -1;
  if (t1->offset > t2->offset)
    return 1;

  if (t1->sec_index < t2->sec_index)
    return -1;
  if (t1->sec_index > t2->sec_index)
    return 1;

  return 0;
}

static void
build_table (Dwarf_Macro_Op_Table *table,
	     Dwarf_Macro_Op_Proto op_protos[static 255])
{
  unsigned ct = 0;
  for (unsigned i = 1; i < 256; ++i)
    if (op_protos[i - 1].forms != NULL)
      table->table[table->opcodes[i - 1] = ct++] = op_protos[i - 1];
    else
      table->opcodes[i - 1] = 0xff;
}

#define MACRO_PROTO(NAME, ...)					\
  Dwarf_Macro_Op_Proto NAME = ({				\
      static const uint8_t proto[] = {__VA_ARGS__};		\
      (Dwarf_Macro_Op_Proto) {sizeof proto, proto};		\
    })

enum { macinfo_data_size = offsetof (Dwarf_Macro_Op_Table, table[5]) };
static unsigned char macinfo_data[macinfo_data_size]
	__attribute__ ((aligned (__alignof (Dwarf_Macro_Op_Table))));

static __attribute__ ((constructor)) void
init_macinfo_table (void)
{
  MACRO_PROTO (p_udata_str, DW_FORM_udata, DW_FORM_string);
  MACRO_PROTO (p_udata_udata, DW_FORM_udata, DW_FORM_udata);
  MACRO_PROTO (p_none);

  Dwarf_Macro_Op_Proto op_protos[255] =
    {
      [DW_MACINFO_define - 1] = p_udata_str,
      [DW_MACINFO_undef - 1] = p_udata_str,
      [DW_MACINFO_vendor_ext - 1] = p_udata_str,
      [DW_MACINFO_start_file - 1] = p_udata_udata,
      [DW_MACINFO_end_file - 1] = p_none,
    };

  Dwarf_Macro_Op_Table *macinfo_table = (void *) macinfo_data;
  memset (macinfo_table, 0, sizeof macinfo_data);
  build_table (macinfo_table, op_protos);
  macinfo_table->sec_index = IDX_debug_macinfo;
}

static Dwarf_Macro_Op_Table *
get_macinfo_table (Dwarf *dbg, Dwarf_Word macoff, Dwarf_Die *cudie)
{
  assert (cudie != NULL);

  Dwarf_Attribute attr_mem, *attr
    = INTUSE(dwarf_attr) (cudie, DW_AT_stmt_list, &attr_mem);
  Dwarf_Off line_offset = (Dwarf_Off) -1;
  if (attr != NULL)
    INTUSE(dwarf_formudata) (attr, &line_offset);

  Dwarf_Macro_Op_Table *table = libdw_alloc (dbg, Dwarf_Macro_Op_Table,
					     macinfo_data_size, 1);
  memcpy (table, macinfo_data, macinfo_data_size);

  table->offset = macoff;
  table->sec_index = IDX_debug_macinfo;
  table->line_offset = line_offset;
  table->is_64bit = cudie->cu->address_size == 8;
  table->comp_dir = __libdw_getcompdir (cudie);

  return table;
}

static Dwarf_Macro_Op_Table *
get_table_for_offset (Dwarf *dbg, Dwarf_Word macoff,
		      const unsigned char *readp,
		      const unsigned char *const endp,
		      Dwarf_Die *cudie)
{
  const unsigned char *startp = readp;

  
  if (readp + 3 > endp)
    {
    invalid_dwarf:
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return NULL;
    }

  uint16_t version = read_2ubyte_unaligned_inc (dbg, readp);
  if (version != 4)
    {
      __libdw_seterrno (DWARF_E_INVALID_VERSION);
      return NULL;
    }

  uint8_t flags = *readp++;
  bool is_64bit = (flags & 0x1) != 0;

  Dwarf_Off line_offset = (Dwarf_Off) -1;
  if ((flags & 0x2) != 0)
    {
      line_offset = read_addr_unaligned_inc (is_64bit ? 8 : 4, dbg, readp);
      if (readp > endp)
	goto invalid_dwarf;
    }
  else if (cudie != NULL)
    {
      Dwarf_Attribute attr_mem, *attr
	= INTUSE(dwarf_attr) (cudie, DW_AT_stmt_list, &attr_mem);
      if (attr != NULL)
	INTUSE(dwarf_formudata) (attr, &line_offset);
    }


  MACRO_PROTO (p_udata_str, DW_FORM_udata, DW_FORM_string);
  MACRO_PROTO (p_udata_strp, DW_FORM_udata, DW_FORM_strp);
  MACRO_PROTO (p_udata_udata, DW_FORM_udata, DW_FORM_udata);
  MACRO_PROTO (p_secoffset, DW_FORM_sec_offset);
  MACRO_PROTO (p_none);

  Dwarf_Macro_Op_Proto op_protos[255] =
    {
      [DW_MACRO_GNU_define - 1] = p_udata_str,
      [DW_MACRO_GNU_undef - 1] = p_udata_str,
      [DW_MACRO_GNU_define_indirect - 1] = p_udata_strp,
      [DW_MACRO_GNU_undef_indirect - 1] = p_udata_strp,
      [DW_MACRO_GNU_start_file - 1] = p_udata_udata,
      [DW_MACRO_GNU_end_file - 1] = p_none,
      [DW_MACRO_GNU_transparent_include - 1] = p_secoffset,
    };

  if ((flags & 0x4) != 0)
    {
      unsigned count = *readp++;
      for (unsigned i = 0; i < count; ++i)
	{
	  unsigned opcode = *readp++;

	  Dwarf_Macro_Op_Proto e;
	  if (readp >= endp)
	    goto invalid;
	  get_uleb128 (e.nforms, readp, endp);
	  e.forms = readp;
	  op_protos[opcode - 1] = e;

	  readp += e.nforms;
	  if (readp > endp)
	    {
	    invalid:
	      __libdw_seterrno (DWARF_E_INVALID_DWARF);
	      return NULL;
	    }
	}
    }

  size_t ct = 0;
  for (unsigned i = 1; i < 256; ++i)
    if (op_protos[i - 1].forms != NULL)
      ++ct;

  assert (ct < 0xff);

  size_t macop_table_size = offsetof (Dwarf_Macro_Op_Table, table[ct]);

  Dwarf_Macro_Op_Table *table = libdw_alloc (dbg, Dwarf_Macro_Op_Table,
					     macop_table_size, 1);

  *table = (Dwarf_Macro_Op_Table) {
    .offset = macoff,
    .sec_index = IDX_debug_macro,
    .line_offset = line_offset,
    .header_len = readp - startp,
    .version = version,
    .is_64bit = is_64bit,

    
    .comp_dir = __libdw_getcompdir (cudie),
  };
  build_table (table, op_protos);

  return table;
}

static Dwarf_Macro_Op_Table *
cache_op_table (Dwarf *dbg, int sec_index, Dwarf_Off macoff,
		const unsigned char *startp,
		const unsigned char *const endp,
		Dwarf_Die *cudie)
{
  Dwarf_Macro_Op_Table fake = { .offset = macoff, .sec_index = sec_index };
  Dwarf_Macro_Op_Table **found = tfind (&fake, &dbg->macro_ops,
					macro_op_compare);
  if (found != NULL)
    return *found;

  Dwarf_Macro_Op_Table *table = sec_index == IDX_debug_macro
    ? get_table_for_offset (dbg, macoff, startp, endp, cudie)
    : get_macinfo_table (dbg, macoff, cudie);

  if (table == NULL)
    return NULL;

  Dwarf_Macro_Op_Table **ret = tsearch (table, &dbg->macro_ops,
					macro_op_compare);
  if (unlikely (ret == NULL))
    {
      __libdw_seterrno (DWARF_E_NOMEM);
      return NULL;
    }

  return *ret;
}

static ptrdiff_t
read_macros (Dwarf *dbg, int sec_index,
	     Dwarf_Off macoff, int (*callback) (Dwarf_Macro *, void *),
	     void *arg, ptrdiff_t offset, bool accept_0xff,
	     Dwarf_Die *cudie)
{
  Elf_Data *d = dbg->sectiondata[sec_index];
  if (unlikely (d == NULL || d->d_buf == NULL))
    {
      __libdw_seterrno (DWARF_E_NO_ENTRY);
      return -1;
    }

  if (unlikely (macoff >= d->d_size))
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return -1;
    }

  const unsigned char *const startp = d->d_buf + macoff;
  const unsigned char *const endp = d->d_buf + d->d_size;

  Dwarf_Macro_Op_Table *table = cache_op_table (dbg, sec_index, macoff,
						startp, endp, cudie);
  if (table == NULL)
    return -1;

  if (offset == 0)
    offset = table->header_len;

  assert (offset >= 0);
  assert (offset < endp - startp);
  const unsigned char *readp = startp + offset;

  while (readp < endp)
    {
      unsigned int opcode = *readp++;
      if (opcode == 0)
	
	return 0;

      if (unlikely (opcode == 0xff && ! accept_0xff))
	{
	  __libdw_seterrno (DWARF_E_INVALID_OPCODE);
	  return -1;
	}

      unsigned int idx = table->opcodes[opcode - 1];
      if (idx == 0xff)
	{
	  __libdw_seterrno (DWARF_E_INVALID_OPCODE);
	  return -1;
	}

      Dwarf_Macro_Op_Proto *proto = &table->table[idx];

      Dwarf_CU fake_cu = {
	.dbg = dbg,
	.version = 4,
	.offset_size = table->is_64bit ? 8 : 4,
	.startp = (void *) startp + offset,
	.endp = (void *) endp,
      };

      Dwarf_Attribute attributes[proto->nforms];
      for (Dwarf_Word i = 0; i < proto->nforms; ++i)
	{
	  attributes[i].code = DW_AT_GNU_macros;
	  attributes[i].form = proto->forms[i];
	  attributes[i].valp = (void *) readp;
	  attributes[i].cu = &fake_cu;

	  size_t len = __libdw_form_val_len (&fake_cu, proto->forms[i], readp);
	  if (len == (size_t) -1)
	    return -1;

	  readp += len;
	}

      Dwarf_Macro macro = {
	.table = table,
	.opcode = opcode,
	.attributes = attributes,
      };

      if (callback (&macro, arg) != DWARF_CB_OK)
	return readp - startp;
    }

  return 0;
}


static ptrdiff_t
token_from_offset (ptrdiff_t offset, bool accept_0xff)
{
  if (offset == -1 || offset == 0)
    return offset;

  
  if ((offset & DWARF_GETMACROS_START) != 0)
    {
      __libdw_seterrno (DWARF_E_TOO_BIG);
      return -1;
    }

  if (accept_0xff)
    offset |= DWARF_GETMACROS_START;

  return offset;
}

static ptrdiff_t
offset_from_token (ptrdiff_t token, bool *accept_0xffp)
{
  *accept_0xffp = (token & DWARF_GETMACROS_START) != 0;
  token &= ~DWARF_GETMACROS_START;

  return token;
}

static ptrdiff_t
gnu_macros_getmacros_off (Dwarf *dbg, Dwarf_Off macoff,
			  int (*callback) (Dwarf_Macro *, void *),
			  void *arg, ptrdiff_t offset, bool accept_0xff,
			  Dwarf_Die *cudie)
{
  assert (offset >= 0);

  if (macoff >= dbg->sectiondata[IDX_debug_macro]->d_size)
    {
      __libdw_seterrno (DWARF_E_INVALID_OFFSET);
      return -1;
    }

  return read_macros (dbg, IDX_debug_macro, macoff,
		      callback, arg, offset, accept_0xff, cudie);
}

static ptrdiff_t
macro_info_getmacros_off (Dwarf *dbg, Dwarf_Off macoff,
			  int (*callback) (Dwarf_Macro *, void *),
			  void *arg, ptrdiff_t offset, Dwarf_Die *cudie)
{
  assert (offset >= 0);

  return read_macros (dbg, IDX_debug_macinfo, macoff,
		      callback, arg, offset, true, cudie);
}

ptrdiff_t
dwarf_getmacros_off (Dwarf *dbg, Dwarf_Off macoff,
		     int (*callback) (Dwarf_Macro *, void *),
		     void *arg, ptrdiff_t token)
{
  if (dbg == NULL)
    {
      __libdw_seterrno (DWARF_E_NO_DWARF);
      return -1;
    }

  bool accept_0xff;
  ptrdiff_t offset = offset_from_token (token, &accept_0xff);
  assert (accept_0xff);

  offset = gnu_macros_getmacros_off (dbg, macoff, callback, arg, offset,
				     accept_0xff, NULL);

  return token_from_offset (offset, accept_0xff);
}

ptrdiff_t
dwarf_getmacros (cudie, callback, arg, token)
     Dwarf_Die *cudie;
     int (*callback) (Dwarf_Macro *, void *);
     void *arg;
     ptrdiff_t token;
{
  if (cudie == NULL)
    {
      __libdw_seterrno (DWARF_E_NO_DWARF);
      return -1;
    }


  bool accept_0xff;
  ptrdiff_t offset = offset_from_token (token, &accept_0xff);

  
  if (dwarf_hasattr (cudie, DW_AT_macro_info))
    {
      Dwarf_Word macoff;
      if (get_offset_from (cudie, DW_AT_macro_info, &macoff) != 0)
	return -1;
      offset = macro_info_getmacros_off (cudie->cu->dbg, macoff,
					 callback, arg, offset, cudie);
    }
  else
    {
      
      Dwarf_Word macoff;
      if (get_offset_from (cudie, DW_AT_GNU_macros, &macoff) != 0)
	return -1;
      offset = gnu_macros_getmacros_off (cudie->cu->dbg, macoff,
					 callback, arg, offset, accept_0xff,
					 cudie);
    }

  return token_from_offset (offset, accept_0xff);
}
