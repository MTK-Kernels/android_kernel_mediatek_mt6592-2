

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

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_vkiscnums.h"
#include "pub_core_threadstate.h"
#include "pub_core_xarray.h"
#include "pub_core_clientstate.h"
#include "pub_core_aspacemgr.h"
#include "pub_core_aspacehl.h"
#include "pub_core_commandline.h"
#include "pub_core_debuglog.h"
#include "pub_core_errormgr.h"
#include "pub_core_execontext.h"
#include "pub_core_gdbserver.h"
#include "pub_core_initimg.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcfile.h"
#include "pub_core_libcprint.h"
#include "pub_core_libcproc.h"
#include "pub_core_libcsignal.h"
#include "pub_core_sbprofile.h"
#include "pub_core_syscall.h"       
#include "pub_core_mach.h"
#include "pub_core_machine.h"
#include "pub_core_mallocfree.h"
#include "pub_core_options.h"
#include "pub_core_debuginfo.h"
#include "pub_core_redir.h"
#include "pub_core_scheduler.h"
#include "pub_core_seqmatch.h"      
#include "pub_core_signals.h"
#include "pub_core_stacks.h"        
#include "pub_core_syswrap.h"
#include "pub_core_tooliface.h"
#include "pub_core_translate.h"     
#include "pub_core_trampoline.h"
#include "pub_core_transtab.h"
#include "pub_core_inner.h"
#if defined(ENABLE_INNER_CLIENT_REQUEST)
#include "pub_core_clreq.h"
#endif 




static void usage_NORETURN ( Bool debug_help )
{
   const HChar usage1[] = 
"usage: valgrind [options] prog-and-args\n"
"\n"
"  tool-selection option, with default in [ ]:\n"
"    --tool=<name>             use the Valgrind tool named <name> [memcheck]\n"
"\n"
"  basic user options for all Valgrind tools, with defaults in [ ]:\n"
"    -h --help                 show this message\n"
"    --help-debug              show this message, plus debugging options\n"
"    --version                 show version\n"
"    -q --quiet                run silently; only print error msgs\n"
"    -v --verbose              be more verbose -- show misc extra info\n"
"    --trace-children=no|yes   Valgrind-ise child processes (follow execve)? [no]\n"
"    --trace-children-skip=patt1,patt2,...    specifies a list of executables\n"
"                              that --trace-children=yes should not trace into\n"
"    --trace-children-skip-by-arg=patt1,patt2,...   same as --trace-children-skip=\n"
"                              but check the argv[] entries for children, rather\n"
"                              than the exe name, to make a follow/no-follow decision\n"
"    --child-silent-after-fork=no|yes omit child output between fork & exec? [no]\n"
"    --vgdb=no|yes|full        activate gdbserver? [yes]\n"
"                              full is slower but provides precise watchpoint/step\n"
"    --vgdb-error=<number>     invoke gdbserver after <number> errors [%d]\n"
"                              to get started quickly, use --vgdb-error=0\n"
"                              and follow the on-screen directions\n"
"    --vgdb-stop-at=event1,event2,... invoke gdbserver for given events [none]\n"
"         where event is one of:\n"
"           startup exit valgrindabexit all none\n"
"    --track-fds=no|yes        track open file descriptors? [no]\n"
"    --time-stamp=no|yes       add timestamps to log messages? [no]\n"
"    --log-fd=<number>         log messages to file descriptor [2=stderr]\n"
"    --log-file=<file>         log messages to <file>\n"
"    --log-socket=ipaddr:port  log messages to socket ipaddr:port\n"
"\n"
"  user options for Valgrind tools that report errors:\n"
"    --xml=yes                 emit error output in XML (some tools only)\n"
"    --xml-fd=<number>         XML output to file descriptor\n"
"    --xml-file=<file>         XML output to <file>\n"
"    --xml-socket=ipaddr:port  XML output to socket ipaddr:port\n"
"    --xml-user-comment=STR    copy STR verbatim into XML output\n"
"    --demangle=no|yes         automatically demangle C++ names? [yes]\n"
"    --num-callers=<number>    show <number> callers in stack traces [12]\n"
"    --error-limit=no|yes      stop showing new errors if too many? [yes]\n"
"    --error-exitcode=<number> exit code to return if errors found [0=disable]\n"
"    --error-markers=<begin>,<end> add lines with begin/end markers before/after\n"
"                              each error output in plain text mode [none]\n"
"    --show-below-main=no|yes  continue stack traces below main() [no]\n"
"    --default-suppressions=yes|no\n"
"                              load default suppressions [yes]\n"
"    --suppressions=<filename> suppress errors described in <filename>\n"
"    --gen-suppressions=no|yes|all    print suppressions for errors? [no]\n"
"    --db-attach=no|yes        start debugger when errors detected? [no]\n"
"                              Note: deprecated feature\n"
"    --db-command=<command>    command to start debugger [%s -nw %%f %%p]\n"
"    --input-fd=<number>       file descriptor for input [0=stdin]\n"
"    --dsymutil=no|yes         run dsymutil on Mac OS X when helpful? [no]\n"
"    --max-stackframe=<number> assume stack switch for SP changes larger\n"
"                              than <number> bytes [2000000]\n"
"    --main-stacksize=<number> set size of main thread's stack (in bytes)\n"
"                              [min(max(current 'ulimit' value,1MB),16MB)]\n"
"\n"
"  user options for Valgrind tools that replace malloc:\n"
"    --alignment=<number>      set minimum alignment of heap allocations [%s]\n"
"    --redzone-size=<number>   set minimum size of redzones added before/after\n"
"                              heap blocks (in bytes). [%s]\n"
"\n"
"  uncommon user options for all Valgrind tools:\n"
"    --fullpath-after=         (with nothing after the '=')\n"
"                              show full source paths in call stacks\n"
"    --fullpath-after=string   like --fullpath-after=, but only show the\n"
"                              part of the path after 'string'.  Allows removal\n"
"                              of path prefixes.  Use this flag multiple times\n"
"                              to specify a set of prefixes to remove.\n"
"    --extra-debuginfo-path=path    absolute path to search for additional\n"
"                              debug symbols, in addition to existing default\n"
"                              well known search paths.\n"
"    --debuginfo-server=ipaddr:port    also query this server\n"
"                              (valgrind-di-server) for debug symbols\n"
"    --allow-mismatched-debuginfo=no|yes  [no]\n"
"                              for the above two flags only, accept debuginfo\n"
"                              objects that don't \"match\" the main object\n"
"    --smc-check=none|stack|all|all-non-file [stack]\n"
"                              checks for self-modifying code: none, only for\n"
"                              code found in stacks, for all code, or for all\n"
"                              code except that from file-backed mappings\n"
"    --read-inline-info=yes|no read debug info about inlined function calls\n"
"                              and use it to do better stack traces.  [yes]\n"
"                              on Linux/Android for Memcheck/Helgrind/DRD\n"
"                              only.  [no] for all other tools and platforms.\n"
"    --read-var-info=yes|no    read debug info on stack and global variables\n"
"                              and use it to print better error messages in\n"
"                              tools that make use of it (Memcheck, Helgrind,\n"
"                              DRD) [no]\n"
"    --vgdb-poll=<number>      gdbserver poll max every <number> basic blocks [%d] \n"
"    --vgdb-shadow-registers=no|yes   let gdb see the shadow registers [no]\n"
"    --vgdb-prefix=<prefix>    prefix for vgdb FIFOs [%s]\n"
"    --run-libc-freeres=no|yes free up glibc memory at exit on Linux? [yes]\n"
"    --sim-hints=hint1,hint2,...  activate unusual sim behaviours [none] \n"
"         where hint is one of:\n"
"           lax-ioctls fuse-compatible enable-outer\n"
"           no-inner-prefix no-nptl-pthread-stackcache none\n"
"    --fair-sched=no|yes|try   schedule threads fairly on multicore systems [no]\n"
"    --kernel-variant=variant1,variant2,...\n"
"         handle non-standard kernel variants [none]\n"
"         where variant is one of:\n"
"           bproc android-no-hw-tls\n"
"           android-gpu-sgx5xx android-gpu-adreno3xx none\n"
"    --merge-recursive-frames=<number>  merge frames between identical\n"
"           program counters in max <number> frames) [0]\n"
"    --num-transtab-sectors=<number> size of translated code cache [%d]\n"
"           more sectors may increase performance, but use more memory.\n"
"    --avg-transtab-entry-size=<number> avg size in bytes of a translated\n"
"           basic block [0, meaning use tool provided default]\n"
"    --aspace-minaddr=0xPP     avoid mapping memory below 0xPP [guessed]\n"
"    --valgrind-stacksize=<number> size of valgrind (host) thread's stack\n"
"                               (in bytes) ["
                                VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB) 
                                                "]\n"
"    --show-emwarns=no|yes     show warnings about emulation limits? [no]\n"
"    --require-text-symbol=:sonamepattern:symbolpattern    abort run if the\n"
"                              stated shared object doesn't have the stated\n"
"                              text symbol.  Patterns can contain ? and *.\n"
"    --soname-synonyms=syn1=pattern1,syn2=pattern2,... synonym soname\n"
"              specify patterns for function wrapping or replacement.\n"
"              To use a non-libc malloc library that is\n"
"                  in the main exe:  --soname-synonyms=somalloc=NONE\n"
"                  in libxyzzy.so:   --soname-synonyms=somalloc=libxyzzy.so\n"
"    --sigill-diagnostics=yes|no  warn about illegal instructions? [yes]\n"
"    --unw-stack-scan-thresh=<number>   Enable stack-scan unwind if fewer\n"
"                  than <number> good frames found  [0, meaning \"disabled\"]\n"
"                  NOTE: stack scanning is only available on arm-linux.\n"
"    --unw-stack-scan-frames=<number>   Max number of frames that can be\n"
"                  recovered by stack scanning [5]\n"
"    --resync-filter=no|yes|verbose [yes on MacOS, no on other OSes]\n"
"              attempt to avoid expensive address-space-resync operations\n"
"    --max-threads=<number>    maximum number of threads that valgrind can\n"
"                              handle [%d]\n"
"\n";

   const HChar usage2[] = 
"\n"
"  debugging options for all Valgrind tools:\n"
"    -d                        show verbose debugging output\n"
"    --stats=no|yes            show tool and core statistics [no]\n"
"    --sanity-level=<number>   level of sanity checking to do [1]\n"
"    --trace-flags=<XXXXXXXX>   show generated code? (X = 0|1) [00000000]\n"
"    --profile-flags=<XXXXXXXX> ditto, but for profiling (X = 0|1) [00000000]\n"
"    --profile-interval=<number> show profile every <number> event checks\n"
"                                [0, meaning only at the end of the run]\n"
"    --trace-notbelow=<number> only show BBs above <number> [999999999]\n"
"    --trace-notabove=<number> only show BBs below <number> [0]\n"
"    --trace-syscalls=no|yes   show all system calls? [no]\n"
"    --trace-signals=no|yes    show signal handling details? [no]\n"
"    --trace-symtab=no|yes     show symbol table details? [no]\n"
"    --trace-symtab-patt=<patt> limit debuginfo tracing to obj name <patt>\n"
"    --trace-cfi=no|yes        show call-frame-info details? [no]\n"
"    --debug-dump=syms         mimic /usr/bin/readelf --syms\n"
"    --debug-dump=line         mimic /usr/bin/readelf --debug-dump=line\n"
"    --debug-dump=frames       mimic /usr/bin/readelf --debug-dump=frames\n"
"    --trace-redir=no|yes      show redirection details? [no]\n"
"    --trace-sched=no|yes      show thread scheduler details? [no]\n"
"    --profile-heap=no|yes     profile Valgrind's own space use\n"
"    --core-redzone-size=<number>  set minimum size of redzones added before/after\n"
"                              heap blocks allocated for Valgrind internal use (in bytes) [4]\n"
"    --wait-for-gdb=yes|no     pause on startup to wait for gdb attach\n"
"    --sym-offsets=yes|no      show syms in form 'name+offset' ? [no]\n"
"    --command-line-only=no|yes  only use command line options [no]\n"
"\n"
"  Vex options for all Valgrind tools:\n"
"    --vex-iropt-verbosity=<0..9>           [0]\n"
"    --vex-iropt-level=<0..2>               [2]\n"
"    --vex-iropt-unroll-thresh=<0..400>     [120]\n"
"    --vex-guest-max-insns=<1..100>         [50]\n"
"    --vex-guest-chase-thresh=<0..99>       [10]\n"
"    --vex-guest-chase-cond=no|yes          [no]\n"
"    Precise exception control.  Possible values for 'mode' are as follows\n"
"      and specify the minimum set of registers guaranteed to be correct\n"
"      immediately prior to memory access instructions:\n"
"         sp-at-mem-access          stack pointer only\n"
"         unwindregs-at-mem-access  registers needed for stack unwinding\n"
"         allregs-at-mem-access     all registers\n"
"         allregs-at-each-insn      all registers are always correct\n"
"      Default value for all 3 following flags is [unwindregs-at-mem-access].\n"
"      --vex-iropt-register-updates=mode   setting to use by default\n"
"      --px-default=mode      synonym for --vex-iropt-register-updates\n"
"      --px-file-backed=mode  optional setting for file-backed (non-JIT) code\n"
"    Tracing and profile control:\n"
"      --trace-flags and --profile-flags values (omit the middle space):\n"
"         1000 0000   show conversion into IR\n"
"         0100 0000   show after initial opt\n"
"         0010 0000   show after instrumentation\n"
"         0001 0000   show after second opt\n"
"         0000 1000   show after tree building\n"
"         0000 0100   show selecting insns\n"
"         0000 0010   show after reg-alloc\n"
"         0000 0001   show final assembly\n"
"         0000 0000   show summary profile only\n"
"        (Nb: you need --trace-notbelow and/or --trace-notabove\n"
"             with --trace-flags for full details)\n"
"\n"
"  debugging options for Valgrind tools that report errors\n"
"    --dump-error=<number>     show translation for basic block associated\n"
"                              with <number>'th error context [0=show none]\n"
"\n"
"  debugging options for Valgrind tools that replace malloc:\n"
"    --trace-malloc=no|yes     show client malloc details? [no]\n"
"\n";

   const HChar usage3[] =
