

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2013 Julian Seward
      jseward@acm.org

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#if defined(VGO_linux) || defined(VGO_darwin)

#include "pub_core_basics.h"
#include "pub_core_debuginfo.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcprint.h"
#include "pub_core_options.h"
#include "pub_core_xarray.h"
#include "pub_core_tooliface.h"    
#include "priv_misc.h"             
#include "priv_image.h"
#include "priv_d3basics.h"
#include "priv_tytypes.h"
#include "priv_storage.h"
#include "priv_readdwarf.h"        




static UInt safe_fndn_ix (XArray* fndn_ix_xa, Int xa_ix)
{
   if (xa_ix < 0) return 0;
   if (xa_ix >= VG_(sizeXA) (fndn_ix_xa)) return 0;
   return *(UInt*)VG_(indexXA) ( fndn_ix_xa, xa_ix );
}

static const HChar* safe_dirname_ix (XArray* dirname_xa, Int xa_ix)
{
   if (xa_ix < 0) return NULL;
   if (xa_ix >= VG_(sizeXA) (dirname_xa)) return NULL;
   return *(HChar**)VG_(indexXA) ( dirname_xa, xa_ix );
}


typedef struct
{
  ULong  li_length;
  UShort li_version;
  ULong  li_header_length;
  UChar  li_min_insn_length;
  UChar  li_max_ops_per_insn;
  UChar  li_default_is_stmt;
  Int    li_line_base;
  UChar  li_line_range;
  UChar  li_opcode_base;
}
DebugLineInfo;

typedef struct
{
  
  DiCursor compdir;  
  DiCursor name;     
  ULong    stmt_list; 
  Bool     dw64;      
} 
UnitInfo;

enum dwarf_line_number_ops
  {
    DW_LNS_extended_op = 0,
    DW_LNS_copy = 1,
    DW_LNS_advance_pc = 2,
    DW_LNS_advance_line = 3,
    DW_LNS_set_file = 4,
    DW_LNS_set_column = 5,
    DW_LNS_negate_stmt = 6,
    DW_LNS_set_basic_block = 7,
    DW_LNS_const_add_pc = 8,
    DW_LNS_fixed_advance_pc = 9,
    
    DW_LNS_set_prologue_end = 10,
    DW_LNS_set_epilogue_begin = 11,
    DW_LNS_set_isa = 12
  };

enum dwarf_line_number_x_ops
  {
    DW_LNE_end_sequence = 1,
    DW_LNE_set_address = 2,
    DW_LNE_define_file = 3,
    DW_LNE_set_discriminator = 4
  };

typedef struct
{
  Addr  last_address;
  UInt  last_file;
  UInt  last_line;

  Addr  address;
  UInt  file;
  UInt  line;
  UInt  column;
  Int   is_stmt;
  Int   basic_block;
  UChar end_sequence;
} LineSMR;


static ULong step_leb128 ( DiCursor* data, Int sign )
{
   ULong  result = 0;
   Int    shift = 0;
   UChar  byte;

   vg_assert(sign == 0 || sign == 1);

   do {
      byte = ML_(cur_step_UChar)(data);
      result |= ((ULong)(byte & 0x7f)) << shift;
      shift += 7;
   }
   while (byte & 0x80);

   if (sign && (shift < 64) && (byte & 0x40))
      result |= -(1ULL << shift);

   return result;
}

static ULong step_leb128U( DiCursor* data ) {
   return step_leb128( data, 0 );
}

static Long step_leb128S( DiCursor* data ) {
   return step_leb128( data, 1 );
}

static
ULong step_initial_length_field ( DiCursor* p_img, Bool* is64 )
{
   UInt w32 = ML_(cur_step_UInt)(p_img);
   if (w32 == 0xFFFFFFFF) {
      *is64 = True;
      return ML_(cur_step_ULong)(p_img);
   } else {
      *is64 = False;
      return (ULong)w32;
   }
}

static
ULong read_initial_length_field ( DiCursor p_img, Bool* is64 )
{
   return step_initial_length_field( &p_img, is64 );
}


static LineSMR state_machine_regs;

static 
void reset_state_machine ( Int is_stmt )
{
   if (0) VG_(printf)("smr.a := %p (reset)\n", NULL );
   state_machine_regs.last_address = 0;
   state_machine_regs.last_file = 1;
   state_machine_regs.last_line = 1;
   state_machine_regs.address = 0;
   state_machine_regs.file = 1;
   state_machine_regs.line = 1;
   state_machine_regs.column = 0;
   state_machine_regs.is_stmt = is_stmt;
   state_machine_regs.basic_block = 0;
   state_machine_regs.end_sequence = 0;
}


static 
void process_extended_line_op( struct _DebugInfo* di,
                               XArray* fndn_ix_xa,
                               DiCursor* data, Int is_stmt)
{
   UInt len = step_leb128U(data);
   if (len == 0) {
      VG_(message)(Vg_UserMsg,
                   "Warning: DWARF2 reader: "
                   "Badly formed extended line op encountered\n");
      return;
   }

   UChar op_code = ML_(cur_step_UChar)(data);
   if (0) VG_(printf)("dwarf2: ext OPC: %d\n", op_code);

   switch (op_code) {
      case DW_LNE_end_sequence:
         if (0) VG_(printf)("1001: si->o %#lx, smr.a %#lx\n",
                            di->text_debug_bias, state_machine_regs.address );
         state_machine_regs.end_sequence = 1; 

         if (state_machine_regs.is_stmt) {
            if (state_machine_regs.last_address) {
               ML_(addLineInfo) (
                  di,
                  safe_fndn_ix (fndn_ix_xa,
                                state_machine_regs.last_file),
                  di->text_debug_bias + state_machine_regs.last_address, 
                  di->text_debug_bias + state_machine_regs.address, 
                  state_machine_regs.last_line, 0
               );
            }
         }
         reset_state_machine (is_stmt);
         if (di->ddump_line)
            VG_(printf)("  Extended opcode %d: End of Sequence\n\n", 
                        (Int)op_code);
         break;

      case DW_LNE_set_address: {
         Addr adr = ML_(cur_step_Addr)(data);
         state_machine_regs.address = adr;
         if (di->ddump_line)
            VG_(printf)("  Extended opcode %d: set Address to 0x%lx\n",
                        (Int)op_code, (Addr)adr);
         break;
      }

      case DW_LNE_define_file: {
         HChar* name = ML_(cur_step_strdup)(data, "di.pelo.1");
         UInt fndn_ix = ML_(addFnDn) (di, name, NULL);
         VG_(addToXA) (fndn_ix_xa, &fndn_ix);
         ML_(dinfo_free)(name);
         (void)step_leb128U(data); 
         (void)step_leb128U(data); 
         (void)step_leb128U(data); 
         if (di->ddump_line)
            VG_(printf)("  DWARF2-line: set_address\n");
         break;
      }

      case DW_LNE_set_discriminator:
         (void)step_leb128U(data); 
         break;

      default:
         if (di->ddump_line)
            VG_(printf)("process_extended_line_op:default\n");
         break;
   }
}


static 
void read_dwarf2_lineblock ( struct _DebugInfo* di,
                             const UnitInfo* ui, 
                             DiCursor  theBlock, 
                             Int       noLargerThan )
{
   Int            i;
   DebugLineInfo  info;
   Bool           is64;
   XArray*        fndn_ix_xa; 
   UInt           fndn_ix;
   XArray*        dirname_xa;   
   const HChar*   dirname;

   DiCursor       external = theBlock;
   DiCursor       data = theBlock;



   dirname_xa = VG_(newXA) (ML_(dinfo_zalloc), "di.rd2l.1", ML_(dinfo_free),
                            sizeof(HChar*) );
   fndn_ix_xa = VG_(newXA) (ML_(dinfo_zalloc), "di.rd2l.2", ML_(dinfo_free),
                            sizeof(UInt) );

   fndn_ix = 0; 
   VG_(addToXA) (fndn_ix_xa, &fndn_ix);

   if (ML_(cur_is_valid)(ui->compdir))
      dirname = ML_(addStrFromCursor)(di, ui->compdir);
   else
      dirname = ML_(addStr)(di, ".", -1);
   VG_(addToXA) (dirname_xa, &dirname);

   info.li_length = step_initial_length_field( &external, &is64 );
   if (di->ddump_line)
      VG_(printf)("  Length:                      %llu\n", 
                  info.li_length);

   
   if (info.li_length > noLargerThan) {
      ML_(symerr)(di, True,
                  "DWARF line info appears to be corrupt "
                  "- the section is too small");
      goto out;
   }

   
   info.li_version = ML_(cur_step_UShort)(&external);
   if (di->ddump_line)
      VG_(printf)("  DWARF Version:               %d\n", 
                  (Int)info.li_version);

   if (info.li_version != 2 && info.li_version != 3 && info.li_version != 4) {
      ML_(symerr)(di, True,
                  "Only DWARF version 2, 3 and 4 line info "
                  "is currently supported.");
      goto out;
   }

   info.li_header_length = is64 ? ML_(cur_step_ULong)(&external) 
                                : (ULong)(ML_(cur_step_UInt)(&external));
   if (di->ddump_line)
      VG_(printf)("  Prologue Length:             %llu\n", 
                  info.li_header_length);

   info.li_min_insn_length = ML_(cur_step_UChar)(&external);
   if (di->ddump_line)
      VG_(printf)("  Minimum Instruction Length:  %d\n", 
                  (Int)info.li_min_insn_length);

   if (info.li_version >= 4) {
      info.li_max_ops_per_insn = ML_(cur_step_UChar)(&external);
      if (info.li_max_ops_per_insn != 1) {
         ML_(symerr)(di, True,
                     "Invalid Maximum Ops Per Insn in line info.");
         goto out;
      }
      if (di->ddump_line)
         VG_(printf)("  Maximum Ops Per Insn:        %d\n", 
                  (Int)info.li_max_ops_per_insn);
   } else {
      info.li_max_ops_per_insn = 1;
   }

   info.li_default_is_stmt = ML_(cur_step_UChar)(&external);
   if (di->ddump_line)
      VG_(printf)("  Initial value of 'is_stmt':  %d\n", 
                  (Int)info.li_default_is_stmt);

   info.li_default_is_stmt = True; 

   
   info.li_line_base = ML_(cur_step_UChar)(&external);
   info.li_line_base = (Int)(Char)info.li_line_base;
   if (di->ddump_line)
      VG_(printf)("  Line Base:                   %d\n", 
                  info.li_line_base);

   info.li_line_range = ML_(cur_step_UChar)(&external);
   if (di->ddump_line)
      VG_(printf)("  Line Range:                  %d\n", 
                  (Int)info.li_line_range);

   info.li_opcode_base = ML_(cur_step_UChar)(&external);
   if (di->ddump_line)
      VG_(printf)("  Opcode Base:                 %d\n\n", 
                  info.li_opcode_base);

   if (0) VG_(printf)("dwarf2: line base: %d, range %d, opc base: %d\n",
                      (Int)info.li_line_base, 
                      (Int)info.li_line_range,
                      (Int)info.li_opcode_base);

   DiCursor end_of_sequence
     = ML_(cur_plus)(data, info.li_length + (is64 ? 12 : 4));

   reset_state_machine (info.li_default_is_stmt);

   
   DiCursor standard_opcodes = external;
   if (di->ddump_line) {
      VG_(printf)(" Opcodes:\n");
      for (i = 1; i < (Int)info.li_opcode_base; i++) {
         VG_(printf)("  Opcode %d has %d args\n", 
                     i, (Int)ML_(cur_read_UChar)(
                                ML_(cur_plus)(standard_opcodes,
                                              (i-1) * sizeof(UChar)) ));
      }
      VG_(printf)("\n");
   }
   
   data = ML_(cur_plus)(standard_opcodes, info.li_opcode_base - 1);

   
   if (di->ddump_line)
      VG_(printf)(" The Directory Table%s\n", 
                  ML_(cur_read_UChar)(data) == 0 ? " is empty." : ":" );

   while (ML_(cur_read_UChar)(data) != 0) {

      HChar* data_str = ML_(cur_read_strdup)(data, "di.rd2l.1");
      if (di->ddump_line)
         VG_(printf)("  %s\n", data_str);


      if (data_str[0] != '/' 
          
          && ML_(cur_is_valid)(ui->compdir)
          
          && ML_(cur_strlen)(ui->compdir))
      {
         HChar* compdir_str = ML_(cur_read_strdup)(ui->compdir, "di.rd2l.1b");
         SizeT  len = VG_(strlen)(compdir_str) + 1 + VG_(strlen)(data_str);
         HChar *buf = ML_(dinfo_zalloc)("di.rd2l.1c", len + 1);

         VG_(strcpy)(buf, compdir_str);
         VG_(strcat)(buf, "/");
         VG_(strcat)(buf, data_str);

         dirname = ML_(addStr)(di, buf, len);
         VG_(addToXA) (dirname_xa, &dirname);
         if (0) VG_(printf)("rel path  %s\n", buf);
         ML_(dinfo_free)(compdir_str);
         ML_(dinfo_free)(buf);
      } else {
         
         dirname = ML_(addStr)(di,data_str,-1);
         VG_(addToXA) (dirname_xa, &dirname);
         if (0) VG_(printf)("abs path  %s\n", data_str);
      }

      data = ML_(cur_plus)(data, VG_(strlen)(data_str) + 1);
      ML_(dinfo_free)(data_str);
   }

   if (di->ddump_line)
      VG_(printf)("\n");

   if (ML_(cur_read_UChar)(data) != 0) {
      ML_(symerr)(di, True,
                  "can't find NUL at end of DWARF2 directory table");
      goto out;
   }
   data = ML_(cur_plus)(data, 1);

   if (di->ddump_line) {
      VG_(printf)(" The File Name Table:\n");
      VG_(printf)("  Entry	Dir	Time	Size	Name\n");
   }

   i = 1;
   while (ML_(cur_read_UChar)(data) != 0) {
      HChar* name    = ML_(cur_step_strdup)(&data, "di.rd2l.2");
      Int    diridx  = step_leb128U(&data);
      Int    uu_time = step_leb128U(&data); 
      Int    uu_size = step_leb128U(&data); 

      dirname = safe_dirname_ix( dirname_xa, diridx );
      fndn_ix = ML_(addFnDn) (di, name, dirname);
      VG_(addToXA) (fndn_ix_xa, &fndn_ix);
      if (0) VG_(printf)("file %s diridx %d\n", name, diridx );
      if (di->ddump_line)
         VG_(printf)("  %d\t%d\t%d\t%d\t%s\n", 
                     i, diridx, uu_time, uu_size, name);
      i++;
      ML_(dinfo_free)(name);
   }

   if (di->ddump_line)
      VG_(printf)("\n");

   if (ML_(cur_read_UChar)(data) != 0) {
      ML_(symerr)(di, True,
                  "can't find NUL at end of DWARF2 file name table");
      goto out;
   }
   data = ML_(cur_plus)(data, 1);

   if (di->ddump_line)
      VG_(printf)(" Line Number Statements:\n");

   

   while (ML_(cur_cmpLT)(data, end_of_sequence)) {
      UChar op_code = ML_(cur_step_UChar)(&data);

      if (0) VG_(printf)("dwarf2: OPC: %d\n", op_code);

      if (op_code >= info.li_opcode_base) {
         op_code -= info.li_opcode_base;
         Word adv = (op_code / info.li_line_range)
                       * info.li_min_insn_length;
         Int advAddr = adv;
         state_machine_regs.address += adv;

         if (0) VG_(printf)("smr.a += %#lx\n", adv );
         adv = (op_code % info.li_line_range) + info.li_line_base;
         if (0) VG_(printf)("1002: di->o %#lx, smr.a %#lx\n",
                            di->text_debug_bias, state_machine_regs.address );
         state_machine_regs.line += adv;

         if (di->ddump_line)
            VG_(printf)("  Special opcode %d: advance Address by %d "
                        "to 0x%lx and Line by %d to %d\n", 
                        (Int)op_code, advAddr, state_machine_regs.address,
                        (Int)adv, (Int)state_machine_regs.line );

         if (state_machine_regs.is_stmt) {
            
            if (state_machine_regs.last_address) {
               ML_(addLineInfo)(
                  di,
                  safe_fndn_ix (fndn_ix_xa,
                                state_machine_regs.last_file),
                  di->text_debug_bias + state_machine_regs.last_address, 
                  di->text_debug_bias + state_machine_regs.address, 
                  state_machine_regs.last_line, 
                  0
               );
            }
            state_machine_regs.last_address = state_machine_regs.address;
            state_machine_regs.last_file = state_machine_regs.file;
            state_machine_regs.last_line = state_machine_regs.line;
         }
      }

      else { 

      switch (op_code) {
         case DW_LNS_extended_op:
            process_extended_line_op (
                       di, fndn_ix_xa,
                       &data, info.li_default_is_stmt);
            break;

         case DW_LNS_copy:
            if (0) VG_(printf)("1002: di->o %#lx, smr.a %#lx\n",
                               di->text_debug_bias, state_machine_regs.address );
            if (state_machine_regs.is_stmt) {
               
               if (state_machine_regs.last_address) {
                  ML_(addLineInfo)(
                     di,
                     safe_fndn_ix (fndn_ix_xa,
                                   state_machine_regs.last_file), 
                     di->text_debug_bias + state_machine_regs.last_address, 
                     di->text_debug_bias + state_machine_regs.address,
                     state_machine_regs.last_line, 
                     0
                  );
               }
               state_machine_regs.last_address = state_machine_regs.address;
               state_machine_regs.last_file = state_machine_regs.file;
               state_machine_regs.last_line = state_machine_regs.line;
            }
            state_machine_regs.basic_block = 0; 
            if (di->ddump_line)
               VG_(printf)("  Copy\n");
            break;

         case DW_LNS_advance_pc: {
            Word adv = info.li_min_insn_length * step_leb128U(&data);
            state_machine_regs.address += adv;
            if (0) VG_(printf)("smr.a += %#lx\n", adv );
            if (di->ddump_line)
               VG_(printf)("  Advance PC by %ld to 0x%lx\n", 
                           adv, state_machine_regs.address);
            break;
         }
         case DW_LNS_advance_line: {
            Word adv = step_leb128S(&data);
            state_machine_regs.line += adv;
            if (di->ddump_line)
               VG_(printf)("  Advance Line by %ld to %d\n", 
                           adv, (Int)state_machine_regs.line);
            break;
         }
         case DW_LNS_set_file: {
            Word adv = step_leb128U(&data);
            state_machine_regs.file = adv;
            if (di->ddump_line)
               VG_(printf)("  Set File Name to entry %ld in the "
                           "File Name Table\n", adv);
            break;
         }
         case DW_LNS_set_column: {
            Word adv = step_leb128U(&data);
            state_machine_regs.column = adv;
            if (di->ddump_line)
               VG_(printf)("  Set column to %ld\n", adv);
            break;
         }
         case DW_LNS_negate_stmt: {
            Int adv = state_machine_regs.is_stmt;
            adv = ! adv;
            state_machine_regs.is_stmt = adv;
            if (di->ddump_line)
               VG_(printf)("  DWARF2-line: negate_stmt\n");
            break;
         }
         case DW_LNS_set_basic_block: {
            state_machine_regs.basic_block = 1;
            if (di->ddump_line)
               VG_(printf)("  DWARF2-line: set_basic_block\n");
            break;
         }
         case DW_LNS_const_add_pc: {
            Word adv = (((255 - info.li_opcode_base) / info.li_line_range)
                          * info.li_min_insn_length);
            state_machine_regs.address += adv;
            if (0) VG_(printf)("smr.a += %#lx\n", adv );
            if (di->ddump_line)
               VG_(printf)("  Advance PC by constant %ld to 0x%lx\n", 
                           adv, (Addr)state_machine_regs.address);
            break;
         }
         case DW_LNS_fixed_advance_pc: {
            
            Word adv = ML_(cur_step_UShort)(&data);
            state_machine_regs.address += adv;
            if (0) VG_(printf)("smr.a += %#lx\n", adv );
            if (di->ddump_line)
               VG_(printf)("  DWARF2-line: fixed_advance_pc\n");
            break;
         }
         case DW_LNS_set_prologue_end:
            if (di->ddump_line)
               VG_(printf)("  DWARF2-line: set_prologue_end\n");
            break;

         case DW_LNS_set_epilogue_begin:
            if (di->ddump_line)
               VG_(printf)("  DWARF2-line: set_epilogue_begin\n");
            break;

         case DW_LNS_set_isa:
            (void)step_leb128U(&data);
            if (di->ddump_line)
               VG_(printf)("  DWARF2-line: set_isa\n");
            break;

         default: {
            Int j;
            for (j = (Int)ML_(cur_read_UChar)(
                             ML_(cur_plus)(standard_opcodes,
                                           (op_code-1) * sizeof(UChar)));
                 j > 0 ; --j) {
               step_leb128U(&data);
            }
            if (di->ddump_line)
               VG_(printf)("  Unknown opcode %d\n", (Int)op_code);
            break;
         }
      } 

      } 

   } 

   if (di->ddump_line)
      VG_(printf)("\n");

  out:
   VG_(deleteXA)(dirname_xa);
   VG_(deleteXA)(fndn_ix_xa);
}


