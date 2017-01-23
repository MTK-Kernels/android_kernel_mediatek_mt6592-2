
/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2004-2014 Josef Weidendorfer
      josef.weidendorfer@gmx.de

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

#ifndef CLG_GLOBAL
#define CLG_GLOBAL

#include "pub_tool_basics.h"
#include "pub_tool_vki.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_libcproc.h"
#include "pub_tool_machine.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_options.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_xarray.h"
#include "pub_tool_clientstate.h"
#include "pub_tool_machine.h"      

#include "events.h" 
#include "costs.h"



#define CLG_ENABLE_DEBUG 1

#define CLG_EXPERIMENTAL 0

#define CLG_MICROSYSTIME 0




#define DEFAULT_OUTFORMAT   "callgrind.out.%p"

typedef struct _CommandLineOptions CommandLineOptions;
struct _CommandLineOptions {

  
  const HChar* out_format;  
  Bool combine_dumps;       
  Bool compress_strings;
  Bool compress_events;
  Bool compress_pos;
  Bool mangle_names;
  Bool compress_mangled;
  Bool dump_line;
  Bool dump_instr;
  Bool dump_bb;
  Bool dump_bbs;         
  
  
  ULong dump_every_bb;     
  
  
  Bool separate_threads; 
  Int  separate_callers; 
  Int  separate_recursions; 
  Bool skip_plt;         
  Bool skip_direct_recursion; 

  Bool collect_atstart;  
  Bool collect_jumps;    

  Bool collect_alloc;    
  Bool collect_systime;  

  Bool collect_bus;      

  
  Bool instrument_atstart;  
  Bool simulate_cache;      
  Bool simulate_branch;     

  
  Bool pop_on_jump;       

#if CLG_ENABLE_DEBUG
  Int   verbose;
  ULong verbose_start;
#endif
};


#define MIN_LINE_SIZE   16



typedef struct _Statistics Statistics;
struct _Statistics {
  ULong call_counter;
  ULong jcnd_counter;
  ULong jump_counter;
  ULong rec_call_counter;
  ULong ret_counter;
  ULong bb_executions;

  Int  context_counter;
  Int  bb_retranslations;  

  Int  distinct_objs;
  Int  distinct_files;
  Int  distinct_fns;
  Int  distinct_contexts;
  Int  distinct_bbs;
  Int  distinct_jccs;
  Int  distinct_bbccs;
  Int  distinct_instrs;
  Int  distinct_skips;

  Int  bb_hash_resizes;
  Int  bbcc_hash_resizes;
  Int  jcc_hash_resizes;
  Int  cxt_hash_resizes;
  Int  fn_array_resizes;
  Int  call_stack_resizes;
  Int  fn_stack_resizes;

  Int  full_debug_BBs;
  Int  file_line_debug_BBs;
  Int  fn_name_debug_BBs;
  Int  no_debug_BBs;
  Int  bbcc_lru_misses;
  Int  jcc_lru_misses;
  Int  cxt_lru_misses;
  Int  bbcc_clones;
};



typedef struct _Context     Context;
typedef struct _CC          CC;
typedef struct _BB          BB;
typedef struct _BBCC        BBCC;
typedef struct _jCC         jCC;
typedef struct _fCC         fCC;
typedef struct _fn_node     fn_node;
typedef struct _file_node   file_node;
typedef struct _obj_node    obj_node;
typedef struct _fn_config   fn_config;
typedef struct _call_entry  call_entry;
typedef struct _thread_info thread_info;

typedef ULong* SimCost;  
typedef ULong* UserCost;
typedef ULong* FullCost; 


typedef enum {
  jk_None = 0,   
  jk_Jump,       
  jk_Call,
  jk_Return,
  jk_CondJump    
} ClgJumpKind;



struct _jCC {
  ClgJumpKind jmpkind; 
  jCC* next_hash;   
  jCC* next_from;   
  BBCC *from, *to;  
  UInt jmp;         

  ULong call_counter; 

  FullCost cost; 
};


typedef struct _InstrInfo InstrInfo;
struct _InstrInfo {
  UInt instr_offset;
  UInt instr_size;
  UInt cost_offset;
  EventSet* eventset;
};



typedef struct _CJmpInfo CJmpInfo;
struct _CJmpInfo {
  UInt instr;          
  ClgJumpKind jmpkind; 
};


struct _BB {
  obj_node*  obj;         
  PtrdiffT   offset;      
  BB*        next;       

  VgSectKind sect_kind;  
  UInt       instr_count;
  
  
  fn_node*   fn;          
  UInt       line;
  Bool       is_entry;    
        