"\n"
"  Extra options read from ~/.valgrindrc, $VALGRIND_OPTS, ./.valgrindrc\n"
"\n"
"  %s is %s\n"
"  Valgrind is Copyright (C) 2000-2013, and GNU GPL'd, by Julian Seward et al.\n"
"  LibVEX is Copyright (C) 2004-2013, and GNU GPL'd, by OpenWorks LLP et al.\n"
"\n"
"  Bug reports, feedback, admiration, abuse, etc, to: %s.\n"
"\n";

   const HChar* gdb_path = GDB_PATH;
   HChar default_alignment[30];      
   HChar default_redzone_size[30];   

   
   VG_(log_output_sink).fd = 1;
   VG_(log_output_sink).is_socket = False;

   if (VG_(needs).malloc_replacement) {
      VG_(sprintf)(default_alignment,    "%d",  VG_MIN_MALLOC_SZB);
      VG_(sprintf)(default_redzone_size, "%lu", VG_(tdict).tool_client_redzone_szB);
   } else {
      VG_(strcpy)(default_alignment,    "not used by this tool");
      VG_(strcpy)(default_redzone_size, "not used by this tool");
   }
   
   VG_(printf)(usage1, 
               VG_(clo_vgdb_error)        ,
               gdb_path                   ,
               default_alignment          ,
               default_redzone_size       ,
               VG_(clo_vgdb_poll)         ,
               VG_(vgdb_prefix_default)() ,
               N_SECTORS_DEFAULT          ,
               MAX_THREADS_DEFAULT        
               ); 
   if (VG_(details).name) {
      VG_(printf)("  user options for %s:\n", VG_(details).name);
      if (VG_(needs).command_line_options)
	 VG_TDICT_CALL(tool_print_usage);
      else
	 VG_(printf)("    (none)\n");
   }
   if (debug_help) {
      VG_(printf)("%s", usage2);

      if (VG_(details).name) {
         VG_(printf)("  debugging options for %s:\n", VG_(details).name);
      
         if (VG_(needs).command_line_options)
            VG_TDICT_CALL(tool_print_debug_usage);
         else
            VG_(printf)("    (none)\n");
      }
   }
   VG_(printf)(usage3, VG_(details).name, VG_(details).copyright_author,
               VG_BUGS_TO);
   VG_(exit)(0);
}


static void early_process_cmd_line_options ( Int* need_help,
                                             const HChar** tool )
{
   UInt   i;
   HChar* str;

   vg_assert( VG_(args_for_valgrind) );

   
   for (i = 0; i < VG_(sizeXA)( VG_(args_for_valgrind) ); i++) {

      str = * (HChar**) VG_(indexXA)( VG_(args_for_valgrind), i );
      vg_assert(str);

      
      if VG_XACT_CLO(str, "--version", VG_(log_output_sink).fd, 1) {
         VG_(log_output_sink).is_socket = False;
         VG_(printf)("valgrind-" VERSION "\n");
         VG_(exit)(0);
      }
      else if VG_XACT_CLO(str, "--help", *need_help, *need_help+1) {}
      else if VG_XACT_CLO(str, "-h",     *need_help, *need_help+1) {}

      else if VG_XACT_CLO(str, "--help-debug", *need_help, *need_help+2) {}

      
      
      else if VG_STR_CLO(str, "--tool", *tool) {} 

      
      
      
      else if VG_INT_CLO(str, "--max-stackframe", VG_(clo_max_stackframe)) {}
      else if VG_INT_CLO(str, "--main-stacksize", VG_(clo_main_stacksize)) {}

      
      else if VG_INT_CLO(str, "--max-threads", VG_(clo_max_threads)) {}

      
      
      
      else if VG_USETX_CLO (str, "--sim-hints",
                            "lax-ioctls,fuse-compatible,"
                            "enable-outer,no-inner-prefix,"
                            "no-nptl-pthread-stackcache",
                            VG_(clo_sim_hints)) {}
   }

   
   VG_N_THREADS = VG_(clo_max_threads);
}