static DiCursor lookup_abbrev( DiCursor p, ULong acode )
{
   while (1) {
      ULong code = step_leb128U(&p);
      if (code == acode)
         return p;
      (void)step_leb128U(&p);  
      p = ML_(cur_plus)(p,1);  
      ULong name;
      do {
         name = step_leb128U(&p); 
         (void)step_leb128U(&p);  
      }
      while (name != 0); 
   }
}

static 
void read_unitinfo_dwarf2( UnitInfo* ui,
                                  DiCursor  unitblock_img,
                                  DiCursor  debugabbrev_img,
                                  DiCursor  debugstr_img,
                                  DiCursor  debugstr_alt_img )
{
   UInt   acode, abcode;
   ULong  atoffs, blklen;
   UShort ver;

   UChar    addr_size;
   DiCursor p = unitblock_img;
   DiCursor end_img;
   DiCursor abbrev_img;

   VG_(memset)( ui, 0, sizeof( UnitInfo ) );
   ui->stmt_list = -1LL;
   
   

   
   blklen = step_initial_length_field( &p, &ui->dw64 );

   
   ver = ML_(cur_step_UShort)(&p);

   
   atoffs = ui->dw64 ? ML_(cur_step_ULong)(&p)
                     : (ULong)(ML_(cur_step_UInt)(&p));

   
   addr_size = ML_(cur_step_UChar)(&p);

   
   end_img = ML_(cur_plus)(unitblock_img, blklen + (ui->dw64 ? 12 : 4)); 

   
   abbrev_img = ML_(cur_plus)(debugabbrev_img, atoffs);
   
   if (ML_(cur_cmpLT)(p, end_img)) {
      UInt tag;

      acode = step_leb128U( &p ); 
      
      
      abcode = step_leb128U( &abbrev_img ); 
      if ( acode != abcode ) {
         abbrev_img = lookup_abbrev( ML_(cur_plus)(debugabbrev_img, atoffs),
                                     acode );
      }

      tag = step_leb128U( &abbrev_img );

      if ( tag != 0x0011  )
         return; 

      
      abbrev_img = ML_(cur_plus)(abbrev_img, 1);

      
      for ( ; ; ) {
         
         ULong    cval = -1LL;  
         DiCursor sval = DiCursor_INVALID; 
         UInt     name = step_leb128U( &abbrev_img );
         UInt     form = step_leb128U( &abbrev_img );
         if (name == 0)
            break;
       
         
         
         if ( form == 0x16  )
            form = step_leb128U( &p );
         switch( form ) {
            
            case 0x05: 
               cval = ML_(cur_step_UShort)(&p);
               break;
            case 0x06: 
               cval = ML_(cur_step_UInt)(&p);
               break;
            case 0x0e:       
               if (ML_(cur_is_valid)(debugstr_img) && !ui->dw64)
                  sval = ML_(cur_plus)(debugstr_img, ML_(cur_read_UInt)(p));
               if (ML_(cur_is_valid)(debugstr_img) && ui->dw64)
                  sval = ML_(cur_plus)(debugstr_img, ML_(cur_read_ULong)(p));
               p = ML_(cur_plus)(p, ui->dw64 ? 8 : 4);
               break;
            case 0x08: 
               sval = p;
               p = ML_(cur_plus)(p, ML_(cur_strlen)(p) + 1);
               break;
            case 0x0b: 
               cval = ML_(cur_step_UChar)(&p);
               break;
            case 0x17: 
               if (ui->dw64) {
                 cval = ML_(cur_step_ULong)(&p);
               } else {
                 cval = ML_(cur_step_UInt)(&p);
               };
               break;
            case 0x07: 
               if (ui->dw64) cval = ML_(cur_read_ULong)(p);
               p = ML_(cur_plus)(p, 8);
               
               break;
            
            case 0x01: 
               p = ML_(cur_plus)(p, addr_size);
               break;
            case 0x03: 
               p = ML_(cur_plus)(p, ML_(cur_read_UShort)(p) + 2);
               break;
            case 0x04: 
               p = ML_(cur_plus)(p, ML_(cur_read_UInt)(p) + 4);
               break;
            case 0x09:        
            case 0x18: { 
               ULong block_len = step_leb128U(&p);
               p = ML_(cur_plus)(p, block_len);
               break;
            }
            case 0x0a: 
               p = ML_(cur_plus)(p, ML_(cur_read_UChar)(p) + 1);
               break;
            case 0x0c: 
               p = ML_(cur_plus)(p, 1);
               break;
            case 0x0d: 
               (void)step_leb128S(&p);
               break;
            case 0x0f: 
               (void)step_leb128U(&p);
               break;
            case 0x10: 
               p = ML_(cur_plus)(p, (ver == 2) ? addr_size 
                                               : (ui->dw64 ? 8 : 4));
               break;
            case 0x11: 
               p = ML_(cur_plus)(p, 1);
               break;
            case 0x12: 
               p = ML_(cur_plus)(p, 2);
               break;
            case 0x13: 
               p = ML_(cur_plus)(p, 4);
               break;
            case 0x14: 
               p = ML_(cur_plus)(p, 8);
               break;
            case 0x15: 
               (void)step_leb128U(&p);
               break;
            case 0x19: 
               break;
            case 0x20: 
               p = ML_(cur_plus)(p, 8);
               break;
            case 0x1f20: 
               p = ML_(cur_plus)(p, ui->dw64 ? 8 : 4);
               break;
            case 0x1f21: 
               if (ML_(cur_is_valid)(debugstr_alt_img) && !ui->dw64)
                  sval = ML_(cur_plus)(debugstr_alt_img,
                                       ML_(cur_read_UInt)(p));
               if (ML_(cur_is_valid)(debugstr_alt_img) && ui->dw64)
                  sval = ML_(cur_plus)(debugstr_alt_img,
                                       ML_(cur_read_ULong)(p));
               p = ML_(cur_plus)(p, ui->dw64 ? 8 : 4);
               break;

            default:
               VG_(printf)( "### unhandled dwarf2 abbrev form code 0x%x\n",
                            form );
               break;
         }
         
         
         if ( tag == 0x0011  ) {
                 if ( name == 0x03 ) ui->name = sval;      
            else if ( name == 0x1b ) ui->compdir = sval;   
            else if ( name == 0x10 ) ui->stmt_list = cval; 
         }
      }
   } 
}



void ML_(read_debuginfo_dwarf3)
        ( struct _DebugInfo* di,
          DiSlice escn_debug_info,      
          DiSlice escn_debug_types,     
          DiSlice escn_debug_abbv,      
          DiSlice escn_debug_line,      
          DiSlice escn_debug_str,       
          DiSlice escn_debug_str_alt )  
{
   UnitInfo ui;
   UShort   ver;
   ULong    blklen;
   Bool     blklen_is_64;

   
   if (escn_debug_info.szB < 4) {
      ML_(symerr)( di, True, 
                   "Last block truncated in .debug_info; ignoring" );
      return;
   }

   DiCursor block_img = DiCursor_INVALID;
   DiCursor end1_img  = ML_(cur_plus)( ML_(cur_from_sli)(escn_debug_info), 
                                       escn_debug_info.szB );
   Int blklen_len = 0;

   
   for ( block_img = ML_(cur_from_sli)(escn_debug_info);
         ML_(cur_cmpLT)(block_img, ML_(cur_plus)(end1_img, -(DiOffT)4));
         block_img = ML_(cur_plus)(block_img, blklen + blklen_len) ) {

      
      blklen     = read_initial_length_field( block_img, &blklen_is_64 );
      blklen_len = blklen_is_64 ? 12 : 4;

      if (ML_(cur_cmpGT)( ML_(cur_plus)(block_img, blklen + blklen_len),
                          end1_img )) {
         ML_(symerr)( di, True,
                      "Last block truncated in .debug_info; ignoring" );
         return;
      }

      
      ver = ML_(cur_read_UShort)( ML_(cur_plus)(block_img, blklen_len) );
      if ( ver != 2 && ver != 3 && ver != 4 ) {
         ML_(symerr)( di, True,
                      "Ignoring non-Dwarf2/3/4 block in .debug_info" );
         continue;
      }
      
      
      if (0)
         VG_(printf)(
            "Reading UnitInfo at 0x%llx.....\n",
            (ULong)ML_(cur_minus)( block_img,
                                   ML_(cur_from_sli)(escn_debug_info)) );
      read_unitinfo_dwarf2( &ui, block_img, 
                                 ML_(cur_from_sli)(escn_debug_abbv),
                                 ML_(cur_from_sli)(escn_debug_str),
                                 ML_(cur_from_sli)(escn_debug_str_alt) );
      if (0) {
         HChar* str_name    = ML_(cur_read_strdup)(ui.name,    "di.rdd3.1");
         HChar* str_compdir = ML_(cur_read_strdup)(ui.compdir, "di.rdd3.2");
         VG_(printf)( "   => LINES=0x%llx    NAME=%s     DIR=%s\n", 
                      ui.stmt_list, str_name, str_compdir );
         ML_(dinfo_free)(str_name);
         ML_(dinfo_free)(str_compdir);
      }

      
      if ( ui.stmt_list == -1LL )
         continue;
      
      if (0) {
         HChar* str_name = ML_(cur_read_strdup)(ui.name, "di.rdd3.3");
         VG_(printf)("debug_line_sz %lld, ui.stmt_list %lld  %s\n",
                     escn_debug_line.szB, ui.stmt_list, str_name );
         ML_(dinfo_free)(str_name);
      }

      
      read_dwarf2_lineblock(
         di, &ui,
         ML_(cur_plus)(ML_(cur_from_sli)(escn_debug_line), ui.stmt_list),
         escn_debug_line.szB  - ui.stmt_list
      );
   }
}





/* The following three enums (dwarf_tag, dwarf_form, dwarf_attribute)
   are taken from the file include/elf/dwarf.h in the GNU gdb-6.0
   sources, which are Copyright 1992, 1993, 1995, 1999 Free Software
   Foundation, Inc and naturally licensed under the GNU General Public
   License version 2 or later. 
*/


