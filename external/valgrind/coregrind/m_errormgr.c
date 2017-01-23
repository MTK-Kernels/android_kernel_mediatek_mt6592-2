

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
#include "pub_core_threadstate.h"      
#include "pub_core_debugger.h"
#include "pub_core_debuginfo.h"
#include "pub_core_debuglog.h"
#include "pub_core_errormgr.h"
#include "pub_core_execontext.h"
#include "pub_core_gdbserver.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcfile.h"
#include "pub_core_libcprint.h"
#include "pub_core_libcproc.h"         
#include "pub_core_seqmatch.h"
#include "pub_core_mallocfree.h"
#include "pub_core_options.h"
#include "pub_core_stacktrace.h"
#include "pub_core_tooliface.h"
#include "pub_core_translate.h"        
#include "pub_core_xarray.h"           

#define DEBUG_ERRORMGR 0 


#define M_COLLECT_ERRORS_SLOWLY_AFTER 100

#define M_COLLECT_NO_ERRORS_AFTER_SHOWN 1000

#define M_COLLECT_NO_ERRORS_AFTER_FOUND 10000000

static Error* errors = NULL;

static Supp* suppressions = NULL;

static UInt n_errs_found = 0;

static UInt n_errs_suppressed = 0;

static UInt n_errs_shown = 0;

static UInt n_err_contexts = 0;

static UInt n_supp_contexts = 0;


static Supp* is_suppressible_error ( const Error* err );

static ThreadId last_tid_printed = 1;

static UWord em_errlist_searches = 0;

static UWord em_errlist_cmps = 0;

static UWord em_supplist_searches = 0;

static UWord em_supplist_cmps = 0;


struct _Error {
   struct _Error* next;
   
   
   
   UInt unique;
   
   Supp* supp;
   Int count;

   
   ThreadId tid;           
   ExeContext* where;      
   ErrorKind ekind;        
   Addr addr;              
   const HChar* string;    
   void* extra;            
};


ExeContext* VG_(get_error_where) ( const Error* err )
{
   return err->where;
}

ErrorKind VG_(get_error_kind) ( const Error* err )
{
   return err->ekind;
}

Addr VG_(get_error_address) ( const Error* err )
{
   return err->addr;
}

const HChar* VG_(get_error_string) ( const Error* err )
{
   return err->string;
}

void* VG_(get_error_extra)  ( const Error* err )
{
   return err->extra;
}

UInt VG_(get_n_errs_found)( void )
{
   return n_errs_found;
}

UInt VG_(get_n_errs_shown)( void )
{
   return n_errs_shown;
}


typedef
   enum {
      
      
      
      ThreadSupp = -1,    
   }
   CoreSuppKind;

#define VG_MAX_SUPP_CALLERS  24

typedef
   enum { 
      NoName,     
      ObjName,    
      FunName,    
      DotDotDot   
   }
   SuppLocTy;

typedef
   struct {
      SuppLocTy ty;
      Bool      name_is_simple_str; 
      HChar*    name; 
   }
   SuppLoc;

struct _Supp {
   struct _Supp* next;
   Int count;     
   HChar* sname;  

   
   
   Int    clo_suppressions_i;
   Int    sname_lineno;

   
   Int n_callers;
   
   
   SuppLoc* callers;

   
   SuppKind skind;   
   HChar* string;    
   void* extra;      
};

SuppKind VG_(get_supp_kind) ( const Supp* su )
{
   return su->skind;
}

HChar* VG_(get_supp_string) ( const Supp* su )
{
   return su->string;
}

void* VG_(get_supp_extra)  ( const Supp* su )
{
   return su->extra;
}


void VG_(set_supp_kind)   ( Supp* su, SuppKind skind )
{
   su->skind = skind;
}

void VG_(set_supp_string) ( Supp* su, HChar* string )
{
   su->string = string;
}

void VG_(set_supp_extra)  ( Supp* su, void* extra )
{
   su->extra = extra;
}



Bool VG_(showing_core_errors)(void)
{
   return VG_(needs).core_errors && VG_(clo_verbosity) >= 1 && !VG_(clo_xml);
}

static Bool eq_Error ( VgRes res, const Error* e1, const Error* e2 )
{
   if (e1->ekind != e2->ekind) 
      return False;
   if (!VG_(eq_ExeContext)(res, e1->where, e2->where))
      return False;

   switch (e1->ekind) {
      
      
      
      
      default: 
         if (VG_(needs).tool_errors) {
            return VG_TDICT_CALL(tool_eq_Error, res, e1, e2);
         } else {
            VG_(printf)("\nUnhandled error type: %u. VG_(needs).tool_errors\n"
                        "probably needs to be set.\n",
                        e1->ekind);
            VG_(core_panic)("unhandled error type");
         }
   }
}


#define ERRTXT_LEN   4096

static void printSuppForIp_XML(UInt n, Addr ip, void* uu_opaque)
{
   const HChar *buf;
   InlIPCursor* iipc = VG_(new_IIPC)(ip);
   do {
      if ( VG_(get_fnname_no_cxx_demangle) (ip, &buf, iipc) ) {
         VG_(printf_xml)("    <sframe> <fun>%pS</fun> </sframe>\n", buf);
      } else
      if ( VG_(get_objname)(ip, &buf) ) {
         VG_(printf_xml)("    <sframe> <obj>%pS</obj> </sframe>\n", buf);
      } else {
         VG_(printf_xml)("    <sframe> <obj>*</obj> </sframe>\n");
      }
   } while (VG_(next_IIPC)(iipc));
   VG_(delete_IIPC)(iipc);
}

static void printSuppForIp_nonXML(UInt n, Addr ip, void* textV)
{
   const HChar *buf;
   XArray*  text = (XArray*)textV;
   InlIPCursor* iipc = VG_(new_IIPC)(ip);
   do {
      if ( VG_(get_fnname_no_cxx_demangle) (ip, &buf, iipc) ) {
         VG_(xaprintf)(text, "   fun:%s\n", buf);
      } else
      if ( VG_(get_objname)(ip, &buf) ) {
         VG_(xaprintf)(text, "   obj:%s\n", buf);
      } else {
         VG_(xaprintf)(text, "   obj:*\n");
      }
   } while (VG_(next_IIPC)(iipc));
   VG_(delete_IIPC)(iipc);
}