static
void main_process_cmd_line_options ( Bool* logging_to_fd,
                                     const HChar** xml_fname_unexpanded,
                                     const HChar* toolname )
{
   
   
   
   SysRes sres;
   Int    i, tmp_log_fd, tmp_xml_fd;
   Int    toolname_len = VG_(strlen)(toolname);
   const HChar* tmp_str;         
   enum {
      VgLogTo_Fd,
      VgLogTo_File,
      VgLogTo_Socket
   } log_to = VgLogTo_Fd,   
     xml_to = VgLogTo_Fd;   

   const HChar* log_fsname_unexpanded = NULL;
   const HChar* xml_fsname_unexpanded = NULL;

   Bool sigill_diag_set = False;

   tmp_log_fd = 2; 
   tmp_xml_fd = -1;
 
   
   if (VG_LIBDIR[0] != '/') 
      VG_(err_config_error)("Please use absolute paths in "
                            "./configure --prefix=... or --libdir=...\n");

   vg_assert( VG_(args_for_valgrind) );

   VG_(clo_suppressions) = VG_(newXA)(VG_(malloc), "main.mpclo.4",
                                      VG_(free), sizeof(HChar *));
   VG_(clo_fullpath_after) = VG_(newXA)(VG_(malloc), "main.mpclo.5",
                                        VG_(free), sizeof(HChar *));
   VG_(clo_req_tsyms) = VG_(newXA)(VG_(malloc), "main.mpclo.6",
                                   VG_(free), sizeof(HChar *));

   
   const HChar* pxStrings[5]
      = { "sp-at-mem-access",      "unwindregs-at-mem-access",
          "allregs-at-mem-access", "allregs-at-each-insn", NULL };
   const VexRegisterUpdates pxVals[5]
      = { VexRegUpdSpAtMemAccess,      VexRegUpdUnwindregsAtMemAccess,
          VexRegUpdAllregsAtMemAccess, VexRegUpdAllregsAtEachInsn, 0 };

   

   for (i = 0; i < VG_(sizeXA)( VG_(args_for_valgrind) ); i++) {

      HChar* arg   = * (HChar**) VG_(indexXA)( VG_(args_for_valgrind), i );
      HChar* colon = arg;
      UInt   ix    = 0;

      
      while (*colon && *colon != ':' && *colon != '=')
         colon++;

      
      
      
      if (*colon == ':') {
         if (VG_STREQN(2,            arg,                "--") && 
             VG_STREQN(toolname_len, arg+2,              toolname) &&
             VG_STREQN(1,            arg+2+toolname_len, ":"))
         {
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            if (0)
               VG_(printf)("tool-specific arg: %s\n", arg);
            arg = VG_(strdup)("main.mpclo.1", arg + toolname_len + 1);
            arg[0] = '-';
            arg[1] = '-';

         } else {
            
            continue;
         }
      }
      
      
      if      VG_STREQN( 7, arg, "--tool=")              {}
      else if VG_STREQN(20, arg, "--command-line-only=") {}
      else if VG_STREQ(     arg, "--")                   {}
      else if VG_STREQ(     arg, "-d")                   {}
      else if VG_STREQN(17, arg, "--max-stackframe=")    {}
      else if VG_STREQN(17, arg, "--main-stacksize=")    {}
      else if VG_STREQN(14, arg, "--max-threads=")       {}
      else if VG_STREQN(12, arg, "--sim-hints=")         {}
      else if VG_STREQN(15, arg, "--profile-heap=")      {}
      else if VG_STREQN(20, arg, "--core-redzone-size=") {}
      else if VG_STREQN(15, arg, "--redzone-size=")      {}
      else if VG_STREQN(17, arg, "--aspace-minaddr=")    {}

      else if VG_BINT_CLO(arg, "--valgrind-stacksize",
                          VG_(clo_valgrind_stacksize), 
                          2*VKI_PAGE_SIZE, 10*VG_DEFAULT_STACK_ACTIVE_SZB)
                            {VG_(clo_valgrind_stacksize) 
                                  = VG_PGROUNDUP(VG_(clo_valgrind_stacksize));}

      
      else if VG_STREQN(34, arg, "--vex-iropt-precise-memory-exns=no") {
         VG_(fmsg_bad_option)
            (arg,
             "--vex-iropt-precise-memory-exns is obsolete\n"
             "Use --vex-iropt-register-updates=unwindregs-at-mem-access instead\n");
      }
      else if VG_STREQN(35, arg, "--vex-iropt-precise-memory-exns=yes") {
         VG_(fmsg_bad_option)
            (arg,
             "--vex-iropt-precise-memory-exns is obsolete\n"
             "Use --vex-iropt-register-updates=allregs-at-mem-access instead\n"
             " (or --vex-iropt-register-updates=allregs-at-each-insn)\n");
      }

      
      else if (VG_STREQ(arg, "-v") ||
               VG_STREQ(arg, "--verbose"))
         VG_(clo_verbosity)++;

      else if (VG_STREQ(arg, "-q") ||
               VG_STREQ(arg, "--quiet"))
         VG_(clo_verbosity)--;

      else if VG_BOOL_CLO(arg, "--sigill-diagnostics", VG_(clo_sigill_diag))
         sigill_diag_set = True;

      else if VG_BOOL_CLO(arg, "--stats",          VG_(clo_stats)) {}
      else if VG_BOOL_CLO(arg, "--xml",            VG_(clo_xml))
         VG_(debugLog_setXml)(VG_(clo_xml));

      else if VG_XACT_CLO(arg, "--vgdb=no",        VG_(clo_vgdb), Vg_VgdbNo) {}
      else if VG_XACT_CLO(arg, "--vgdb=yes",       VG_(clo_vgdb), Vg_VgdbYes) {}
      else if VG_XACT_CLO(arg, "--vgdb=full",      VG_(clo_vgdb), Vg_VgdbFull) {
         VG_(clo_vex_control).iropt_register_updates_default
            = VG_(clo_px_file_backed)
            = VexRegUpdAllregsAtEachInsn;
      }
      else if VG_INT_CLO (arg, "--vgdb-poll",      VG_(clo_vgdb_poll)) {}
      else if VG_INT_CLO (arg, "--vgdb-error",     VG_(clo_vgdb_error)) {}
      else if VG_USET_CLO (arg, "--vgdb-stop-at",
                           "startup,exit,valgrindabexit",
                           VG_(clo_vgdb_stop_at)) {}
      else if VG_STR_CLO (arg, "--vgdb-prefix",    VG_(clo_vgdb_prefix)) {
         VG_(arg_vgdb_prefix) = arg;
      }
      else if VG_BOOL_CLO(arg, "--vgdb-shadow-registers",
                            VG_(clo_vgdb_shadow_registers)) {}
      else if VG_BOOL_CLO(arg, "--db-attach",      VG_(clo_db_attach)) {}
      else if VG_BOOL_CLO(arg, "--demangle",       VG_(clo_demangle)) {}
      else if VG_STR_CLO (arg, "--soname-synonyms",VG_(clo_soname_synonyms)) {}
      else if VG_BOOL_CLO(arg, "--error-limit",    VG_(clo_error_limit)) {}
      else if VG_INT_CLO (arg, "--error-exitcode", VG_(clo_error_exitcode)) {}
      else if VG_STR_CLO (arg, "--error-markers",  tmp_str) {
         Int m;
         const HChar *startpos = tmp_str;
         const HChar *nextpos;
         for (m = 0; 
              m < sizeof(VG_(clo_error_markers))
                 /sizeof(VG_(clo_error_markers)[0]);
              m++) {
            
            VG_(free)(VG_(clo_error_markers)[m]);
            VG_(clo_error_markers)[m] = NULL;

            nextpos = VG_(strchr)(startpos, ',');
            if (!nextpos)
               nextpos = startpos + VG_(strlen)(startpos);
            if (startpos != nextpos) {
               VG_(clo_error_markers)[m] 
                  = VG_(malloc)("main.mpclo.2", nextpos - startpos + 1);
               VG_(memcpy)(VG_(clo_error_markers)[m], startpos, 
                           nextpos - startpos);
               VG_(clo_error_markers)[m][nextpos - startpos] = '\0';
            }
            startpos = *nextpos ? nextpos + 1 : nextpos;
         }
      }
      else if VG_BOOL_CLO(arg, "--show-emwarns",   VG_(clo_show_emwarns)) {}

      else if VG_BOOL_CLO(arg, "--run-libc-freeres", VG_(clo_run_libc_freeres)) {}
      else if VG_BOOL_CLO(arg, "--show-below-main",  VG_(clo_show_below_main)) {}
      else if VG_BOOL_CLO(arg, "--time-stamp",       VG_(clo_time_stamp)) {}
      else if VG_BOOL_CLO(arg, "--track-fds",        VG_(clo_track_fds)) {}
      else if VG_BOOL_CLO(arg, "--trace-children",   VG_(clo_trace_children)) {}
      else if VG_BOOL_CLO(arg, "--child-silent-after-fork",
                            VG_(clo_child_silent_after_fork)) {}
      else if VG_STR_CLO(arg, "--fair-sched",        tmp_str) {
         if (VG_(strcmp)(tmp_str, "yes") == 0)
            VG_(clo_fair_sched) = enable_fair_sched;
         else if (VG_(strcmp)(tmp_str, "try") == 0)
            VG_(clo_fair_sched) = try_fair_sched;
         else if (VG_(strcmp)(tmp_str, "no") == 0)
            VG_(clo_fair_sched) = disable_fair_sched;
         else
            VG_(fmsg_bad_option)(arg,
               "Bad argument, should be 'yes', 'try' or 'no'\n");
      }
      else if VG_BOOL_CLO(arg, "--trace-sched",      VG_(clo_trace_sched)) {}
      else if VG_BOOL_CLO(arg, "--trace-signals",    VG_(clo_trace_signals)) {}
      else if VG_BOOL_CLO(arg, "--trace-symtab",     VG_(clo_trace_symtab)) {}
      else if VG_STR_CLO (arg, "--trace-symtab-patt", VG_(clo_trace_symtab_patt)) {}
      else if VG_BOOL_CLO(arg, "--trace-cfi",        VG_(clo_trace_cfi)) {}
      else if VG_XACT_CLO(arg, "--debug-dump=syms",  VG_(clo_debug_dump_syms),
                                                     True) {}
      else if VG_XACT_CLO(arg, "--debug-dump=line",  VG_(clo_debug_dump_line),
                                                     True) {}
      else if VG_XACT_CLO(arg, "--debug-dump=frames",
                               VG_(clo_debug_dump_frames), True) {}
      else if VG_BOOL_CLO(arg, "--trace-redir",      VG_(clo_trace_redir)) {}

      else if VG_BOOL_CLO(arg, "--trace-syscalls",   VG_(clo_trace_syscalls)) {}
      else if VG_BOOL_CLO(arg, "--wait-for-gdb",     VG_(clo_wait_for_gdb)) {}
      else if VG_STR_CLO (arg, "--db-command",       VG_(clo_db_command)) {}
      else if VG_BOOL_CLO(arg, "--sym-offsets",      VG_(clo_sym_offsets)) {}
      else if VG_BOOL_CLO(arg, "--read-inline-info", VG_(clo_read_inline_info)) {}
      else if VG_BOOL_CLO(arg, "--read-var-info",    VG_(clo_read_var_info)) {}

      else if VG_INT_CLO (arg, "--dump-error",       VG_(clo_dump_error))   {}
      else if VG_INT_CLO (arg, "--input-fd",         VG_(clo_input_fd))     {}
      else if VG_INT_CLO (arg, "--sanity-level",     VG_(clo_sanity_level)) {}
      else if VG_BINT_CLO(arg, "--num-callers",      VG_(clo_backtrace_size), 1,
                                                     VG_DEEPEST_BACKTRACE) {}
      else if VG_BINT_CLO(arg, "--num-transtab-sectors",
                               VG_(clo_num_transtab_sectors),
                               MIN_N_SECTORS, MAX_N_SECTORS) {}
      else if VG_BINT_CLO(arg, "--avg-transtab-entry-size",
                               VG_(clo_avg_transtab_entry_size),
                               50, 5000) {}
      else if VG_BINT_CLO(arg, "--merge-recursive-frames",
                               VG_(clo_merge_recursive_frames), 0,
                               VG_DEEPEST_BACKTRACE) {}

      else if VG_XACT_CLO(arg, "--smc-check=none", 
                          VG_(clo_smc_check), Vg_SmcNone) {}
      else if VG_XACT_CLO(arg, "--smc-check=stack",
                          VG_(clo_smc_check), Vg_SmcStack) {}
      else if VG_XACT_CLO(arg, "--smc-check=all",
                          VG_(clo_smc_check), Vg_SmcAll) {}
      else if VG_XACT_CLO(arg, "--smc-check=all-non-file",
                          VG_(clo_smc_check), Vg_SmcAllNonFile) {}

      else if VG_USETX_CLO (arg, "--kernel-variant",
                            "bproc,"
                            "android-no-hw-tls,"
                            "android-gpu-sgx5xx,"
                            "android-gpu-adreno3xx",
                            VG_(clo_kernel_variant)) {}

      else if VG_BOOL_CLO(arg, "--dsymutil",        VG_(clo_dsymutil)) {}

      else if VG_STR_CLO (arg, "--trace-children-skip",
                               VG_(clo_trace_children_skip)) {}
      else if VG_STR_CLO (arg, "--trace-children-skip-by-arg",
                               VG_(clo_trace_children_skip_by_arg)) {}

      else if VG_BINT_CLO(arg, "--vex-iropt-verbosity",
                       VG_(clo_vex_control).iropt_verbosity, 0, 10) {}
      else if VG_BINT_CLO(arg, "--vex-iropt-level",
                       VG_(clo_vex_control).iropt_level, 0, 2) {}

      else if VG_STRINDEX_CLO(arg, "--vex-iropt-register-updates",
                                   pxStrings, ix) {
         vg_assert(ix < 4);
         vg_assert(pxVals[ix] >= VexRegUpdSpAtMemAccess);
         vg_assert(pxVals[ix] <= VexRegUpdAllregsAtEachInsn);
         VG_(clo_vex_control).iropt_register_updates_default = pxVals[ix];
      }
      else if VG_STRINDEX_CLO(arg, "--px-default", pxStrings, ix) {
         
         
         vg_assert(ix < 4);
         vg_assert(pxVals[ix] >= VexRegUpdSpAtMemAccess);
         vg_assert(pxVals[ix] <= VexRegUpdAllregsAtEachInsn);
         VG_(clo_vex_control).iropt_register_updates_default = pxVals[ix];
      }
      else if VG_STRINDEX_CLO(arg, "--px-file-backed", pxStrings, ix) {
         
         
         vg_assert(ix < 4);
         vg_assert(pxVals[ix] >= VexRegUpdSpAtMemAccess);
         vg_assert(pxVals[ix] <= VexRegUpdAllregsAtEachInsn);
         VG_(clo_px_file_backed) = pxVals[ix];
      }

      else if VG_BINT_CLO(arg, "--vex-iropt-unroll-thresh",
                       VG_(clo_vex_control).iropt_unroll_thresh, 0, 400) {}
      else if VG_BINT_CLO(arg, "--vex-guest-max-insns",
                       VG_(clo_vex_control).guest_max_insns, 1, 100) {}
      else if VG_BINT_CLO(arg, "--vex-guest-chase-thresh",
                       VG_(clo_vex_control).guest_chase_thresh, 0, 99) {}
      else if VG_BOOL_CLO(arg, "--vex-guest-chase-cond",
                       VG_(clo_vex_control).guest_chase_cond) {}

      else if VG_INT_CLO(arg, "--log-fd", tmp_log_fd) {
         log_to = VgLogTo_Fd;
         log_fsname_unexpanded = NULL;
      }
      else if VG_INT_CLO(arg, "--xml-fd", tmp_xml_fd) {
         xml_to = VgLogTo_Fd;
         xml_fsname_unexpanded = NULL;
      }

      else if VG_STR_CLO(arg, "--log-file", log_fsname_unexpanded) {
         log_to = VgLogTo_File;
      }
      else if VG_STR_CLO(arg, "--xml-file", xml_fsname_unexpanded) {
         xml_to = VgLogTo_File;
      }
 
      else if VG_STR_CLO(arg, "--log-socket", log_fsname_unexpanded) {
         log_to = VgLogTo_Socket;
      }
      else if VG_STR_CLO(arg, "--xml-socket", xml_fsname_unexpanded) {
         xml_to = VgLogTo_Socket;
      }

      else if VG_STR_CLO(arg, "--debuginfo-server",
                              VG_(clo_debuginfo_server)) {}

      else if VG_BOOL_CLO(arg, "--allow-mismatched-debuginfo",
                               VG_(clo_allow_mismatched_debuginfo)) {}

      else if VG_STR_CLO(arg, "--xml-user-comment",
                              VG_(clo_xml_user_comment)) {}

      else if VG_BOOL_CLO(arg, "--default-suppressions",
                          VG_(clo_default_supp)) {}

      else if VG_STR_CLO(arg, "--suppressions", tmp_str) {
         VG_(addToXA)(VG_(clo_suppressions), &tmp_str);
      }

      else if VG_STR_CLO (arg, "--fullpath-after", tmp_str) {
         VG_(addToXA)(VG_(clo_fullpath_after), &tmp_str);
      }

      else if VG_STR_CLO (arg, "--extra-debuginfo-path",
                      VG_(clo_extra_debuginfo_path)) {}

      else if VG_STR_CLO(arg, "--require-text-symbol", tmp_str) {
         HChar patt[7];
         Bool ok = True;
         ok = tmp_str && VG_(strlen)(tmp_str) > 0;
         if (ok) {
           patt[0] = patt[3] = tmp_str[0];
           patt[1] = patt[4] = '?';
           patt[2] = patt[5] = '*';
           patt[6] = 0;
           ok = VG_(string_match)(patt, tmp_str);
         }
         if (!ok) {
            VG_(fmsg_bad_option)(arg,
               "Invalid --require-text-symbol= specification.\n");
         }
         VG_(addToXA)(VG_(clo_req_tsyms), &tmp_str);
      }

      
      else if VG_STR_CLO(arg, "--trace-flags", tmp_str) {
         Int j;
         if (8 != VG_(strlen)(tmp_str)) {
            VG_(fmsg_bad_option)(arg,
               "--trace-flags argument must have 8 digits\n");
         }
         for (j = 0; j < 8; j++) {
            if      ('0' == tmp_str[j]) {  }
            else if ('1' == tmp_str[j]) VG_(clo_trace_flags) |= (1 << (7-j));
            else {
               VG_(fmsg_bad_option)(arg,
                  "--trace-flags argument can only contain 0s and 1s\n");
            }
         }
      }

      else if VG_INT_CLO (arg, "--trace-notbelow", VG_(clo_trace_notbelow)) {}

      else if VG_INT_CLO (arg, "--trace-notabove", VG_(clo_trace_notabove)) {}

      
      else if VG_STR_CLO(arg, "--profile-flags", tmp_str) {
         Int j;
         if (8 != VG_(strlen)(tmp_str)) {
            VG_(fmsg_bad_option)(arg, 
               "--profile-flags argument must have 8 digits\n");
         }
         for (j = 0; j < 8; j++) {
            if      ('0' == tmp_str[j]) {  }
            else if ('1' == tmp_str[j]) VG_(clo_profyle_flags) |= (1 << (7-j));
            else {
               VG_(fmsg_bad_option)(arg,
                  "--profile-flags argument can only contain 0s and 1s\n");
            }
         }
         VG_(clo_profyle_sbs) = True;
      }

      else if VG_INT_CLO (arg, "--profile-interval",
                          VG_(clo_profyle_interval)) {}

      else if VG_XACT_CLO(arg, "--gen-suppressions=no",
                               VG_(clo_gen_suppressions), 0) {}
      else if VG_XACT_CLO(arg, "--gen-suppressions=yes",
                               VG_(clo_gen_suppressions), 1) {}
      else if VG_XACT_CLO(arg, "--gen-suppressions=all",
                               VG_(clo_gen_suppressions), 2) {}

      else if VG_BINT_CLO(arg, "--unw-stack-scan-thresh",
                          VG_(clo_unw_stack_scan_thresh), 0, 100) {}
      else if VG_BINT_CLO(arg, "--unw-stack-scan-frames",
                          VG_(clo_unw_stack_scan_frames), 0, 32) {}

      else if VG_XACT_CLO(arg, "--resync-filter=no",
                               VG_(clo_resync_filter), 0) {}
      else if VG_XACT_CLO(arg, "--resync-filter=yes",
                               VG_(clo_resync_filter), 1) {}
      else if VG_XACT_CLO(arg, "--resync-filter=verbose",
                               VG_(clo_resync_filter), 2) {}

      else if ( ! VG_(needs).command_line_options
             || ! VG_TDICT_CALL(tool_process_cmd_line_option, arg) ) {
         VG_(fmsg_unknown_option)(arg);
      }
   }

   

   
   if (VG_(clo_db_attach))
      VG_(umsg)
         ("\nWarning: --db-attach is a deprecated feature which will be\n"
          "   removed in the next release. Use --vgdb-error=1 instead\n\n");

   
   if (VG_(clo_vgdb_prefix) == NULL)
     VG_(clo_vgdb_prefix) = VG_(vgdb_prefix_default)();

   

   if (VG_(clo_vex_control).guest_chase_thresh
       >= VG_(clo_vex_control).guest_max_insns)
      VG_(clo_vex_control).guest_chase_thresh
         = VG_(clo_vex_control).guest_max_insns - 1;

   if (VG_(clo_vex_control).guest_chase_thresh < 0)
      VG_(clo_vex_control).guest_chase_thresh = 0;

   

   if (VG_(clo_verbosity) < 0)
      VG_(clo_verbosity) = 0;

   if (!sigill_diag_set)
      VG_(clo_sigill_diag) = (VG_(clo_verbosity) > 0);

   if (VG_(clo_trace_notbelow) == -1) {
     if (VG_(clo_trace_notabove) == -1) {
       
       VG_(clo_trace_notbelow) = 2147483647;
       VG_(clo_trace_notabove) = 0;
     } else {
       
       VG_(clo_trace_notbelow) = 0;
     }
   } else {
     if (VG_(clo_trace_notabove) == -1) {
       
       VG_(clo_trace_notabove) = 2147483647;
     } else {
       
     }
   }

   VG_(dyn_vgdb_error) = VG_(clo_vgdb_error);

   if (VG_(clo_gen_suppressions) > 0 && 
       !VG_(needs).core_errors && !VG_(needs).tool_errors) {
      VG_(fmsg_bad_option)("--gen-suppressions=yes",
         "Can't use --gen-suppressions= with %s\n"
         "because it doesn't generate errors.\n", VG_(details).name);
   }

#  if !defined(VGO_darwin)
   if (VG_(clo_resync_filter) != 0) {
      VG_(fmsg_bad_option)("--resync-filter=yes or =verbose", 
                           "--resync-filter= is only available on MacOS X.\n");
      
   }
#  endif

   if (VG_(clo_xml) && !VG_(needs).xml_output) {
      VG_(clo_xml) = False;
      VG_(fmsg_bad_option)("--xml=yes",
         "%s does not support XML output.\n", VG_(details).name); 
      
   }

   vg_assert( VG_(clo_gen_suppressions) >= 0 );
   vg_assert( VG_(clo_gen_suppressions) <= 2 );

   if (VG_(clo_xml)) {

      if (VG_(clo_gen_suppressions) == 1) {
         VG_(fmsg_bad_option)(
            "--xml=yes together with --gen-suppressions=yes",
            "When --xml=yes is specified, --gen-suppressions=no\n"
            "or --gen-suppressions=all is allowed, but not "
            "--gen-suppressions=yes.\n");
      }

      if (VG_(clo_db_attach)) {
         VG_(fmsg_bad_option)(
            "--xml=yes together with --db-attach=yes",
            "--db-attach=yes is not allowed with --xml=yes\n"
            "because it would require user input.\n");
      }

      if (VG_(clo_dump_error) > 0) {
         VG_(fmsg_bad_option)("--xml=yes",
            "Cannot be used together with --dump-error");
      }

      
      VG_(clo_error_limit) = False;
      

   }


   vg_assert(VG_(log_output_sink).fd == 2 );
   vg_assert(VG_(log_output_sink).is_socket == False);
   vg_assert(VG_(clo_log_fname_expanded) == NULL);

   vg_assert(VG_(xml_output_sink).fd == -1 );
   vg_assert(VG_(xml_output_sink).is_socket == False);
   vg_assert(VG_(clo_xml_fname_expanded) == NULL);

   

   switch (log_to) {

      case VgLogTo_Fd: 
         vg_assert(log_fsname_unexpanded == NULL);
         break;

      case VgLogTo_File: {
         HChar* logfilename;

         vg_assert(log_fsname_unexpanded != NULL);
         vg_assert(VG_(strlen)(log_fsname_unexpanded) <= 900); 

         
         
         logfilename = VG_(expand_file_name)("--log-file",
                                             log_fsname_unexpanded);
         sres = VG_(open)(logfilename, 
                          VKI_O_CREAT|VKI_O_WRONLY|VKI_O_TRUNC, 
                          VKI_S_IRUSR|VKI_S_IWUSR|VKI_S_IRGRP|VKI_S_IROTH);
         if (!sr_isError(sres)) {
            tmp_log_fd = sr_Res(sres);
            VG_(clo_log_fname_expanded) = logfilename;
         } else {
            VG_(fmsg)("can't create log file '%s': %s\n", 
                      logfilename, VG_(strerror)(sr_Err(sres)));
            VG_(exit)(1);
            
         }
         break;
      }

      case VgLogTo_Socket: {
         vg_assert(log_fsname_unexpanded != NULL);
         vg_assert(VG_(strlen)(log_fsname_unexpanded) <= 900); 
         tmp_log_fd = VG_(connect_via_socket)( log_fsname_unexpanded );
         if (tmp_log_fd == -1) {
            VG_(fmsg)("Invalid --log-socket spec of '%s'\n",
                      log_fsname_unexpanded);
            VG_(exit)(1);
            
	 }
         if (tmp_log_fd == -2) {
            VG_(umsg)("failed to connect to logging server '%s'.\n"
                      "Log messages will sent to stderr instead.\n",
                      log_fsname_unexpanded ); 

            
            vg_assert(VG_(log_output_sink).fd == 2);
            tmp_log_fd = 2;
	 } else {
            vg_assert(tmp_log_fd > 0);
            VG_(log_output_sink).is_socket = True;
         }
         break;
      }
   }

   

   switch (xml_to) {

      case VgLogTo_Fd: 
         vg_assert(xml_fsname_unexpanded == NULL);
         break;

      case VgLogTo_File: {
         HChar* xmlfilename;

         vg_assert(xml_fsname_unexpanded != NULL);
         vg_assert(VG_(strlen)(xml_fsname_unexpanded) <= 900); 

         
         
         xmlfilename = VG_(expand_file_name)("--xml-file",
                                             xml_fsname_unexpanded);
         sres = VG_(open)(xmlfilename, 
                          VKI_O_CREAT|VKI_O_WRONLY|VKI_O_TRUNC, 
                          VKI_S_IRUSR|VKI_S_IWUSR|VKI_S_IRGRP|VKI_S_IROTH);
         if (!sr_isError(sres)) {
            tmp_xml_fd = sr_Res(sres);
            VG_(clo_xml_fname_expanded) = xmlfilename;
            *xml_fname_unexpanded = xml_fsname_unexpanded;
         } else {
            VG_(fmsg)("can't create XML file '%s': %s\n", 
                      xmlfilename, VG_(strerror)(sr_Err(sres)));
            VG_(exit)(1);
            
         }
         break;
      }

      case VgLogTo_Socket: {
         vg_assert(xml_fsname_unexpanded != NULL);
         vg_assert(VG_(strlen)(xml_fsname_unexpanded) <= 900); 
         tmp_xml_fd = VG_(connect_via_socket)( xml_fsname_unexpanded );
         if (tmp_xml_fd == -1) {
            VG_(fmsg)("Invalid --xml-socket spec of '%s'\n",
                      xml_fsname_unexpanded );
            VG_(exit)(1);
            
	 }
         if (tmp_xml_fd == -2) {
            VG_(umsg)("failed to connect to XML logging server '%s'.\n"
                      "XML output will sent to stderr instead.\n",
                      xml_fsname_unexpanded); 
            
            vg_assert(VG_(xml_output_sink).fd == 2);
            tmp_xml_fd = 2;
	 } else {
            vg_assert(tmp_xml_fd > 0);
            VG_(xml_output_sink).is_socket = True;
         }
         break;
      }
   }

   if (VG_(clo_xml) && tmp_xml_fd == -1) {
      VG_(fmsg_bad_option)(
          "--xml=yes, but no XML destination specified",
          "--xml=yes has been specified, but there is no XML output\n"
          "destination.  You must specify an XML output destination\n"
          "using --xml-fd, --xml-file or --xml-socket.\n"
      );
   }

   

   if (tmp_log_fd >= 0) {
      
      
      tmp_log_fd = VG_(fcntl)(tmp_log_fd, VKI_F_DUPFD, VG_(fd_hard_limit));
      if (tmp_log_fd < 0) {
         VG_(message)(Vg_UserMsg, "valgrind: failed to move logfile fd "
                                  "into safe range, using stderr\n");
         VG_(log_output_sink).fd = 2;   
         VG_(log_output_sink).is_socket = False;
      } else {
         VG_(log_output_sink).fd = tmp_log_fd;
         VG_(fcntl)(VG_(log_output_sink).fd, VKI_F_SETFD, VKI_FD_CLOEXEC);
      }
   } else {
      
      
      VG_(log_output_sink).fd = -1;
      VG_(log_output_sink).is_socket = False;
   }

   

   if (tmp_xml_fd >= 0) {
      
      
      tmp_xml_fd = VG_(fcntl)(tmp_xml_fd, VKI_F_DUPFD, VG_(fd_hard_limit));
      if (tmp_xml_fd < 0) {
         VG_(message)(Vg_UserMsg, "valgrind: failed to move XML file fd "
                                  "into safe range, using stderr\n");
         VG_(xml_output_sink).fd = 2;   
         VG_(xml_output_sink).is_socket = False;
      } else {
         VG_(xml_output_sink).fd = tmp_xml_fd;
         VG_(fcntl)(VG_(xml_output_sink).fd, VKI_F_SETFD, VKI_FD_CLOEXEC);
      }
   } else {
      
      
      VG_(xml_output_sink).fd = -1;
      VG_(xml_output_sink).is_socket = False;
   }

   

   if (VG_(clo_default_supp) &&
       (VG_(needs).core_errors || VG_(needs).tool_errors)) {
      static const HChar default_supp[] = "default.supp";
      Int len = VG_(strlen)(VG_(libdir)) + 1 + sizeof(default_supp);
      HChar *buf = VG_(malloc)("main.mpclo.3", len);
      VG_(sprintf)(buf, "%s/%s", VG_(libdir), default_supp);
      VG_(addToXA)(VG_(clo_suppressions), &buf);
   }

   *logging_to_fd = log_to == VgLogTo_Fd || log_to == VgLogTo_Socket;
}