  BBCC*      bbcc_list;  
  BBCC*      last_bbcc;  

  
  UInt       cjmp_count;  
  CJmpInfo*  jmp;         
  Bool       cjmp_inverted; 

  UInt       instr_len;
  UInt       cost_count;
  InstrInfo  instr[0];   
};



struct _Context {
    UInt size;        
    UInt base_number; 
    Context* next;    
    UWord hash;       
    fn_node* fn[0];
};


typedef struct _JmpData JmpData;
struct _JmpData {
    ULong ecounter; 
    jCC*  jcc_list; 
};


struct _BBCC {
    BB*      bb;           

    Context* cxt;          
    ThreadId tid;          
    UInt     rec_index;    
    BBCC**   rec_array;    
    ULong    ret_counter;  
    
    BBCC*    next_bbcc;    
    BBCC*    lru_next_bbcc; 
    
    jCC*     lru_from_jcc; 
    jCC*     lru_to_jcc;   
    FullCost skipped;      
    
    BBCC*    next;         
    ULong*   cost;         
    ULong    ecounter_sum; 
    JmpData  jmp[0];
};



struct _fn_node {
  HChar*     name;
  UInt       number;
  Context*   last_cxt; 
  Context*   pure_cxt; 
  file_node* file;     
  fn_node* next;

  Bool dump_before :1;
  Bool dump_after :1;
  Bool zero_before :1;
  Bool toggle_collect :1;
  Bool skip :1;
  Bool pop_on_jump : 1;

  Bool is_malloc :1;
  Bool is_realloc :1;
  Bool is_free :1;

  Int  group;
  Int  separate_callers;
  Int  separate_recursions;
#if CLG_ENABLE_DEBUG
  Int  verbosity; 
#endif
};


#define   N_OBJ_ENTRIES         47
#define  N_FILE_ENTRIES         53
#define    N_FN_ENTRIES         87

struct _file_node {
   HChar*     name;
   fn_node*   fns[N_FN_ENTRIES];
   UInt       number;
   obj_node*  obj;
   file_node* next;
};

struct _obj_node {
   const HChar* name;
   UInt       last_slash_pos;

   Addr       start;  
   SizeT      size;   
   PtrdiffT   offset; 

   file_node* files[N_FILE_ENTRIES];
   UInt       number;
   obj_node*  next;
};

struct _call_entry {
    jCC* jcc;           
    FullCost enter_cost; 
    Addr sp;            
    Addr ret_addr;      
    BBCC* nonskipped;   
    Context* cxt;       
    Int fn_sp;          
};


typedef struct _exec_state exec_state;
struct _exec_state {

  Int sig;
  
  
  Int orig_sp;
  
  FullCost cost;
  Bool     collect;
  Context* cxt;
  
  
  Int   jmps_passed;
  BBCC* bbcc;      
  BBCC* nonskipped;

  Int call_stack_bottom; 
};

typedef struct _bb_hash bb_hash;
struct _bb_hash {
  UInt size, entries;
  BB** table;
};

typedef struct _cxt_hash cxt_hash;
struct _cxt_hash {
  UInt size, entries;
  Context** table;
};  

typedef struct _bbcc_hash bbcc_hash;
struct _bbcc_hash {
  UInt size, entries;
  BBCC** table;
};

typedef struct _jcc_hash jcc_hash;
struct _jcc_hash {
  UInt size, entries;
  jCC** table;
  jCC* spontaneous;
};

typedef struct _fn_array fn_array;
struct _fn_array {
  UInt size;
  UInt* array;
};

typedef struct _call_stack call_stack;
struct _call_stack {
  UInt size;
  Int sp;
  call_entry* entry;
};

typedef struct _fn_stack fn_stack;
struct _fn_stack {
  UInt size;
  fn_node **bottom, **top;
};

#define MAX_SIGHANDLERS 10

typedef struct _exec_stack exec_stack;
struct _exec_stack {
  Int sp; 
  exec_state* entry[MAX_SIGHANDLERS];
};

struct _thread_info {

  
  fn_stack fns;       
  call_stack calls;   
  exec_stack states;  

  
  FullCost lastdump_cost;    
  FullCost sighandler_cost;

  
  fn_array fn_active;
  jcc_hash jccs;
  bbcc_hash bbccs;
};


typedef struct _AddrPos AddrPos;
struct _AddrPos {
    Addr addr;
    Addr bb_addr;
    file_node* file;
    UInt line;
};

/* a simulator cost entity that can be written out in one line */
typedef struct _AddrCost AddrCost;
struct _AddrCost {
    AddrPos p;
    SimCost cost;
};

typedef struct _FnPos FnPos;
struct _FnPos {
    file_node* file;
    fn_node* fn;
    obj_node* obj;
    Context* cxt;
    int rec_index;
    UInt line;
};