enum dwarf_tag {
    TAG_padding			= 0x0000,
    TAG_array_type		= 0x0001,
    TAG_class_type		= 0x0002,
    TAG_entry_point		= 0x0003,
    TAG_enumeration_type	= 0x0004,
    TAG_formal_parameter	= 0x0005,
    TAG_global_subroutine	= 0x0006,
    TAG_global_variable		= 0x0007,
    				
				
    TAG_label			= 0x000a,
    TAG_lexical_block		= 0x000b,
    TAG_local_variable		= 0x000c,
    TAG_member			= 0x000d,
				
    TAG_pointer_type		= 0x000f,
    TAG_reference_type		= 0x0010,
    TAG_compile_unit		= 0x0011,
    TAG_string_type		= 0x0012,
    TAG_structure_type		= 0x0013,
    TAG_subroutine		= 0x0014,
    TAG_subroutine_type		= 0x0015,
    TAG_typedef			= 0x0016,
    TAG_union_type		= 0x0017,
    TAG_unspecified_parameters	= 0x0018,
    TAG_variant			= 0x0019,
    TAG_common_block		= 0x001a,
    TAG_common_inclusion	= 0x001b,
    TAG_inheritance		= 0x001c,
    TAG_inlined_subroutine	= 0x001d,
    TAG_module			= 0x001e,
    TAG_ptr_to_member_type	= 0x001f,
    TAG_set_type		= 0x0020,
    TAG_subrange_type		= 0x0021,
    TAG_with_stmt		= 0x0022,

    

    TAG_format_label		= 0x8000,  
    TAG_namelist		= 0x8001,  
    TAG_function_template	= 0x8002,  
    TAG_class_template		= 0x8003   
};


enum dwarf_form {
    FORM_ADDR	= 0x1,
    FORM_REF	= 0x2,
    FORM_BLOCK2	= 0x3,
    FORM_BLOCK4	= 0x4,
    FORM_DATA2	= 0x5,
    FORM_DATA4	= 0x6,
    FORM_DATA8	= 0x7,
    FORM_STRING	= 0x8
};


enum dwarf_attribute {
    AT_sibling			= (0x0010|FORM_REF),
    AT_location			= (0x0020|FORM_BLOCK2),
    AT_name			= (0x0030|FORM_STRING),
    AT_fund_type		= (0x0050|FORM_DATA2),
    AT_mod_fund_type		= (0x0060|FORM_BLOCK2),
    AT_user_def_type		= (0x0070|FORM_REF),
    AT_mod_u_d_type		= (0x0080|FORM_BLOCK2),
    AT_ordering			= (0x0090|FORM_DATA2),
    AT_subscr_data		= (0x00a0|FORM_BLOCK2),
    AT_byte_size		= (0x00b0|FORM_DATA4),
    AT_bit_offset		= (0x00c0|FORM_DATA2),
    AT_bit_size			= (0x00d0|FORM_DATA4),
				
    AT_element_list		= (0x00f0|FORM_BLOCK4),
    AT_stmt_list		= (0x0100|FORM_DATA4),
    AT_low_pc			= (0x0110|FORM_ADDR),
    AT_high_pc			= (0x0120|FORM_ADDR),
    AT_language			= (0x0130|FORM_DATA4),
    AT_member			= (0x0140|FORM_REF),
    AT_discr			= (0x0150|FORM_REF),
    AT_discr_value		= (0x0160|FORM_BLOCK2),
				
				
    AT_string_length		= (0x0190|FORM_BLOCK2),
    AT_common_reference		= (0x01a0|FORM_REF),
    AT_comp_dir			= (0x01b0|FORM_STRING),
        AT_const_value_string	= (0x01c0|FORM_STRING),
        AT_const_value_data2	= (0x01c0|FORM_DATA2),
        AT_const_value_data4	= (0x01c0|FORM_DATA4),
        AT_const_value_data8	= (0x01c0|FORM_DATA8),
        AT_const_value_block2	= (0x01c0|FORM_BLOCK2),
        AT_const_value_block4	= (0x01c0|FORM_BLOCK4),
    AT_containing_type		= (0x01d0|FORM_REF),
        AT_default_value_addr	= (0x01e0|FORM_ADDR),
        AT_default_value_data2	= (0x01e0|FORM_DATA2),
        AT_default_value_data4	= (0x01e0|FORM_DATA4),
        AT_default_value_data8	= (0x01e0|FORM_DATA8),
        AT_default_value_string	= (0x01e0|FORM_STRING),
    AT_friends			= (0x01f0|FORM_BLOCK2),
    AT_inline			= (0x0200|FORM_STRING),
    AT_is_optional		= (0x0210|FORM_STRING),
        AT_lower_bound_ref	= (0x0220|FORM_REF),
        AT_lower_bound_data2	= (0x0220|FORM_DATA2),
        AT_lower_bound_data4	= (0x0220|FORM_DATA4),
        AT_lower_bound_data8	= (0x0220|FORM_DATA8),
    AT_private			= (0x0240|FORM_STRING),
    AT_producer			= (0x0250|FORM_STRING),
    AT_program			= (0x0230|FORM_STRING),
    AT_protected		= (0x0260|FORM_STRING),
    AT_prototyped		= (0x0270|FORM_STRING),
    AT_public			= (0x0280|FORM_STRING),
    AT_pure_virtual		= (0x0290|FORM_STRING),
    AT_return_addr		= (0x02a0|FORM_BLOCK2),
    AT_abstract_origin		= (0x02b0|FORM_REF),
    AT_start_scope		= (0x02c0|FORM_DATA4),
    AT_stride_size		= (0x02e0|FORM_DATA4),
        AT_upper_bound_ref	= (0x02f0|FORM_REF),
        AT_upper_bound_data2	= (0x02f0|FORM_DATA2),
        AT_upper_bound_data4	= (0x02f0|FORM_DATA4),
        AT_upper_bound_data8	= (0x02f0|FORM_DATA8),
    AT_virtual			= (0x0300|FORM_STRING),

    

    AT_sf_names			= (0x8000|FORM_DATA4),
    AT_src_info			= (0x8010|FORM_DATA4),
    AT_mac_info			= (0x8020|FORM_DATA4),
    AT_src_coords		= (0x8030|FORM_DATA4),
    AT_body_begin		= (0x8040|FORM_ADDR),
    AT_body_end			= (0x8050|FORM_ADDR)
};

#if 0
void ML_(read_debuginfo_dwarf1) ( 
        struct _DebugInfo* di, 
        UChar* dwarf1d, Int dwarf1d_sz, 
        UChar* dwarf1l, Int dwarf1l_sz )
{
   UInt   stmt_list;
   Bool   stmt_list_found;
   Int    die_offset, die_szb, at_offset;
   UShort die_kind, at_kind;
   UChar* at_base;
   HChar* src_filename;

   if (0) 
      VG_(printf)("read_debuginfo_dwarf1 ( %p, %d, %p, %d )\n",
	          dwarf1d, dwarf1d_sz, dwarf1l, dwarf1l_sz );

   
   die_offset = 0;
   while (True) {
      if (die_offset >= dwarf1d_sz) break;

      die_szb  = ML_(read_Int)(dwarf1d + die_offset);
      die_kind = ML_(read_UShort)(dwarf1d + die_offset + 4);

      
      if (die_kind != TAG_compile_unit) {
         die_offset += die_szb;
         continue; 
      }

      if (0) 
         VG_(printf)("compile-unit DIE: offset %d, tag 0x%x, size %d\n", 
                     die_offset, (Int)die_kind, die_szb );


      
      src_filename    = NULL;
      stmt_list_found = False;
      stmt_list       = 0;

      
      at_base = dwarf1d + die_offset + 6;
      at_offset = 0;
      while (True) {
         if (at_offset >= die_szb-6) break;

         at_kind = ML_(read_UShort)(at_base + at_offset);
         if (0) VG_(printf)("atoffset %d, attag 0x%x\n", 
                            at_offset, (Int)at_kind );
         at_offset += 2; 
         switch (at_kind) {
            case AT_stmt_list:
            case AT_language:
            case AT_sibling:
               if (at_kind == AT_stmt_list) {
                  stmt_list_found = True;
                  stmt_list = ML_(read_Int)(at_base+at_offset);
               }
               at_offset += 4; break;
            case AT_high_pc:
            case AT_low_pc: 
               at_offset += sizeof(void*); break;
            case AT_name: 
            case AT_producer:
            case AT_comp_dir:
               
               if (at_kind == AT_name)
                 src_filename = (HChar *)(at_base + at_offset);
               while (at_offset < die_szb-6 && at_base[at_offset] != 0)
                  at_offset++;
               at_offset++;
               break;
            default: 
               VG_(printf)("Unhandled DWARF-1 attribute 0x%x\n", 
                           (Int)at_kind );
               VG_(core_panic)("Unhandled DWARF-1 attribute");
         } 
      } 

      if (stmt_list_found 
          && src_filename != NULL 
         ) {
         Addr   base;
	 Int    len;
         HChar* curr_filenm;
         UChar* ptr;
         UInt   prev_line, prev_delta;

         curr_filenm = ML_(addStr) ( di, src_filename, -1 );
         prev_line = prev_delta = 0;

         ptr = dwarf1l + stmt_list;
         len  = ML_(read_Int)(ptr);  ptr += sizeof(Int);
         base = ML_(read_Addr)(ptr); ptr += sizeof(void*);
         len -= (sizeof(Int) + sizeof(void*));
         while (len > 0) {
            UInt   line;
            UShort col;
            UInt   delta;
            line = ML_(read_UInt)(ptr);    ptr += sizeof(UInt);
            col  = ML_(read_UShort)(ptr);  ptr += sizeof(UShort);
            delta = ML_(read_UInt)(ptr);   ptr += sizeof(UInt);
	    if (0) VG_(printf)("line %d, col %d, delta %d\n", 
                               line, (Int)col, delta );
            len -= (sizeof(UInt) + sizeof(UShort) + sizeof(UInt));

	    if (delta > 0 && prev_line > 0) {
	       if (0) VG_(printf) ("     %d  %d-%d\n",
                                   prev_line, prev_delta, delta-1);
	       ML_(addLineInfo) ( di, curr_filenm, NULL,
		 	          base + prev_delta, base + delta,
			          prev_line, 0 );
	    }
	    prev_line = line;
	    prev_delta = delta;
	 }        
      }  

      
      die_offset += die_szb;

   } 

}
#endif








#if defined(VGP_x86_linux)
#  define FP_REG         5
#  define SP_REG         4
#  define RA_REG_DEFAULT 8
#elif defined(VGP_amd64_linux)
#  define FP_REG         6
#  define SP_REG         7
#  define RA_REG_DEFAULT 16
#elif defined(VGP_ppc32_linux)
#  define FP_REG         1
#  define SP_REG         1
#  define RA_REG_DEFAULT 65
#elif defined(VGP_ppc64be_linux) || defined(VGP_ppc64le_linux)
#  define FP_REG         1
#  define SP_REG         1
#  define RA_REG_DEFAULT 65
#elif defined(VGP_arm_linux)
#  define FP_REG         12
#  define SP_REG         13
#  define RA_REG_DEFAULT 14
#elif defined(VGP_arm64_linux)
#  define FP_REG         29
#  define SP_REG         31
#  define RA_REG_DEFAULT 30
#elif defined(VGP_x86_darwin)
#  define FP_REG         5
#  define SP_REG         4
#  define RA_REG_DEFAULT 8
#elif defined(VGP_amd64_darwin)
#  define FP_REG         6
#  define SP_REG         7
#  define RA_REG_DEFAULT 16
#elif defined(VGP_s390x_linux)
#  define FP_REG         11    
#  define SP_REG         15    
#  define RA_REG_DEFAULT 14    
#elif defined(VGP_mips32_linux)
#  define FP_REG         30
#  define SP_REG         29
#  define RA_REG_DEFAULT 31
#elif defined(VGP_mips64_linux)
#  define FP_REG         30
#  define SP_REG         29
#  define RA_REG_DEFAULT 31
#elif defined(VGP_tilegx_linux)
#  define FP_REG         52
#  define SP_REG         54
#  define RA_REG_DEFAULT 55
#else
#  error "Unknown platform"
#endif

#if defined(VGP_ppc32_linux) || defined(VGP_ppc64be_linux) \
     || defined(VGP_ppc64le_linux) || defined(VGP_mips32_linux) \
     || defined(VGP_mips64_linux)
# define N_CFI_REGS 72
#elif defined(VGP_arm_linux) || defined(VGP_tilegx_linux)
# define N_CFI_REGS 320
#elif defined(VGP_arm64_linux)
# define N_CFI_REGS 128
#else
# define N_CFI_REGS 20
#endif

enum dwarf_cfa_primary_ops
  {
    DW_CFA_use_secondary = 0,
    DW_CFA_advance_loc   = 1,
    DW_CFA_offset        = 2,
    DW_CFA_restore       = 3
  };

enum dwarf_cfa_secondary_ops
  {
    DW_CFA_nop                = 0x00,
    DW_CFA_set_loc            = 0x01,
    DW_CFA_advance_loc1       = 0x02,
    DW_CFA_advance_loc2       = 0x03,
    DW_CFA_advance_loc4       = 0x04,
    DW_CFA_offset_extended    = 0x05,
    DW_CFA_restore_extended   = 0x06,
    DW_CFA_undefined          = 0x07,
    DW_CFA_same_value         = 0x08,
    DW_CFA_register           = 0x09,
    DW_CFA_remember_state     = 0x0a,
    DW_CFA_restore_state      = 0x0b,
    DW_CFA_def_cfa            = 0x0c,
    DW_CFA_def_cfa_register   = 0x0d,
    DW_CFA_def_cfa_offset     = 0x0e,
    DW_CFA_def_cfa_expression = 0x0f, 
    DW_CFA_expression         = 0x10, 
    DW_CFA_offset_extended_sf = 0x11, 
    DW_CFA_def_cfa_sf         = 0x12, 
    DW_CFA_def_cfa_offset_sf  = 0x13, 
    DW_CFA_val_offset         = 0x14, 
    DW_CFA_val_offset_sf      = 0x15, 
    DW_CFA_val_expression     = 0x16, 
    DW_CFA_lo_user            = 0x1c,
    DW_CFA_GNU_window_save    = 0x2d, 
    DW_CFA_GNU_args_size      = 0x2e, 
    DW_CFA_GNU_negative_offset_extended = 0x2f, 
    DW_CFA_hi_user            = 0x3f
  };

#define DW_EH_PE_absptr		0x00
#define DW_EH_PE_omit		0xff

#define DW_EH_PE_uleb128	0x01
#define DW_EH_PE_udata2		0x02
#define DW_EH_PE_udata4		0x03
#define DW_EH_PE_udata8		0x04
#define DW_EH_PE_sleb128	0x09
#define DW_EH_PE_sdata2		0x0A
#define DW_EH_PE_sdata4		0x0B
#define DW_EH_PE_sdata8		0x0C
#define DW_EH_PE_signed		0x08

#define DW_EH_PE_pcrel		0x10
#define DW_EH_PE_textrel	0x20
#define DW_EH_PE_datarel	0x30
#define DW_EH_PE_funcrel	0x40
#define DW_EH_PE_aligned	0x50

#define DW_EH_PE_indirect	0x80


typedef
   struct {
      enum { RR_Undef, RR_Same, RR_CFAOff, RR_CFAValOff, 
             RR_Reg,  RR_ValExpr, RR_Arch } tag;
      Int arg;
   }
   RegRule;