static void print_file_vars(const HChar* format)
{
   Int i = 0;
   
   while (format[i]) {
      if (format[i] == '%') {
         
         i++;
	 if ('q' == format[i]) {
            i++;
            if ('{' == format[i]) {
	       
               HChar* qual;
               Int begin_qualname = ++i;
               while (True) {
		  if ('}' == format[i]) {
                     Int qualname_len = i - begin_qualname;
                     HChar qualname[qualname_len + 1];
                     VG_(strncpy)(qualname, format + begin_qualname,
                                  qualname_len);
                     qualname[qualname_len] = '\0';
                     qual = VG_(getenv)(qualname);
                     i++;
                     VG_(printf_xml)("<logfilequalifier> <var>%pS</var> "
                                     "<value>%pS</value> </logfilequalifier>\n",
                                     qualname, qual);
		     break;
                  }
                  i++;
               }
	    }
         }
      } else {
	 i++;
      }
   }
}



static void umsg_arg(const HChar* arg)
{
   SizeT len = VG_(strlen)(arg);
   const HChar* special = " \\<>";
   Int i;
   for (i = 0; i < len; i++) {
      if (VG_(strchr)(special, arg[i])) {
         VG_(umsg)("\\");   
      }
      VG_(umsg)("%c", arg[i]);
   }
}

static void xml_arg(const HChar* arg)
{
   VG_(printf_xml)("%pS", arg);
}

static void print_preamble ( Bool logging_to_fd, 
                             const HChar* xml_fname_unexpanded,
                             const HChar* toolname )
{
   Int    i;
   const HChar* xpre  = VG_(clo_xml) ? "  <line>" : "";
   const HChar* xpost = VG_(clo_xml) ? "</line>" : "";
   UInt (*umsg_or_xml)( const HChar*, ... )
      = VG_(clo_xml) ? VG_(printf_xml) : VG_(umsg);

   void (*umsg_or_xml_arg)( const HChar* )
      = VG_(clo_xml) ? xml_arg : umsg_arg;

   vg_assert( VG_(args_for_client) );
   vg_assert( VG_(args_for_valgrind) );
   vg_assert( toolname );

   if (VG_(clo_xml)) {
      VG_(printf_xml)("<?xml version=\"1.0\"?>\n");
      VG_(printf_xml)("\n");
      VG_(printf_xml)("<valgrindoutput>\n");
      VG_(printf_xml)("\n");
      VG_(printf_xml)("<protocolversion>4</protocolversion>\n");
      VG_(printf_xml)("<protocoltool>%s</protocoltool>\n", toolname);
      VG_(printf_xml)("\n");
   }

   if (VG_(clo_xml) || VG_(clo_verbosity > 0)) {

      if (VG_(clo_xml))
         VG_(printf_xml)("<preamble>\n");

      
      umsg_or_xml( VG_(clo_xml) ? "%s%pS%pS%pS, %pS%s\n" : "%s%s%s%s, %s%s\n",
                   xpre,
                   VG_(details).name, 
                   NULL == VG_(details).version ? "" : "-",
                   NULL == VG_(details).version 
                      ? "" : VG_(details).version,
                   VG_(details).description,
                   xpost );

      if (VG_(strlen)(toolname) >= 4 && VG_STREQN(4, toolname, "exp-")) {
         umsg_or_xml(
            "%sNOTE: This is an Experimental-Class Valgrind Tool%s\n",
            xpre, xpost
         );
      }

      umsg_or_xml( VG_(clo_xml) ? "%s%pS%s\n" : "%s%s%s\n",
                   xpre, VG_(details).copyright_author, xpost );

      
      umsg_or_xml(
         "%sUsing Valgrind-%s and LibVEX; rerun with -h for copyright info%s\n",
         xpre, VERSION, xpost
      );

      
      
      
      
      umsg_or_xml("%sCommand: ", xpre);
      umsg_or_xml_arg(VG_(args_the_exename));
          
      for (i = 0; i < VG_(sizeXA)( VG_(args_for_client) ); i++) {
         HChar* s = *(HChar**)VG_(indexXA)( VG_(args_for_client), i );
         umsg_or_xml(" ");
         umsg_or_xml_arg(s);
      }
      umsg_or_xml("%s\n", xpost);

      if (VG_(clo_xml))
         VG_(printf_xml)("</preamble>\n");
   }

   
   if (!VG_(clo_xml) && VG_(clo_verbosity) > 0 && !logging_to_fd) {
      VG_(umsg)("Parent PID: %d\n", VG_(getppid)());
   }
   else
   if (VG_(clo_xml)) {
      VG_(printf_xml)("\n");
      VG_(printf_xml)("<pid>%d</pid>\n", VG_(getpid)());
      VG_(printf_xml)("<ppid>%d</ppid>\n", VG_(getppid)());
      VG_(printf_xml)("<tool>%pS</tool>\n", toolname);
      if (xml_fname_unexpanded)
         print_file_vars(xml_fname_unexpanded);
      if (VG_(clo_xml_user_comment)) {
         VG_(printf_xml)("<usercomment>%s</usercomment>\n",
                         VG_(clo_xml_user_comment));
      }
      VG_(printf_xml)("\n");
      VG_(printf_xml)("<args>\n");

      VG_(printf_xml)("  <vargv>\n");
      if (VG_(name_of_launcher))
         VG_(printf_xml)("    <exe>%pS</exe>\n",
                                VG_(name_of_launcher));
      else
         VG_(printf_xml)("    <exe>%pS</exe>\n",
                                "(launcher name unknown)");
      for (i = 0; i < VG_(sizeXA)( VG_(args_for_valgrind) ); i++) {
         VG_(printf_xml)(
            "    <arg>%pS</arg>\n",
            * (HChar**) VG_(indexXA)( VG_(args_for_valgrind), i )
         );
      }
      VG_(printf_xml)("  </vargv>\n");

      VG_(printf_xml)("  <argv>\n");
      VG_(printf_xml)("    <exe>%pS</exe>\n",
                                VG_(args_the_exename));
      for (i = 0; i < VG_(sizeXA)( VG_(args_for_client) ); i++) {
         VG_(printf_xml)(
            "    <arg>%pS</arg>\n",
            * (HChar**) VG_(indexXA)( VG_(args_for_client), i )
         );
      }
      VG_(printf_xml)("  </argv>\n");

      VG_(printf_xml)("</args>\n");
   }

   
   if (VG_(clo_xml))
      VG_(printf_xml)("\n");
   else if (VG_(clo_verbosity) > 0)
      VG_(umsg)("\n");

   if (VG_(clo_verbosity) > 1) {
# if !defined(VGO_darwin)
      SysRes fd;
# endif
      VexArch vex_arch;
      VexArchInfo vex_archinfo;
      if (!logging_to_fd)
         VG_(message)(Vg_DebugMsg, "\n");
      VG_(message)(Vg_DebugMsg, "Valgrind options:\n");
      for (i = 0; i < VG_(sizeXA)( VG_(args_for_valgrind) ); i++) {
         VG_(message)(Vg_DebugMsg, 
                     "   %s\n", 
                     * (HChar**) VG_(indexXA)( VG_(args_for_valgrind), i ));
      }

# if !defined(VGO_darwin)
      VG_(message)(Vg_DebugMsg, "Contents of /proc/version:\n");
      fd = VG_(open) ( "/proc/version", VKI_O_RDONLY, 0 );
      if (sr_isError(fd)) {
         VG_(message)(Vg_DebugMsg, "  can't open /proc/version\n");
      } else {
         const SizeT bufsiz = 255;
         HChar version_buf[bufsiz+1];
         VG_(message)(Vg_DebugMsg, "  ");
         Int n, fdno = sr_Res(fd);
         do {
            n = VG_(read)(fdno, version_buf, bufsiz);
            if (n < 0) {
               VG_(message)(Vg_DebugMsg, "  error reading /proc/version\n");
               break;
            }
            version_buf[n] = '\0';
            VG_(message)(Vg_DebugMsg, "%s", version_buf);
         } while (n == bufsiz);
         VG_(message)(Vg_DebugMsg, "\n");
         VG_(close)(fdno);
      }
# else
      VG_(message)(Vg_DebugMsg, "Output from sysctl({CTL_KERN,KERN_VERSION}):\n");
      Int mib[] = {CTL_KERN, KERN_VERSION};
      SizeT len;
      VG_(sysctl)(mib, sizeof(mib)/sizeof(Int), NULL, &len, NULL, 0);
      HChar *kernelVersion = VG_(malloc)("main.pp.1", len);
      VG_(sysctl)(mib, sizeof(mib)/sizeof(Int), kernelVersion, &len, NULL, 0);
      VG_(message)(Vg_DebugMsg, "  %s\n", kernelVersion);
      VG_(free)( kernelVersion );
# endif

      VG_(machine_get_VexArchInfo)( &vex_arch, &vex_archinfo );
      VG_(message)(
         Vg_DebugMsg, 
         "Arch and hwcaps: %s, %s, %s\n",
         LibVEX_ppVexArch    ( vex_arch ),
         LibVEX_ppVexEndness ( vex_archinfo.endness ),
         LibVEX_ppVexHwCaps  ( vex_arch, vex_archinfo.hwcaps )
      );
      VG_(message)(
         Vg_DebugMsg, 
         "Page sizes: currently %d, max supported %d\n", 
         (Int)VKI_PAGE_SIZE, (Int)VKI_MAX_PAGE_SIZE
      );
      VG_(message)(Vg_DebugMsg,
                   "Valgrind library directory: %s\n", VG_(libdir));
   }
}



#define N_RESERVED_FDS (10)

static void setup_file_descriptors(void)
{
   struct vki_rlimit rl;
   Bool show = False;

   
   if (VG_(getrlimit)(VKI_RLIMIT_NOFILE, &rl) < 0) {
      rl.rlim_cur = 1024;
      rl.rlim_max = 1024;
   }

#  if defined(VGO_darwin)
   if (rl.rlim_cur >= 10240  &&  rl.rlim_max == 0x7fffffffffffffffULL) {
      rl.rlim_max = 10240;
   }
#  endif

   if (show)
      VG_(printf)("fd limits: host, before: cur %lu max %lu\n", 
                  (UWord)rl.rlim_cur, (UWord)rl.rlim_max);

   
   if (rl.rlim_cur + N_RESERVED_FDS <= rl.rlim_max) {
      rl.rlim_cur = rl.rlim_cur + N_RESERVED_FDS;
   } else {
      rl.rlim_cur = rl.rlim_max;
   }

   
   VG_(fd_soft_limit) = rl.rlim_cur - N_RESERVED_FDS;
   VG_(fd_hard_limit) = rl.rlim_cur - N_RESERVED_FDS;

   
   VG_(setrlimit)(VKI_RLIMIT_NOFILE, &rl);

   if (show) {
      VG_(printf)("fd limits: host,  after: cur %lu max %lu\n",
                  (UWord)rl.rlim_cur, (UWord)rl.rlim_max);
      VG_(printf)("fd limits: guest       : cur %u max %u\n",
                  VG_(fd_soft_limit), VG_(fd_hard_limit));
   }

   if (VG_(cl_exec_fd) != -1)
      VG_(cl_exec_fd) = VG_(safe_fd)( VG_(cl_exec_fd) );
}




 struct {
   HChar bytes [VG_STACK_GUARD_SZB + VG_DEFAULT_STACK_ACTIVE_SZB + VG_STACK_GUARD_SZB];
} VG_(interim_stack);