struct cachesim_if
{
    void (*print_opts)(void);
    Bool (*parse_opt)(const HChar* arg);
    void (*post_clo_init)(void);
    void (*clear)(void);
    void (*dump_desc)(VgFile *fp);
    void (*printstat)(Int,Int,Int);
    void (*add_icost)(SimCost, BBCC*, InstrInfo*, ULong);
    void (*finish)(void);
    
    void (*log_1I0D)(InstrInfo*) VG_REGPARM(1);
    void (*log_2I0D)(InstrInfo*, InstrInfo*) VG_REGPARM(2);
    void (*log_3I0D)(InstrInfo*, InstrInfo*, InstrInfo*) VG_REGPARM(3);

    void (*log_1I1Dr)(InstrInfo*, Addr, Word) VG_REGPARM(3);
    void (*log_1I1Dw)(InstrInfo*, Addr, Word) VG_REGPARM(3);

    void (*log_0I1Dr)(InstrInfo*, Addr, Word) VG_REGPARM(3);
    void (*log_0I1Dw)(InstrInfo*, Addr, Word) VG_REGPARM(3);

    
    const HChar *log_1I0D_name, *log_2I0D_name, *log_3I0D_name;
    const HChar *log_1I1Dr_name, *log_1I1Dw_name;
    const HChar *log_0I1Dr_name, *log_0I1Dw_name;
};

#define EG_USE   0
#define EG_IR    1
#define EG_DR    2
#define EG_DW    3
#define EG_BC    4
#define EG_BI    5
#define EG_BUS   6
#define EG_ALLOC 7
#define EG_SYS   8

struct event_sets {
    EventSet *base, *full;
};

#define fullOffset(group) (CLG_(sets).full->offset[group])




void CLG_(set_clo_defaults)(void);
void CLG_(update_fn_config)(fn_node*);
Bool CLG_(process_cmd_line_option)(const HChar*);
void CLG_(print_usage)(void);
void CLG_(print_debug_usage)(void);

void CLG_(init_eventsets)(void);

Bool CLG_(get_debug_info)(Addr, const HChar **dirname,
                          const HChar **filename,
                          const HChar **fn_name, UInt*, DebugInfo**);
void CLG_(collectBlockInfo)(IRSB* bbIn, UInt*, UInt*, Bool*);
void CLG_(set_instrument_state)(const HChar*,Bool);
void CLG_(dump_profile)(const HChar* trigger,Bool only_current_thread);
void CLG_(zero_all_cost)(Bool only_current_thread);
Int CLG_(get_dump_counter)(void);
void CLG_(fini)(Int exitcode);

void CLG_(init_bb_hash)(void);
bb_hash* CLG_(get_bb_hash)(void);
BB*  CLG_(get_bb)(Addr addr, IRSB* bb_in, Bool *seen_before);
void CLG_(delete_bb)(Addr addr);

static __inline__ Addr bb_addr(BB* bb)
 { return bb->offset + bb->obj->offset; }
static __inline__ Addr bb_jmpaddr(BB* bb)
 { UInt off = (bb->instr_count > 0) ? bb->instr[bb->instr_count-1].instr_offset : 0;
   return off + bb->offset + bb->obj->offset; }

void CLG_(init_fn_array)(fn_array*);
void CLG_(copy_current_fn_array)(fn_array* dst);
fn_array* CLG_(get_current_fn_array)(void);
void CLG_(set_current_fn_array)(fn_array*);
UInt* CLG_(get_fn_entry)(Int n);

void      CLG_(init_obj_table)(void);
obj_node* CLG_(get_obj_node)(DebugInfo* si);
file_node* CLG_(get_file_node)(obj_node*, const HChar *dirname,
                               const HChar* filename);
fn_node*  CLG_(get_fn_node)(BB* bb);

void CLG_(init_bbcc_hash)(bbcc_hash* bbccs);
void CLG_(copy_current_bbcc_hash)(bbcc_hash* dst);
bbcc_hash* CLG_(get_current_bbcc_hash)(void);
void CLG_(set_current_bbcc_hash)(bbcc_hash*);
void CLG_(forall_bbccs)(void (*func)(BBCC*));
void CLG_(zero_bbcc)(BBCC* bbcc);
BBCC* CLG_(get_bbcc)(BB* bb);
BBCC* CLG_(clone_bbcc)(BBCC* orig, Context* cxt, Int rec_index);
void CLG_(setup_bbcc)(BB* bb) VG_REGPARM(1);


void CLG_(init_jcc_hash)(jcc_hash*);
void CLG_(copy_current_jcc_hash)(jcc_hash* dst);
void CLG_(set_current_jcc_hash)(jcc_hash*);
jCC* CLG_(get_jcc)(BBCC* from, UInt, BBCC* to);