static void ppRegRule ( const XArray* exprs, const RegRule* rrule )
{
   vg_assert(exprs);
   switch (rrule->tag) {
      case RR_Undef:     VG_(printf)("u  "); break;
      case RR_Same:      VG_(printf)("s  "); break;
      case RR_CFAOff:    VG_(printf)("c%d ", rrule->arg); break;
      case RR_CFAValOff: VG_(printf)("v%d ", rrule->arg); break;
      case RR_Reg:       VG_(printf)("r%d ", rrule->arg); break;
      case RR_ValExpr:   VG_(printf)("ve{"); 
                         ML_(ppCfiExpr)( exprs, rrule->arg ); 
                         VG_(printf)("} "); 
                         break;
      case RR_Arch:      VG_(printf)("a  "); break;
      default:           VG_(core_panic)("ppRegRule");
   }
}


#define N_RR_STACK 4

typedef
   struct {
      
      Int     code_a_f;
      Int     data_a_f;
      Addr    initloc;
      Int     ra_reg;
      
      Addr    loc;
      struct UnwindContextState {
          
          Bool    cfa_is_regoff; 
          Int     cfa_reg;
          Int     cfa_off;  
          Int     cfa_expr_ix; 
          
          RegRule reg[N_CFI_REGS];
      }
      state[N_RR_STACK];
      Int     state_sp; 
      
      XArray* exprs;
   }
   UnwindContext;

static void ppUnwindContext ( const UnwindContext* ctx )
{
   Int j, i;
   VG_(printf)("0x%llx: ", (ULong)ctx->loc);
   for (j = 0; j <= ctx->state_sp; j++) {
      const struct UnwindContextState* ctxs = &ctx->state[j];
      VG_(printf)("%s[%d]={ ", j > 0 ? " " : "", j);
      if (ctxs->cfa_is_regoff) {
         VG_(printf)("%d(r%d) ", ctxs->cfa_off, ctxs->cfa_reg);
      } else {
         vg_assert(ctx->exprs);
         VG_(printf)("{");
         ML_(ppCfiExpr)( ctx->exprs, ctxs->cfa_expr_ix );
         VG_(printf)("} ");
      }
      VG_(printf)("{ ");
      for (i = 0; i < N_CFI_REGS; i++)
         ppRegRule(ctx->exprs, &ctxs->reg[i]);
      VG_(printf)("}");
   }
   VG_(printf)("\n");
}

static void initUnwindContext ( UnwindContext* ctx )
{
   Int j, i;
   VG_(memset)(ctx, 0, sizeof(*ctx));
   ctx->ra_reg        = RA_REG_DEFAULT;
   for (j = 0; j < N_RR_STACK; j++) {
      ctx->state[j].cfa_is_regoff = True;
      for (i = 0; i < N_CFI_REGS; i++) {
         if (RR_Undef != 0)
           ctx->state[j].reg[i].tag = RR_Undef;
         
      }
#     if defined(VGA_arm)
      ctx->state[j].reg[11].tag = RR_Same;
      
      ctx->state[j].reg[14].tag = RR_Same;
      ctx->state[j].reg[12].tag = RR_Same;
      ctx->state[j].reg[7].tag  = RR_Same;
      
#     elif defined(VGA_arm64)
      ctx->state[j].reg[29].tag = RR_Same;
      ctx->state[j].reg[30].tag = RR_Same;
#     endif
   }
}


typedef
   struct {
      UChar    encoding;
      DiCursor ehframe_image;
      Addr     ehframe_avma;
      Addr     text_bias;
   }
   AddressDecodingInfo;




static 
Int copy_convert_CfiExpr_tree ( XArray* dst, const UnwindContext* srcuc, 
                                Int nd );

static Bool summarise_context(Addr* base,
                              UInt* len,
                              DiCfSI_m* si_m,
                               Addr loc_start,
	                       const UnwindContext* ctx,
                               DebugInfo* debuginfo )
{
   Int why = 0;
   const struct UnwindContextState* ctxs;

   *base = 0;
   *len = 0;
   VG_(bzero_inline)(si_m, sizeof(*si_m));


   if (ctx->state_sp < 0)           { why = 8; goto failed; }
   if (ctx->state_sp >= N_RR_STACK) { why = 9; goto failed; }
   ctxs = &ctx->state[ctx->state_sp];

   
   if (!ctxs->cfa_is_regoff) {
      
      XArray *src, *dst;
      Int    conv;
      src = ctx->exprs;
      dst = debuginfo->cfsi_exprs;
      if (src && (VG_(sizeXA)(src) > 0) && (!dst)) {
         dst = VG_(newXA)( ML_(dinfo_zalloc), "di.ccCt.1", ML_(dinfo_free),
                           sizeof(CfiExpr) );
         debuginfo->cfsi_exprs = dst;
      }
      conv = copy_convert_CfiExpr_tree
                    ( dst, ctx, ctxs->cfa_expr_ix );
      vg_assert(conv >= -1);
      if (conv == -1) { why = 6; goto failed; }
      si_m->cfa_how = CFIC_EXPR;
      si_m->cfa_off = conv;
      if (0 && debuginfo->ddump_frames)
         ML_(ppCfiExpr)(dst, conv);
   }
   else
   if (ctxs->cfa_is_regoff && ctxs->cfa_reg == SP_REG) {
      si_m->cfa_off = ctxs->cfa_off;
#     if defined(VGA_x86) || defined(VGA_amd64) || defined(VGA_s390x) \
         || defined(VGA_mips32) || defined(VGA_mips64) \
         || defined(VGA_tilegx)
      si_m->cfa_how = CFIC_IA_SPREL;
#     elif defined(VGA_arm)
      si_m->cfa_how = CFIC_ARM_R13REL;
#     elif defined(VGA_arm64)
      si_m->cfa_how = CFIC_ARM64_SPREL;
#     else
      si_m->cfa_how = 0; 
#     endif
   }
   else
   if (ctxs->cfa_is_regoff && ctxs->cfa_reg == FP_REG) {
      si_m->cfa_off = ctxs->cfa_off;
#     if defined(VGA_x86) || defined(VGA_amd64) || defined(VGA_s390x) \
         || defined(VGA_mips32) || defined(VGA_mips64) \
         || defined(VGA_tilegx)
      si_m->cfa_how = CFIC_IA_BPREL;
#     elif defined(VGA_arm)
      si_m->cfa_how = CFIC_ARM_R12REL;
#     elif defined(VGA_arm64)
      si_m->cfa_how = CFIC_ARM64_X29REL;
#     else
      si_m->cfa_how = 0; 
#     endif
   }
#  if defined(VGA_arm)
   else
   if (ctxs->cfa_is_regoff && ctxs->cfa_reg == 11) {
      si_m->cfa_how = CFIC_ARM_R11REL;
      si_m->cfa_off = ctxs->cfa_off;
   }
   else
   if (ctxs->cfa_is_regoff && ctxs->cfa_reg == 7) {
      si_m->cfa_how = CFIC_ARM_R7REL;
      si_m->cfa_off = ctxs->cfa_off;
   }
#  elif defined(VGA_arm64)
   
#  endif
   else {
      why = 1;
      goto failed;
   }

#  define SUMMARISE_HOW(_how, _off, _ctxreg)                  \
   switch (_ctxreg.tag) {                                     \
      case RR_Undef:                                          \
         _how = CFIR_UNKNOWN;   _off = 0; break;              \
      case RR_Same:                                           \
         _how = CFIR_SAME;      _off = 0; break;              \
      case RR_CFAOff:                                         \
         _how = CFIR_MEMCFAREL; _off = _ctxreg.arg; break;    \
      case RR_CFAValOff:                                      \
         _how = CFIR_CFAREL;    _off = _ctxreg.arg; break;    \
      case RR_ValExpr: {                                      \
         XArray *src, *dst;                                   \
         Int    conv;                                         \
         src = ctx->exprs;                                    \
         dst = debuginfo->cfsi_exprs;                         \
         if (src && (VG_(sizeXA)(src) > 0) && (!dst)) {       \
            dst = VG_(newXA)( ML_(dinfo_zalloc),              \
                              "di.ccCt.2",                    \
                              ML_(dinfo_free),                \
                              sizeof(CfiExpr) );              \
            debuginfo->cfsi_exprs = dst;                      \
         }                                                    \
         conv = copy_convert_CfiExpr_tree                     \
                       ( dst, ctx, _ctxreg.arg );             \
         vg_assert(conv >= -1);                               \
         if (conv == -1) { why = 7; goto failed; }            \
         _how = CFIR_EXPR;                                    \
         _off = conv;                                         \
         if (0 && debuginfo->ddump_frames)                    \
            ML_(ppCfiExpr)(dst, conv);                        \
         break;                                               \
      }                                                       \
      default:                                                \
         why = 2; goto failed;         \
   }


#  if defined(VGA_x86) || defined(VGA_amd64)

   

   SUMMARISE_HOW(si_m->ra_how, si_m->ra_off,
                               ctxs->reg[ctx->ra_reg] );
   SUMMARISE_HOW(si_m->bp_how, si_m->bp_off,
                               ctxs->reg[FP_REG] );

   si_m->sp_how = CFIR_CFAREL;
   si_m->sp_off = 0;

   if (ctxs->reg[FP_REG].tag == RR_Undef)
      si_m->bp_how = CFIR_SAME;

   
   if (si_m->ra_how == CFIR_SAME) 
      { why = 3; goto failed; }

   if (loc_start >= ctx->loc) 
      { why = 4; goto failed; }
   if (ctx->loc - loc_start > 10000000 )
      { why = 5; goto failed; }

   *base = loc_start + ctx->initloc;
   *len  = (UInt)(ctx->loc - loc_start);

   return True;

#  elif defined(VGA_arm)

   

   SUMMARISE_HOW(si_m->r14_how, si_m->r14_off,
                                ctxs->reg[14] );

   
   

   SUMMARISE_HOW(si_m->r12_how, si_m->r12_off,
                                ctxs->reg[FP_REG] );

   SUMMARISE_HOW(si_m->r11_how, si_m->r11_off,
                                ctxs->reg[11] );

   SUMMARISE_HOW(si_m->r7_how, si_m->r7_off,
                               ctxs->reg[7] );

   if (ctxs->reg[14].tag == RR_Same
       && ctx->ra_reg == 14) {
      if (!debuginfo->cfsi_exprs)
         debuginfo->cfsi_exprs = VG_(newXA)( ML_(dinfo_zalloc),
                                             "di.ccCt.2a",
                                             ML_(dinfo_free),
                                             sizeof(CfiExpr) );
      si_m->ra_off = ML_(CfiExpr_CfiReg)( debuginfo->cfsi_exprs,
                                         Creg_ARM_R14);
      si_m->ra_how = CFIR_EXPR;
   } else {
      
      SUMMARISE_HOW(si_m->ra_how, si_m->ra_off,
                                  ctxs->reg[ctx->ra_reg] );
   }

   si_m->r13_how = CFIR_CFAREL;
   si_m->r13_off = 0;

   if (loc_start >= ctx->loc) 
      { why = 4; goto failed; }
   if (ctx->loc - loc_start > 10000000 )
      { why = 5; goto failed; }

   *base = loc_start + ctx->initloc;
   *len  = (UInt)(ctx->loc - loc_start);

   return True;

#  elif defined(VGA_arm64)

   

   SUMMARISE_HOW(si_m->x30_how, si_m->x30_off, ctxs->reg[30]);
   SUMMARISE_HOW(si_m->x29_how, si_m->x29_off, ctxs->reg[29]);

   if (ctxs->reg[30].tag == RR_Same
       && ctx->ra_reg == 30) {
      if (!debuginfo->cfsi_exprs)
         debuginfo->cfsi_exprs = VG_(newXA)( ML_(dinfo_zalloc),
                                             "di.ccCt.2a-arm64",
                                             ML_(dinfo_free),
                                             sizeof(CfiExpr) );
      si_m->ra_off = ML_(CfiExpr_CfiReg)( debuginfo->cfsi_exprs,
                                          Creg_ARM64_X30);
      si_m->ra_how = CFIR_EXPR;
   } else {
      
      SUMMARISE_HOW(si_m->ra_how, si_m->ra_off, ctxs->reg[ctx->ra_reg]);
   }

   si_m->sp_how = CFIR_CFAREL;
   si_m->sp_off = 0;

   if (loc_start >= ctx->loc) 
      { why = 4; goto failed; }
   if (ctx->loc - loc_start > 10000000 )
      { why = 5; goto failed; }

   *base = loc_start + ctx->initloc;
   *len  = (UInt)(ctx->loc - loc_start);

   return True;

#  elif defined(VGA_s390x)

   

   SUMMARISE_HOW(si_m->ra_how, si_m->ra_off,
                               ctxs->reg[ctx->ra_reg] );
   SUMMARISE_HOW(si_m->fp_how, si_m->fp_off,
                               ctxs->reg[FP_REG] );
   SUMMARISE_HOW(si_m->sp_how, si_m->sp_off,
                               ctxs->reg[SP_REG] );

   
   if (si_m->sp_how == CFIR_UNKNOWN)
      si_m->sp_how = CFIR_SAME;

   if (si_m->fp_how == CFIR_UNKNOWN)
      si_m->fp_how = CFIR_SAME;

   if (si_m->cfa_how == CFIR_UNKNOWN) {
      si_m->cfa_how = CFIC_IA_SPREL;
      si_m->cfa_off = 160;
   }
   if (si_m->ra_how == CFIR_UNKNOWN) {
      if (!debuginfo->cfsi_exprs)
         debuginfo->cfsi_exprs = VG_(newXA)( ML_(dinfo_zalloc),
                                             "di.ccCt.2a",
                                             ML_(dinfo_free),
                                             sizeof(CfiExpr) );
      si_m->ra_how = CFIR_EXPR;
      si_m->ra_off = ML_(CfiExpr_CfiReg)( debuginfo->cfsi_exprs,
                                          Creg_S390_LR);
   }

   
   if (si_m->ra_how == CFIR_SAME)
      { why = 3; goto failed; }

   if (loc_start >= ctx->loc)
      { why = 4; goto failed; }
   if (ctx->loc - loc_start > 10000000 )
      { why = 5; goto failed; }

   *base = loc_start + ctx->initloc;
   *len  = (UInt)(ctx->loc - loc_start);

   return True;

#  elif defined(VGA_mips32) || defined(VGA_mips64)

   

   SUMMARISE_HOW(si_m->ra_how, si_m->ra_off,
                               ctxs->reg[ctx->ra_reg] );
   SUMMARISE_HOW(si_m->fp_how, si_m->fp_off,
                               ctxs->reg[FP_REG] );
   SUMMARISE_HOW(si_m->sp_how, si_m->sp_off,
                               ctxs->reg[SP_REG] );
   si_m->sp_how = CFIR_CFAREL;
   si_m->sp_off = 0;

   if (si_m->fp_how == CFIR_UNKNOWN)
       si_m->fp_how = CFIR_SAME;
   if (si_m->cfa_how == CFIR_UNKNOWN) {
      si_m->cfa_how = CFIC_IA_SPREL;
      si_m->cfa_off = 160;
   }
   if (si_m->ra_how == CFIR_UNKNOWN) {
      if (!debuginfo->cfsi_exprs)
         debuginfo->cfsi_exprs = VG_(newXA)( ML_(dinfo_zalloc),
                                             "di.ccCt.2a",
                                             ML_(dinfo_free),
                                             sizeof(CfiExpr) );
      si_m->ra_how = CFIR_EXPR;
      si_m->ra_off = ML_(CfiExpr_CfiReg)( debuginfo->cfsi_exprs,
                                          Creg_MIPS_RA);
   }

   if (si_m->ra_how == CFIR_SAME)
      { why = 3; goto failed; }

   if (loc_start >= ctx->loc) 
      { why = 4; goto failed; }
   if (ctx->loc - loc_start > 10000000 )
      { why = 5; goto failed; }

   *base = loc_start + ctx->initloc;
   *len  = (UInt)(ctx->loc - loc_start);

   return True;
#  elif defined(VGA_tilegx)

   

   SUMMARISE_HOW(si_m->ra_how, si_m->ra_off,
                               ctxs->reg[ctx->ra_reg] );
   SUMMARISE_HOW(si_m->fp_how, si_m->fp_off,
                               ctxs->reg[FP_REG] );
   SUMMARISE_HOW(si_m->sp_how, si_m->sp_off,
                               ctxs->reg[SP_REG] );
   si_m->sp_how = CFIR_CFAREL;
   si_m->sp_off = 0;

   if (si_m->fp_how == CFIR_UNKNOWN)
       si_m->fp_how = CFIR_SAME;
   if (si_m->cfa_how == CFIR_UNKNOWN) {
      si_m->cfa_how = CFIC_IA_SPREL;
      si_m->cfa_off = 160;
   }
   if (si_m->ra_how == CFIR_UNKNOWN) {
      if (!debuginfo->cfsi_exprs)
         debuginfo->cfsi_exprs = VG_(newXA)( ML_(dinfo_zalloc),
                                             "di.ccCt.2a",
                                             ML_(dinfo_free),
                                             sizeof(CfiExpr) );
      si_m->ra_how = CFIR_EXPR;
      si_m->ra_off = ML_(CfiExpr_CfiReg)( debuginfo->cfsi_exprs,
                                          Creg_TILEGX_LR);
   }

   if (si_m->ra_how == CFIR_SAME)
      { why = 3; goto failed; }

   if (loc_start >= ctx->loc) 
      { why = 4; goto failed; }
   if (ctx->loc - loc_start > 10000000 )
      { why = 5; goto failed; }

   *base = loc_start + ctx->initloc;
   *len  = (UInt)(ctx->loc - loc_start);

   return True;
#  elif defined(VGA_ppc32) || defined(VGA_ppc64be) || defined(VGA_ppc64le)
   

#  else
#    error "Unknown arch"
#  endif

   

#  undef SUMMARISE_HOW

  failed:
   if (VG_(clo_verbosity) > 2 || debuginfo->trace_cfi) {
      VG_(message)(Vg_DebugMsg,
                  "summarise_context(loc_start = %#lx)"
                  ": cannot summarise(why=%d):   \n", loc_start, why);
      ppUnwindContext(ctx);
   }
   return False;
}