static void gen_suppression(const Error* err)
{
   const HChar* name;
   ExeContext* ec;
   XArray*  text;

   const HChar* dummy_name = "insert_a_suppression_name_here";

   vg_assert(err);

   ec = VG_(get_error_where)(err);
   vg_assert(ec);

   name = VG_TDICT_CALL(tool_get_error_name, err);
   if (NULL == name) {
      VG_(umsg)("(%s does not allow error to be suppressed)\n",
                VG_(details).name);
      return;
   }

   text = VG_(newXA)( VG_(malloc), "errormgr.gen_suppression.1",
                      VG_(free), sizeof(HChar) );

   
   VG_(xaprintf)(text, "{\n");
   VG_(xaprintf)(text, "   <%s>\n", dummy_name);
   VG_(xaprintf)(text, "   %s:%s\n", VG_(details).name, name);

   HChar       *xtra = NULL;
   SizeT       xtra_size = 0;
   SizeT       num_written;

   do {
      xtra_size += 256;
      xtra = VG_(realloc)("errormgr.gen_suppression.2", xtra,xtra_size);
      num_written = VG_TDICT_CALL(tool_get_extra_suppression_info,
                                  err, xtra, xtra_size);
   } while (num_written == xtra_size);  

   
   vg_assert(xtra[num_written] == '\0');

   if (num_written)
      VG_(xaprintf)(text, "   %s\n", xtra);

   
   UInt n_ips = VG_(get_ExeContext_n_ips)(ec);
   vg_assert(n_ips > 0);
   if (n_ips > VG_MAX_SUPP_CALLERS)
      n_ips = VG_MAX_SUPP_CALLERS;
   VG_(apply_StackTrace)(printSuppForIp_nonXML,
                         text,
                         VG_(get_ExeContext_StackTrace)(ec),
                         n_ips);

   VG_(xaprintf)(text, "}\n");
   
   VG_(xaprintf)(text, "%c", (HChar)0 );
   

   
   if (! VG_(clo_xml) ) {

      
      VG_(printf)("%s", (HChar*) VG_(indexXA)(text, 0) );

   } else {

      VG_(printf_xml)("  <suppression>\n");
      VG_(printf_xml)("    <sname>%s</sname>\n", dummy_name);
      VG_(printf_xml)(
                      "    <skind>%pS:%pS</skind>\n", VG_(details).name, name);
      if (num_written)
         VG_(printf_xml)("    <skaux>%pS</skaux>\n", xtra);

      
      VG_(apply_StackTrace)(printSuppForIp_XML,
                            NULL,
                            VG_(get_ExeContext_StackTrace)(ec),
                            VG_(get_ExeContext_n_ips)(ec));

      
      
      
      VG_(printf_xml)("    <rawtext>\n");
      VG_(printf_xml)("<![CDATA[\n");
      VG_(printf_xml)("%s", (HChar*) VG_(indexXA)(text, 0) );
      VG_(printf_xml)("]]>\n");
      VG_(printf_xml)("    </rawtext>\n");
      VG_(printf_xml)("  </suppression>\n");

   }

   VG_(deleteXA)(text);
   VG_(free)(xtra);
}


Bool VG_(is_action_requested) ( const HChar* action, Bool* clo )
{
   HChar ch, ch2;
   Int res;

   if (VG_(clo_xml))
      return False; 

   if (*clo == False)
      return False;

   VG_(umsg)("\n");

  again:
   VG_(printf)(
      "==%d== "
      "---- %s ? --- [Return/N/n/Y/y/C/c] ---- ", 
      VG_(getpid)(), action
   );

   res = VG_(read)(VG_(clo_input_fd), &ch, 1);
   if (res != 1) goto ioerror;
   
   if (ch == '\n') return False;
   if (ch != 'N' && ch != 'n' && ch != 'Y' && ch != 'y' 
      && ch != 'C' && ch != 'c') goto again;

   res = VG_(read)(VG_(clo_input_fd), &ch2, 1);
   if (res != 1) goto ioerror;
   if (ch2 != '\n') goto again;

   
   if (ch == 'n' || ch == 'N') return False;
   
   if (ch == 'y' || ch == 'Y') return True;
   
   vg_assert(ch == 'c' || ch == 'C');

  ioerror:
   *clo = False;
   return False;
}


static 
void do_actions_on_error(const Error* err, Bool allow_db_attach)
{
   Bool still_noisy = True;

   
   if (VG_(clo_vgdb) != Vg_VgdbNo
       && allow_db_attach 
       && VG_(dyn_vgdb_error) <= n_errs_shown) {
      VG_(umsg)("(action on error) vgdb me ... \n");
      VG_(gdbserver)( err->tid );
      VG_(umsg)("Continuing ...\n");
   }

   
   if (allow_db_attach &&
       VG_(is_action_requested)( "Attach to debugger", & VG_(clo_db_attach) ))
   {   
      if (0) VG_(printf)("starting debugger\n");
      VG_(start_debugger)( err->tid );
   }  
   
   if (VG_(clo_gen_suppressions) == 2
       || (VG_(clo_gen_suppressions) == 1
           && VG_(is_action_requested)( "Print suppression", &still_noisy ))
      ) {
      gen_suppression(err);
   }
   if (VG_(clo_gen_suppressions) == 1 && !still_noisy)
      VG_(clo_gen_suppressions) = 0;
}


