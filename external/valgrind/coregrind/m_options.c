

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2013 Nicholas Nethercote
      njn@valgrind.org

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

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_options.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcfile.h"
#include "pub_core_libcprint.h"
#include "pub_core_libcproc.h"
#include "pub_core_mallocfree.h"
#include "pub_core_seqmatch.h"     
#include "pub_core_aspacemgr.h"




VexControl VG_(clo_vex_control);
VexRegisterUpdates VG_(clo_px_file_backed) = VexRegUpd_INVALID;

Bool   VG_(clo_error_limit)    = True;
Int    VG_(clo_error_exitcode) = 0;
HChar *VG_(clo_error_markers)[2] = {NULL, NULL};

#if defined(VGPV_arm_linux_android) \
    || defined(VGPV_x86_linux_android) \
    || defined(VGPV_mips32_linux_android) \
    || defined(VGPV_arm64_linux_android)
VgVgdb VG_(clo_vgdb)           = Vg_VgdbNo; 
#else
VgVgdb VG_(clo_vgdb)           = Vg_VgdbYes;
#endif
Int    VG_(clo_vgdb_poll)      = 5000; 
Int    VG_(clo_vgdb_error)     = 999999999;
UInt   VG_(clo_vgdb_stop_at)   = 0;
const HChar *VG_(clo_vgdb_prefix)    = NULL;
const HChar *VG_(arg_vgdb_prefix)    = NULL;
Bool   VG_(clo_vgdb_shadow_registers) = False;

Bool   VG_(clo_db_attach)      = False;
const HChar*  VG_(clo_db_command)     = GDB_PATH " -nw %f %p";
Int    VG_(clo_gen_suppressions) = 0;
Int    VG_(clo_sanity_level)   = 1;
Int    VG_(clo_verbosity)      = 1;
Bool   VG_(clo_stats)          = False;
Bool   VG_(clo_xml)            = False;
const HChar* VG_(clo_xml_user_comment) = NULL;
Bool   VG_(clo_demangle)       = True;
const HChar* VG_(clo_soname_synonyms)    = NULL;
Bool   VG_(clo_trace_children) = False;
const HChar* VG_(clo_trace_children_skip) = NULL;
const HChar* VG_(clo_trace_children_skip_by_arg) = NULL;
Bool   VG_(clo_child_silent_after_fork) = False;
const HChar* VG_(clo_log_fname_expanded) = NULL;
const HChar* VG_(clo_xml_fname_expanded) = NULL;
Bool   VG_(clo_time_stamp)     = False;
Int    VG_(clo_input_fd)       = 0; 
Bool   VG_(clo_default_supp)   = True;
XArray *VG_(clo_suppressions);   
XArray *VG_(clo_fullpath_after); 
const HChar* VG_(clo_extra_debuginfo_path) = NULL;
const HChar* VG_(clo_debuginfo_server) = NULL;
Bool   VG_(clo_allow_mismatched_debuginfo) = False;
UChar  VG_(clo_trace_flags)    = 0; 
Bool   VG_(clo_profyle_sbs)    = False;
UChar  VG_(clo_profyle_flags)  = 0; 
ULong  VG_(clo_profyle_interval) = 0;
Int    VG_(clo_trace_notbelow) = -1;  
Int    VG_(clo_trace_notabove) = -1;  
Bool   VG_(clo_trace_syscalls) = False;
Bool   VG_(clo_trace_signals)  = False;
Bool   VG_(clo_trace_symtab)   = False;
const HChar* VG_(clo_trace_symtab_patt) = "*";
Bool   VG_(clo_trace_cfi)      = False;
Bool   VG_(clo_debug_dump_syms) = False;
Bool   VG_(clo_debug_dump_line) = False;
Bool   VG_(clo_debug_dump_frames) = False;
Bool   VG_(clo_trace_redir)    = False;
enum FairSchedType
       VG_(clo_fair_sched)     = disable_fair_sched;