static Int copy_convert_CfiExpr_tree ( XArray*        dstxa,
                                       const UnwindContext* srcuc, 
                                       Int            srcix )
{
   CfiExpr* src;
   Int      cpL, cpR, cpA;
   XArray*  srcxa = srcuc->exprs;
   vg_assert(srcxa);
   vg_assert(dstxa);
   vg_assert(srcix >= 0 && srcix < VG_(sizeXA)(srcxa));

   src = VG_(indexXA)( srcxa, srcix );
   switch (src->tag) {
      case Cex_Undef:
         return ML_(CfiExpr_Undef)( dstxa );
      case Cex_Deref:
         cpA = copy_convert_CfiExpr_tree( dstxa, srcuc, src->Cex.Deref.ixAddr );
         if (cpA == -1)
            return -1; 
         return ML_(CfiExpr_Deref)( dstxa, cpA );
      case Cex_Const:
         return ML_(CfiExpr_Const)( dstxa, src->Cex.Const.con );
      case Cex_Binop:
         cpL = copy_convert_CfiExpr_tree( dstxa, srcuc, src->Cex.Binop.ixL );
         cpR = copy_convert_CfiExpr_tree( dstxa, srcuc, src->Cex.Binop.ixR );
         vg_assert(cpL >= -1 && cpR >= -1);
         if (cpL == -1 || cpR == -1)
            return -1; 
         return ML_(CfiExpr_Binop)( dstxa, src->Cex.Binop.op, cpL, cpR );
      case Cex_CfiReg:
         VG_(core_panic)("copy_convert_CfiExpr_tree: CfiReg in input");
      case Cex_DwReg: {
         
         Int dwreg __attribute__((unused));
         dwreg = src->Cex.DwReg.reg;
#        if defined(VGA_x86) || defined(VGA_amd64)
         if (dwreg == SP_REG)
            return ML_(CfiExpr_CfiReg)( dstxa, Creg_IA_SP );
         if (dwreg == FP_REG)
            return ML_(CfiExpr_CfiReg)( dstxa, Creg_IA_BP );
         if (dwreg == srcuc->ra_reg)
            return ML_(CfiExpr_CfiReg)( dstxa, Creg_IA_IP ); 
#        elif defined(VGA_arm)
         if (dwreg == SP_REG)
            return ML_(CfiExpr_CfiReg)( dstxa, Creg_ARM_R13 );
         if (dwreg == FP_REG)
            return ML_(CfiExpr_CfiReg)( dstxa, Creg_ARM_R12 );
         if (dwreg == srcuc->ra_reg)
           return ML_(CfiExpr_CfiReg)( dstxa, Creg_ARM_R15 ); 
#        elif defined(VGA_s390x)
         if (dwreg == SP_REG)
            return ML_(CfiExpr_CfiReg)( dstxa, Creg_S390_SP );
         if (dwreg == FP_REG)
            return ML_(CfiExpr_CfiReg)( dstxa, Creg_S390_FP );
         if (dwreg == srcuc->ra_reg)
            return ML_(CfiExpr_CfiReg)( dstxa, Creg_S390_IA );
#        elif defined(VGA_mips32) || defined(VGA_mips64)
         if (dwreg == SP_REG)
            return ML_(CfiExpr_CfiReg)( dstxa, Creg_IA_SP );
         if (dwreg == FP_REG)
            return ML_(CfiExpr_CfiReg)( dstxa, Creg_IA_BP );
         if (dwreg == srcuc->ra_reg)
            return ML_(CfiExpr_CfiReg)( dstxa, Creg_IA_IP );
#        elif defined(VGA_arm64)
         I_die_here;
#        elif defined(VGA_ppc32) || defined(VGA_ppc64be) \
            || defined(VGA_ppc64le)
#        elif defined(VGA_tilegx)
         if (dwreg == SP_REG)
            return ML_(CfiExpr_CfiReg)( dstxa, Creg_TILEGX_SP );
         if (dwreg == FP_REG)
            return ML_(CfiExpr_CfiReg)( dstxa, Creg_TILEGX_BP );
         if (dwreg == srcuc->ra_reg)
            return ML_(CfiExpr_CfiReg)( dstxa, Creg_TILEGX_IP );
#        else
#           error "Unknown arch"
#        endif
         
         return -1;
      }
      default:
         VG_(core_panic)("copy_convert_CfiExpr_tree: default");
   }
}


static void ppUnwindContext_summary ( const UnwindContext* ctx )
{
   const struct UnwindContextState* ctxs = &ctx->state[ctx->state_sp];

   VG_(printf)("0x%llx-1: ", (ULong)ctx->loc);

   if (ctxs->cfa_reg == SP_REG) {
      VG_(printf)("SP/CFA=%d+SP   ", ctxs->cfa_off);
   } else
   if (ctxs->cfa_reg == FP_REG) {
      VG_(printf)("SP/CFA=%d+FP   ", ctxs->cfa_off);
   } else {
      VG_(printf)("SP/CFA=unknown  ");
   }

   VG_(printf)("RA=");
   ppRegRule( ctx->exprs, &ctxs->reg[ctx->ra_reg] );

   VG_(printf)("FP=");
   ppRegRule( ctx->exprs, &ctxs->reg[FP_REG] );
   VG_(printf)("\n");
}



static ULong step_le_u_encoded_literal ( DiCursor* data, UInt size )
{
   switch (size) {
      case 8:  return (ULong)ML_(cur_step_ULong)( data );
      case 4:  return (ULong)ML_(cur_step_UInt)( data );
      case 2:  return (ULong)ML_(cur_step_UShort)( data );
      case 1:  return (ULong)ML_(cur_step_UChar)( data );
      default: vg_assert(0);  return 0;
   }
}

static Long step_le_s_encoded_literal ( DiCursor* data, UInt size )
{
   ULong u64 = step_le_u_encoded_literal( data, size );
   Long s64;
   switch (size) {
      case 8:  s64 = u64; break;
      case 4:  s64 = u64 << 32; s64 >>= 32; break;
      case 2:  s64 = u64 << 48; s64 >>= 48; break;
      case 1:  s64 = u64 << 56; s64 >>= 56; break;
      default: vg_assert(0);  return 0;
   }
   return s64;
}

static UChar default_Addr_encoding ( void )
{
   switch (sizeof(Addr)) {
      case 4: return DW_EH_PE_udata4;
      case 8: return DW_EH_PE_udata8;
      default: vg_assert(0);
   }
}

static UInt size_of_encoded_Addr ( UChar encoding )
{
   if (encoding == DW_EH_PE_omit)
      return 0;

   switch (encoding & 0x07) {
      case DW_EH_PE_absptr: return sizeof(Addr);
      case DW_EH_PE_udata2: return sizeof(UShort);
      case DW_EH_PE_udata4: return sizeof(UInt);
      case DW_EH_PE_udata8: return sizeof(ULong);
      default: vg_assert(0);
   }
}

static Addr step_encoded_Addr ( const AddressDecodingInfo* adi,
                                DiCursor* data )
{
   Addr     base;
   Word     offset;
   UChar    encoding      = adi->encoding;
   DiCursor ehframe_image = adi->ehframe_image;
   Addr     ehframe_avma  = adi->ehframe_avma;

   vg_assert((encoding & DW_EH_PE_indirect) == 0);

   switch (encoding & 0x70) {
      case DW_EH_PE_absptr:
         base = adi->text_bias;
         break;
      case DW_EH_PE_pcrel:
         base = ehframe_avma + ML_(cur_minus)(*data, ehframe_image);
         break;
      case DW_EH_PE_datarel:
         vg_assert(0);
         base =  0;
         break;
      case DW_EH_PE_textrel:
         vg_assert(0);
         base =  0;
         break;
      case DW_EH_PE_funcrel:
         base = 0;
         break;
      case DW_EH_PE_aligned:
         base = 0;
         offset = ML_(cur_minus)(*data, ehframe_image);
         if ((offset % sizeof(Addr)) != 0) {
            Word nbytes = sizeof(Addr) - (offset % sizeof(Addr));
            *data = ML_(cur_plus)(*data, nbytes);
         }
         break;
      default:
         vg_assert(0);
   }

   if ((encoding & 0x07) == 0x00)
      encoding |= default_Addr_encoding();

   switch (encoding & 0x0f) {
      case DW_EH_PE_udata2:
         return base + ML_(cur_step_UShort)(data);
      case DW_EH_PE_udata4:
         return base + ML_(cur_step_UInt)(data);
      case DW_EH_PE_udata8:
         return base + ML_(cur_step_ULong)(data);
      case DW_EH_PE_sdata2:
         return base + ML_(cur_step_Short)(data);
      case DW_EH_PE_sdata4:
         return base + ML_(cur_step_Int)(data);
      case DW_EH_PE_sdata8:
         return base + ML_(cur_step_Long)(data);
      default:
         vg_assert2(0, "read encoded address %d\n", encoding & 0x0f);
   }
}



static Int dwarfexpr_to_dag ( const UnwindContext* ctx, 
                              DiCursor expr, Int exprlen, 
                              Bool push_cfa_at_start,
                              Bool ddump_frames )
{
#  define N_EXPR_STACK 20

#  define PUSH(_arg)                               \
      do {                                         \
         vg_assert(sp >= -1 && sp < N_EXPR_STACK); \
         if (sp == N_EXPR_STACK-1)                 \
            return -1;                             \
         sp++;                                     \
         stack[sp] = (_arg);                       \
      } while (0)

#  define POP(_lval)                               \
      do {                                         \
         vg_assert(sp >= -1 && sp < N_EXPR_STACK); \
         if (sp == -1)                             \
            return -1;                             \
         _lval = stack[sp];                        \
         sp--;                                     \
      } while (0)

   Int      ix, ix2, reg;
   UChar    opcode;
   Word     sw;
   UWord    uw;
   CfiUnop  uop;
   CfiBinop bop;
   const HChar* opname;

   Int sp; 
   Int stack[N_EXPR_STACK];  
   const struct UnwindContextState* ctxs = &ctx->state[ctx->state_sp];

   XArray*  dst   = ctx->exprs;
   DiCursor limit = ML_(cur_plus)(expr, exprlen);

   vg_assert(dst);
   vg_assert(exprlen >= 0);

   sp = -1; 

   
   if (push_cfa_at_start) {
      if (ctxs->cfa_is_regoff) {
         
         ix = ML_(CfiExpr_Binop)( dst,
                 Cbinop_Add,
                 ML_(CfiExpr_DwReg)( dst, ctxs->cfa_reg ),
                 ML_(CfiExpr_Const)( dst, (UWord)(Word)ctxs->cfa_off )
              );
         PUSH(ix);
      } else {
         
         PUSH(ctxs->cfa_expr_ix);
      }
   }

   while (True) {

      vg_assert(sp >= -1 && sp < N_EXPR_STACK);

      if (ML_(cur_cmpGT)(expr, limit)) 
         return -1;  

      if (ML_(cur_cmpEQ)(expr, limit)) { 
        
        if (sp == -1)
           return -1; 
        else
           break;
      }

      uop = 0; bop = 0; opname = NULL; 

      opcode = ML_(cur_step_UChar)(&expr);
      switch (opcode) {

         case DW_OP_lit0 ... DW_OP_lit31:
            
            sw = (Word)opcode - (Word)DW_OP_lit0;
            vg_assert(sw >= 0 && sw <= 31);
            PUSH( ML_(CfiExpr_Const)( dst, (UWord)sw ) );
            if (ddump_frames)
               VG_(printf)("DW_OP_lit%ld", sw);
            break;

         case DW_OP_breg0 ... DW_OP_breg31:
            
            reg = (Int)opcode - (Int)DW_OP_breg0;
            vg_assert(reg >= 0 && reg <= 31);
            sw = step_leb128S( &expr );
            ix = ML_(CfiExpr_Binop)( dst,
                    Cbinop_Add,
                    ML_(CfiExpr_DwReg)( dst, reg ),
                    ML_(CfiExpr_Const)( dst, (UWord)sw )
                 );
            PUSH(ix);
            if (ddump_frames)
               VG_(printf)("DW_OP_breg%d: %ld", reg, sw);
            break;

         case DW_OP_reg0 ... DW_OP_reg31:
            
            reg = (Int)opcode - (Int)DW_OP_reg0;
            vg_assert(reg >= 0 && reg <= 31);
            ix = ML_(CfiExpr_DwReg)( dst, reg );
            PUSH(ix);
            if (ddump_frames)
               VG_(printf)("DW_OP_reg%d", reg);
            break;

         case DW_OP_plus_uconst:
            uw = step_leb128U( &expr );
            PUSH( ML_(CfiExpr_Const)( dst, uw ) );
            POP( ix );
            POP( ix2 );
            PUSH( ML_(CfiExpr_Binop)( dst, Cbinop_Add, ix2, ix ) );
            if (ddump_frames)
               VG_(printf)("DW_OP_plus_uconst: %lu", uw);
            break;

         case DW_OP_const4s:
            
            sw = step_le_s_encoded_literal( &expr, 4 );
            PUSH( ML_(CfiExpr_Const)( dst, (UWord)sw ) );
            if (ddump_frames)
               VG_(printf)("DW_OP_const4s: %ld", sw);
            break;

         case DW_OP_const2s:
            
            sw = step_le_s_encoded_literal( &expr, 2 );
            PUSH( ML_(CfiExpr_Const)( dst, (UWord)sw ) );
            if (ddump_frames)
               VG_(printf)("DW_OP_const2s: %ld", sw);
            break;

         case DW_OP_const1s:
            
            sw = step_le_s_encoded_literal( &expr, 1 );
            PUSH( ML_(CfiExpr_Const)( dst, (UWord)sw ) );
            if (ddump_frames)
               VG_(printf)("DW_OP_const1s: %ld", sw);
            break;

         case DW_OP_const1u:
            
            uw = step_le_u_encoded_literal( &expr, 1 );
            PUSH( ML_(CfiExpr_Const)( dst, uw ) );
            if (ddump_frames)
               VG_(printf)("DW_OP_const1: %lu", uw);
            break;

         case DW_OP_const2u:
            
            uw = step_le_u_encoded_literal( &expr, 2 );
            PUSH( ML_(CfiExpr_Const)( dst, uw ) );
            if (ddump_frames)
               VG_(printf)("DW_OP_const2: %lu", uw);
            break;

         case DW_OP_const4u:
            
            uw = step_le_u_encoded_literal( &expr, 4 );
            PUSH( ML_(CfiExpr_Const)( dst, uw ) );
            if (ddump_frames)
               VG_(printf)("DW_OP_const4: %lu", uw);
            break;

         case DW_OP_abs:
            uop = Cunop_Abs; opname = "abs"; goto unop;
         case DW_OP_neg:
            uop = Cunop_Neg; opname = "neg"; goto unop;
         case DW_OP_not:
            uop = Cunop_Not; opname = "not"; goto unop;
         unop:
            POP( ix );
            PUSH( ML_(CfiExpr_Unop)( dst, uop, ix ) );
            if (ddump_frames)
               VG_(printf)("DW_OP_%s", opname);
            break;

         case DW_OP_minus:
            bop = Cbinop_Sub; opname = "minus"; goto binop;
         case DW_OP_plus:
            bop = Cbinop_Add; opname = "plus"; goto binop;
         case DW_OP_and:
            bop = Cbinop_And; opname = "and"; goto binop;
         case DW_OP_mul:
            bop = Cbinop_Mul; opname = "mul"; goto binop;
         case DW_OP_shl:
            bop = Cbinop_Shl; opname = "shl"; goto binop;
         case DW_OP_shr:
            bop = Cbinop_Shr; opname = "shr"; goto binop;
         case DW_OP_eq:
            bop = Cbinop_Eq; opname = "eq"; goto binop;
         case DW_OP_ge:
            bop = Cbinop_Ge; opname = "ge"; goto binop;
         case DW_OP_gt:
            bop = Cbinop_Gt; opname = "gt"; goto binop;
         case DW_OP_le:
            bop = Cbinop_Le; opname = "le"; goto binop;
         case DW_OP_lt:
            bop = Cbinop_Lt; opname = "lt"; goto binop;
         case DW_OP_ne:
            bop = Cbinop_Ne; opname = "ne"; goto binop;
         binop:
            POP( ix );
            POP( ix2 );
            PUSH( ML_(CfiExpr_Binop)( dst, bop, ix2, ix ) );
            if (ddump_frames)
               VG_(printf)("DW_OP_%s", opname);
            break;

         case DW_OP_deref:
            POP( ix );
            PUSH( ML_(CfiExpr_Deref)( dst, ix ) );
            if (ddump_frames)
               VG_(printf)("DW_OP_deref");
            break;

         default:
            if (!VG_(clo_xml))
               VG_(message)(Vg_DebugMsg, 
                            "Warning: DWARF2 CFI reader: unhandled DW_OP_ "
                            "opcode 0x%x\n", (Int)opcode); 
            return -1;
      }

      if (ML_(cur_cmpLT)(expr, limit) && ddump_frames)
         VG_(printf)("; ");

   }

   vg_assert(sp >= -1 && sp < N_EXPR_STACK);
   if (sp == -1)
      return -1;

   if (0 && ddump_frames)
      ML_(ppCfiExpr)( dst, stack[sp] );
   return stack[sp];

#  undef POP
#  undef PUSH
#  undef N_EXPR_STACK
}



