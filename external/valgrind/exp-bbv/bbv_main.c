

/*
   This file is part of BBV, a Valgrind tool for generating SimPoint
   basic block vectors.

   Copyright (C) 2006-2013 Vince Weaver
      vince _at_ csl.cornell.edu

   pcfile code is Copyright (C) 2006-2013 Oriol Prat
      oriol.prat _at _ bsc.es

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


#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_options.h"    

#include "pub_tool_vki.h"        
#include "pub_tool_libcbase.h"   
#include "pub_tool_libcprint.h"  
#include "pub_tool_libcassert.h" 
#include "pub_tool_mallocfree.h" 
#include "pub_tool_machine.h"    
#include "pub_tool_debuginfo.h"  

#include "pub_tool_oset.h"       

   
#define REP_INSTRUCTION   0x1
#define FLDCW_INSTRUCTION 0x2

   
#define DEFAULT_GRAIN_SIZE 100000000  
static Int interval_size=DEFAULT_GRAIN_SIZE;

   
static const HChar *clo_bb_out_file="bb.out.%p";
static const HChar *clo_pc_out_file="pc.out.%p";
static HChar *pc_out_file=NULL;
static HChar *bb_out_file=NULL;


   
static Bool instr_count_only=False;
static Bool generate_pc_file=False;

   
static OSet* instr_info_table;  
static Int block_num=1;         
static Int current_thread=0;
static Int allocated_threads=1;
struct thread_info *bbv_thread=NULL;

   
struct thread_info {
   ULong dyn_instr;         
   ULong total_instr;       
   Addr last_rep_addr;      
   ULong rep_count;
   ULong global_rep_count;
   ULong unique_rep_count;
   ULong fldcw_count;       
   VgFile *bbtrace_fp;      
};

struct BB_info {
   Addr       BB_addr;           
   Int        n_instrs;          
   Int        block_num;         
   Int        *inst_counter;     
   Bool       is_entry;          
   const HChar *fn_name;         
};


   
   
static void dumpPcFile(void)
{
   struct BB_info   *bb_elem;
   VgFile *fp;

   pc_out_file =
          VG_(expand_file_name)("--pc-out-file", clo_pc_out_file);

   fp = VG_(fopen)(pc_out_file, VKI_O_CREAT|VKI_O_TRUNC|VKI_O_WRONLY,
                   VKI_S_IRUSR|VKI_S_IWUSR|VKI_S_IRGRP|VKI_S_IWGRP);
   if (fp == NULL) {
      VG_(umsg)("Error: cannot create pc file %s\n", pc_out_file);
      VG_(exit)(1);
   }

      
      
   VG_(OSetGen_ResetIter)(instr_info_table);
   while ( (bb_elem = VG_(OSetGen_Next)(instr_info_table)) ) {
      VG_(fprintf)( fp, "F:%d:%x:%s\n", bb_elem->block_num,
                    (Int)bb_elem->BB_addr, bb_elem->fn_name);
   }

   VG_(fclose)(fp);
}

static VgFile *open_tracefile(Int thread_num)
{
   VgFile *fp;
   
   HChar temp_string[VG_(strlen)(bb_out_file) + 1 + 10 + 1];

      
      
      
   if (thread_num==1) {
      VG_(strcpy)(temp_string, bb_out_file);
   }
   else {
      VG_(sprintf)(temp_string,"%s.%d",bb_out_file,thread_num);
   }

   fp = VG_(fopen)(temp_string, VKI_O_CREAT|VKI_O_TRUNC|VKI_O_WRONLY,
                   VKI_S_IRUSR|VKI_S_IWUSR|VKI_S_IRGRP|VKI_S_IWGRP);

   if (fp == NULL) {
      VG_(umsg)("Error: cannot create bb file %s\n",temp_string);
      VG_(exit)(1);
   }

   return fp;
}

static void handle_overflow(void)
{
   struct BB_info *bb_elem;

   if (bbv_thread[current_thread].dyn_instr > interval_size) {

      if (!instr_count_only) {

            
         if (bbv_thread[current_thread].bbtrace_fp == NULL) {
            bbv_thread[current_thread].bbtrace_fp=open_tracefile(current_thread);
         }

           

         VG_(fprintf)(bbv_thread[current_thread].bbtrace_fp, "T");

         VG_(OSetGen_ResetIter)(instr_info_table);
         while ( (bb_elem = VG_(OSetGen_Next)(instr_info_table)) ) {
            if ( bb_elem->inst_counter[current_thread] != 0 ) {
               VG_(fprintf)(bbv_thread[current_thread].bbtrace_fp, ":%d:%d   ",
                            bb_elem->block_num,
                            bb_elem->inst_counter[current_thread]);
               bb_elem->inst_counter[current_thread] = 0;
            }
         }

         VG_(fprintf)(bbv_thread[current_thread].bbtrace_fp, "\n");
      }

      bbv_thread[current_thread].dyn_instr -= interval_size;
   }
}


static void close_out_reps(void)
{
   bbv_thread[current_thread].global_rep_count+=bbv_thread[current_thread].rep_count;
   bbv_thread[current_thread].unique_rep_count++;
   bbv_thread[current_thread].rep_count=0;
}

   
static VG_REGPARM(1) void per_instruction_BBV(struct BB_info *bbInfo)
{
   Int n_instrs=1;

   tl_assert(bbInfo);

      
   if (bbv_thread[current_thread].rep_count) {
      n_instrs++;
      close_out_reps();
   }

   bbInfo->inst_counter[current_thread]+=n_instrs;

   bbv_thread[current_thread].total_instr+=n_instrs;
   bbv_thread[current_thread].dyn_instr +=n_instrs;

   handle_overflow();
}

   
static VG_REGPARM(1) void per_instruction_BBV_rep(Addr addr)
{
      
   if (bbv_thread[current_thread].last_rep_addr!=addr) {
      if (bbv_thread[current_thread].rep_count) {
         close_out_reps();
         bbv_thread[current_thread].total_instr++;
         bbv_thread[current_thread].dyn_instr++;
      }
      bbv_thread[current_thread].last_rep_addr=addr;
   }

   bbv_thread[current_thread].rep_count++;

}

   
static VG_REGPARM(1) void per_instruction_BBV_fldcw(struct BB_info *bbInfo)
{
   Int n_instrs=1;

   tl_assert(bbInfo);

      
   if (bbv_thread[current_thread].rep_count) {
      n_instrs++;
      close_out_reps();
   }

      
   bbv_thread[current_thread].fldcw_count++;

   bbInfo->inst_counter[current_thread]+=n_instrs;

   bbv_thread[current_thread].total_instr+=n_instrs;
   bbv_thread[current_thread].dyn_instr +=n_instrs;

   handle_overflow();
}

   
   
   
static Int get_inst_type(UInt len, Addr addr)
{
   int result=0;

#if defined(VGA_x86) || defined(VGA_amd64)

   UChar *inst_pointer;
   UChar  inst_byte;
   int i,possible_rep;

   
   

   
   
   

   inst_pointer=(UChar *)addr;
   i=0;
   inst_byte=0;
   possible_rep=0;

   while (i<len) {

      inst_byte=*inst_pointer;

      if ( (inst_byte == 0x67) ||            
           (inst_byte == 0x66) ||            
           (inst_byte == 0x48) ) {           
      } else if ( (inst_byte == 0xf2) ||     
                  (inst_byte == 0xf3) ) {    
         possible_rep=1;
      } else {
         break;                              
      }

      i++;
      inst_pointer++;
   }

   if ( possible_rep &&
        ( ( (inst_byte >= 0xa4) &&     
            (inst_byte <= 0xaf) ) ||   
          ( (inst_byte >= 0x6c) &&
            (inst_byte <= 0x6f) ) ) ) {  

      result|=REP_INSTRUCTION;
   }

   
   
   

   inst_pointer=(UChar *)addr;
   if (len>1) {
         
         
      if ((*inst_pointer==0xd9) &&
          (*(inst_pointer+1)<0xb0) &&  
          ( (*(inst_pointer+1) & 0x38) == 0x28)) {
         result|=FLDCW_INSTRUCTION;
      }
   }

#endif
   return result;
}



   
   
   
   
   
static IRSB* bbv_instrument ( VgCallbackClosure* closure,
                              IRSB* sbIn, const VexGuestLayout* layout,
                              const VexGuestExtents* vge,
                              const VexArchInfo* archinfo_host,
                              IRType gWordTy, IRType hWordTy )
{
   Int      i,n_instrs=1;
   IRSB     *sbOut;
   IRStmt   *st;
   struct BB_info  *bbInfo;
   Addr     origAddr,ourAddr;
   IRDirty  *di;
   IRExpr   **argv, *arg1;
   Int      regparms,opcode_type;

      
   if (gWordTy != hWordTy) {
      VG_(tool_panic)("host/guest word size mismatch");
   }

      
   sbOut = deepCopyIRSBExceptStmts(sbIn);

      
   i = 0;
   while ( (i < sbIn->stmts_used) && (sbIn->stmts[i]->tag!=Ist_IMark)) {
      addStmtToIRSB( sbOut, sbIn->stmts[i] );
      i++;
   }

      
   tl_assert(sbIn->stmts_used > 0);
   st = sbIn->stmts[i];

      
   tl_assert(Ist_IMark == st->tag);

   origAddr=st->Ist.IMark.addr;

      
   bbInfo = VG_(OSetGen_Lookup)(instr_info_table, &origAddr);

   if (bbInfo==NULL) {

         
         

         
      bbInfo=VG_(OSetGen_AllocNode)(instr_info_table, sizeof(struct BB_info));
      bbInfo->BB_addr = origAddr;
      bbInfo->n_instrs = n_instrs;
      bbInfo->inst_counter=VG_(calloc)("bbv_instrument",
                                       allocated_threads,
                                       sizeof(Int));

         
      bbInfo->block_num=block_num;
      block_num++;
         
      const HChar *fn_name;
      VG_(get_fnname)(origAddr, &fn_name);
      bbInfo->is_entry=VG_(get_fnname_if_entry)(origAddr, &fn_name);
      bbInfo->fn_name =VG_(strdup)("bbv_strings", fn_name);
         
      VG_(OSetGen_Insert)( instr_info_table, bbInfo );
   }

      
      
      

      
      
      


   while(i < sbIn->stmts_used) {
      st=sbIn->stmts[i];

      if (st->tag == Ist_IMark) {

         ourAddr = st->Ist.IMark.addr;

         opcode_type=get_inst_type(st->Ist.IMark.len,ourAddr);

         regparms=1;
         arg1= mkIRExpr_HWord( (HWord)bbInfo);
         argv= mkIRExprVec_1(arg1);


         if (opcode_type&REP_INSTRUCTION) {
            arg1= mkIRExpr_HWord(ourAddr);
            argv= mkIRExprVec_1(arg1);
            di= unsafeIRDirty_0_N( regparms, "per_instruction_BBV_rep",
                                VG_(fnptr_to_fnentry)( &per_instruction_BBV_rep ),
                                argv);
         }
         else if (opcode_type&FLDCW_INSTRUCTION) {
            di= unsafeIRDirty_0_N( regparms, "per_instruction_BBV_fldcw",
                                VG_(fnptr_to_fnentry)( &per_instruction_BBV_fldcw ),
                                argv);
         }
         else {
         di= unsafeIRDirty_0_N( regparms, "per_instruction_BBV",
                                VG_(fnptr_to_fnentry)( &per_instruction_BBV ),
                                argv);
         }


            
         addStmtToIRSB( sbOut,  IRStmt_Dirty(di));
      }

         
      addStmtToIRSB( sbOut, st );

      i++;
   }

   return sbOut;
}

static struct thread_info *allocate_new_thread(struct thread_info *old,
                                     Int old_number, Int new_number)
{
   struct thread_info *temp;
   struct BB_info   *bb_elem;
   Int i;

   temp=VG_(realloc)("bbv_main.c allocate_threads",
                     old,
                     new_number*sizeof(struct thread_info));

      
      
   for(i=old_number;i<new_number;i++) {
      temp[i].last_rep_addr=0;
      temp[i].dyn_instr=0;
      temp[i].total_instr=0;
      temp[i].global_rep_count=0;
      temp[i].unique_rep_count=0;
      temp[i].rep_count=0;
      temp[i].fldcw_count=0;
      temp[i].bbtrace_fp=NULL;
   }
      
   VG_(OSetGen_ResetIter)(instr_info_table);
   while ( (bb_elem = VG_(OSetGen_Next)(instr_info_table)) ) {
      bb_elem->inst_counter =
                    VG_(realloc)("bbv_main.c inst_counter",
                                 bb_elem->inst_counter,
                                 new_number*sizeof(Int));
      for(i=old_number;i<new_number;i++) {
         bb_elem->inst_counter[i]=0;
      }
   }

   return temp;
}

static void bbv_thread_called ( ThreadId tid, ULong nDisp )
{
   if (tid >= allocated_threads) {
      bbv_thread=allocate_new_thread(bbv_thread,allocated_threads,tid+1);
      allocated_threads=tid+1;
   }
   current_thread=tid;
}





static void bbv_post_clo_init(void)
{
   bb_out_file =
          VG_(expand_file_name)("--bb-out-file", clo_bb_out_file);

      
      
      
   VG_(clo_vex_control).guest_chase_thresh = 0;
}

   
static Bool bbv_process_cmd_line_option(const HChar* arg)
{
   if VG_INT_CLO       (arg, "--interval-size",    interval_size) {}
   else if VG_STR_CLO  (arg, "--bb-out-file",      clo_bb_out_file) {}
   else if VG_STR_CLO  (arg, "--pc-out-file",      clo_pc_out_file) {
      generate_pc_file = True;
   }
   else if VG_BOOL_CLO (arg, "--instr-count-only", instr_count_only) {}
   else {
      return False;
   }

   return True;
}

static void bbv_print_usage(void)
{
   VG_(printf)(
"   --bb-out-file=<file>       filename for BBV info\n"
"   --pc-out-file=<file>       filename for BB addresses and function names\n"
"   --interval-size=<num>      interval size\n"
"   --instr-count-only=yes|no  only print total instruction count\n"
   );
}

static void bbv_print_debug_usage(void)
{
   VG_(printf)("    (none)\n");
}

static void bbv_fini(Int exitcode)
{
   Int i;

   if (generate_pc_file) {
      dumpPcFile();
   }

   for(i=0;i<allocated_threads;i++) {

      if (bbv_thread[i].total_instr!=0) {
         HChar buf[500];  
         VG_(sprintf)(buf,"\n\n"
                          "# Thread %d\n"
                          "#   Total intervals: %d (Interval Size %d)\n"
                          "#   Total instructions: %lld\n"
                          "#   Total reps: %lld\n"
                          "#   Unique reps: %lld\n"
                          "#   Total fldcw instructions: %lld\n\n",
                i,
                (Int)(bbv_thread[i].total_instr/(ULong)interval_size),
                interval_size,
                bbv_thread[i].total_instr,
                bbv_thread[i].global_rep_count,
                bbv_thread[i].unique_rep_count,
                bbv_thread[i].fldcw_count);

            
         VG_(umsg)("%s\n", buf);

            
         if (bbv_thread[i].bbtrace_fp == NULL) {
            bbv_thread[i].bbtrace_fp=open_tracefile(i);
         }
            
         VG_(fprintf)(bbv_thread[i].bbtrace_fp, "%s", buf);
         VG_(fclose)(bbv_thread[i].bbtrace_fp);
      }
   }
}

static void bbv_pre_clo_init(void)
{
   VG_(details_name)            ("exp-bbv");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("a SimPoint basic block vector generator");
   VG_(details_copyright_author)(
      "Copyright (C) 2006-2013 Vince Weaver");
   VG_(details_bug_reports_to)  (VG_BUGS_TO);

   VG_(basic_tool_funcs)          (bbv_post_clo_init,
                                   bbv_instrument,
                                   bbv_fini);

   VG_(needs_command_line_options)(bbv_process_cmd_line_option,
                                   bbv_print_usage,
                                   bbv_print_debug_usage);

   VG_(track_start_client_code)( bbv_thread_called );


   instr_info_table = VG_(OSetGen_Create)(0,
                                          NULL,
                                          VG_(malloc), "bbv.1", VG_(free));

   bbv_thread=allocate_new_thread(bbv_thread,0,allocated_threads);
}

VG_DETERMINE_INTERFACE_VERSION(bbv_pre_clo_init)