Bool   VG_(clo_trace_sched)    = False;
Bool   VG_(clo_profile_heap)   = False;
Int    VG_(clo_core_redzone_size) = CORE_REDZONE_DEFAULT_SZB;
Int    VG_(clo_redzone_size)   = -1;
Int    VG_(clo_dump_error)     = 0;
Int    VG_(clo_backtrace_size) = 12;
Int    VG_(clo_merge_recursive_frames) = 0; 
UInt   VG_(clo_sim_hints)      = 0;
Bool   VG_(clo_sym_offsets)    = False;
Bool   VG_(clo_read_inline_info) = False; 
Bool   VG_(clo_read_var_info)  = False;
XArray *VG_(clo_req_tsyms);  
Bool   VG_(clo_run_libc_freeres) = True;
Bool   VG_(clo_track_fds)      = False;
Bool   VG_(clo_show_below_main)= False;
Bool   VG_(clo_show_emwarns)   = False;
Word   VG_(clo_max_stackframe) = 2000000;
UInt   VG_(clo_max_threads)    = MAX_THREADS_DEFAULT;
Word   VG_(clo_main_stacksize) = 0; 
Word   VG_(clo_valgrind_stacksize) = VG_DEFAULT_STACK_ACTIVE_SZB;
Bool   VG_(clo_wait_for_gdb)   = False;
VgSmc  VG_(clo_smc_check)      = Vg_SmcStack;
UInt   VG_(clo_kernel_variant) = 0;
Bool   VG_(clo_dsymutil)       = False;
Bool   VG_(clo_sigill_diag)    = True;
UInt   VG_(clo_unw_stack_scan_thresh) = 0; 
UInt   VG_(clo_unw_stack_scan_frames) = 5;

#if defined(VGO_darwin)
UInt VG_(clo_resync_filter) = 1; 
#else
UInt VG_(clo_resync_filter) = 0; 
#endif



HChar* VG_(expand_file_name)(const HChar* option_name, const HChar* format)
{
   const HChar *base_dir;
   Int len, i = 0, j = 0;
   HChar* out;
   const HChar *message = NULL;

   base_dir = VG_(get_startup_wd)();

   if (VG_STREQ(format, "")) {
      
      message = "No filename given\n";
      goto bad;
   }
   
   
   
   
   
   if (format[0] == '~') {
      message = 
         "Filename begins with '~'\n"
         "You probably expected the shell to expand the '~', but it\n"
         "didn't.  The rules for '~'-expansion vary from shell to shell.\n"
         "You might have more luck using $HOME instead.\n";
      goto bad;
   }

   len = VG_(strlen)(format) + 1;
   out = VG_(malloc)( "options.efn.1", len );

#define ENSURE_THIS_MUCH_SPACE(x) \
   if (j + x >= len) { \
      len += (10 + x); \
      out = VG_(realloc)("options.efn.2(multiple)", out, len); \
   }

   while (format[i]) {
      if (format[i] != '%') {
         ENSURE_THIS_MUCH_SPACE(1);
         out[j++] = format[i++];
         
      } else {
         
         i++;
         if      ('%' == format[i]) {
            
            ENSURE_THIS_MUCH_SPACE(1);
            out[j++] = format[i++];
         }
         else if ('p' == format[i]) {
            
            
            Int pid = VG_(getpid)();
            ENSURE_THIS_MUCH_SPACE(10);
            j += VG_(sprintf)(&out[j], "%d", pid);
            i++;
         } 
         else if ('q' == format[i]) {
            i++;
            if ('{' == format[i]) {
               
               HChar *qual;
               Int begin_qualname = ++i;
               while (True) {
                  if (0 == format[i]) {
                     message = "Missing '}' in %q specifier\n";
                     goto bad;
                  } else if ('}' == format[i]) {
                     Int qualname_len = i - begin_qualname;
                     HChar qualname[qualname_len + 1];
                     VG_(strncpy)(qualname, format + begin_qualname,
                                  qualname_len);
                     qualname[qualname_len] = '\0';
                     qual = VG_(getenv)(qualname);
                     if (NULL == qual) {
                        
                        
                        HChar *str = VG_(malloc)("options.efn.3",
                                                 100 + qualname_len);
                        VG_(sprintf)(str,
                                     "Environment variable '%s' is not set\n",
                                     qualname);
                        message = str;
                        goto bad;
                     }
                     i++;
                     break;
                  }
                  i++;
               }
               ENSURE_THIS_MUCH_SPACE(VG_(strlen)(qual));
               j += VG_(sprintf)(&out[j], "%s", qual);
            } else {
               message = "Expected '{' after '%q'\n";
               goto bad;
            }
         } 
         else {
            
            message = "Expected 'p' or 'q' or '%' after '%'\n";
            goto bad;
         }
      }
   }
   ENSURE_THIS_MUCH_SPACE(1);
   out[j++] = 0;

   
   if (out[0] != '/') {
      len = VG_(strlen)(base_dir) + 1 + VG_(strlen)(out) + 1;

      HChar *absout = VG_(malloc)("options.efn.4", len);
      VG_(strcpy)(absout, base_dir);
      VG_(strcat)(absout, "/");
      VG_(strcat)(absout, out);
      VG_(free)(out);
      out = absout;
   }

   return out;

  bad: {
   vg_assert(message != NULL);
   
   HChar opt[VG_(strlen)(option_name) + VG_(strlen)(format) + 2];
   VG_(sprintf)(opt, "%s=%s", option_name, format);
   VG_(fmsg_bad_option)(opt, "%s", message);
  }
}