static void pp_Error ( const Error* err, Bool allow_db_attach, Bool xml )
{
   vg_assert(VG_(needs).tool_errors);

   if (xml) {

      vg_assert( VG_(clo_gen_suppressions) == 0 
                 || VG_(clo_gen_suppressions) == 2  );

      
      VG_TDICT_CALL( tool_before_pp_Error, err );
   
      
      VG_(printf_xml)("<error>\n");
      VG_(printf_xml)("  <unique>0x%x</unique>\n", err->unique);
      VG_(printf_xml)("  <tid>%d</tid>\n", err->tid);
      ThreadState* tst = VG_(get_ThreadState)(err->tid);
      if (tst->thread_name) {
         VG_(printf_xml)("  <threadname>%s</threadname>\n", tst->thread_name);
      }

      
      VG_TDICT_CALL( tool_pp_Error, err );

      if (VG_(clo_gen_suppressions) > 0)
        gen_suppression(err);

      
      VG_(printf_xml)("</error>\n");
      VG_(printf_xml)("\n");

   } else {

      if (VG_(clo_error_markers)[0])
         VG_(umsg)("%s\n", VG_(clo_error_markers)[0]);
      VG_TDICT_CALL( tool_before_pp_Error, err );

      if (VG_(tdict).tool_show_ThreadIDs_for_errors
          && err->tid > 0 && err->tid != last_tid_printed) {
         ThreadState* tst = VG_(get_ThreadState)(err->tid);
         if (tst->thread_name) {
            VG_(umsg)("Thread %d %s:\n", err->tid, tst->thread_name );
         } else {
            VG_(umsg)("Thread %d:\n", err->tid );
         }
         last_tid_printed = err->tid;
      }
   
      VG_TDICT_CALL( tool_pp_Error, err );
      VG_(umsg)("\n");
      if (VG_(clo_error_markers)[1])
         VG_(umsg)("%s\n", VG_(clo_error_markers)[1]);

   }

   do_actions_on_error(err, allow_db_attach);
}


static
void construct_error ( Error* err, ThreadId tid, ErrorKind ekind, Addr a,
                       const HChar* s, void* extra, ExeContext* where )
{
   
   static UInt unique_counter = 0;

   vg_assert(tid < VG_N_THREADS);

   
   err->unique   = unique_counter++;
   err->next     = NULL;
   err->supp     = NULL;
   err->count    = 1;
   err->tid      = tid;
   if (NULL == where)
     err->where = VG_(record_ExeContext)( tid, 0 );
   else
      err->where = where;

   
   err->ekind  = ekind;
   err->addr   = a;
   err->extra  = extra;
   err->string = s;

   
   vg_assert( tid < VG_N_THREADS );
}



void VG_(maybe_record_error) ( ThreadId tid, 
                               ErrorKind ekind, Addr a, 
                               const HChar* s, void* extra )
{
          Error  err;
          Error* p;
          Error* p_prev;
          UInt   extra_size;
          VgRes  exe_res          = Vg_MedRes;
   static Bool   stopping_message = False;
   static Bool   slowdown_message = False;

   if (VG_(clo_error_limit) 
       && (n_errs_shown >= M_COLLECT_NO_ERRORS_AFTER_SHOWN
           || n_errs_found >= M_COLLECT_NO_ERRORS_AFTER_FOUND)
       && !VG_(clo_xml)) {
      if (!stopping_message) {
         VG_(umsg)("\n");

	 if (n_errs_shown >= M_COLLECT_NO_ERRORS_AFTER_SHOWN) {
            VG_(umsg)(
               "More than %d different errors detected.  "
               "I'm not reporting any more.\n",
               M_COLLECT_NO_ERRORS_AFTER_SHOWN );
         } else {
            VG_(umsg)(
               "More than %d total errors detected.  "
               "I'm not reporting any more.\n",
               M_COLLECT_NO_ERRORS_AFTER_FOUND );
	 }

         VG_(umsg)("Final error counts will be inaccurate.  "
                   "Go fix your program!\n");
         VG_(umsg)("Rerun with --error-limit=no to disable "
                   "this cutoff.  Note\n");
         VG_(umsg)("that errors may occur in your program without "
                   "prior warning from\n");
         VG_(umsg)("Valgrind, because errors are no longer "
                   "being displayed.\n");
         VG_(umsg)("\n");
         stopping_message = True;
      }
      return;
   }

   
   { ThreadState* tst = VG_(get_ThreadState)(tid);
     if (tst->err_disablement_level > 0)
        return;
   }

   if (n_errs_shown >= M_COLLECT_ERRORS_SLOWLY_AFTER
       && !VG_(clo_xml)) {
      exe_res = Vg_LowRes;
      if (!slowdown_message) {
         VG_(umsg)("\n");
         VG_(umsg)("More than %d errors detected.  Subsequent errors\n",
                   M_COLLECT_ERRORS_SLOWLY_AFTER);
         VG_(umsg)("will still be recorded, but in less "
                   "detail than before.\n");
         slowdown_message = True;
      }
   }

   
   construct_error ( &err, tid, ekind, a, s, extra, NULL );

   
   em_errlist_searches++;
   p       = errors;
   p_prev  = NULL;
   while (p != NULL) {
      em_errlist_cmps++;
      if (eq_Error(exe_res, p, &err)) {
         
         p->count++;
	 if (p->supp != NULL) {
            
            p->supp->count++;
            n_errs_suppressed++;	 
         } else {
            n_errs_found++;
         }

         if (p_prev != NULL) {
            vg_assert(p_prev->next == p);
            p_prev->next = p->next;
            p->next      = errors;
            errors       = p;
	 }

         return;
      }
      p_prev = p;
      p      = p->next;
   }

   


   
   p = VG_(malloc)("errormgr.mre.1", sizeof(Error));
   *p = err;

   
   switch (ekind) {
      
      
      
      
      
      default:
         vg_assert(VG_(needs).tool_errors);
         extra_size = VG_TDICT_CALL(tool_update_extra, p);
         break;
   }

   if (NULL != p->string) {
      p->string = VG_(strdup)("errormgr.mre.2", p->string);
   }

   
   if (NULL != p->extra && 0 != extra_size) { 
      void* new_extra = VG_(malloc)("errormgr.mre.3", extra_size);
      VG_(memcpy)(new_extra, p->extra, extra_size);
      p->extra = new_extra;
   }

   p->next = errors;
   p->supp = is_suppressible_error(&err);
   errors  = p;
   if (p->supp == NULL) {
      
      n_err_contexts++;
      n_errs_found++;
      n_errs_shown++;
      
      pp_Error( p, True, VG_(clo_xml) );
   } else {
      n_supp_contexts++;
      n_errs_suppressed++;
      p->supp->count++;
   }
}

