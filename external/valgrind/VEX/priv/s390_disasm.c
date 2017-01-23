

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright IBM Corp. 2010-2013

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   The GNU General Public License is contained in the file COPYING.
*/


#include <stdarg.h>
#include "libvex_basictypes.h"
#include "main_util.h"        
#include "main_globals.h"     
#include "s390_defs.h"        
#include "s390_disasm.h"


static const HChar *
mnemonic(const HChar *mnm)
{
   vassert(vex_strlen(mnm) <= S390_MAX_MNEMONIC_LEN);

   static HChar buf[S390_MAX_MNEMONIC_LEN + 1];

   vex_sprintf(buf, "%-*s", S390_MAX_MNEMONIC_LEN, mnm);

   return buf;
}


static const HChar *
gpr_operand(UInt archreg)
{
   static const HChar names[16][5] = {
      "%r0", "%r1", "%r2", "%r3",
      "%r4", "%r5", "%r6", "%r7",
      "%r8", "%r9", "%r10", "%r11",
      "%r12", "%r13", "%r14", "%r15",
   };

   vassert(archreg < 16);

   return names[archreg];
}


static const HChar *
fpr_operand(UInt archreg)
{
   static const HChar names[16][5] = {
      "%f0", "%f1", "%f2", "%f3",
      "%f4", "%f5", "%f6", "%f7",
      "%f8", "%f9", "%f10", "%f11",
      "%f12", "%f13", "%f14", "%f15",
   };

   vassert(archreg < 16);

   return names[archreg];
}


static const HChar *
ar_operand(UInt archreg)
{
   static const HChar names[16][5] = {
      "%a0", "%a1", "%a2", "%a3",
      "%a4", "%a5", "%a6", "%a7",
      "%a8", "%a9", "%a10", "%a11",
      "%a12", "%a13", "%a14", "%a15",
   };

   vassert(archreg < 16);

   return names[archreg];
}


static const HChar *
cab_operand(const HChar *base, UInt mask)
{
   HChar *to;
   const HChar *from;

   static HChar buf[S390_MAX_MNEMONIC_LEN + 1];

   static const HChar suffix[8][3] = {
      "", "h", "l", "ne", "e", "nl", "nh", ""
   };

   
   vassert(vex_strlen(base) + sizeof suffix[0] <= sizeof buf);

   
   for (from = base, to = buf; *from; ++from, ++to) {
      *to = *from;
   }
   
   for (from = suffix[mask >> 1]; *from; ++from, ++to) {
      *to = *from;
   }
   *to = '\0';

   return buf;
}


static const HChar *
construct_mnemonic(const HChar *prefix, const HChar *suffix, UInt mask)
{
   HChar *to;
   const HChar *from;

   static HChar buf[S390_MAX_MNEMONIC_LEN + 1];

   static HChar mask_id[16][4] = {
      "", 
      "o", "h", "nle", "l", "nhe", "lh", "ne",
      "e", "nlh", "he", "nl", "le", "nh", "no",
      ""  
   };

   
   vassert(vex_strlen(prefix) + vex_strlen(suffix) +
           sizeof mask_id[0] <= sizeof buf);

   
   for (from = prefix, to = buf; *from; ++from, ++to) {
      *to = *from;
   }
   
   for (from = mask_id[mask]; *from; ++from, ++to) {
      *to = *from;
   }
   
   for (from = suffix; *from; ++from, ++to) {
      *to = *from;
   }
   *to = '\0';

   return buf;
}


static const HChar *
bcr_operand(UInt m1)
{
   if (m1 ==  0) return "nopr";
   if (m1 == 15) return "br";

   return construct_mnemonic("b", "r", m1);
}


static const HChar *
bc_operand(UInt m1)
{
   if (m1 ==  0) return "nop";
   if (m1 == 15) return "b";

   return construct_mnemonic("b", "", m1);
}


static const HChar *
brc_operand(UInt m1)
{
   if (m1 == 0)  return "brc";
   if (m1 == 15) return "j";

   return construct_mnemonic("j", "", m1);
}


static const HChar *
brcl_operand(UInt m1)
{
   if (m1 == 0)  return "brcl";
   if (m1 == 15) return "jg";

   return construct_mnemonic("jg", "", m1);
}


static const HChar *
cls_operand(Int kind, UInt mask)
{
   const HChar *prefix;

   switch (kind) {
   case S390_XMNM_LOCR:   prefix = "locr";  break;
   case S390_XMNM_LOCGR:  prefix = "locgr"; break;
   case S390_XMNM_LOC:    prefix = "loc";   break;
   case S390_XMNM_LOCG:   prefix = "locg";  break;
   case S390_XMNM_STOC:   prefix = "stoc";  break;
   case S390_XMNM_STOCG:  prefix = "stocg"; break;
   default:
      vpanic("cls_operand");
   }

   return construct_mnemonic(prefix, "", mask);
}


static HChar *
dxb_operand(HChar *p, UInt d, UInt x, UInt b, Bool displacement_is_signed)
{
   if (displacement_is_signed) {
      Int displ = (Int)(d << 12) >> 12;  

      p += vex_sprintf(p, "%d", displ);
   } else {
      p += vex_sprintf(p, "%u", d);
   }
   if (x != 0) {
      p += vex_sprintf(p, "(%s", gpr_operand(x));
      if (b != 0) {
         p += vex_sprintf(p, ",%s", gpr_operand(b));
      }
      p += vex_sprintf(p, ")");
   } else {
      if (b != 0) {
         p += vex_sprintf(p, "(%s)", gpr_operand(b));
      }
   }

   return p;
}