static HChar const* consume_commas ( HChar const* c ) {
   while (*c && *c == ',') {
      ++c;
   }
   return c;
}

static HChar const* consume_field ( HChar const* c ) {
   while (*c && *c != ',') {
      ++c;
   }
   return c;
}

Bool VG_(should_we_trace_this_child) ( const HChar* child_exe_name,
                                       const HChar** child_argv )
{
   
   
   
   if (child_exe_name == NULL || VG_(strlen)(child_exe_name) == 0)
      return VG_(clo_trace_children);  

   
   if (! VG_(clo_trace_children))
      return False;

   
   
   
   if (VG_(clo_trace_children_skip)) {
      HChar const* last = VG_(clo_trace_children_skip);
      HChar const* name = child_exe_name;
      while (*last) {
         Bool   matches;
         HChar* patt;
         HChar const* first = consume_commas(last);
         last = consume_field(first);
         if (first == last)
            break;
         vg_assert(last > first);
         patt = VG_(calloc)("m_options.swttc.1", last - first + 1, 1);
         VG_(memcpy)(patt, first, last - first);
         vg_assert(patt[last-first] == 0);
         matches = VG_(string_match)(patt, name);
         VG_(free)(patt);
         if (matches)
            return False;
      }
   }

   
   
   if (VG_(clo_trace_children_skip_by_arg) && child_argv != NULL) {
      HChar const* last = VG_(clo_trace_children_skip_by_arg);
      while (*last) {
         Int    i;
         Bool   matches;
         HChar* patt;
         HChar const* first = consume_commas(last);
         last = consume_field(first);
         if (first == last)
            break;
         vg_assert(last > first);
         patt = VG_(calloc)("m_options.swttc.1", last - first + 1, 1);
         VG_(memcpy)(patt, first, last - first);
         vg_assert(patt[last-first] == 0);
         for (i = 0; child_argv[i]; i++) {
            matches = VG_(string_match)(patt, child_argv[i]);
            if (matches) {
               VG_(free)(patt);
               return False;
            }
         }
         VG_(free)(patt);
      }
   }

   
   
   return True;
}