Bool VG_(unique_error) ( ThreadId tid, ErrorKind ekind, Addr a, const HChar* s,
                         void* extra, ExeContext* where, Bool print_error,
                         Bool allow_db_attach, Bool count_error )
{
   Error err;
   Supp *su;

   
   ThreadState* tst = VG_(get_ThreadState)(tid);
   if (tst->err_disablement_level > 0)
      return False; 
   
   
   construct_error ( &err, tid, ekind, a, s, extra, where );

   (void)VG_TDICT_CALL(tool_update_extra, &err);

   su = is_suppressible_error(&err);
   if (NULL == su) {
      if (count_error) {
         n_errs_found++;
         n_err_contexts++;
      }

      if (print_error) {
         
         n_errs_shown++;
         
         pp_Error(&err, allow_db_attach, VG_(clo_xml));
      }
      return False;

   } else {
      if (count_error) {
         n_errs_suppressed++;
         n_supp_contexts++;
      }
      su->count++;
      return True;
   }
}



static Bool show_used_suppressions ( void )
{
   Supp  *su;
   Bool  any_supp;

   if (VG_(clo_xml))
      VG_(printf_xml)("<suppcounts>\n");

   any_supp = False;
   for (su = suppressions; su != NULL; su = su->next) {
      if (su->count <= 0)
         continue;
      if (VG_(clo_xml)) {
         VG_(printf_xml)( "  <pair>\n"
                                 "    <count>%d</count>\n"
                                 "    <name>%pS</name>\n"
                                 "  </pair>\n",
                                 su->count, su->sname );
      } else {
         HChar      *xtra = NULL;
         Int         xtra_size = 0;
         SizeT       num_written;
         
         if (!any_supp)
            VG_(dmsg)("\n");

         do {
            xtra_size += 256;
            xtra = VG_(realloc)("errormgr.sus.1", xtra, xtra_size);
            num_written = VG_TDICT_CALL(tool_print_extra_suppression_use,
                                        su, xtra, xtra_size);
         } while (num_written == xtra_size); 

         
         vg_assert(xtra[num_written] == '\0');

         HChar *filename = *(HChar**) VG_(indexXA)(VG_(clo_suppressions),
                                                   su->clo_suppressions_i);
         VG_(dmsg)("used_suppression: %6d %s %s:%d%s%s\n", su->count, su->sname,
                   filename,
                   su->sname_lineno,
                   num_written ? " " : "", xtra);
         VG_(free)(xtra);
      }
      any_supp = True;
   }

   if (VG_(clo_xml))
      VG_(printf_xml)("</suppcounts>\n");

   return any_supp;
}

void VG_(show_all_errors) (  Int verbosity, Bool xml )
{
   Int    i, n_min;
   Error *p, *p_min;
   Bool   any_supp;

   if (verbosity == 0)
      return;

   
   if (xml) {
      (void)show_used_suppressions();
      return;
   }

   
   VG_(umsg)("ERROR SUMMARY: "
             "%d errors from %d contexts (suppressed: %d from %d)\n",
             n_errs_found, n_err_contexts, 
             n_errs_suppressed, n_supp_contexts );

   if (verbosity <= 1)
      return;

   
   

   for (i = 0; i < n_err_contexts; i++) {
      n_min = (1 << 30) - 1;
      p_min = NULL;
      for (p = errors; p != NULL; p = p->next) {
         if (p->supp != NULL) continue;
         if (p->count < n_min) {
            n_min = p->count;
            p_min = p;
         }
      }
      
      if (p_min == NULL) continue; 

      VG_(umsg)("\n");
      VG_(umsg)("%d errors in context %d of %d:\n",
                p_min->count, i+1, n_err_contexts);
      pp_Error( p_min, False, False  );

      
      vg_assert(! xml);

      if ((i+1 == VG_(clo_dump_error))) {
         StackTrace ips = VG_(get_ExeContext_StackTrace)(p_min->where);
         VG_(translate) ( 0 ,
                          ips[0], True, 0xFE,
                          0,
                          True);
      }

      p_min->count = p_min->count + (1 << 30);
   } 

    
   for (p = errors; p != NULL; p = p->next) {
      if (p->count >= (1 << 30))
         p->count = p->count - (1 << 30);
   }


   any_supp = show_used_suppressions();

   if (any_supp) 
      VG_(umsg)("\n");
   
   
   VG_(umsg)("ERROR SUMMARY: "
             "%d errors from %d contexts (suppressed: %d from %d)\n",
             n_errs_found, n_err_contexts, n_errs_suppressed,
             n_supp_contexts );
}

void VG_(show_last_error) ( void )
{
   if (n_err_contexts == 0) {
      VG_(umsg)("No errors yet\n");
      return;
   }

   pp_Error( errors, False, False );
}


void VG_(show_error_counts_as_XML) ( void )
{
   Error* err;
   VG_(printf_xml)("<errorcounts>\n");
   for (err = errors; err != NULL; err = err->next) {
      if (err->supp != NULL)
         continue;
      if (err->count <= 0)
         continue;
      VG_(printf_xml)("  <pair>\n");
      VG_(printf_xml)("    <count>%d</count>\n", err->count);
      VG_(printf_xml)("    <unique>0x%x</unique>\n", err->unique);
      VG_(printf_xml)("  </pair>\n");
   }
   VG_(printf_xml)("</errorcounts>\n");
   VG_(printf_xml)("\n");
}




static Int get_char ( Int fd, HChar* out_buf )
{
   Int r;
   static HChar buf[256];
   static Int buf_size = 0;
   static Int buf_used = 0;
   vg_assert(buf_size >= 0 && buf_size <= sizeof buf);
   vg_assert(buf_used >= 0 && buf_used <= buf_size);
   if (buf_used == buf_size) {
      r = VG_(read)(fd, buf, sizeof buf);
      if (r < 0) return r; 
      vg_assert(r >= 0 && r <= sizeof buf);
      buf_size = r;
      buf_used = 0;
   }
   if (buf_size == 0)
     return 0; 
   vg_assert(buf_size >= 0 && buf_size <= sizeof buf);
   vg_assert(buf_used >= 0 && buf_used < buf_size);
   *out_buf = buf[buf_used];
   buf_used++;
   return 1;
}