static IICreateImageInfo   the_iicii;
static IIFinaliseImageInfo the_iifii;


typedef  struct { Addr a; ULong ull; }  Addr_n_ULong;



static void final_tidyup(ThreadId tid); 

static 
void shutdown_actions_NORETURN( ThreadId tid, 
                                VgSchedReturnCode tids_schedretcode );



static
Int valgrind_main ( Int argc, HChar **argv, HChar **envp )
{
   const HChar* toolname      = "memcheck";    
   Int     need_help          = 0; 
   ThreadId tid_main          = VG_INVALID_THREADID;
   Bool    logging_to_fd      = False;
   const HChar* xml_fname_unexpanded = NULL;
   Int     loglevel, i;
   struct vki_rlimit zero = { 0, 0 };
   XArray* addr2dihandle = NULL;

   
   
   
   
   
   
   
   
   
   
   
   
   VG_(client_envp) = (HChar**)envp;

   
   
   
   
#  if defined(VGO_darwin)
   VG_(mach_init)();
#  endif

   
   
   
   
   loglevel = 0;
   for (i = 1; i < argc; i++) {
      const HChar* tmp_str;
      if (argv[i][0] != '-') break;
      if VG_STREQ(argv[i], "--") break;
      if VG_STREQ(argv[i], "-d") loglevel++;
      if VG_BOOL_CLO(argv[i], "--profile-heap", VG_(clo_profile_heap)) {}
      if VG_BINT_CLO(argv[i], "--core-redzone-size", VG_(clo_core_redzone_size),
                     0, MAX_CLO_REDZONE_SZB) {}
      if VG_BINT_CLO(argv[i], "--redzone-size", VG_(clo_redzone_size),
                     0, MAX_CLO_REDZONE_SZB) {}
      if VG_STR_CLO(argv[i], "--aspace-minaddr", tmp_str) {
         Bool ok = VG_(parse_Addr) (&tmp_str, &VG_(clo_aspacem_minAddr));
         if (!ok)
            VG_(fmsg_bad_option)(argv[i], "Invalid address\n");
         const HChar *errmsg;
         if (!VG_(am_is_valid_for_aspacem_minAddr)(VG_(clo_aspacem_minAddr),
                                                   &errmsg))
            VG_(fmsg_bad_option)(argv[i], "%s\n", errmsg);
      }
   }

   VG_(debugLog_startup)(loglevel, "Stage 2 (main)");
   VG_(debugLog)(1, "main", "Welcome to Valgrind version " 
                            VERSION " debug logging\n");

   
   
   
   
   VG_(debugLog)(1, "main", "Checking current stack is plausible\n");
   { HChar* limLo  = (HChar*)(&VG_(interim_stack).bytes[0]);
     HChar* limHi  = limLo + sizeof(VG_(interim_stack));
     HChar* volatile 
            aLocal = (HChar*)&limLo; 
     if (aLocal < limLo || aLocal >= limHi) {
        
        VG_(debugLog)(0, "main", "Root stack %p to %p, a local %p\n",
                          limLo, limHi, aLocal );
        VG_(debugLog)(0, "main", "Valgrind: FATAL: "
                                 "Initial stack switched failed.\n");
        VG_(debugLog)(0, "main", "   Cannot continue.  Sorry.\n");
        VG_(exit)(1);
     }
   }

   
   
   
   
   
   VG_(debugLog)(1, "main", "Checking initial stack was noted\n");
   if (the_iicii.sp_at_startup == 0) {
      VG_(debugLog)(0, "main", "Valgrind: FATAL: "
                               "Initial stack was not noted.\n");
      VG_(debugLog)(0, "main", "   Cannot continue.  Sorry.\n");
      VG_(exit)(1);
   }

   
   
   
   
   
   VG_(debugLog)(1, "main", "Starting the address space manager\n");
   vg_assert(VKI_PAGE_SIZE     == 4096 || VKI_PAGE_SIZE     == 65536
             || VKI_PAGE_SIZE     == 16384);
   vg_assert(VKI_MAX_PAGE_SIZE == 4096 || VKI_MAX_PAGE_SIZE == 65536
             || VKI_MAX_PAGE_SIZE == 16384);
   vg_assert(VKI_PAGE_SIZE <= VKI_MAX_PAGE_SIZE);
   vg_assert(VKI_PAGE_SIZE     == (1 << VKI_PAGE_SHIFT));
   vg_assert(VKI_MAX_PAGE_SIZE == (1 << VKI_MAX_PAGE_SHIFT));
   the_iicii.clstack_end = VG_(am_startup)( the_iicii.sp_at_startup );
   VG_(debugLog)(1, "main", "Address space manager is running\n");

   
   
   
   
   
   
   
   
   VG_(debugLog)(1, "main", "Starting the dynamic memory manager\n");
   { void* p = VG_(malloc)( "main.vm.1", 12345 );
     VG_(free)( p );
   }
   VG_(debugLog)(1, "main", "Dynamic memory manager is running\n");

   
   
   
   
   

   
   
   
   VG_(debugLog)(1, "main", "Initialise m_debuginfo\n");
   VG_(di_initialise)();

   
   
   { HChar *cp = VG_(getenv)(VALGRIND_LIB);
     if (cp != NULL)
        VG_(libdir) = cp;
     VG_(debugLog)(1, "main", "VG_(libdir) = %s\n", VG_(libdir));
   }

   
   
   VG_(debugLog)(1, "main", "Getting launcher's name ...\n");
   VG_(name_of_launcher) = VG_(getenv)(VALGRIND_LAUNCHER);
   if (VG_(name_of_launcher) == NULL) {
      VG_(printf)("valgrind: You cannot run '%s' directly.\n", argv[0]);
      VG_(printf)("valgrind: You should use $prefix/bin/valgrind.\n");
      VG_(exit)(1);
   }
   VG_(debugLog)(1, "main", "... %s\n", VG_(name_of_launcher));

   
   
   
   
   
   VG_(getrlimit)(VKI_RLIMIT_DATA, &VG_(client_rlimit_data));
   zero.rlim_max = VG_(client_rlimit_data).rlim_max;
   VG_(setrlimit)(VKI_RLIMIT_DATA, &zero);

   
   VG_(getrlimit)(VKI_RLIMIT_STACK, &VG_(client_rlimit_stack));

   
   
   
   VexArchInfo vex_archinfo;
   VG_(debugLog)(1, "main", "Get hardware capabilities ...\n");
   { VexArch     vex_arch;
     Bool ok = VG_(machine_get_hwcaps)();
     if (!ok) {
        VG_(printf)("\n");
        VG_(printf)("valgrind: fatal error: unsupported CPU.\n");
        VG_(printf)("   Supported CPUs are:\n");
        VG_(printf)("   * x86 (practically any; Pentium-I or above), "
                    "AMD Athlon or above)\n");
        VG_(printf)("   * AMD Athlon64/Opteron\n");
        VG_(printf)("   * ARM (armv7)\n");
        VG_(printf)("   * PowerPC (most; ppc405 and above)\n");
        VG_(printf)("   * System z (64bit only - s390x; z990 and above)\n");
        VG_(printf)("\n");
        VG_(exit)(1);
     }
     VG_(machine_get_VexArchInfo)( &vex_arch, &vex_archinfo );
     VG_(debugLog)(
        1, "main", "... arch = %s, hwcaps = %s\n",
           LibVEX_ppVexArch   ( vex_arch ),
           LibVEX_ppVexHwCaps ( vex_arch, vex_archinfo.hwcaps ) 
     );
   }

   
   
   
   VG_(debugLog)(1, "main", "Getting the working directory at startup\n");
   { Bool ok = VG_(record_startup_wd)();
     if (!ok) 
        VG_(err_config_error)( "Can't establish current working "
                               "directory at startup\n");
   }
   VG_(debugLog)(1, "main", "... %s\n", VG_(get_startup_wd)() );

   
   
   
   
   
   
   
   

   
   
   
   
   VG_(debugLog)(1, "main", "Split up command line\n");
   VG_(split_up_argv)( argc, argv );
   vg_assert( VG_(args_for_valgrind) );
   vg_assert( VG_(args_for_client) );
   if (0) {
      for (i = 0; i < VG_(sizeXA)( VG_(args_for_valgrind) ); i++)
         VG_(printf)(
            "varg %s\n", 
            * (HChar**) VG_(indexXA)( VG_(args_for_valgrind), i )
         );
      VG_(printf)(" exe %s\n", VG_(args_the_exename));
      for (i = 0; i < VG_(sizeXA)( VG_(args_for_client) ); i++)
         VG_(printf)(
            "carg %s\n", 
            * (HChar**) VG_(indexXA)( VG_(args_for_client), i )
         );
   }

   
   
   
   
   
   
   VG_(debugLog)(1, "main",
                    "(early_) Process Valgrind's command line options\n");
   early_process_cmd_line_options(&need_help, &toolname);

   
   vg_assert(toolname != NULL);
   vg_assert(VG_(clo_read_inline_info) == False);
#  if !defined(VGO_darwin)
   if (0 == VG_(strcmp)(toolname, "memcheck")
       || 0 == VG_(strcmp)(toolname, "helgrind")
       || 0 == VG_(strcmp)(toolname, "drd")) {
      VG_(clo_read_inline_info) = True;
   }
#  endif
   

   
   LibVEX_default_VexControl(& VG_(clo_vex_control));

   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   if (!need_help) {
      VG_(debugLog)(1, "main", "Create initial image\n");

#     if defined(VGO_linux) || defined(VGO_darwin)
      the_iicii.argv              = argv;
      the_iicii.envp              = envp;
      the_iicii.toolname          = toolname;
#     else
#       error "Unknown platform"
#     endif

      
      the_iifii = VG_(ii_create_image)( the_iicii, &vex_archinfo );
   }

   
   
   
   
   

   
   
   
   
   VG_(debugLog)(1, "main", "Setup file descriptors\n");
   setup_file_descriptors();

   
   
   
   
   
   
   
   
#if !defined(VGO_linux)
   
   VG_(cl_cmdline_fd) = -1;
   VG_(cl_auxv_fd) = -1;
#else
   if (!need_help) {
      HChar  buf[50];   
      HChar  buf2[VG_(mkstemp_fullname_bufsz)(sizeof buf - 1)];
      HChar  nul[1];
      Int    fd, r;
      const HChar* exename;

      VG_(debugLog)(1, "main", "Create fake /proc/<pid>/cmdline\n");

      VG_(sprintf)(buf, "proc_%d_cmdline", VG_(getpid)());
      fd = VG_(mkstemp)( buf, buf2 );
      if (fd == -1)
         VG_(err_config_error)("Can't create client cmdline file in %s\n", buf2);

      nul[0] = 0;
      exename = VG_(args_the_exename);
      VG_(write)(fd, exename, VG_(strlen)( exename ));
      VG_(write)(fd, nul, 1);

      for (i = 0; i < VG_(sizeXA)( VG_(args_for_client) ); i++) {
         HChar* arg = * (HChar**) VG_(indexXA)( VG_(args_for_client), i );
         VG_(write)(fd, arg, VG_(strlen)( arg ));
         VG_(write)(fd, nul, 1);
      }


      
      r = VG_(unlink)( buf2 );
      if (r)
         VG_(err_config_error)("Can't delete client cmdline file in %s\n", buf2);

      VG_(cl_cmdline_fd) = fd;

      VG_(debugLog)(1, "main", "Create fake /proc/<pid>/auxv\n");

      VG_(sprintf)(buf, "proc_%d_auxv", VG_(getpid)());
      fd = VG_(mkstemp)( buf, buf2 );
      if (fd == -1)
         VG_(err_config_error)("Can't create client auxv file in %s\n", buf2);

      UWord *client_auxv = VG_(client_auxv);
      unsigned int client_auxv_len = 0;
      while (*client_auxv != 0) {
         client_auxv++;
         client_auxv++;
         client_auxv_len += 2 * sizeof(UWord);
      }
      client_auxv_len += 2 * sizeof(UWord);

      VG_(write)(fd, VG_(client_auxv), client_auxv_len);


      
      r = VG_(unlink)( buf2 );
      if (r)
         VG_(err_config_error)("Can't delete client auxv file in %s\n", buf2);

      VG_(cl_auxv_fd) = fd;
   }
#endif

   
   
   
   
   
   VG_(debugLog)(1, "main", "Initialise the tool part 1 (pre_clo_init)\n");
   VG_(tl_pre_clo_init)();
   
   if (VG_(needs).var_info)
      VG_(clo_read_var_info) = True;

   
   
   
   
   
   
   VG_(debugLog)(1, "main", "Print help and quit, if requested\n");
   if (need_help) {
      usage_NORETURN(need_help >= 2);
   }

   
   
   
   
   
   VG_(debugLog)(1, "main",
                    "(main_) Process Valgrind's command line options, "
                    "setup logging\n");
   main_process_cmd_line_options ( &logging_to_fd, &xml_fname_unexpanded,
                                   toolname );

   
   
   
   
   (void) VG_(read_millisecond_timer)();

   
   
   
   
   
   
   
   VG_(debugLog)(1, "main", "Print the preamble...\n");
   print_preamble(logging_to_fd, xml_fname_unexpanded, toolname);
   VG_(debugLog)(1, "main", "...finished the preamble\n");

   
   
   
   
   
   
   
   VG_(debugLog)(1, "main", "Initialise the tool part 2 (post_clo_init)\n");
   VG_TDICT_CALL(tool_post_clo_init);
   {
      const HChar* s;
      Bool  ok;
      ok = VG_(sanity_check_needs)( &s );
      if (!ok) {
         VG_(core_panic)(s);
      }
   }

   
   
   
   
   
   VG_(debugLog)(1, "main", "Initialise TT/TC\n");
   VG_(init_tt_tc)();

   
   
   
   
   
   VG_(debugLog)(1, "main", "Initialise redirects\n");
   VG_(redir_initialise)();

   
   
   
   
   if (VG_(clo_wait_for_gdb)) {
      ULong iters, q;
      VG_(debugLog)(1, "main", "Wait for GDB\n");
      VG_(printf)("pid=%d, entering delay loop\n", VG_(getpid)());

#     if defined(VGP_x86_linux)
      iters = 10;
#     elif defined(VGP_amd64_linux) || defined(VGP_ppc64be_linux) \
         || defined(VGP_ppc64le_linux) || defined(VGP_tilegx_linux)
      iters = 10;
#     elif defined(VGP_ppc32_linux)
      iters = 5;
#     elif defined(VGP_arm_linux)
      iters = 5;
#     elif defined(VGP_arm64_linux)
      iters = 5;
#     elif defined(VGP_s390x_linux)
      iters = 10;
#     elif defined(VGP_mips32_linux) || defined(VGP_mips64_linux)
      iters = 10;
#     elif defined(VGO_darwin)
      iters = 3;
#     else
#       error "Unknown plat"
#     endif

      iters *= 1000ULL * 1000 * 1000;
      for (q = 0; q < iters; q++) 
         __asm__ __volatile__("" ::: "memory","cc");
   }

   
   
   
   
   if (VG_(clo_track_fds)) {
      VG_(debugLog)(1, "main", "Init preopened fds\n");
      VG_(init_preopened_fds)();
   }

   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   VG_(debugLog)(1, "main", "Load initial debug info\n");

   vg_assert(!addr2dihandle);
   addr2dihandle = VG_(newXA)( VG_(malloc), "main.vm.2",
                               VG_(free), sizeof(Addr_n_ULong) );

#  if defined(VGO_linux)
   { Addr* seg_starts;
     Int   n_seg_starts;
     Addr_n_ULong anu;

     seg_starts = VG_(get_segment_starts)( SkFileC | SkFileV, &n_seg_starts );
     vg_assert(seg_starts && n_seg_starts >= 0);

     for (i = 0; i < n_seg_starts; i++) {
        anu.ull = VG_(di_notify_mmap)( seg_starts[i], True,
                                       -1);
        if (anu.ull > 0) {
           anu.a = seg_starts[i];
           VG_(addToXA)( addr2dihandle, &anu );
        }
     }

     VG_(free)( seg_starts );
   }
#  elif defined(VGO_darwin)
   { Addr* seg_starts;
     Int   n_seg_starts;
     seg_starts = VG_(get_segment_starts)( SkFileC, &n_seg_starts );
     vg_assert(seg_starts && n_seg_starts >= 0);

     
     for (i = 0; i < n_seg_starts; i++) {
        VG_(di_notify_mmap)( seg_starts[i], False,
                             -1);
     }

     VG_(free)( seg_starts );
   }
#  else
#    error Unknown OS
#  endif

   
   
   
   
   
   
   
   { Bool change_ownership_v_c_OK;
     Addr co_start   = VG_PGROUNDDN( (Addr)&VG_(trampoline_stuff_start) );
     Addr co_endPlus = VG_PGROUNDUP( (Addr)&VG_(trampoline_stuff_end) );
     VG_(debugLog)(1,"redir",
                     "transfer ownership V -> C of 0x%llx .. 0x%llx\n",
                     (ULong)co_start, (ULong)co_endPlus-1 );

     change_ownership_v_c_OK 
        = VG_(am_change_ownership_v_to_c)( co_start, co_endPlus - co_start );
     vg_assert(change_ownership_v_c_OK);
   }

   if (VG_(clo_xml)) {
      HChar buf[50];    
      VG_(elapsed_wallclock_time)(buf, sizeof buf);
      VG_(printf_xml)( "<status>\n"
                       "  <state>RUNNING</state>\n"
                       "  <time>%pS</time>\n"
                       "</status>\n",
                       buf );
      VG_(printf_xml)( "\n" );
   }

   VG_(init_Threads)();

   
   
   
   
   VG_(debugLog)(1, "main", "Initialise scheduler (phase 1)\n");
   tid_main = VG_(scheduler_init_phase1)();
   vg_assert(tid_main >= 0 && tid_main < VG_N_THREADS
             && tid_main != VG_INVALID_THREADID);
   
   VG_TRACK( pre_thread_ll_create, VG_INVALID_THREADID, tid_main );
   
   
   
   
   
   
   
   
   
   
   
   
   
   VG_(debugLog)(1, "main", "Tell tool about initial permissions\n");
   { Addr*     seg_starts;
     Int       n_seg_starts;

     vg_assert(addr2dihandle);

     vg_assert(VG_(running_tid) == VG_INVALID_THREADID);
     VG_(running_tid) = tid_main;

     seg_starts = VG_(get_segment_starts)( SkFileC | SkAnonC | SkShmC,
                                           &n_seg_starts );
     vg_assert(seg_starts && n_seg_starts >= 0);

     
     for (i = 0; i < n_seg_starts; i++) {
        Word j, n;
        NSegment const* seg 
           = VG_(am_find_nsegment)( seg_starts[i] );
        vg_assert(seg);
        vg_assert(seg->kind == SkFileC || seg->kind == SkAnonC ||
                  seg->kind == SkShmC);
        vg_assert(seg->start == seg_starts[i]);
        {
           VG_(debugLog)(2, "main", 
                            "tell tool about %010lx-%010lx %c%c%c\n",
                             seg->start, seg->end,
                             seg->hasR ? 'r' : '-',
                             seg->hasW ? 'w' : '-',
                             seg->hasX ? 'x' : '-' );
           n = VG_(sizeXA)( addr2dihandle );
           for (j = 0; j < n; j++) {
              Addr_n_ULong* anl = VG_(indexXA)( addr2dihandle, j );
              if (anl->a == seg->start) {
                  vg_assert(anl->ull > 0); 
                  break;
              }
           }
           vg_assert(j >= 0 && j <= n);
           VG_TRACK( new_mem_startup, seg->start, seg->end+1-seg->start, 
                     seg->hasR, seg->hasW, seg->hasX,
                     
                     j < n
                     ? ((Addr_n_ULong*)VG_(indexXA)( addr2dihandle, j ))->ull
                        : 0 );
        }
     }

     VG_(free)( seg_starts );
     VG_(deleteXA)( addr2dihandle );

     
     {
       SSizeT inaccessible_len;
       NSegment const* seg 
          = VG_(am_find_nsegment)( the_iifii.initial_client_SP );
       vg_assert(seg);
       vg_assert(seg->kind == SkAnonC);
       vg_assert(the_iifii.initial_client_SP >= seg->start);
       vg_assert(the_iifii.initial_client_SP <= seg->end);

       inaccessible_len = the_iifii.initial_client_SP - VG_STACK_REDZONE_SZB 
                          - seg->start;
       vg_assert(inaccessible_len >= 0);
       if (inaccessible_len > 0)
          VG_TRACK( die_mem_stack, 
                    seg->start, 
                    inaccessible_len );
       VG_(debugLog)(2, "main", "mark stack inaccessible %010lx-%010lx\n",
                        seg->start, 
                        the_iifii.initial_client_SP-1 - VG_STACK_REDZONE_SZB);
     }

     
     VG_TRACK( new_mem_startup,
               (Addr)&VG_(trampoline_stuff_start),
               (Addr)&VG_(trampoline_stuff_end) 
                  - (Addr)&VG_(trampoline_stuff_start),
               False, 
               False, 
               True   ,
               0  );

     
     VG_(running_tid) = VG_INVALID_THREADID;
     vg_assert(VG_(running_tid) == VG_INVALID_THREADID);

#    if defined(VGP_amd64_darwin)
     VG_TRACK( new_mem_startup,
               0x7fffffe00000, 0x7ffffffff000-0x7fffffe00000,
               True, False, True, 
               0  );
#    elif defined(VGP_x86_darwin)
     VG_TRACK( new_mem_startup,
               0xfffec000, 0xfffff000-0xfffec000,
               True, False, True, 
               0  );
#    endif
   }

   
   
   
   
   
   
   VG_(debugLog)(1, "main", "Initialise scheduler (phase 2)\n");
   { NSegment const* seg 
        = VG_(am_find_nsegment)( the_iifii.initial_client_SP );
     vg_assert(seg);
     vg_assert(seg->kind == SkAnonC);
     vg_assert(the_iifii.initial_client_SP >= seg->start);
     vg_assert(the_iifii.initial_client_SP <= seg->end);
     VG_(scheduler_init_phase2)( tid_main, 
                                 seg->end, the_iifii.clstack_max_size );
   }

   
   
   
   
   
   
   VG_(debugLog)(1, "main", "Finalise initial image\n");
   VG_(ii_finalise_image)( the_iifii );

   
   
   
   
   
   VG_(debugLog)(1, "main", "Initialise signal management\n");
   
   VG_(vki_do_initial_consistency_checks)();
   
   VG_(sigstartup_actions)();

   
   
   
   
   if (VG_(needs).core_errors || VG_(needs).tool_errors) {
      VG_(debugLog)(1, "main", "Load suppressions\n");
      VG_(load_suppressions)();
   }

   
   
   
   VG_(clstk_id) = VG_(register_stack)(VG_(clstk_start_base), VG_(clstk_end));

   
   
   
   VG_(debugLog)(1, "main", "\n");
   VG_(debugLog)(1, "main", "\n");
   VG_(am_show_nsegments)(1,"Memory layout at client startup");
   VG_(debugLog)(1, "main", "\n");
   VG_(debugLog)(1, "main", "\n");

   
   
   
   VG_(debugLog)(1, "main", "Running thread 1\n");


   
   VG_(address_of_m_main_shutdown_actions_NORETURN)
      = & shutdown_actions_NORETURN;

   VG_(main_thread_wrapper_NORETURN)(1);

   
   vg_assert(0);
}