static Int run_CF_instruction ( UnwindContext* ctx, 
                                DiCursor instrIN,
                                const UnwindContext* restore_ctx,
                                const AddressDecodingInfo* adi,
                                const DebugInfo* di )
{
   Int      off, reg, reg2, len, j;
   UInt     delta;
   Addr     printing_bias = ((Addr)ctx->initloc) - ((Addr)di->text_bias);
   struct UnwindContextState* ctxs;

   DiCursor instr   = instrIN;
   UChar    instr_0 = ML_(cur_step_UChar)(&instr);
   UChar    hi2     = (instr_0 >> 6) & 3;
   UChar    lo6     = instr_0 & 0x3F;

   if (ctx->state_sp < 0 || ctx->state_sp >= N_RR_STACK)
      return 0; 

   ctxs = &ctx->state[ctx->state_sp];
   if (hi2 == DW_CFA_advance_loc) {
      delta = (UInt)lo6;
      delta *= ctx->code_a_f;
      ctx->loc += delta;
      if (di->ddump_frames)
         VG_(printf)("  DW_CFA_advance_loc: %d to %08lx\n", 
                     (Int)delta, (Addr)ctx->loc + printing_bias);
      return ML_(cur_minus)(instr, instrIN);
   }

   if (hi2 == DW_CFA_offset) {
      
      off = step_leb128( &instr, 0 );
      reg = (Int)lo6;
      if (reg < 0 || reg >= N_CFI_REGS) 
         return 0; 
      ctxs->reg[reg].tag = RR_CFAOff;
      ctxs->reg[reg].arg = off * ctx->data_a_f;
      if (di->ddump_frames)
         VG_(printf)("  DW_CFA_offset: r%d at cfa%s%d\n",
                     (Int)reg,
                     ctxs->reg[reg].arg < 0 ? "" : "+", 
                     (Int)ctxs->reg[reg].arg );
      return ML_(cur_minus)(instr, instrIN);
   }

   if (hi2 == DW_CFA_restore) {
      reg = (Int)lo6;
      if (reg < 0 || reg >= N_CFI_REGS) 
         return 0; 
      if (restore_ctx == NULL)
         return 0; 
      ctxs->reg[reg] = restore_ctx->state[restore_ctx->state_sp].reg[reg];
      if (di->ddump_frames)
         VG_(printf)("  DW_CFA_restore: r%d\n", (Int)reg);
      return ML_(cur_minus)(instr, instrIN);
   }

   vg_assert(hi2 == DW_CFA_use_secondary);

   switch (lo6) {
      case DW_CFA_nop: 
         if (di->ddump_frames)
            VG_(printf)("  DW_CFA_nop\n");
         break;
      case DW_CFA_set_loc:
         ctx->loc = step_encoded_Addr(adi, &instr);
         if (di->ddump_frames)
            VG_(printf)("  rci:DW_CFA_set_loc\n");
         break;
      case DW_CFA_advance_loc1:
         delta = (UInt)ML_(cur_step_UChar)(&instr);
         delta *= ctx->code_a_f;
         ctx->loc += delta;
         if (di->ddump_frames)
            VG_(printf)("  DW_CFA_advance_loc1: %d to %08lx\n", 
                        (Int)delta, (Addr)ctx->loc + printing_bias);
         break;
      case DW_CFA_advance_loc2:
         delta = (UInt)ML_(cur_step_UShort)(&instr);
         delta *= ctx->code_a_f;
         ctx->loc += delta;
         if (di->ddump_frames)
            VG_(printf)("  DW_CFA_advance_loc2: %d to %08lx\n", 
                        (Int)delta, (Addr)ctx->loc + printing_bias);
         break;
      case DW_CFA_advance_loc4:
         delta = (UInt)ML_(cur_step_UInt)(&instr);
         delta *= ctx->code_a_f;
         ctx->loc += delta;
         if (di->ddump_frames)
            VG_(printf)("  DW_CFA_advance_loc4: %d to %08lx\n", 
                        (Int)delta, (Addr)ctx->loc + printing_bias);
         break;

      case DW_CFA_def_cfa:
         reg = step_leb128( &instr, 0 );
         off = step_leb128( &instr, 0 );
         if (reg < 0 || reg >= N_CFI_REGS) 
            return 0; 
         ctxs->cfa_is_regoff = True;
         ctxs->cfa_expr_ix   = 0;
         ctxs->cfa_reg       = reg;
         ctxs->cfa_off       = off;
         if (di->ddump_frames)
            VG_(printf)("  DW_CFA_def_cfa: r%d ofs %d\n", (Int)reg, (Int)off);
         break;

      case DW_CFA_def_cfa_sf:
         reg = step_leb128( &instr, 0 );
         off = step_leb128( &instr, 1 );
         if (reg < 0 || reg >= N_CFI_REGS)
            return 0; 
         ctxs->cfa_is_regoff = True;
         ctxs->cfa_expr_ix   = 0;
         ctxs->cfa_reg       = reg;
         ctxs->cfa_off       = off * ctx->data_a_f;
         if (di->ddump_frames)
            VG_(printf)("  rci:DW_CFA_def_cfa_sf\n");
         break;

      case DW_CFA_register:
         reg  = step_leb128( &instr, 0 );
         reg2 = step_leb128( &instr, 0 );
         if (reg < 0 || reg >= N_CFI_REGS) 
            return 0; 
         if (reg2 < 0 || reg2 >= N_CFI_REGS) 
            return 0; 
         ctxs->reg[reg].tag = RR_Reg;
         ctxs->reg[reg].arg = reg2;
         if (di->ddump_frames)
            VG_(printf)("  DW_CFA_register: r%d in r%d\n", 
                        (Int)reg, (Int)reg2);
         break;

      case DW_CFA_offset_extended:
         reg = step_leb128( &instr, 0 );
         off = step_leb128( &instr, 0 );
         if (reg < 0 || reg >= N_CFI_REGS)
            return 0; 
         ctxs->reg[reg].tag = RR_CFAOff;
         ctxs->reg[reg].arg = off * ctx->data_a_f;
         if (di->ddump_frames)
            VG_(printf)("  rci:DW_CFA_offset_extended\n");
         break;

      case DW_CFA_offset_extended_sf:
         reg = step_leb128( &instr, 0 );
         off = step_leb128( &instr, 1 );
         if (reg < 0 || reg >= N_CFI_REGS) 
            return 0; 
         ctxs->reg[reg].tag = RR_CFAOff;
         ctxs->reg[reg].arg = off * ctx->data_a_f;
         if (di->ddump_frames)
            VG_(printf)("  DW_CFA_offset_extended_sf: r%d at cfa%s%d\n", 
                        reg,
                        ctxs->reg[reg].arg < 0 ? "" : "+", 
                        (Int)ctxs->reg[reg].arg);
         break;

      case DW_CFA_GNU_negative_offset_extended:
         reg = step_leb128( &instr, 0 );
         off = step_leb128( &instr, 0 );
         if (reg < 0 || reg >= N_CFI_REGS)
            return 0; 
         ctxs->reg[reg].tag = RR_CFAOff;
         ctxs->reg[reg].arg = (-off) * ctx->data_a_f;
         if (di->ddump_frames)
            VG_(printf)("  rci:DW_CFA_GNU_negative_offset_extended\n");
         break;

      case DW_CFA_restore_extended:
         reg = step_leb128( &instr, 0 );
         if (reg < 0 || reg >= N_CFI_REGS)
            return 0; 
	 if (restore_ctx == NULL)
	    return 0; 
	 ctxs->reg[reg] = restore_ctx->state[restore_ctx->state_sp].reg[reg];
         if (di->ddump_frames)
            VG_(printf)("  rci:DW_CFA_restore_extended\n");
         break;

      case DW_CFA_val_offset:
         reg = step_leb128( &instr, 0 );
         off = step_leb128( &instr, 0 );
         if (reg < 0 || reg >= N_CFI_REGS)
            return 0; 
         ctxs->reg[reg].tag = RR_CFAValOff;
         ctxs->reg[reg].arg = off * ctx->data_a_f;
         if (di->ddump_frames)
            VG_(printf)("  rci:DW_CFA_val_offset\n");
         break;

      case DW_CFA_val_offset_sf:
         reg = step_leb128( &instr, 0 );
         off = step_leb128( &instr, 1 );
         if (reg < 0 || reg >= N_CFI_REGS)
            return 0; 
         ctxs->reg[reg].tag = RR_CFAValOff;
         ctxs->reg[reg].arg = off * ctx->data_a_f;
         if (di->ddump_frames)
            VG_(printf)("  rci:DW_CFA_val_offset_sf\n");
         break;

      case DW_CFA_def_cfa_register:
         reg = step_leb128( &instr, 0);
         if (reg < 0 || reg >= N_CFI_REGS) 
            return 0; 
         ctxs->cfa_is_regoff = True;
         ctxs->cfa_expr_ix   = 0;
         ctxs->cfa_reg       = reg;
         
         if (di->ddump_frames)
            VG_(printf)("  DW_CFA_def_cfa_register: r%d\n", (Int)reg );
         break;

      case DW_CFA_def_cfa_offset:
         off = step_leb128( &instr, 0);
         ctxs->cfa_is_regoff = True;
         ctxs->cfa_expr_ix   = 0;
         
         ctxs->cfa_off       = off;
         if (di->ddump_frames)
            VG_(printf)("  DW_CFA_def_cfa_offset: %d\n", (Int)off);
         break;

      case DW_CFA_def_cfa_offset_sf:
         off = step_leb128( &instr, 1);
         ctxs->cfa_is_regoff = True;
         ctxs->cfa_expr_ix   = 0;
         
         ctxs->cfa_off       = off * ctx->data_a_f;
         if (di->ddump_frames)
            VG_(printf)("  DW_CFA_def_cfa_offset_sf: %d\n", ctxs->cfa_off);
         break;

      case DW_CFA_undefined:
         reg = step_leb128( &instr, 0);
         if (reg < 0 || reg >= N_CFI_REGS) 
            return 0; 
         ctxs->reg[reg].tag = RR_Undef;
         ctxs->reg[reg].arg = 0;
         if (di->ddump_frames)
            VG_(printf)("  rci:DW_CFA_undefined\n");
         break;

      case DW_CFA_same_value:
         reg = step_leb128( &instr, 0);
         if (reg < 0 || reg >= N_CFI_REGS) 
            return 0; 
         ctxs->reg[reg].tag = RR_Same;
         ctxs->reg[reg].arg = 0;
         if (di->ddump_frames)
            VG_(printf)("  rci:DW_CFA_same_value\n");
         break;

      case DW_CFA_GNU_args_size:
          (void)step_leb128( &instr, 0 );
         if (di->ddump_frames)
            VG_(printf)("  rci:DW_CFA_GNU_args_size (ignored)\n");
         break;

      case DW_CFA_expression: {
         DiCursor expr;
         reg = step_leb128( &instr, 0 );
         len = step_leb128( &instr, 0 );
         expr = instr;
         instr = ML_(cur_plus)(instr, len);
         if (reg < 0 || reg >= N_CFI_REGS)
            return 0; 
         if (di->ddump_frames)
            VG_(printf)("  DW_CFA_expression: r%d (", 
                        (Int)reg);
         j = dwarfexpr_to_dag ( ctx, expr, len, True, 
                                di->ddump_frames);
         if (di->ddump_frames)
            VG_(printf)(")\n");
         vg_assert(j >= -1);
         if (j >= 0) {
            vg_assert(ctx->exprs);
            vg_assert( j < VG_(sizeXA)(ctx->exprs) );
         }
         if (j == -1)
            return 0; 
         
         j = ML_(CfiExpr_Deref)( ctx->exprs, j );
         ctxs->reg[reg].tag = RR_ValExpr;
         ctxs->reg[reg].arg = j;
         break;
      }

      case DW_CFA_val_expression: {
         DiCursor expr;
         reg = step_leb128( &instr, 0 );
         len = step_leb128( &instr, 0 );
         expr = instr;
         instr = ML_(cur_plus)(instr, len);
         if (reg < 0 || reg >= N_CFI_REGS)
            return 0; 
         if (di->ddump_frames)
            VG_(printf)("  DW_CFA_val_expression: r%d (", 
                        (Int)reg);
         j = dwarfexpr_to_dag ( ctx, expr, len, True, 
                                di->ddump_frames);
         if (di->ddump_frames)
            VG_(printf)(")\n");
         vg_assert(j >= -1);
         if (j >= 0) {
            vg_assert(ctx->exprs);
            vg_assert( j < VG_(sizeXA)(ctx->exprs) );
         }
         if (j == -1)
            return 0; 
         ctxs->reg[reg].tag = RR_ValExpr;
         ctxs->reg[reg].arg = j;
         break;
      }

      case DW_CFA_def_cfa_expression: {
         DiCursor expr;
         len = step_leb128( &instr, 0 );
         expr = instr;
         instr = ML_(cur_plus)(instr, len);
         if (di->ddump_frames)
            VG_(printf)("  DW_CFA_def_cfa_expression (");
         j = dwarfexpr_to_dag ( ctx, expr, len, False, 
                                di->ddump_frames);
         if (di->ddump_frames)
            VG_(printf)(")\n");
         ctxs->cfa_is_regoff = False;
         ctxs->cfa_reg       = 0;
         ctxs->cfa_off       = 0;
         ctxs->cfa_expr_ix   = j;
         break;
      }

      case DW_CFA_GNU_window_save:
         if (di->ddump_frames)
            VG_(printf)("  DW_CFA_GNU_window_save\n");
         break;

      case DW_CFA_remember_state:
         if (di->ddump_frames)
            VG_(printf)("  DW_CFA_remember_state\n");
         
         vg_assert(ctx->state_sp >= 0 && ctx->state_sp < N_RR_STACK);
         ctx->state_sp++;
         if (ctx->state_sp == N_RR_STACK) {
            
            VG_(message)(Vg_DebugMsg, "DWARF2 CFI reader: N_RR_STACK is "
                                      "too low; increase and recompile.");
            return 0; 
         } else {
            VG_(memcpy)(&ctx->state[ctx->state_sp],
                        &ctx->state[ctx->state_sp - 1],
                        sizeof(ctx->state[ctx->state_sp]) );
         }
         break;

      case DW_CFA_restore_state:
         if (di->ddump_frames)
            VG_(printf)("  DW_CFA_restore_state\n");
         
         vg_assert(ctx->state_sp >= 0 && ctx->state_sp < N_RR_STACK);
         if (ctx->state_sp == 0) {
            
            return 0; 
         } else {
            
            ctx->state_sp--;
         }
         break;

      default: 
         VG_(message)(Vg_DebugMsg, "DWARF2 CFI reader: unhandled CFI "
                                   "instruction 0:%d\n", (Int)lo6); 
         if (di->ddump_frames)
            VG_(printf)("  rci:run_CF_instruction:default\n");
         return 0; 
         
   }

   return ML_(cur_minus)(instr, instrIN);
}