static Bool get_nbnc_line ( Int fd, HChar** bufpp, SizeT* nBufp, Int* lineno )
{
   HChar* buf  = *bufpp;
   SizeT nBuf = *nBufp;
   HChar  ch;
   Int   n, i;

   vg_assert(lineno); 

   while (True) {
      buf[0] = 0;
      
      while (True) {
         n = get_char(fd, &ch);
         if (n == 1 && !VG_(isspace)(ch)) break;
         if (n == 1 && ch == '\n')
            (*lineno)++;
         if (n <= 0) return True;
      }

      
      i = 0;
      buf[i++] = ch; buf[i] = 0;
      while (True) {
         n = get_char(fd, &ch);
         if (n <= 0) return False; 
         if (ch == '\n')
            (*lineno)++;
         if (ch == '\n') break;
         if (i > 0 && i == nBuf-1) {
            *nBufp = nBuf = nBuf * 2;
            #define RIDICULOUS   100000
            vg_assert2(nBuf < RIDICULOUS,  
               "VG_(get_line): line longer than %d chars, aborting\n",
               RIDICULOUS);
            *bufpp = buf = VG_(realloc)("errormgr.get_line.1", buf, nBuf);
         }
         buf[i++] = ch; buf[i] = 0;
      }
      while (i > 1 && VG_(isspace)(buf[i-1])) { 
         i--; buf[i] = 0; 
      };

      
      if (buf[0] != '#') return False;
   }
}

static Bool is_location_line (const HChar* buf)
{
   return VG_(strncmp)(buf, "fun:", 4) == 0
      || VG_(strncmp)(buf, "obj:", 4) == 0
      || VG_(strcmp)(buf, "...") == 0;
}

Bool VG_(get_line) ( Int fd, HChar** bufpp, SizeT* nBufp, Int* lineno )
{
   Bool eof = get_nbnc_line (fd, bufpp, nBufp, lineno);

   if (eof)
      return True;

   if (is_location_line(*bufpp))
      return True; 
   else
      return False; 
}

static Bool is_simple_str (const HChar *s)
{
   while (*s) {
      if (*s == '?' || *s == '*')
         return False;
      s++;
   }
   return True;
}

static Bool setLocationTy ( SuppLoc* p, const HChar *buf )
{
   if (VG_(strncmp)(buf, "fun:", 4) == 0) {
      p->name = VG_(strdup)("errormgr.sLTy.1", buf+4);
      p->name_is_simple_str = is_simple_str (p->name);
      p->ty = FunName;
      return True;
   }
   if (VG_(strncmp)(buf, "obj:", 4) == 0) {
      p->name = VG_(strdup)("errormgr.sLTy.2", buf+4);
      p->name_is_simple_str = is_simple_str (p->name);
      p->ty = ObjName;
      return True;
   }
   if (VG_(strcmp)(buf, "...") == 0) {
      p->name = NULL;
      p->name_is_simple_str = False;
      p->ty = DotDotDot;
      return True;
   }
   VG_(printf)("location should be \"...\", or should start "
               "with \"fun:\" or \"obj:\"\n");
   return False;
}


static Bool tool_name_present(const HChar *name, const HChar *names)
{
   Bool  found;
   HChar *s = NULL;   
   Int   len = VG_(strlen)(name);

   found = (NULL != (s = VG_(strstr)(names, name)) &&
            (s        == names || *(s-1)   == ',') &&
            (*(s+len) == ','   || *(s+len) == '\0')
           );

   return found;
}