static 
void shutdown_actions_NORETURN( ThreadId tid, 
                                VgSchedReturnCode tids_schedretcode )
{
   VG_(debugLog)(1, "main", "entering VG_(shutdown_actions_NORETURN)\n");
   VG_(am_show_nsegments)(1,"Memory layout at client shutdown");

   vg_assert(VG_(is_running_thread)(tid));
   vg_assert(tids_schedretcode == VgSrc_ExitThread
	     || tids_schedretcode == VgSrc_ExitProcess
             || tids_schedretcode == VgSrc_FatalSig );

   if (tids_schedretcode == VgSrc_ExitThread) {

      
      vg_assert( VG_(count_living_threads)() == 1 );

      
      
      VG_(reap_threads)(tid);

      
      
      final_tidyup(tid);

      
      vg_assert(VG_(is_running_thread)(tid));
      vg_assert(VG_(count_living_threads)() == 1);

   } else {

      
      
      
      
      vg_assert( VG_(count_living_threads)() >= 1 );

      
      
      
      final_tidyup(tid);

      
      vg_assert(VG_(is_running_thread)(tid));
      vg_assert(VG_(count_living_threads)() >= 1);
   }

   
   if (VG_(gdbserver_stop_at) (VgdbStopAt_Exit)) {
      VG_(umsg)("(action at exit) vgdb me ... \n");
      VG_(gdbserver) (tid);
   }
   VG_(threads)[tid].status = VgTs_Empty;

   
   
   
   
   
   if (VG_(clo_xml))
      VG_(printf_xml)("\n");
   else if (VG_(clo_verbosity) > 0)
      VG_(message)(Vg_UserMsg, "\n");

   if (VG_(clo_xml)) {
      HChar buf[50];    
      VG_(elapsed_wallclock_time)(buf, sizeof buf);
      VG_(printf_xml)( "<status>\n"
                              "  <state>FINISHED</state>\n"
                              "  <time>%pS</time>\n"
                              "</status>\n"
                              "\n",
                              buf);
   }

   
   if (VG_(clo_track_fds))
      VG_(show_open_fds)("at exit");

   VG_TDICT_CALL(tool_fini, 0);

   
   if (VG_(clo_xml)
       && (VG_(needs).core_errors || VG_(needs).tool_errors)) {
      VG_(show_error_counts_as_XML)();
   }

   
   if (VG_(needs).core_errors || VG_(needs).tool_errors)
      VG_(show_all_errors)(VG_(clo_verbosity), VG_(clo_xml));

   if (VG_(clo_xml)) {
      VG_(printf_xml)("\n");
      VG_(printf_xml)("</valgrindoutput>\n");
      VG_(printf_xml)("\n");
   }

   VG_(sanity_check_general)( True  );

   if (VG_(clo_stats))
      VG_(print_all_stats)(VG_(clo_verbosity) >= 1, 
                           False );

   if (VG_(clo_profile_heap)) {
      if (0) VG_(di_discard_ALL_debuginfo)();
      VG_(print_arena_cc_analysis)();
   }

   if (VG_(clo_profyle_sbs) && VG_(clo_profyle_interval) == 0) {
      VG_(get_and_show_SB_profile)(0);
   }

   
   if (0)
       LibVEX_ShowAllocStats();

   
   VG_(message_flush)();

   VG_(gdbserver_exit) (tid, tids_schedretcode);

   VG_(debugLog)(1, "core_os", 
                    "VG_(terminate_NORETURN)(tid=%lld)\n", (ULong)tid);

   switch (tids_schedretcode) {
   case VgSrc_ExitThread:  
   case VgSrc_ExitProcess: 
      if (VG_(clo_error_exitcode) > 0 
          && VG_(get_n_errs_found)() > 0) {
         VG_(client_exit)( VG_(clo_error_exitcode) );
      } else {
         VG_(client_exit)( VG_(threads)[tid].os_state.exitcode );
      }
      
      VG_(core_panic)("entered the afterlife in main() -- ExitT/P");
      break; 

   case VgSrc_FatalSig:
      
      vg_assert(VG_(threads)[tid].os_state.fatalsig != 0);
      VG_(kill_self)(VG_(threads)[tid].os_state.fatalsig);
#     if defined(VGO_darwin)
      VG_(debugLog)(0, "main", "VG_(kill_self) failed.  Exiting normally.\n");
      VG_(exit)(0); 
      
#     endif
      VG_(core_panic)("main(): signal was supposed to be fatal");
      break;

   default:
      VG_(core_panic)("main(): unexpected scheduler return code");
   }
}


static void final_tidyup(ThreadId tid)
{
#if !defined(VGO_darwin)
   Addr __libc_freeres_wrapper = VG_(client___libc_freeres_wrapper);

   vg_assert(VG_(is_running_thread)(tid));
   
   if ( !VG_(needs).libc_freeres ||
        !VG_(clo_run_libc_freeres) ||
        0 == __libc_freeres_wrapper )
      return;			

#  if defined(VGP_ppc64be_linux)
   Addr r2 = VG_(get_tocptr)( __libc_freeres_wrapper );
   if (r2 == 0) {
      VG_(message)(Vg_UserMsg, 
                   "Caught __NR_exit, but can't run __libc_freeres()\n");
      VG_(message)(Vg_UserMsg, 
                   "   since cannot establish TOC pointer for it.\n");
      return;
   }
#  endif

   if (VG_(clo_verbosity) > 2  ||
       VG_(clo_trace_syscalls) ||
       VG_(clo_trace_sched))
      VG_(message)(Vg_DebugMsg, 
		   "Caught __NR_exit; running __libc_freeres()\n");
      
   
   VG_(set_IP)(tid, __libc_freeres_wrapper);
#  if defined(VGP_ppc64be_linux)
   VG_(threads)[tid].arch.vex.guest_GPR2 = r2;
#  elif  defined(VGP_ppc64le_linux)
   
   VG_(threads)[tid].arch.vex.guest_GPR2  = __libc_freeres_wrapper;
   VG_(threads)[tid].arch.vex.guest_GPR12 = __libc_freeres_wrapper;
#  endif
   
#  if defined(VGP_mips32_linux) || defined(VGP_mips64_linux)
   VG_(threads)[tid].arch.vex.guest_r25 = __libc_freeres_wrapper;
#  endif

   VG_(sigprocmask)(VKI_SIG_BLOCK, NULL, &VG_(threads)[tid].sig_mask);
   VG_(threads)[tid].tmp_sig_mask = VG_(threads)[tid].sig_mask;

   
   VG_(set_default_handler)(VKI_SIGSEGV);
   VG_(set_default_handler)(VKI_SIGBUS);
   VG_(set_default_handler)(VKI_SIGILL);
   VG_(set_default_handler)(VKI_SIGFPE);

   
   vg_assert(VG_(is_exiting)(tid));
   
   VG_(threads)[tid].exitreason = VgSrc_None;

   
   
   VG_(scheduler)(tid);

   vg_assert(VG_(is_exiting)(tid));
#endif
}