static HChar *
udlb_operand(HChar *p, UInt d, UInt length, UInt b)
{
   p += vex_sprintf(p, "%u", d);
   p += vex_sprintf(p, "(%u", length + 1);  
   if (b != 0) {
      p += vex_sprintf(p, ",%s", gpr_operand(b));
   }
   p += vex_sprintf(p, ")");

   return p;
}


void
s390_disasm(UInt command, ...)
{
   va_list  args;
   UInt argkind;
   HChar buf[128];  
   HChar *p;
   HChar separator;
   Int mask_suffix = -1;

   va_start(args, command);

   p = buf;
   separator = 0;

   while (42) {
      argkind = command & 0xF;
      command >>= 4;

      if (argkind == S390_ARG_DONE) goto done;

      if (argkind == S390_ARG_CABM) separator = 0;  

      
      if (separator) *p++ = separator;

      
      switch (argkind) {
      case S390_ARG_MNM:
         p += vex_sprintf(p, "%s", mnemonic(va_arg(args, HChar *)));
         separator = ' ';
         continue;

      case S390_ARG_XMNM: {
         UInt mask, kind;
         const HChar *mnm;

         kind = va_arg(args, UInt);

         separator = ' ';
         switch (kind) {
         case S390_XMNM_BC:
         case S390_XMNM_BCR:
            mask = va_arg(args, UInt);
            mnm = kind == S390_XMNM_BCR ? bcr_operand(mask) : bc_operand(mask);
            p  += vex_sprintf(p, "%s", mnemonic(mnm));
            
            if (mask == 0) goto done;
            break;

         case S390_XMNM_BRC:
         case S390_XMNM_BRCL:
            mask = va_arg(args, UInt);
            mnm = kind == S390_XMNM_BRC ? brc_operand(mask) : brcl_operand(mask);
            p  += vex_sprintf(p, "%s", mnemonic(mnm));

            
            if (mask == 0) {
               p += vex_sprintf(p, " 0");
               separator = ',';
            }
            break;

         case S390_XMNM_CAB:
            mnm  = va_arg(args, HChar *);
            mask = va_arg(args, UInt);
            p  += vex_sprintf(p, "%s", mnemonic(cab_operand(mnm, mask)));
            break;

         case S390_XMNM_LOCR:
         case S390_XMNM_LOCGR:
         case S390_XMNM_LOC:
         case S390_XMNM_LOCG:
         case S390_XMNM_STOC:
         case S390_XMNM_STOCG:
            mask = va_arg(args, UInt);
            mnm = cls_operand(kind, mask);
            p  += vex_sprintf(p, "%s", mnemonic(mnm));
            if (mask == 0 || mask == 15) mask_suffix = mask;
            break;
         }
      }
      continue;

      case S390_ARG_GPR:
         p += vex_sprintf(p, "%s", gpr_operand(va_arg(args, UInt)));
         break;

      case S390_ARG_FPR:
         p += vex_sprintf(p, "%s", fpr_operand(va_arg(args, UInt)));
         break;

      case S390_ARG_AR:
         p += vex_sprintf(p, "%s", ar_operand(va_arg(args, UInt)));
         break;

      case S390_ARG_UINT:
         p += vex_sprintf(p, "%u", va_arg(args, UInt));
         break;

      case S390_ARG_INT:
         p += vex_sprintf(p, "%d", (Int)(va_arg(args, UInt)));
         break;

      case S390_ARG_PCREL: {
         Long offset = va_arg(args, Int);

         
         offset <<= 1;

         if (offset < 0) {
            p += vex_sprintf(p, ".%lld", offset);
         } else {
            p += vex_sprintf(p, ".+%lld", offset);
         }
         break;
      }

      case S390_ARG_SDXB: {
         UInt dh, dl, x, b;

         dh = va_arg(args, UInt);
         dl = va_arg(args, UInt);
         x  = va_arg(args, UInt);
         b  = va_arg(args, UInt);

         p = dxb_operand(p, (dh << 12) | dl, x, b, 1 );
         break;
      }

      case S390_ARG_UDXB: {
         UInt d, x, b;

         d = va_arg(args, UInt);
         x = va_arg(args, UInt);
         b = va_arg(args, UInt);

         p = dxb_operand(p, d, x, b, 0 );
         break;
      }

      case S390_ARG_UDLB: {
         UInt d, l, b;

         d = va_arg(args, UInt);
         l = va_arg(args, UInt);
         b = va_arg(args, UInt);

         p = udlb_operand(p, d, l, b);
         break;
      }

      case S390_ARG_CABM: {
         UInt mask;

         mask = va_arg(args, UInt) & 0xE;
         if (mask == 0 || mask == 14) {
            p += vex_sprintf(p, ",%u", mask);
         }
         break;
      }
      }

      separator = ',';
   }

 done:
   va_end(args);

   if (mask_suffix != -1)
      p += vex_sprintf(p, ",%d", mask_suffix);
   *p = '\0';

   vassert(p < buf + sizeof buf);  

   
   vex_printf("%s\n", buf);
}