static void load_one_suppressions_file ( Int clo_suppressions_i )
{
   const HChar* filename = *(HChar**) VG_(indexXA)(VG_(clo_suppressions),
                                                   clo_suppressions_i);
   SysRes sres;
   Int    fd, i, j, lineno = 0;
   Bool   got_a_location_line_read_by_tool;
   Bool   eof;
   SizeT  nBuf = 200;
   HChar* buf = VG_(malloc)("errormgr.losf.1", nBuf);
   HChar* tool_names;
   HChar* supp_name;
   const HChar* err_str = NULL;
   SuppLoc tmp_callers[VG_MAX_SUPP_CALLERS];

   
   if (VG_(is_dir)( filename )) {
      if (VG_(clo_xml))
         VG_(printf_xml)("</valgrindoutput>\n");
      VG_(umsg)("FATAL: suppressions file \"%s\" is a directory\n", filename );
      VG_(exit)(1);
   }

   
   sres = VG_(open)( filename, VKI_O_RDONLY, 0 );
   if (sr_isError(sres)) {
      if (VG_(clo_xml))
         VG_(printf_xml)("</valgrindoutput>\n");
      VG_(umsg)("FATAL: can't open suppressions file \"%s\"\n", filename );
      VG_(exit)(1);
   }
   fd = sr_Res(sres);

#  define BOMB(S)  { err_str = S;  goto syntax_error; }

   while (True) {
      
      Supp* supp;
      supp        = VG_(malloc)("errormgr.losf.1", sizeof(Supp));
      supp->count = 0;

      
      for (i = 0; i < VG_MAX_SUPP_CALLERS; i++) {
         tmp_callers[i].ty   = NoName;
         tmp_callers[i].name_is_simple_str = False;
         tmp_callers[i].name = NULL;
      }

      supp->string = supp->extra = NULL;

      eof = get_nbnc_line ( fd, &buf, &nBuf, &lineno );
      if (eof) {
         VG_(free)(supp);
         break;
      }

      if (!VG_STREQ(buf, "{")) BOMB("expected '{' or end-of-file");
      
      eof = get_nbnc_line ( fd, &buf, &nBuf, &lineno );

      if (eof || VG_STREQ(buf, "}")) BOMB("unexpected '}'");

      supp->sname = VG_(strdup)("errormgr.losf.2", buf);
      supp->clo_suppressions_i = clo_suppressions_i;
      supp->sname_lineno = lineno;

      eof = get_nbnc_line ( fd, &buf, &nBuf, &lineno );

      if (eof) BOMB("unexpected end-of-file (expecting tool:suppr)");

      
      i = 0;
      while (True) {
         if (buf[i] == ':')  break;
         if (buf[i] == '\0') BOMB("malformed 'tool1,tool2,...:supp' line");
         i++;
      }
      buf[i]    = '\0';    

      tool_names = & buf[0];
      supp_name  = & buf[i+1];

      if (VG_(needs).core_errors && tool_name_present("core", tool_names))
      {
         
         
         
         
         
            BOMB("unknown core suppression type");
      }
      else if (VG_(needs).tool_errors && 
               tool_name_present(VG_(details).name, tool_names))
      {
         
         if (VG_TDICT_CALL(tool_recognised_suppression, supp_name, supp)) {
            
         } else {
            BOMB("unknown tool suppression type");
         }
      }
      else {
         
         while (True) {
            eof = get_nbnc_line ( fd, &buf, &nBuf, &lineno );
            if (eof) BOMB("unexpected end-of-file (when skipping suppression)");
            if (VG_STREQ(buf, "}"))
               break;
         }
         VG_(free)(supp->sname);
         VG_(free)(supp);
         continue;
      }

      buf[0] = 0;
      
      
      if (VG_(needs).tool_errors && 
          !VG_TDICT_CALL(tool_read_extra_suppression_info,
                         fd, &buf, &nBuf, &lineno, supp))
      {
         BOMB("bad or missing extra suppression info");
      }

      got_a_location_line_read_by_tool = buf[0] != 0 && is_location_line(buf);

      
      i = 0;
      while (True) {
         if (got_a_location_line_read_by_tool) {
            got_a_location_line_read_by_tool = False;
            eof = False;
         } else {
            eof = get_nbnc_line ( fd, &buf, &nBuf, &lineno );
         }
         if (eof)
            BOMB("unexpected end-of-file (when reading stack trace)");
         if (VG_STREQ(buf, "}")) {
            if (i > 0) {
               break;
            } else {
               BOMB("missing stack trace");
            }
         }
         if (i == VG_MAX_SUPP_CALLERS)
            BOMB("too many callers in stack trace");
         if (i > 0 && i >= VG_(clo_backtrace_size)) 
            break;
         if (!setLocationTy(&(tmp_callers[i]), buf))
            BOMB("location should be \"...\", or should start "
                 "with \"fun:\" or \"obj:\"");
         i++;
      }

      
      
      if (!VG_STREQ(buf, "}")) {
         do {
            eof = get_nbnc_line ( fd, &buf, &nBuf, &lineno );
         } while (!eof && !VG_STREQ(buf, "}"));
      }

      
      
      vg_assert(i > 0); 
      for (j = 0; j < i; j++) {
         if (tmp_callers[j].ty == FunName || tmp_callers[j].ty == ObjName)
            break;
         vg_assert(tmp_callers[j].ty == DotDotDot);
      }
      vg_assert(j >= 0 && j <= i);
      if (j == i) {
         
         BOMB("suppression must contain at least one location "
              "line which is not \"...\"");
      } 

      
      supp->n_callers = i;
      supp->callers = VG_(malloc)("errormgr.losf.4", i * sizeof(SuppLoc));
      for (i = 0; i < supp->n_callers; i++) {
         supp->callers[i] = tmp_callers[i];
      }

      supp->next = suppressions;
      suppressions = supp;
   }
   VG_(free)(buf);
   VG_(close)(fd);
   return;

  syntax_error:
   if (VG_(clo_xml))
      VG_(printf_xml)("</valgrindoutput>\n");
   VG_(umsg)("FATAL: in suppressions file \"%s\" near line %d:\n",
           filename, lineno );
   VG_(umsg)("   %s\n", err_str );
   
   VG_(close)(fd);
   VG_(umsg)("exiting now.\n");
   VG_(exit)(1);

#  undef BOMB
}


void VG_(load_suppressions) ( void )
{
   Int i;
   suppressions = NULL;
   for (i = 0; i < VG_(sizeXA)(VG_(clo_suppressions)); i++) {
      if (VG_(clo_verbosity) > 1) {
         VG_(dmsg)("Reading suppressions file: %s\n", 
                   *(HChar**) VG_(indexXA)(VG_(clo_suppressions), i));
      }
      load_one_suppressions_file( i );
   }
}



static Bool supploc_IsStar ( const void* supplocV )
{
   const SuppLoc* supploc = supplocV;
   return supploc->ty == DotDotDot;
}

static Bool supploc_IsQuery ( const void* supplocV )
{
   return False; 
}

typedef
   struct {
      StackTrace ips; 
      UWord n_ips; 

      
      
      
      
      
      
      
      
      
      
      

      UWord n_ips_expanded;
      
      
      UWord n_expanded;
      
      
      
      

      Int* n_offsets_per_ip;
      
      
      
      
      

      
      
      
      UWord sz_offsets;

      Int* fun_offsets;
      
      
      
      
      
      
      Int* obj_offsets;
      
      
      

      
      
      HChar *names;
      Int   names_szB;  
      Int   names_free; 
   }
   IPtoFunOrObjCompleter;

static void pp_ip2fo (const IPtoFunOrObjCompleter* ip2fo)
{
  Int i, j;
  Int o;

  VG_(printf)("n_ips %lu n_ips_expanded %lu resulting in n_expanded %lu\n",
              ip2fo->n_ips, ip2fo->n_ips_expanded, ip2fo->n_expanded);
  for (i = 0; i < ip2fo->n_ips_expanded; i++) {
     o = 0;
     for (j = 0; j < i; j++)
        o += ip2fo->n_offsets_per_ip[j];
     VG_(printf)("ips %d 0x08%lx offset [%d,%d] ", 
                 i, ip2fo->ips[i], 
                 o, o+ip2fo->n_offsets_per_ip[i]-1);
     for (j = 0; j < ip2fo->n_offsets_per_ip[i]; j++) {
        VG_(printf)("%sfun:%s obj:%s\n",
                    j == 0 ? "" : "                              ",
                    ip2fo->fun_offsets[o+j] == -1 ? 
                    "<not expanded>" : &ip2fo->names[ip2fo->fun_offsets[o+j]],
                    ip2fo->obj_offsets[o+j] == -1 ?
                    "<not expanded>" : &ip2fo->names[ip2fo->obj_offsets[o+j]]);
    }
  }
}