#if defined(VGO_linux)



void* memcpy(void *dest, const void *src, SizeT n);
void* memcpy(void *dest, const void *src, SizeT n) {
   return VG_(memcpy)(dest,src,n);
}
void* memmove(void *dest, const void *src, SizeT n);
void* memmove(void *dest, const void *src, SizeT n) {
   return VG_(memmove)(dest,src,n);
}
void* memset(void *s, int c, SizeT n);
void* memset(void *s, int c, SizeT n) {
  return VG_(memset)(s,c,n);
}

void abort(void);
void abort(void){
   VG_(printf)("Something called raise().\n");
   vg_assert(0);
}

#if defined(VGP_arm_linux)
void raise(void);
void raise(void){
   VG_(printf)("Something called raise().\n");
   vg_assert(0);
}

void __aeabi_unwind_cpp_pr0(void);
void __aeabi_unwind_cpp_pr0(void){
   VG_(printf)("Something called __aeabi_unwind_cpp_pr0()\n");
   vg_assert(0);
}

void __aeabi_unwind_cpp_pr1(void);
void __aeabi_unwind_cpp_pr1(void){
   VG_(printf)("Something called __aeabi_unwind_cpp_pr1()\n");
   vg_assert(0);
}
#endif




#if defined(VGP_x86_linux)
asm("\n"
    ".text\n"
    "\t.globl _start\n"
    "\t.type _start,@function\n"
    "_start:\n"
    
    "\tmovl  $vgPlain_interim_stack, %eax\n"
    "\taddl  $"VG_STRINGIFY(VG_STACK_GUARD_SZB)", %eax\n"
    "\taddl  $"VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB)", %eax\n"
    "\tsubl  $16, %eax\n"
    "\tandl  $~15, %eax\n"
    
    "\txchgl %eax, %esp\n"
    
    "\tpushl %eax\n"
    "\tcall  _start_in_C_linux\n"
    "\thlt\n"
    ".previous\n"
);
#elif defined(VGP_amd64_linux)
asm("\n"
    ".text\n"
    "\t.globl _start\n"
    "\t.type _start,@function\n"
    "_start:\n"
    
    "\tmovq  $vgPlain_interim_stack, %rdi\n"
    "\taddq  $"VG_STRINGIFY(VG_STACK_GUARD_SZB)", %rdi\n"
    "\taddq  $"VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB)", %rdi\n"
    "\tandq  $~15, %rdi\n"
    
    "\txchgq %rdi, %rsp\n"
    
    "\tcall  _start_in_C_linux\n"
    "\thlt\n"
    ".previous\n"
);
#elif defined(VGP_ppc32_linux)
asm("\n"
    ".text\n"
    "\t.globl _start\n"
    "\t.type _start,@function\n"
    "_start:\n"
    
    "\tlis 16,vgPlain_interim_stack@ha\n"
    "\tla  16,vgPlain_interim_stack@l(16)\n"
    "\tlis    17,("VG_STRINGIFY(VG_STACK_GUARD_SZB)" >> 16)\n"
    "\tori 17,17,("VG_STRINGIFY(VG_STACK_GUARD_SZB)" & 0xFFFF)\n"
    "\tlis    18,("VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB)" >> 16)\n"
    "\tori 18,18,("VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB)" & 0xFFFF)\n"
    "\tadd 16,17,16\n"
    "\tadd 16,18,16\n"
    "\trlwinm 16,16,0,0,27\n"
    /* now r16 = &vgPlain_interim_stack + VG_STACK_GUARD_SZB +
       VG_DEFAULT_STACK_ACTIVE_SZB rounded down to the nearest 16-byte
       boundary.  And r1 is the original SP.  Set the SP to r16 and
       call _start_in_C_linux, passing it the initial SP. */
    "\tmr 3,1\n"
    "\tmr 1,16\n"
    "\tbl _start_in_C_linux\n"
    "\ttrap\n"
    ".previous\n"
);
#elif defined(VGP_ppc64be_linux)
asm("\n"
    "\t.align 2\n"
    "\t.global _start\n"
    "\t.section \".opd\",\"aw\"\n"
    "\t.align 3\n"
    "_start:\n"
    "\t.quad ._start,.TOC.@tocbase,0\n"
    "\t.previous\n"
    "\t.type ._start,@function\n"
    "\t.global  ._start\n"
    "._start:\n"
    
    "\tlis  16,   vgPlain_interim_stack@highest\n"
    "\tori  16,16,vgPlain_interim_stack@higher\n"
    "\tsldi 16,16,32\n"
    "\toris 16,16,vgPlain_interim_stack@h\n"
    "\tori  16,16,vgPlain_interim_stack@l\n"
    "\txor  17,17,17\n"
    "\tlis    17,("VG_STRINGIFY(VG_STACK_GUARD_SZB)" >> 16)\n"
    "\tori 17,17,("VG_STRINGIFY(VG_STACK_GUARD_SZB)" & 0xFFFF)\n"
    "\txor 18,18,18\n"
    "\tlis    18,("VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB)" >> 16)\n"
    "\tori 18,18,("VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB)" & 0xFFFF)\n"
    "\tadd 16,17,16\n"
    "\tadd 16,18,16\n"
    "\trldicr 16,16,0,59\n"
    /* now r16 = &vgPlain_interim_stack + VG_STACK_GUARD_SZB +
       VG_DEFAULT_STACK_ACTIVE_SZB rounded down to the nearest 16-byte
       boundary.  And r1 is the original SP.  Set the SP to r16 and
       call _start_in_C_linux, passing it the initial SP. */
    "\tmr 3,1\n"
    "\tmr 1,16\n"
    "\tlis  14,   _start_in_C_linux@highest\n"
    "\tori  14,14,_start_in_C_linux@higher\n"
    "\tsldi 14,14,32\n"
    "\toris 14,14,_start_in_C_linux@h\n"
    "\tori  14,14,_start_in_C_linux@l\n"
    "\tld 14,0(14)\n"
    "\tmtctr 14\n"
    "\tbctrl\n"
    "\tnop\n"
    "\ttrap\n"
);
#elif defined(VGP_ppc64le_linux)
asm("\n"
    "\t.align 2\n"
    "\t.global _start\n"
    "\t.type _start,@function\n"
    "_start:\n"
    "#if _CALL_ELF == 2    \n"
    "0:  addis        2,12,.TOC.-0b@ha\n"
    "    addi         2,2,.TOC.-0b@l\n"
    "    .localentry  _start, .-_start\n"
    "#endif \n"
    
    "\tlis  16,   vgPlain_interim_stack@highest\n"
    "\tori  16,16,vgPlain_interim_stack@higher\n"
    "\tsldi 16,16,32\n"
    "\toris 16,16,vgPlain_interim_stack@h\n"
    "\tori  16,16,vgPlain_interim_stack@l\n"
    "\txor  17,17,17\n"
    "\tlis    17,("VG_STRINGIFY(VG_STACK_GUARD_SZB)" >> 16)\n"
    "\tori 17,17,("VG_STRINGIFY(VG_STACK_GUARD_SZB)" & 0xFFFF)\n"
    "\txor 18,18,18\n"
    "\tlis    18,("VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB)" >> 16)\n"
    "\tori 18,18,("VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB)" & 0xFFFF)\n"
    "\tadd 16,17,16\n"
    "\tadd 16,18,16\n"
    "\trldicr 16,16,0,59\n"
    /* now r16 = &vgPlain_interim_stack + VG_STACK_GUARD_SZB +
       VG_DEFAULT_STACK_ACTIVE_SZB rounded down to the nearest 16-byte
       boundary.  And r1 is the original SP.  Set the SP to r16 and
       call _start_in_C_linux, passing it the initial SP. */
    "\tmr 3,1\n"
    "\tmr 1,16\n"
    "\tlis  14,   _start_in_C_linux@highest\n"
    "\tori  14,14,_start_in_C_linux@higher\n"
    "\tsldi 14,14,32\n"
    "\toris 14,14,_start_in_C_linux@h\n"
    "\tori  14,14,_start_in_C_linux@l\n"
    "\tmtctr 14\n"
    "\tbctrl\n"
    "\tnop\n"
    "\ttrap\n"
);
#elif defined(VGP_s390x_linux)
asm("\n\t"
    ".text\n\t"
    ".globl _start\n\t"
    ".type  _start,@function\n\t"
    "_start:\n\t"
    
    "larl   %r1,  vgPlain_interim_stack\n\t"
    "larl   %r5,  1f\n\t"
    "ag     %r1,  0(%r5)\n\t"
    "ag     %r1,  2f-1f(%r5)\n\t"
    "nill   %r1,  0xFFF0\n\t"
    
    "lgr    %r2,  %r15\n\t"
    "lgr    %r15, %r1\n\t"
    
    "brasl  %r14, _start_in_C_linux\n\t"
    
    "j      .+2\n\t"
    "1:   .quad "VG_STRINGIFY(VG_STACK_GUARD_SZB)"\n\t"
    "2:   .quad "VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB)"\n\t"
    ".previous\n"
);
#elif defined(VGP_arm_linux)
asm("\n"
    "\t.text\n"
    "\t.align 4\n"
    "\t.type _start,#function\n"
    "\t.global _start\n"
    "_start:\n"
    "\tldr  r0, [pc, #36]\n"
    "\tldr  r1, [pc, #36]\n"
    "\tadd  r0, r1, r0\n"
    "\tldr  r1, [pc, #32]\n"
    "\tadd  r0, r1, r0\n"
    "\tmvn  r1, #15\n"
    "\tand  r0, r0, r1\n"
    "\tmov  r1, sp\n"
    "\tmov  sp, r0\n"
    "\tmov  r0, r1\n"
    "\tb _start_in_C_linux\n"
    "\t.word vgPlain_interim_stack\n"
    "\t.word "VG_STRINGIFY(VG_STACK_GUARD_SZB)"\n"
    "\t.word "VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB)"\n"
);
#elif defined(VGP_arm64_linux)
asm("\n"
    "\t.text\n"
    "\t.align 2\n"
    "\t.type _start,#function\n"
    "\t.global _start\n"
    "_start:\n"
    "\tadrp x0, vgPlain_interim_stack\n"
    "\tadd  x0, x0, :lo12:vgPlain_interim_stack\n"
    
    "\tmov  x1, (("VG_STRINGIFY(VG_STACK_GUARD_SZB)") >> 0) & 0xFFFF\n"
    "\tmovk x1, (("VG_STRINGIFY(VG_STACK_GUARD_SZB)") >> 16) & 0xFFFF,"
                " lsl 16\n"
    "\tadd  x0, x0, x1\n"
    
    "\tmov  x1, (("VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB)") >> 0) & 0xFFFF\n"
    "\tmovk x1, (("VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB)") >> 16) & 0xFFFF,"
                " lsl 16\n"
    "\tadd  x0, x0, x1\n"
    "\tand  x0, x0, -16\n"
    "\tmov  x1, sp\n"
    "\tmov  sp, x0\n"
    "\tmov  x0, x1\n"
    "\tb _start_in_C_linux\n"
);
#elif defined(VGP_mips32_linux)
asm("\n"
    "\t.type _gp_disp,@object\n"
    ".text\n"
    "\t.globl __start\n"
    "\t.type __start,@function\n"
    "__start:\n"

    "\tbal 1f\n"
    "\tnop\n"
    
    "1:\n"    

    "\tlui      $28, %hi(_gp_disp)\n"
    "\taddiu    $28, $28, %lo(_gp_disp)\n"
    "\taddu     $28, $28, $31\n"
    
    "\tlui      $9, %hi(vgPlain_interim_stack)\n"
    
    "\taddiu    $9, %lo(vgPlain_interim_stack)\n"


    "\tli    $10, "VG_STRINGIFY(VG_STACK_GUARD_SZB)"\n"
    "\tli    $11, "VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB)"\n"
    
    "\taddu     $9, $9, $10\n"
    "\taddu     $9, $9, $11\n"
    "\tli       $12, 0xFFFFFFF0\n"
    "\tand      $9, $9, $12\n"
    /* now t1/$9 = &vgPlain_interim_stack + VG_STACK_GUARD_SZB +
       VG_DEFAULT_STACK_ACTIVE_SZB rounded down to the nearest 16-byte
       boundary.  And $29 is the original SP.  Set the SP to t1 and
       call _start_in_C, passing it the initial SP. */
       
    "\tmove    $4, $29\n"     
    "\tmove    $29, $9\n"     
    
    "\tlui     $25, %hi(_start_in_C_linux)\n"
    "\taddiu   $25, %lo(_start_in_C_linux)\n"
    
    "\tbal  _start_in_C_linux\n"
    "\tbreak  0x7\n"
    ".previous\n"
);
#elif defined(VGP_mips64_linux)
asm(
".text\n"
".globl __start\n"
".type __start,@function\n"
"__start:\n"
    "\t.set noreorder\n"
    "\t.cpload $25\n"
    "\t.set reorder\n"
    "\t.cprestore 16\n"
    "\tlui    $9, %hi(vgPlain_interim_stack)\n"
    
    "\tdaddiu $9, %lo(vgPlain_interim_stack)\n"

    "\tli     $10, "VG_STRINGIFY(VG_STACK_GUARD_SZB)"\n"
    "\tli     $11, "VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB)"\n"

    "\tdaddu  $9, $9, $10\n"
    "\tdaddu  $9, $9, $11\n"
    "\tli     $12, 0xFFFFFF00\n"
    "\tand    $9, $9, $12\n"
    /* now t1/$9 = &vgPlain_interim_stack + VG_STACK_GUARD_SZB +
       VG_DEFAULT_STACK_ACTIVE_SZB rounded down to the nearest 16-byte
       boundary.  And $29 is the original SP.  Set the SP to t1 and
       call _start_in_C, passing it the initial SP. */

    "\tmove   $4, $29\n"     
    "\tmove   $29, $9\n"     

    "\tlui    $9, %highest(_start_in_C_linux)\n"
    "\tori    $9, %higher(_start_in_C_linux)\n"
    "\tdsll32 $9, $9, 0x0\n"
    "\tlui    $10, %hi(_start_in_C_linux)\n"
    "\tdaddiu $10, %lo(_start_in_C_linux)\n"
    "\tdaddu  $25, $9, $10\n"
    "\tjalr   $25\n"
    "\tnop\n"