static Int show_CF_instruction ( DiCursor instrIN,
                                 const AddressDecodingInfo* adi,
                                 Int code_a_f, Int data_a_f )
{
   Int      off, coff, reg, reg2, len;
   UInt     delta;
   Addr     loc;
   DiCursor instr   = instrIN;
   UChar    instr_0 = ML_(cur_step_UChar)(&instr);
   UChar    hi2     = (instr_0 >> 6) & 3;
   UChar    lo6     = instr_0 & 0x3F;

   if (0) {
      DiCursor tmpi = instrIN;
      UInt i_0 = ML_(cur_step_UChar)(&tmpi);
      UInt i_1 = ML_(cur_step_UChar)(&tmpi);
      UInt i_2 = ML_(cur_step_UChar)(&tmpi);
      UInt i_3 = ML_(cur_step_UChar)(&tmpi);
      UInt i_4 = ML_(cur_step_UChar)(&tmpi);
      UInt i_5 = ML_(cur_step_UChar)(&tmpi);
      UInt i_6 = ML_(cur_step_UChar)(&tmpi);
      UInt i_7 = ML_(cur_step_UChar)(&tmpi);
      VG_(printf)("raw:%x/%x:%x:%x:%x:%x:%x:%x:%x:%x\n",
                  hi2, lo6, i_0, i_1, i_2, i_3, i_4, i_5, i_6, i_7);
   }
   
   if (hi2 == DW_CFA_advance_loc) {
      VG_(printf)("  sci:DW_CFA_advance_loc(%d)\n", (Int)lo6);
      return ML_(cur_minus)(instr, instrIN);
   }

   if (hi2 == DW_CFA_offset) {
      off = step_leb128( &instr, 0 );
      coff = off * data_a_f;
      VG_(printf)("  DW_CFA_offset: r%d at cfa%s%d\n",
                  (Int)lo6, coff < 0 ? "" : "+", (Int)coff );
      return ML_(cur_minus)(instr, instrIN);
   }

   if (hi2 == DW_CFA_restore) {
      VG_(printf)("  sci:DW_CFA_restore(r%d)\n", (Int)lo6);
      return ML_(cur_minus)(instr, instrIN);
   }

   vg_assert(hi2 == DW_CFA_use_secondary);

   switch (lo6) {

      case DW_CFA_nop: 
         VG_(printf)("  DW_CFA_nop\n"); 
         break;

      case DW_CFA_set_loc:
         loc = step_encoded_Addr(adi, &instr);
         VG_(printf)("  sci:DW_CFA_set_loc(%#lx)\n", loc);
         break;

      case DW_CFA_advance_loc1:
         delta = (UInt)ML_(cur_step_UChar)(&instr);
         VG_(printf)("  sci:DW_CFA_advance_loc1(%d)\n", delta); 
         break;

      case DW_CFA_advance_loc2:
         delta = (UInt)ML_(cur_step_UShort)(&instr);
         VG_(printf)("  sci:DW_CFA_advance_loc2(%d)\n", delta); 
         break;

      case DW_CFA_advance_loc4:
         delta = (UInt)ML_(cur_step_UInt)(&instr);
         VG_(printf)("  DW_CFA_advance_loc4(%d)\n", delta); 
         break;

      case DW_CFA_def_cfa:
         reg = step_leb128( &instr, 0 );
         off = step_leb128( &instr, 0 );
         VG_(printf)("  DW_CFA_def_cfa: r%d ofs %d\n", (Int)reg, (Int)off); 
         break;

      case DW_CFA_def_cfa_sf:
         reg = step_leb128( &instr, 0 );
         off = step_leb128( &instr, 1 );
         VG_(printf)("  DW_CFA_def_cfa_sf: r%d ofs %d\n", 
                     (Int)reg, (Int)(off * data_a_f));
         break;

      case DW_CFA_register:
         reg  = step_leb128( &instr, 0);
         reg2 = step_leb128( &instr, 0);
         VG_(printf)("  sci:DW_CFA_register(r%d, r%d)\n", reg, reg2); 
         break;

      case DW_CFA_def_cfa_register:
         reg = step_leb128( &instr, 0);
         VG_(printf)("  sci:DW_CFA_def_cfa_register(r%d)\n", reg); 
         break;

      case DW_CFA_def_cfa_offset: 
         off = step_leb128( &instr, 0);
         VG_(printf)("  sci:DW_CFA_def_cfa_offset(%d)\n", off); 
         break;

      case DW_CFA_def_cfa_offset_sf:
         off = step_leb128( &instr, 1);
         VG_(printf)("  sci:DW_CFA_def_cfa_offset_sf(%d)\n", off);
         break;

      case DW_CFA_restore_extended:
         reg = step_leb128( &instr, 0);
         VG_(printf)("  sci:DW_CFA_restore_extended(r%d)\n", reg);
         break;

      case DW_CFA_undefined:
         reg = step_leb128( &instr, 0);
         VG_(printf)("  sci:DW_CFA_undefined(r%d)\n", reg);
         break;

      case DW_CFA_same_value:
         reg = step_leb128( &instr, 0);
         VG_(printf)("  sci:DW_CFA_same_value(r%d)\n", reg);
         break;

      case DW_CFA_remember_state:
         VG_(printf)("  sci:DW_CFA_remember_state\n");
         break;

      case DW_CFA_restore_state:
         VG_(printf)("  sci:DW_CFA_restore_state\n");
         break;

      case DW_CFA_GNU_args_size:
         off = step_leb128( &instr, 0 );
         VG_(printf)("  sci:DW_CFA_GNU_args_size(%d)\n", off ); 
         break;

      case DW_CFA_def_cfa_expression:
         len = step_leb128( &instr, 0 );
         instr = ML_(cur_plus)(instr, len);
         VG_(printf)("  sci:DW_CFA_def_cfa_expression(length %d)\n", len);
         break;

      case DW_CFA_expression:
         reg = step_leb128( &instr, 0 );
         len = step_leb128( &instr, 0 );
         instr = ML_(cur_plus)(instr, len);
         VG_(printf)("  sci:DW_CFA_expression(r%d, length %d)\n", reg, len);
         break;

      case DW_CFA_val_expression:
         reg = step_leb128( &instr, 0 );
         len = step_leb128( &instr, 0 );
         instr = ML_(cur_plus)(instr, len);
         VG_(printf)("  sci:DW_CFA_val_expression(r%d, length %d)\n", reg, len);
         break;

      case DW_CFA_offset_extended:
         reg = step_leb128( &instr, 0 );
         off = step_leb128( &instr, 0 );
         VG_(printf)("  sci:DW_CFA_offset_extended(r%d, "
                     "off %d x data_af)\n", reg, off);
         break;

      case DW_CFA_offset_extended_sf:
         reg = step_leb128( &instr, 0 );
         off = step_leb128( &instr, 1 );
	 coff = (Int)(off * data_a_f);
         VG_(printf)("  DW_CFA_offset_extended_sf: r%d at cfa%s%d\n", 
                        reg, coff < 0 ? "" : "+", coff);
         break;

      case DW_CFA_GNU_negative_offset_extended:
         reg = step_leb128( &instr, 0 );
         off = step_leb128( &instr, 0 );
         VG_(printf)("  sci:DW_CFA_GNU_negative_offset_extended"
                     "(r%d, off %d x data_af)\n", reg, -off);
         break;

      case DW_CFA_val_offset:
         reg = step_leb128( &instr, 0 );
         off = step_leb128( &instr, 0 );
         VG_(printf)("  sci:DW_CFA_val_offset(r%d, off %d x data_af)\n", 
                     reg, off);
         break;

       case DW_CFA_val_offset_sf:
         reg = step_leb128( &instr, 0 );
         off = step_leb128( &instr, 1 );
         VG_(printf)("  sci:DW_CFA_val_offset_sf(r%d, off %d x data_af)\n", 
                     reg, off);
         break;

      case DW_CFA_GNU_window_save:
         VG_(printf)("  sci:DW_CFA_GNU_window_save\n");
         break;

      default: 
         VG_(printf)("  sci:0:%d\n", (Int)lo6); 
         break;
   }

   return ML_(cur_minus)(instr, instrIN);
}


static void show_CF_instructions ( DiCursor instrs, Int ilen,
                                   const AddressDecodingInfo* adi,
                                   Int code_a_f, Int data_a_f )
{
   Int i = 0;
   while (True) {
      if (i >= ilen) break;
      i += show_CF_instruction( ML_(cur_plus)(instrs, i),
                                adi, code_a_f, data_a_f );
   }
}


static 
Bool run_CF_instructions ( DebugInfo* di,
                           Bool record,
                           UnwindContext* ctx, DiCursor instrs, Int ilen,
                           UWord fde_arange,
                           const UnwindContext* restore_ctx,
                           const AddressDecodingInfo* adi )
{
   Addr base;
   UInt len;
   DiCfSI_m cfsi_m;
   Bool summ_ok;
   Int j, i = 0;
   Addr loc_prev;
   if (0) ppUnwindContext(ctx);
   if (0) ppUnwindContext_summary(ctx);
   while (True) {
      loc_prev = ctx->loc;
      if (i >= ilen) break;
      if (0) (void)show_CF_instruction( ML_(cur_plus)(instrs,i), adi, 
                                        ctx->code_a_f, ctx->data_a_f );
      j = run_CF_instruction( ctx, ML_(cur_plus)(instrs,i),
                              restore_ctx, adi, di );
      if (j == 0)
         return False; 
      i += j;
      if (0) ppUnwindContext(ctx);
      if (record && loc_prev != ctx->loc) {
         summ_ok = summarise_context ( &base, &len, &cfsi_m,
                                       loc_prev, ctx, di );
         if (summ_ok) {
            ML_(addDiCfSI)(di, base, len, &cfsi_m);
            if (di->trace_cfi)
               ML_(ppDiCfSI)(di->cfsi_exprs, base, len, &cfsi_m);
         }
      }
   }
   if (ctx->loc < fde_arange) {
      loc_prev = ctx->loc;
      ctx->loc = fde_arange;
      if (record) {
         summ_ok = summarise_context ( &base, &len, &cfsi_m,
                                       loc_prev, ctx, di );
         if (summ_ok) {
            ML_(addDiCfSI)(di, base, len, &cfsi_m);
            if (di->trace_cfi)
               ML_(ppDiCfSI)(di->cfsi_exprs, base, len, &cfsi_m);
         }
      }
   }
   return True;
}



typedef
   struct {
      
      ULong    offset;
      
      Int      code_a_f;
      Int      data_a_f;
      
      Int      ra_reg;
      UChar    address_encoding;
      
      DiCursor instrs;
      Int      ilen;
      
      Bool     saw_z_augmentation;
   }
   CIE;

static void init_CIE ( CIE* cie )
{
   cie->offset             = 0;
   cie->code_a_f           = 0;
   cie->data_a_f           = 0;
   cie->ra_reg             = 0;
   cie->address_encoding   = 0;
   cie->instrs             = DiCursor_INVALID;
   cie->ilen               = 0;
   cie->saw_z_augmentation = False;
}

static CIE *the_CIEs = NULL;
static SizeT N_CIEs = 0;