static void clearIPtoFunOrObjCompleter ( const Supp  *su, 
                                         IPtoFunOrObjCompleter* ip2fo)
{
   if (DEBUG_ERRORMGR || VG_(debugLog_getLevel)() >= 4) {
      if (su) {
         HChar *filename = *(HChar**) VG_(indexXA)(VG_(clo_suppressions),
                                                   su->clo_suppressions_i);
         VG_(dmsg)("errormgr matching end suppression %s  %s:%d matched:\n",
                   su->sname,
                   filename,
                   su->sname_lineno);
      } else
         VG_(dmsg)("errormgr matching end no suppression matched:\n");
      VG_(pp_StackTrace) (ip2fo->ips, ip2fo->n_ips);
      pp_ip2fo(ip2fo);
   }
   if (ip2fo->n_offsets_per_ip) VG_(free)(ip2fo->n_offsets_per_ip);
   if (ip2fo->fun_offsets)      VG_(free)(ip2fo->fun_offsets);
   if (ip2fo->obj_offsets)      VG_(free)(ip2fo->obj_offsets);
   if (ip2fo->names)            VG_(free)(ip2fo->names);
}

static HChar* grow_names(IPtoFunOrObjCompleter* ip2fo, SizeT needed)
{
   if (ip2fo->names_szB 
       < ip2fo->names_free + needed) {
     if (needed < ERRTXT_LEN) needed = ERRTXT_LEN;

      ip2fo->names 
         = VG_(realloc)("foc_names",
                        ip2fo->names,
                        ip2fo->names_szB + needed);
      ip2fo->names_szB += needed;
   }
   return ip2fo->names + ip2fo->names_free;
}

static HChar* foComplete(IPtoFunOrObjCompleter* ip2fo,
                         Int ixInput, Bool needFun)
{
   vg_assert (ixInput < ip2fo->n_expanded);
   vg_assert (VG_(clo_read_inline_info) || ixInput < ip2fo->n_ips);

   
   
   Int** offsets;
   if (needFun)
      offsets = &ip2fo->fun_offsets;
   else
      offsets = &ip2fo->obj_offsets;

   
   if ((*offsets)[ixInput] == -1) {
      const HChar* caller;

      (*offsets)[ixInput] = ip2fo->names_free;
      if (DEBUG_ERRORMGR) VG_(printf)("marking %s ixInput %d offset %d\n", 
                                      needFun ? "fun" : "obj",
                                      ixInput, ip2fo->names_free);
      if (needFun) {
         
         vg_assert (!VG_(clo_read_inline_info));
         
         
         
         
         
         if (!VG_(get_fnname_no_cxx_demangle)(ip2fo->ips[ixInput],
                                              &caller,
                                              NULL))
            caller = "???";
      } else {
         UWord i;
         UWord last_expand_pos_ips = 0;
         UWord pos_ips;

         
         for (pos_ips = 0; pos_ips < ip2fo->n_expanded; pos_ips++) {
            last_expand_pos_ips += ip2fo->n_offsets_per_ip[pos_ips];
            if (ixInput < last_expand_pos_ips)
               break;
         }

         if (!VG_(get_objname)(ip2fo->ips[pos_ips], &caller))
            caller = "???";

         
         for (i = last_expand_pos_ips - ip2fo->n_offsets_per_ip[pos_ips] + 1;
              i < last_expand_pos_ips;
              i++) {
            ip2fo->obj_offsets[i] = ip2fo->names_free;
            if (DEBUG_ERRORMGR) 
               VG_(printf) ("   set obj_offset %lu to %d\n", 
                            i, ip2fo->names_free);
         }
      }
      SizeT  caller_len = VG_(strlen)(caller);
      HChar* caller_name = grow_names(ip2fo, caller_len + 1);
      VG_(strcpy)(caller_name, caller);
      ip2fo->names_free += caller_len + 1;
      if (DEBUG_ERRORMGR) pp_ip2fo(ip2fo);
   }

   return ip2fo->names + (*offsets)[ixInput];
}

static void grow_offsets(IPtoFunOrObjCompleter* ip2fo, Int n_req)
{
   Int i;

   
   if (ip2fo->n_offsets_per_ip == NULL) {
      ip2fo->n_offsets_per_ip = VG_(malloc)("grow_offsets",
                                            ip2fo->n_ips * sizeof(Int));
      for (i = 0; i < ip2fo->n_ips; i++)
         ip2fo->n_offsets_per_ip[i] = 0;
   }

   if (ip2fo->sz_offsets >= n_req)
      return;

   
   
   if (n_req < ip2fo->n_ips)
      n_req = ip2fo->n_ips;
   if (n_req < ip2fo->sz_offsets + 5)
      n_req = ip2fo->sz_offsets + 5;

   ip2fo->fun_offsets = VG_(realloc)("grow_offsets", ip2fo->fun_offsets,
                                     n_req * sizeof(Int));
   for (i = ip2fo->sz_offsets; i < n_req; i++)
      ip2fo->fun_offsets[i] = -1;

   ip2fo->obj_offsets = VG_(realloc)("grow_offsets", ip2fo->obj_offsets,
                                     n_req * sizeof(Int));
   for (i = ip2fo->sz_offsets; i < n_req; i++)
      ip2fo->obj_offsets[i] = -1;

   ip2fo->sz_offsets = n_req;   
}