".previous\n"
);
#elif defined(VGP_tilegx_linux)
asm("\n"
    ".text\n"
    "\t.align 8\n"
    "\t.globl _start\n"
    "\t.type _start,@function\n"
    "_start:\n"

    "\tjal 1f\n"
    "1:\n"

    
    
    "\tmoveli r19, hw2_last(vgPlain_interim_stack)\n"
    "\tshl16insli r19, r19, hw1(vgPlain_interim_stack)\n"
    "\tshl16insli r19, r19, hw0(vgPlain_interim_stack)\n"

    "\tmoveli r20, hw1("VG_STRINGIFY(VG_STACK_GUARD_SZB)")\n"
    "\tshl16insli r20, r20, hw0("VG_STRINGIFY(VG_STACK_GUARD_SZB)")\n"
    "\tmoveli r21, hw1("VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB)")\n"
    "\tshl16insli r21, r21, hw0("VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB)")\n"
    "\tadd     r19, r19, r20\n"
    "\tadd     r19, r19, r21\n"

    "\tmovei    r12, 0x0F\n"
    "\tnor      r12, zero, r12\n"

    "\tand      r19, r19, r12\n"

    /* now r19 = &vgPlain_interim_stack + VG_STACK_GUARD_SZB +
       VG_STACK_ACTIVE_SZB rounded down to the nearest 16-byte
       boundary.  And $54 is the original SP.  Set the SP to r0 and
       call _start_in_C, passing it the initial SP. */

    "\tmove    r0,  r54\n"    
    "\tmove    r54, r19\n"    

    "\tjal  _start_in_C_linux\n"
);
#else
#  error "Unknown linux platform"
#endif

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <elf.h>

__attribute__ ((used))
void _start_in_C_linux ( UWord* pArgc );
__attribute__ ((used))
void _start_in_C_linux ( UWord* pArgc )
{
   Int     r;
   Word    argc = pArgc[0];
   HChar** argv = (HChar**)&pArgc[1];
   HChar** envp = (HChar**)&pArgc[1+argc+1];

   
   
   
   
   
   
   
   INNER_REQUEST
      ((void) VALGRIND_STACK_REGISTER
       (&VG_(interim_stack).bytes[0],
        &VG_(interim_stack).bytes[0] + sizeof(VG_(interim_stack))));

   VG_(memset)( &the_iicii, 0, sizeof(the_iicii) );
   VG_(memset)( &the_iifii, 0, sizeof(the_iifii) );

   the_iicii.sp_at_startup = (Addr)pArgc;

#  if defined(VGP_ppc32_linux) || defined(VGP_ppc64be_linux) \
      || defined(VGP_ppc64le_linux) || defined(VGP_arm64_linux)
   {
      UWord *sp = &pArgc[1+argc+1];
      while (*sp++ != 0)
         ;
      for (; *sp != AT_NULL && *sp != AT_PAGESZ; sp += 2);
      if (*sp == AT_PAGESZ) {
         VKI_PAGE_SIZE = sp[1];
         for (VKI_PAGE_SHIFT = 12;
              VKI_PAGE_SHIFT <= VKI_MAX_PAGE_SHIFT; VKI_PAGE_SHIFT++)
            if (VKI_PAGE_SIZE == (1UL << VKI_PAGE_SHIFT))
         break;
      }
   }
#  endif

   r = valgrind_main( (Int)argc, argv, envp );
   
   VG_(exit)(r);
}



#elif defined(VGO_darwin)


#if defined(VGP_x86_darwin)
asm("\n"
    ".text\n"
    ".align 2,0x90\n"
    "\t.globl __start\n"
    "__start:\n"
    
    "\tmovl  $_vgPlain_interim_stack, %eax\n"
    "\taddl  $"VG_STRINGIFY(VG_STACK_GUARD_SZB)", %eax\n"
    "\taddl  $"VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB)", %eax\n"
    "\tsubl  $16, %eax\n"
    "\tandl  $~15, %eax\n"
    
    "\txchgl %eax, %esp\n"
    "\tsubl  $12, %esp\n"  
    
    "\tpushl %eax\n"
    "\tcall  __start_in_C_darwin\n"
    "\tint $3\n"
    "\tint $3\n"
);
#elif defined(VGP_amd64_darwin)
asm("\n"
    ".text\n"
    "\t.globl __start\n"
    ".align 3,0x90\n"
    "__start:\n"
    
    "\tmovabsq $_vgPlain_interim_stack, %rdi\n"
    "\taddq    $"VG_STRINGIFY(VG_STACK_GUARD_SZB)", %rdi\n"
    "\taddq    $"VG_STRINGIFY(VG_DEFAULT_STACK_ACTIVE_SZB)", %rdi\n"
    "\tandq    $~15, %rdi\n"
    
    "\txchgq %rdi, %rsp\n"
    
    "\tcall  __start_in_C_darwin\n"
    "\tint $3\n"
    "\tint $3\n"
);
#endif

void* __memcpy_chk(void *dest, const void *src, SizeT n, SizeT n2);
void* __memcpy_chk(void *dest, const void *src, SizeT n, SizeT n2) {
    
   return VG_(memcpy)(dest,src,n);
}
void* __memset_chk(void *s, int c, SizeT n, SizeT n2);
void* __memset_chk(void *s, int c, SizeT n, SizeT n2) {
    
  return VG_(memset)(s,c,n);
}
void bzero(void *s, SizeT n);
void bzero(void *s, SizeT n) {
    VG_(memset)(s,0,n);
}

void* memcpy(void *dest, const void *src, SizeT n);
void* memcpy(void *dest, const void *src, SizeT n) {
   return VG_(memcpy)(dest,src,n);
}
void* memset(void *s, int c, SizeT n);
void* memset(void *s, int c, SizeT n) {
  return VG_(memset)(s,c,n);
}

void _start_in_C_darwin ( UWord* pArgc );
void _start_in_C_darwin ( UWord* pArgc )
{
   Int     r;
   Int     argc = *(Int *)pArgc;  
   HChar** argv = (HChar**)&pArgc[1];
   HChar** envp = (HChar**)&pArgc[1+argc+1];

   
   INNER_REQUEST
      ((void) VALGRIND_STACK_REGISTER
       (&VG_(interim_stack).bytes[0],
        &VG_(interim_stack).bytes[0] + sizeof(VG_(interim_stack))));

   VG_(memset)( &the_iicii, 0, sizeof(the_iicii) );
   VG_(memset)( &the_iifii, 0, sizeof(the_iifii) );

   the_iicii.sp_at_startup = (Addr)pArgc;

   r = valgrind_main( (Int)argc, argv, envp );
   
   VG_(exit)(r);
}


#else

#  error "Unknown OS"
#endif



#if defined(VGP_x86_darwin)

/* Routines for doing signed/unsigned 64 x 64 ==> 64 div and mod
   (udivdi3, umoddi3, divdi3, moddi3) using only 32 x 32 ==> 32
   division.  Cobbled together from

   http://www.hackersdelight.org/HDcode/divlu.c
   http://www.hackersdelight.org/HDcode/divls.c
   http://www.hackersdelight.org/HDcode/newCode/divDouble.c

   The code from those three files is covered by the following license,
   as it appears at:

   http://www.hackersdelight.org/permissions.htm

      You are free to use, copy, and distribute any of the code on
      this web site, whether modified by you or not. You need not give
      attribution. This includes the algorithms (some of which appear
      in Hacker's Delight), the Hacker's Assistant, and any code
      submitted by readers. Submitters implicitly agree to this.
*/


static Int nlz32(UInt x) 
{
   Int n;
   if (x == 0) return(32);
   n = 0;
   if (x <= 0x0000FFFF) {n = n +16; x = x <<16;}
   if (x <= 0x00FFFFFF) {n = n + 8; x = x << 8;}
   if (x <= 0x0FFFFFFF) {n = n + 4; x = x << 4;}
   if (x <= 0x3FFFFFFF) {n = n + 2; x = x << 2;}
   if (x <= 0x7FFFFFFF) {n = n + 1;}
   return n;
}

static UInt divlu2(UInt u1, UInt u0, UInt v, UInt *r)
{
   const UInt b = 65536;     
   UInt un1, un0,            
        vn1, vn0,            
        q1, q0,              
        un32, un21, un10,    
        rhat;                
   Int s;                    

   if (u1 >= v) {            
      if (r != NULL)         
         *r = 0xFFFFFFFF;    
      return 0xFFFFFFFF;}    

   s = nlz32(v);             
   v = v << s;               
   vn1 = v >> 16;            
   vn0 = v & 0xFFFF;         

   un32 = (u1 << s) | ((u0 >> (32 - s)) & (-s >> 31));
   un10 = u0 << s;           

   un1 = un10 >> 16;         
   un0 = un10 & 0xFFFF;      

   q1 = un32/vn1;            
   rhat = un32 - q1*vn1;     
 again1:
   if (q1 >= b || q1*vn0 > b*rhat + un1) {
     q1 = q1 - 1;
     rhat = rhat + vn1;
     if (rhat < b) goto again1;}

   un21 = un32*b + un1 - q1*v;  

   q0 = un21/vn1;            
   rhat = un21 - q0*vn1;     
 again2:
   if (q0 >= b || q0*vn0 > b*rhat + un0) {
     q0 = q0 - 1;
     rhat = rhat + vn1;
     if (rhat < b) goto again2;}

   if (r != NULL)            
      *r = (un21*b + un0 - q0*v) >> s;     
   return q1*b + q0;
}


static Int divls(Int u1, UInt u0, Int v, Int *r)
{
   Int q, uneg, vneg, diff, borrow;

   uneg = u1 >> 31;          
   if (uneg) {               
      u0 = -u0;              
      borrow = (u0 != 0);
      u1 = -u1 - borrow;}

   vneg = v >> 31;           
   v = (v ^ vneg) - vneg;    

   if ((UInt)u1 >= (UInt)v) goto overflow;

   q = divlu2(u1, u0, v, (UInt *)r);

   diff = uneg ^ vneg;       
   q = (q ^ diff) - diff;    
   if (uneg && r != NULL)
      *r = -*r;

   if ((diff ^ q) < 0 && q != 0) {  
 overflow:                    
      if (r != NULL)         
         *r = 0x80000000;    
      q = 0x80000000;}       
   return q;
}





static Int nlz64(ULong x) 
{
   Int n;
   if (x == 0) return(64);
   n = 0;
   if (x <= 0x00000000FFFFFFFFULL) {n = n + 32; x = x << 32;}
   if (x <= 0x0000FFFFFFFFFFFFULL) {n = n + 16; x = x << 16;}
   if (x <= 0x00FFFFFFFFFFFFFFULL) {n = n +  8; x = x <<  8;}
   if (x <= 0x0FFFFFFFFFFFFFFFULL) {n = n +  4; x = x <<  4;}
   if (x <= 0x3FFFFFFFFFFFFFFFULL) {n = n +  2; x = x <<  2;}
   if (x <= 0x7FFFFFFFFFFFFFFFULL) {n = n +  1;}
   return n;
}




static UInt DIVU ( ULong u, UInt v )
{
  UInt uHi = (UInt)(u >> 32);
  UInt uLo = (UInt)u;
  return divlu2(uHi, uLo, v, NULL);
}

static Int DIVS ( Long u, Int v )
{
  Int  uHi = (Int)(u >> 32);
  UInt uLo = (UInt)u;
  return divls(uHi, uLo, v, NULL);
}

static ULong udivdi3(ULong u, ULong v)
{
   ULong u0, u1, v1, q0, q1, k, n;

   if (v >> 32 == 0) {          
      if (u >> 32 < v)          
         return DIVU(u, v)      
            & 0xFFFFFFFF;
      else {                    
         u1 = u >> 32;          
         u0 = u & 0xFFFFFFFF;   
         q1 = DIVU(u1, v)       
            & 0xFFFFFFFF;
         k = u1 - q1*v;         
         q0 = DIVU((k << 32) + u0, v) 
            & 0xFFFFFFFF;
         return (q1 << 32) + q0;
      }
   }
                                
   n = nlz64(v);                
   v1 = (v << n) >> 32;         
                                
   u1 = u >> 1;                 
   q1 = DIVU(u1, v1)            
       & 0xFFFFFFFF;            
   q0 = (q1 << n) >> 31;        
                                
   if (q0 != 0)                 
      q0 = q0 - 1;              
   if ((u - q0*v) >= v)
      q0 = q0 + 1;              
   return q0;
}





static ULong my_llabs ( Long x )
{
   ULong t = x >> 63;
   return (x ^ t) - t;
}

static Long divdi3(Long u, Long v)
{
   ULong au, av;
   Long q, t;
   au = my_llabs(u);
   av = my_llabs(v);
   if (av >> 31 == 0) {         
   
      if (au < av << 31) {      
         q = DIVS(u, v);        
         return (q << 32) >> 32;
      }
   }
   q = udivdi3(au,av);          
   t = (u ^ v) >> 63;           
   return (q ^ t) - t;          
}


ULong __udivdi3 (ULong u, ULong v);
ULong __udivdi3 (ULong u, ULong v)
{
  return udivdi3(u,v);
}

Long __divdi3 (Long u, Long v);
Long __divdi3 (Long u, Long v)
{
  return divdi3(u,v);
}

ULong __umoddi3 (ULong u, ULong v);
ULong __umoddi3 (ULong u, ULong v)
{
  ULong q = __udivdi3(u, v);
  ULong r = u - q * v;
  return r;
}

Long __moddi3 (Long u, Long v);
Long __moddi3 (Long u, Long v)
{
  Long q = __divdi3(u, v);
  Long r = u - q * v;
  return r;
}


/* ===-- fixunsdfdi.c - Implement __fixunsdfdi -----------------------------===
 *
 *                     The LLVM Compiler Infrastructure
 *
 * This file is dual licensed under the MIT and the University of Illinois Open
 * Source Licenses. See LICENSE.TXT for details.
 *
 * ===----------------------------------------------------------------------===
 *
 * This file implements __fixunsdfdi for the compiler_rt library.
 *
 * ===----------------------------------------------------------------------===
 */

/* As per http://www.gnu.org/licenses/license-list.html#GPLCompatibleLicenses,

   the "NCSA/University of Illinois Open Source License" is compatible
   with the GPL (both version 2 and 3).  What is claimed to be
   compatible is this

   http://www.opensource.org/licenses/UoI-NCSA.php

   and the LLVM documentation at

   http://www.llvm.org/docs/DeveloperPolicy.html#license

   says all the code in LLVM is available under the University of
   Illinois/NCSA Open Source License, at this URL

   http://www.opensource.org/licenses/UoI-NCSA.php

   viz, the same one that the FSF pages claim is compatible.  So I
   think it's OK to include it.
*/




typedef unsigned long long du_int;
typedef unsigned su_int;

typedef union
{
    du_int all;
    struct
    {
#if VG_LITTLEENDIAN
        su_int low;
        su_int high;
#else
        su_int high;
        su_int low;
#endif 
    }s;
} udwords;

typedef union
{
    udwords u;
    double  f;
} double_bits;

du_int __fixunsdfdi(double a);

du_int
__fixunsdfdi(double a)
{
    double_bits fb;
    fb.f = a;
    int e = ((fb.u.s.high & 0x7FF00000) >> 20) - 1023;
    if (e < 0 || (fb.u.s.high & 0x80000000))
        return 0;
    udwords r;
    r.s.high = (fb.u.s.high & 0x000FFFFF) | 0x00100000;
    r.s.low = fb.u.s.low;
    if (e > 52)
        r.all <<= (e - 52);
    else
        r.all >>= (52 - e);
    return r.all;
}


#endif



#if defined(VGO_darwin) && DARWIN_VERS == DARWIN_10_10

UWord voucher_mach_msg_set ( UWord arg1 );
UWord voucher_mach_msg_set ( UWord arg1 )
{
   return 0;
}

#endif