void ML_(read_callframe_info_dwarf3)
        ( struct _DebugInfo* di,
          DiSlice escn_frame, Addr frame_avma, Bool is_ehframe )
{
   const HChar* how = NULL;
   Int      n_CIEs = 0;
   DiCursor frame_image = ML_(cur_from_sli)(escn_frame); 
   DiOffT   frame_size  = escn_frame.szB;
   DiCursor data        = frame_image;
   UWord    cfsi_used_orig;

   
   if (!is_ehframe)
      vg_assert(frame_avma == 0);

#  if defined(VGP_ppc32_linux) || defined(VGP_ppc64be_linux) \
      || defined(VGP_ppc64le_linux)
   
   return;
#  endif

   if (di->cfsi_used > 0) {
      ML_(canonicaliseCFI) ( di );
   }
   cfsi_used_orig = di->cfsi_used;

   if (di->trace_cfi) {
      VG_(printf)("\n-----------------------------------------------\n");
      VG_(printf)("CFI info: szB %lld, _avma %#lx\n",
                  escn_frame.szB, frame_avma );
      VG_(printf)("CFI info: name %s\n", di->fsm.filename );
   }

   

   while (True) {
      DiCursor ciefde_start;
      ULong    ciefde_len;
      ULong    cie_pointer;
      Bool     dw64;

      
      if (ML_(cur_cmpEQ)(data, ML_(cur_plus)(frame_image, frame_size)))
         return;

      
      if (ML_(cur_cmpGT)(data, ML_(cur_plus)(frame_image, frame_size))) {
         how = "overran the end of .eh_frame";
         goto bad;
      }


      ciefde_start = data;
      if (di->trace_cfi) 
         VG_(printf)("\ncie/fde.start   = (frame_image + 0x%llx)\n", 
                     ML_(cur_minus)(ciefde_start, frame_image));

      ciefde_len = (ULong)ML_(cur_step_UInt)(&data);
      if (di->trace_cfi) 
         VG_(printf)("cie/fde.length  = %lld\n", ciefde_len);

      if (ciefde_len == 0) {
         if (di->ddump_frames)
            VG_(printf)("%08llx ZERO terminator\n\n",
                        ML_(cur_minus)(ciefde_start, frame_image));
         return;
      }

      dw64 = False;
      if (ciefde_len == 0xFFFFFFFFUL) {
         dw64 = True;
         ciefde_len = ML_(cur_step_ULong)(&data);
      }

      if (dw64) {
         
         cie_pointer = ML_(cur_step_ULong)(&data); 
      } else {
         
         cie_pointer = (ULong)ML_(cur_step_UInt)(&data); 
      }

      if (di->trace_cfi) 
         VG_(printf)("cie.pointer     = %lld\n", cie_pointer);

      if (cie_pointer == (is_ehframe ? 0ULL
                          : dw64 ? 0xFFFFFFFFFFFFFFFFULL : 0xFFFFFFFFULL)) {

         Int      this_CIE;
         UChar    cie_version;
         DiCursor cie_augmentation;

         
	 if (di->trace_cfi) 
            VG_(printf)("------ new CIE #%d ------\n", n_CIEs);

	 
         vg_assert(n_CIEs >= 0);
         if (n_CIEs == N_CIEs) {
            N_CIEs += 1000;
            the_CIEs = ML_(dinfo_realloc)("di.rcid3.2", the_CIEs,
                                          N_CIEs * sizeof the_CIEs[0]);
         }

         this_CIE = n_CIEs;
         n_CIEs++;
         init_CIE( &the_CIEs[this_CIE] );

         the_CIEs[this_CIE].offset
            = (ULong)ML_(cur_minus)(ciefde_start, frame_image);

         if (di->ddump_frames)
            VG_(printf)("%08lx %08lx %08lx CIE\n",
                        (Addr)ML_(cur_minus)(ciefde_start, frame_image),
                        (Addr)ciefde_len,
                        (Addr)(UWord)cie_pointer );

         cie_version = ML_(cur_step_UChar)(&data);
         if (di->trace_cfi)
            VG_(printf)("cie.version     = %d\n", (Int)cie_version);
         if (di->ddump_frames)
            VG_(printf)("  Version:               %d\n", (Int)cie_version);
         if (cie_version != 1 && cie_version != 3 && cie_version != 4) {
            how = "unexpected CIE version (not 1 nor 3 nor 4)";
            goto bad;
         }

         cie_augmentation = data;
         data = ML_(cur_plus)(data, 1 + ML_(cur_strlen)(cie_augmentation));

         if (di->trace_cfi || di->ddump_frames) {
            HChar* str = ML_(cur_read_strdup)(cie_augmentation, "di.rcid3.1");
            if (di->trace_cfi) 
               VG_(printf)("cie.augment     = \"%s\"\n", str);
            if (di->ddump_frames)
               VG_(printf)("  Augmentation:          \"%s\"\n", str);
            ML_(dinfo_free)(str);
         }

         if (ML_(cur_read_UChar)(cie_augmentation) == 'e'
             && ML_(cur_read_UChar)
                   (ML_(cur_plus)(cie_augmentation, 1)) == 'h') {
            data = ML_(cur_plus)(data, sizeof(Addr));
            cie_augmentation = ML_(cur_plus)(cie_augmentation, 2);
         }

         if (cie_version >= 4) {
            if (ML_(cur_step_UChar)(&data) != sizeof(Addr)) {
               how = "unexpected address size";
               goto bad;
            }
            if (ML_(cur_step_UChar)(&data) != 0) {
               how = "unexpected non-zero segment size";
               goto bad;
            }
         }

         the_CIEs[this_CIE].code_a_f = step_leb128( &data, 0);
         if (di->trace_cfi) 
            VG_(printf)("cie.code_af     = %d\n", 
                        the_CIEs[this_CIE].code_a_f);
         if (di->ddump_frames)
            VG_(printf)("  Code alignment factor: %d\n",
                        (Int)the_CIEs[this_CIE].code_a_f);

         the_CIEs[this_CIE].data_a_f = step_leb128( &data, 1);
         if (di->trace_cfi) 
            VG_(printf)("cie.data_af     = %d\n",
                        the_CIEs[this_CIE].data_a_f);
         if (di->ddump_frames)
            VG_(printf)("  Data alignment factor: %d\n",
                        (Int)the_CIEs[this_CIE].data_a_f);

         if (cie_version == 1) {
            the_CIEs[this_CIE].ra_reg = (Int)ML_(cur_step_UChar)(&data); 
         } else {
            the_CIEs[this_CIE].ra_reg = step_leb128( &data, 0);
         }
         if (di->trace_cfi) 
            VG_(printf)("cie.ra_reg      = %d\n", 
                        the_CIEs[this_CIE].ra_reg);
         if (di->ddump_frames)
            VG_(printf)("  Return address column: %d\n",
                        (Int)the_CIEs[this_CIE].ra_reg);

         if (the_CIEs[this_CIE].ra_reg < 0 
             || the_CIEs[this_CIE].ra_reg >= N_CFI_REGS) {
            how = "cie.ra_reg has implausible value";
            goto bad;
         }

         the_CIEs[this_CIE].saw_z_augmentation 
            = ML_(cur_read_UChar)(cie_augmentation) == 'z';
         if (the_CIEs[this_CIE].saw_z_augmentation) {
            UInt length = step_leb128( &data, 0);
            the_CIEs[this_CIE].instrs = ML_(cur_plus)(data, length);
            cie_augmentation = ML_(cur_plus)(cie_augmentation, 1);
            if (di->ddump_frames) {
               UInt i;
               VG_(printf)("  Augmentation data:    ");
               for (i = 0; i < length; i++)
                  VG_(printf)(" %02x", (UInt)ML_(cur_read_UChar)
                                                (ML_(cur_plus)(data, i)));
               VG_(printf)("\n");
            }
         } else {
            the_CIEs[this_CIE].instrs = DiCursor_INVALID;
         }

         the_CIEs[this_CIE].address_encoding = default_Addr_encoding();

         while (ML_(cur_read_UChar)(cie_augmentation)) {
            switch (ML_(cur_read_UChar)(cie_augmentation)) {
               case 'L':
                  data = ML_(cur_plus)(data, 1);
                  cie_augmentation = ML_(cur_plus)(cie_augmentation, 1);
                  break;
               case 'R':
                  the_CIEs[this_CIE].address_encoding 
                     = ML_(cur_step_UChar)(&data);
                  cie_augmentation = ML_(cur_plus)(cie_augmentation, 1);
                  break;
               case 'P':
                  data = ML_(cur_plus)(data, size_of_encoded_Addr(
                                                ML_(cur_read_UChar)(data) ));
                  data = ML_(cur_plus)(data, 1);
                  cie_augmentation = ML_(cur_plus)(cie_augmentation, 1);
                  break;
               case 'S':
                  cie_augmentation = ML_(cur_plus)(cie_augmentation, 1);
                  break;
               default:
                  if (!ML_(cur_is_valid)(the_CIEs[this_CIE].instrs)) {
                     how = "unhandled cie.augmentation";
                     goto bad;
                  }
                  data = the_CIEs[this_CIE].instrs;
                  goto done_augmentation;
            }
         }

        done_augmentation:

         if (di->trace_cfi) 
            VG_(printf)("cie.encoding    = 0x%x\n", 
                        the_CIEs[this_CIE].address_encoding);

         the_CIEs[this_CIE].instrs = data;
         the_CIEs[this_CIE].ilen   = ML_(cur_minus)(ciefde_start, data) 
                                     + (Long)ciefde_len + (Long)sizeof(UInt);
         if (di->trace_cfi) {
            
            VG_(printf)("cie.ilen        = %d\n", the_CIEs[this_CIE].ilen);
	 }

         if (the_CIEs[this_CIE].ilen < 0
             || the_CIEs[this_CIE].ilen > frame_size) {
            how = "implausible # cie initial insns";
            goto bad;
         }

         data = ML_(cur_plus)(data, the_CIEs[this_CIE].ilen);

 
         if (di->ddump_frames)
            VG_(printf)("\n");

         if (di->trace_cfi || di->ddump_frames) {
            AddressDecodingInfo adi;
            adi.encoding      = the_CIEs[this_CIE].address_encoding;
            adi.ehframe_image = frame_image;
            adi.ehframe_avma  = frame_avma;
            adi.text_bias     = di->text_debug_bias;
            show_CF_instructions( the_CIEs[this_CIE].instrs, 
                                  the_CIEs[this_CIE].ilen, &adi,
                                  the_CIEs[this_CIE].code_a_f,
                                  the_CIEs[this_CIE].data_a_f );
         }

         if (di->ddump_frames)
            VG_(printf)("\n");

      } else {

         AddressDecodingInfo adi;
         UnwindContext ctx, restore_ctx;
         Int      cie;
         ULong    look_for;
         Bool     ok;
         Addr     fde_initloc;
         UWord    fde_arange;
         DiCursor fde_instrs;
         Int      fde_ilen;

         


         
         if (is_ehframe)
            look_for = ML_(cur_minus)(data, frame_image)
                       - (dw64 ? sizeof(ULong) : sizeof(UInt))
                       - cie_pointer;
         else
            look_for = cie_pointer;

         for (cie = 0; cie < n_CIEs; cie++) {
            if (0) VG_(printf)("look for %lld   %lld\n",
                               look_for, the_CIEs[cie].offset );
            if (the_CIEs[cie].offset == look_for)
               break;
	 }
         vg_assert(cie >= 0 && cie <= n_CIEs);
         if (cie == n_CIEs) {
            how = "FDE refers to not-findable CIE";
            goto bad;
	 }

         adi.encoding      = the_CIEs[cie].address_encoding;
         adi.ehframe_image = frame_image;
         adi.ehframe_avma  = frame_avma;
         adi.text_bias     = di->text_debug_bias;
         fde_initloc = step_encoded_Addr(&adi, &data);
         if (di->trace_cfi) 
            VG_(printf)("fde.initloc     = %#lx\n", fde_initloc);

         adi.encoding      = the_CIEs[cie].address_encoding & 0xf;
         adi.ehframe_image = frame_image;
         adi.ehframe_avma  = frame_avma;
         adi.text_bias     = di->text_debug_bias;

         { UInt ptr_size = size_of_encoded_Addr( adi.encoding );
           switch (ptr_size) {
              case 8: case 4: case 2: case 1: 
                 fde_arange 
                    = (UWord)step_le_u_encoded_literal(&data, ptr_size);
                 break;
              default: 
                 how = "unknown arange field encoding in FDE";
                 goto bad;
           }
         }

         if (di->trace_cfi) 
            VG_(printf)("fde.arangec     = %#lx\n", fde_arange);

         if (di->ddump_frames)
            VG_(printf)("%08lx %08lx %08lx FDE cie=%08lx pc=%08lx..%08lx\n",
                        (Addr)ML_(cur_minus)(ciefde_start, frame_image),
                        (Addr)ciefde_len,
                        (Addr)(UWord)cie_pointer,
                        (Addr)look_for, 
                        ((Addr)fde_initloc) - di->text_debug_bias, 
                        ((Addr)fde_initloc) - di->text_debug_bias + fde_arange);

         if (the_CIEs[cie].saw_z_augmentation) {
            UInt length = step_leb128( &data, 0);
            if (di->ddump_frames && (length > 0)) {
               UInt i;
               VG_(printf)("  Augmentation data:    ");
               for (i = 0; i < length; i++)
                  VG_(printf)(" %02x", (UInt)ML_(cur_read_UChar)
                                                (ML_(cur_plus)(data, i)));
               VG_(printf)("\n\n");
            }
            data = ML_(cur_plus)(data, length);
         }

         fde_instrs = data;
         fde_ilen   = ML_(cur_minus)(ciefde_start, data)
                      + (Long)ciefde_len + (Long)sizeof(UInt);
         if (di->trace_cfi) {
            
            VG_(printf)("fde.ilen        = %d\n", (Int)fde_ilen);
	 }

         if (fde_ilen < 0 || fde_ilen > frame_size) {
            how = "implausible # fde insns";
            goto bad;
         }

	 data = ML_(cur_plus)(data, fde_ilen);

         if (cfsi_used_orig > 0) {
            Addr a_mid_lo, a_mid_hi;
            Word mid, size, 
                 lo = 0, 
                 hi = cfsi_used_orig-1;
            while (True) {
               
               if (lo > hi) break; 
               mid      = (lo + hi) / 2;
               a_mid_lo = di->cfsi_rd[mid].base;
               size     = di->cfsi_rd[mid].len;
               a_mid_hi = a_mid_lo + size - 1;
               vg_assert(a_mid_hi >= a_mid_lo);
               if (fde_initloc + fde_arange <= a_mid_lo) {
                  hi = mid-1; continue;
               }
               if (fde_initloc > a_mid_hi) { lo = mid+1; continue; }
               break;
            }

            
            if (lo <= hi)
               continue;
         }

         adi.encoding      = the_CIEs[cie].address_encoding;
         adi.ehframe_image = frame_image;
         adi.ehframe_avma  = frame_avma;
         adi.text_bias     = di->text_debug_bias;

         if (di->trace_cfi)
            show_CF_instructions( fde_instrs, fde_ilen, &adi,
                                  the_CIEs[cie].code_a_f,
                                  the_CIEs[cie].data_a_f );

	 initUnwindContext(&ctx);
         ctx.code_a_f = the_CIEs[cie].code_a_f;
         ctx.data_a_f = the_CIEs[cie].data_a_f;
         ctx.initloc  = fde_initloc;
         ctx.ra_reg   = the_CIEs[cie].ra_reg;
         ctx.exprs    = VG_(newXA)( ML_(dinfo_zalloc), "di.rcid.1",
                                    ML_(dinfo_free), 
                                    sizeof(CfiExpr) );

	 { Bool hack = di->ddump_frames; 
           di->ddump_frames = False;
           initUnwindContext(&restore_ctx);
           ok = run_CF_instructions(
                   di, False, &ctx, the_CIEs[cie].instrs, 
                   the_CIEs[cie].ilen, 0, NULL, &adi
                );
           di->ddump_frames = hack;
         }
         if (ok) {
            restore_ctx = ctx;
	    ok = run_CF_instructions(
                    di, True, &ctx, fde_instrs, fde_ilen, fde_arange, 
                    &restore_ctx, &adi
                 );
            if (di->ddump_frames)
               VG_(printf)("\n");
	 }

         VG_(deleteXA)( ctx.exprs );
      }
   }

   return;

   bad:
    if (!VG_(clo_xml) && VG_(clo_verbosity) > 1)
       VG_(message)(Vg_UserMsg,
                    "Warning: %s in DWARF2 CFI reading\n", how);
    return;
}

#endif 