static void expandInput (IPtoFunOrObjCompleter* ip2fo, UWord ixInput )
{
   while (ip2fo->n_ips_expanded < ip2fo->n_ips
          && ip2fo->n_expanded <= ixInput) {
      if (VG_(clo_read_inline_info)) {
         
         const Addr IP = ip2fo->ips[ip2fo->n_ips_expanded];
         InlIPCursor *iipc;

         iipc = VG_(new_IIPC)(IP);
         
         
         
         
         do {
            const HChar *caller;
            grow_offsets(ip2fo, ip2fo->n_expanded+1);
            ip2fo->fun_offsets[ip2fo->n_expanded] = ip2fo->names_free;
            if (!VG_(get_fnname_no_cxx_demangle)(IP, 
                                                 &caller,
                                                 iipc))
               caller = "???";
            SizeT  caller_len = VG_(strlen)(caller);
            HChar* caller_name = grow_names(ip2fo, caller_len + 1);
            VG_(strcpy)(caller_name, caller);
            ip2fo->names_free += caller_len + 1;
            ip2fo->n_expanded++;
            ip2fo->n_offsets_per_ip[ip2fo->n_ips_expanded]++;
         } while (VG_(next_IIPC)(iipc));
         ip2fo->n_ips_expanded++;
         VG_(delete_IIPC) (iipc);
      } else {
         
         
         
         
         Int i;
         grow_offsets(ip2fo, ip2fo->n_ips);
         ip2fo->n_ips_expanded = ip2fo->n_ips;
         ip2fo->n_expanded = ip2fo->n_ips;
         for (i = 0; i < ip2fo->n_ips; i++)
            ip2fo->n_offsets_per_ip[i] = 1;
      }
   }
}

static Bool haveInputInpC (void* inputCompleter, UWord ixInput )
{
   IPtoFunOrObjCompleter* ip2fo = inputCompleter;
   expandInput(ip2fo, ixInput);
   return ixInput < ip2fo->n_expanded;
}

static Bool supp_pattEQinp ( const void* supplocV, const void* addrV,
                             void* inputCompleter, UWord ixInput )
{
   const SuppLoc* supploc = supplocV; 
   IPtoFunOrObjCompleter* ip2fo = inputCompleter;
   HChar* funobj_name; 
   Bool ret;

   expandInput(ip2fo, ixInput);
   vg_assert(ixInput < ip2fo->n_expanded);

   
   switch (supploc->ty) {
      case DotDotDot:
         vg_assert(0);
      case ObjName:
         funobj_name = foComplete(ip2fo, ixInput, False );
         break; 
      case FunName:
         funobj_name = foComplete(ip2fo, ixInput, True );
         break;
      default:
        vg_assert(0);
   }

   if (supploc->name_is_simple_str)
      ret = VG_(strcmp) (supploc->name, funobj_name) == 0;
   else
      ret = VG_(string_match)(supploc->name, funobj_name);
   if (DEBUG_ERRORMGR)
      VG_(printf) ("supp_pattEQinp %s patt %s ixUnput %lu value:%s match:%s\n",
                   supploc->ty == FunName ? "fun" : "obj",
                   supploc->name, ixInput, funobj_name,
                   ret ? "yes" : "no");
   return ret;
}


static Bool supp_matches_callers(IPtoFunOrObjCompleter* ip2fo,
                                 const Supp* su)
{
   
   SuppLoc*   supps    = su->callers;
   UWord      n_supps  = su->n_callers;
   UWord      szbPatt  = sizeof(SuppLoc);
   Bool       matchAll = False; 
   if (DEBUG_ERRORMGR) {
      HChar *filename = *(HChar**) VG_(indexXA)(VG_(clo_suppressions),
                                                su->clo_suppressions_i);
      VG_(dmsg)("   errormgr Checking match with  %s  %s:%d\n",
                su->sname,
                filename,
                su->sname_lineno);
   }
   return
      VG_(generic_match)(
         matchAll,
         supps, szbPatt, n_supps, 0,
         
         NULL, 0, 0,   
         0,
         supploc_IsStar, supploc_IsQuery, supp_pattEQinp,
         ip2fo, haveInputInpC
      );
}


static
Bool supp_matches_error(const Supp* su, const Error* err)
{
   switch (su->skind) {
      
      
      
      default:
         if (VG_(needs).tool_errors) {
            return VG_TDICT_CALL(tool_error_matches_suppression, err, su);
         } else {
            VG_(printf)(
               "\nUnhandled suppression type: %u.  VG_(needs).tool_errors\n"
               "probably needs to be set.\n",
               err->ekind);
            VG_(core_panic)("unhandled suppression type");
         }
   }
}


static Supp* is_suppressible_error ( const Error* err )
{
   Supp* su;
   Supp* su_prev;

   IPtoFunOrObjCompleter ip2fo;

   
   em_supplist_searches++;

   
   ip2fo.ips = VG_(get_ExeContext_StackTrace)(err->where);
   ip2fo.n_ips = VG_(get_ExeContext_n_ips)(err->where);
   ip2fo.n_ips_expanded = 0;
   ip2fo.n_expanded = 0;
   ip2fo.sz_offsets = 0;
   ip2fo.n_offsets_per_ip = NULL;
   ip2fo.fun_offsets = NULL;
   ip2fo.obj_offsets = NULL;
   ip2fo.names = NULL;
   ip2fo.names_szB = 0;
   ip2fo.names_free = 0;

   
   if (DEBUG_ERRORMGR || VG_(debugLog_getLevel)() >= 4)
     VG_(dmsg)("errormgr matching begin\n");
   su_prev = NULL;
   for (su = suppressions; su != NULL; su = su->next) {
      em_supplist_cmps++;
      if (supp_matches_error(su, err) 
          && supp_matches_callers(&ip2fo, su)) {
         
         
         (void)VG_TDICT_CALL(tool_update_extra_suppression_use, err, su);
         if (su_prev) {
            vg_assert(su_prev->next == su);
            su_prev->next = su->next;
            su->next = suppressions;
            suppressions = su;
         }
         clearIPtoFunOrObjCompleter(su, &ip2fo);
         return su;
      }
      su_prev = su;
   }
   clearIPtoFunOrObjCompleter(NULL, &ip2fo);
   return NULL;      
}

void VG_(print_errormgr_stats) ( void )
{
   VG_(dmsg)(
      " errormgr: %'lu supplist searches, %'lu comparisons during search\n",
      em_supplist_searches, em_supplist_cmps
   );
   VG_(dmsg)(
      " errormgr: %'lu errlist searches, %'lu comparisons during search\n",
      em_errlist_searches, em_errlist_cmps
   );
}