void CLG_(init_call_stack)(call_stack*);
void CLG_(copy_current_call_stack)(call_stack* dst);
void CLG_(set_current_call_stack)(call_stack*);
call_entry* CLG_(get_call_entry)(Int n);

void CLG_(push_call_stack)(BBCC* from, UInt jmp, BBCC* to, Addr sp, Bool skip);
void CLG_(pop_call_stack)(void);
Int CLG_(unwind_call_stack)(Addr sp, Int);

void CLG_(init_fn_stack)(fn_stack*);
void CLG_(copy_current_fn_stack)(fn_stack*);
void CLG_(set_current_fn_stack)(fn_stack*);

void CLG_(init_cxt_table)(void);
Context* CLG_(get_cxt)(fn_node** fn);
void CLG_(push_cxt)(fn_node* fn);

void CLG_(init_threads)(void);
thread_info** CLG_(get_threads)(void);
thread_info* CLG_(get_current_thread)(void);
void CLG_(switch_thread)(ThreadId tid);
void CLG_(forall_threads)(void (*func)(thread_info*));
void CLG_(run_thread)(ThreadId tid);

void CLG_(init_exec_state)(exec_state* es);
void CLG_(init_exec_stack)(exec_stack*);
void CLG_(copy_current_exec_stack)(exec_stack*);
void CLG_(set_current_exec_stack)(exec_stack*);
void CLG_(pre_signal)(ThreadId tid, Int sigNum, Bool alt_stack);
void CLG_(post_signal)(ThreadId tid, Int sigNum);
void CLG_(run_post_signal_on_call_stack_bottom)(void);

void CLG_(init_dumps)(void);


extern CommandLineOptions CLG_(clo);
extern Statistics CLG_(stat);
extern EventMapping* CLG_(dumpmap);

extern UInt* CLG_(fn_active_array);
extern Bool CLG_(instrument_state);
 
extern Int CLG_(min_line_size);
extern call_stack CLG_(current_call_stack);
extern fn_stack   CLG_(current_fn_stack);
extern exec_state CLG_(current_state);
extern ThreadId   CLG_(current_tid);
extern FullCost   CLG_(total_cost);
extern struct cachesim_if CLG_(cachesim);
extern struct event_sets  CLG_(sets);

extern Addr   CLG_(bb_base);
extern ULong* CLG_(cost_base);



#if CLG_ENABLE_DEBUG

#define CLG_DEBUGIF(x) \
  if (UNLIKELY( (CLG_(clo).verbose >x) && \
                (CLG_(stat).bb_executions >= CLG_(clo).verbose_start)))

#define CLG_DEBUG(x,format,args...)   \
    CLG_DEBUGIF(x) {                  \
      CLG_(print_bbno)();	      \
      VG_(printf)(format,##args);     \
    }

#define CLG_ASSERT(cond)              \
    if (UNLIKELY(!(cond))) {          \
      CLG_(print_context)();          \
      CLG_(print_bbno)();	      \
      tl_assert(cond);                \
     }

#else
#define CLG_DEBUGIF(x) if (0)
#define CLG_DEBUG(x...) {}
#define CLG_ASSERT(cond) tl_assert(cond);
#endif

void CLG_(print_bbno)(void);
void CLG_(print_context)(void);
void CLG_(print_jcc)(int s, jCC* jcc);
void CLG_(print_bbcc)(int s, BBCC* bbcc);
void CLG_(print_bbcc_fn)(BBCC* bbcc);
void CLG_(print_execstate)(int s, exec_state* es);
void CLG_(print_eventset)(int s, EventSet* es);
void CLG_(print_cost)(int s, EventSet*, ULong* cost);
void CLG_(print_bb)(int s, BB* bb);
void CLG_(print_bbcc_cost)(int s, BBCC*);
void CLG_(print_cxt)(int s, Context* cxt, int rec_index);
void CLG_(print_short_jcc)(jCC* jcc);
void CLG_(print_stackentry)(int s, int sp);
void CLG_(print_addr)(Addr addr);
void CLG_(print_addr_ln)(Addr addr);

void* CLG_(malloc)(const HChar* cc, UWord s, const HChar* f);
void* CLG_(free)(void* p, const HChar* f);
#if 0
#define CLG_MALLOC(_cc,x) CLG_(malloc)((_cc),x,__FUNCTION__)
#define CLG_FREE(p)       CLG_(free)(p,__FUNCTION__)
#else
#define CLG_MALLOC(_cc,x) VG_(malloc)((_cc),x)
#define CLG_FREE(p)       VG_(free)(p)
#endif

#endif 
