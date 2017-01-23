

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

#ifndef __PUB_TOOL_OPTIONS_H
#define __PUB_TOOL_OPTIONS_H

#include "pub_tool_basics.h"     
#include "libvex.h"              



#define VG_BOOL_CLO(qq_arg, qq_option, qq_var) \
   (VG_STREQN(VG_(strlen)(qq_option)+1, qq_arg, qq_option"=") && \
    ({ \
      const HChar* val = &(qq_arg)[ VG_(strlen)(qq_option)+1 ]; \
      if      VG_STREQ(val, "yes") (qq_var) = True; \
      else if VG_STREQ(val, "no")  (qq_var) = False; \
      else VG_(fmsg_bad_option)(qq_arg, "Invalid boolean value '%s'" \
                                " (should be 'yes' or 'no')\n", val);    \
      True; \
    }) \
   )

#define VG_STR_CLO(qq_arg, qq_option, qq_var) \
   (VG_STREQN(VG_(strlen)(qq_option)+1, qq_arg, qq_option"=") && \
    ({ \
      const HChar* val = &(qq_arg)[ VG_(strlen)(qq_option)+1 ]; \
      (qq_var) = val; \
      True; \
    }) \
   )

#define VG_USETGEN_CLO(qq_arg, qq_option, qq_vals, qq_var, qq_all) \
   (VG_STREQN(VG_(strlen)(qq_option)+1, qq_arg, qq_option"=") && \
    ({ \
      const HChar* val = &(qq_arg)[ VG_(strlen)(qq_option)+1 ]; \
      if (!VG_(parse_enum_set)(qq_vals, \
                               qq_all, \
                               val, \
                               &(qq_var))) \
            VG_(fmsg_bad_option)(qq_arg, "%s is an invalid %s set\n", \
                                 val, qq_option+2); \
      True; \
     }) \
    )

#define VG_USET_CLO(qq_arg, qq_option, qq_vals, qq_var) \
   VG_USETGEN_CLO((qq_arg), qq_option, (qq_vals), (qq_var), True)

#define VG_USETX_CLO(qq_arg, qq_option, qq_vals, qq_var) \
   VG_USETGEN_CLO((qq_arg), qq_option, (qq_vals), (qq_var), False)

#define VG_INT_CLO(qq_arg, qq_option, qq_var) \
   (VG_STREQN(VG_(strlen)(qq_option)+1, qq_arg, qq_option"=") && \
    ({ \
      const HChar* val = &(qq_arg)[ VG_(strlen)(qq_option)+1 ]; \
      HChar* s; \
      Long n = VG_(strtoll10)( val, &s ); \
      (qq_var) = n; \
       \
      if ('\0' != s[0] || (qq_var) != n) VG_(fmsg_bad_option)(qq_arg, \
                                  "Invalid integer value '%s'\n", val); \
      True; \
     }) \
    )

#define VG_BINTN_CLO(qq_base, qq_arg, qq_option, qq_var, qq_lo, qq_hi) \
   (VG_STREQN(VG_(strlen)(qq_option)+1, qq_arg, qq_option"=") && \
    ({ \
      const HChar* val = &(qq_arg)[ VG_(strlen)(qq_option)+1 ]; \
      HChar* s; \
      Long n = VG_(strtoll##qq_base)( val, &s ); \
      (qq_var) = n; \
       \
       \
       \
       \
      if ('\0' != s[0] || (qq_var) != n) VG_(fmsg_bad_option)(qq_arg, \
                                  "Invalid integer value '%s'\n", val); \
       \
      if ((qq_var) < (qq_lo) || (qq_var) > (qq_hi)) { \
         VG_(fmsg_bad_option)(qq_arg, \
            "'%s' argument must be between %lld and %lld\n", \
            (qq_option), (Long)(qq_lo), (Long)(qq_hi)); \
      } \
      True; \
     }) \
    )

#define VG_BINT_CLO(qq_arg, qq_option, qq_var, qq_lo, qq_hi) \
   VG_BINTN_CLO(10, (qq_arg), qq_option, (qq_var), (qq_lo), (qq_hi))

#define VG_BHEX_CLO(qq_arg, qq_option, qq_var, qq_lo, qq_hi) \
   VG_BINTN_CLO(16, (qq_arg), qq_option, (qq_var), (qq_lo), (qq_hi))

#define VG_DBL_CLO(qq_arg, qq_option, qq_var) \
   (VG_STREQN(VG_(strlen)(qq_option)+1, qq_arg, qq_option"=") && \
    ({ \
      const HChar* val = &(qq_arg)[ VG_(strlen)(qq_option)+1 ]; \
      HChar* s; \
      double n = VG_(strtod)( val, &s ); \
      (qq_var) = n; \
       \
      if ('\0' != s[0]) VG_(fmsg_bad_option)(qq_arg, \
                            "Invalid floating point value '%s'\n",val); \
      True; \
     }) \
    )

#define VG_XACT_CLO(qq_arg, qq_option, qq_var, qq_val) \
   (VG_STREQ((qq_arg), (qq_option)) && \
    ({ \
      (qq_var) = (qq_val); \
      True; \
    }) \
   )

#define VG_STRINDEX_CLO(qq_arg, qq_option, qq_strings, qq_ix) \
   (VG_STREQN(VG_(strlen)(qq_option)+1, qq_arg, qq_option"=") && \
    ({ \
      const HChar* val = &(qq_arg)[ VG_(strlen)(qq_option)+1 ]; \
      for (qq_ix = 0; (qq_strings)[qq_ix]; qq_ix++) { \
         if (VG_STREQ(val, (qq_strings)[qq_ix])) \
            break; \
      } \
      if ((qq_strings)[qq_ix] == NULL) \
         VG_(fmsg_bad_option)(qq_arg, \
                              "Invalid string '%s' in '%s'\n", val, qq_arg); \
      True; \
     }) \
    )

extern Int  VG_(clo_verbosity);

extern Bool VG_(clo_stats);

extern Int VG_(clo_vgdb_error);

extern const HChar *VG_(arg_vgdb_prefix);

extern Bool VG_(clo_xml);

extern const HChar* VG_(clo_xml_user_comment);

extern VexControl VG_(clo_vex_control);
extern VexRegisterUpdates VG_(clo_px_file_backed);

extern Int   VG_(clo_backtrace_size);

extern Bool VG_(clo_show_below_main);


extern HChar* VG_(expand_file_name)(const HChar* option_name,
                                    const HChar* format);

#endif   

