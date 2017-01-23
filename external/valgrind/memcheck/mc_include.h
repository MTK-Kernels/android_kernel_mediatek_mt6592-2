

/*
   This file is part of MemCheck, a heavyweight Valgrind tool for
   detecting memory errors.

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

#ifndef __MC_INCLUDE_H
#define __MC_INCLUDE_H

#define MC_(str)    VGAPPEND(vgMemCheck_,str)




#define MC_MALLOC_DEFAULT_REDZONE_SZB    16
extern SizeT MC_(Malloc_Redzone_SzB);

typedef
   enum {
      MC_AllocMalloc = 0,
      MC_AllocNew    = 1,
      MC_AllocNewVec = 2,
      MC_AllocCustom = 3
   }
   MC_AllocKind;
   
typedef
   struct _MC_Chunk {
      struct _MC_Chunk* next;
      Addr         data;            
      SizeT        szB : (sizeof(SizeT)*8)-2; 
      MC_AllocKind allockind : 2;   
      ExeContext*  where[0];
   }
   MC_Chunk;

ExeContext* MC_(allocated_at) (MC_Chunk*);
ExeContext* MC_(freed_at) (MC_Chunk*);

void  MC_(set_allocated_at) (ThreadId, MC_Chunk*);
void  MC_(set_freed_at) (ThreadId, MC_Chunk*);

UInt MC_(n_where_pointers) (void);

typedef
   struct _MC_Mempool {
      struct _MC_Mempool* next;
      Addr          pool;           
      SizeT         rzB;            
      Bool          is_zeroed;      
      VgHashTable  *chunks;         
   }
   MC_Mempool;


void* MC_(new_block)  ( ThreadId tid,
                        Addr p, SizeT size, SizeT align,
                        Bool is_zeroed, MC_AllocKind kind,
                        VgHashTable *table);
void MC_(handle_free) ( ThreadId tid,
                        Addr p, UInt rzB, MC_AllocKind kind );

void MC_(create_mempool)  ( Addr pool, UInt rzB, Bool is_zeroed );
void MC_(destroy_mempool) ( Addr pool );
void MC_(mempool_alloc)   ( ThreadId tid, Addr pool,
                            Addr addr, SizeT size );
void MC_(mempool_free)    ( Addr pool, Addr addr );
void MC_(mempool_trim)    ( Addr pool, Addr addr, SizeT size );
void MC_(move_mempool)    ( Addr poolA, Addr poolB );
void MC_(mempool_change)  ( Addr pool, Addr addrA, Addr addrB, SizeT size );
Bool MC_(mempool_exists)  ( Addr pool );

MC_Chunk* MC_(get_freed_block_bracketting)( Addr a );

extern PoolAlloc* MC_(chunk_poolalloc);

extern VgHashTable *MC_(malloc_list);

extern VgHashTable *MC_(mempool_list);

Bool MC_(check_mem_is_noaccess)( Addr a, SizeT len, Addr* bad_addr );
void MC_(make_mem_noaccess)        ( Addr a, SizeT len );
void MC_(make_mem_undefined_w_otag)( Addr a, SizeT len, UInt otag );
void MC_(make_mem_defined)         ( Addr a, SizeT len );
void MC_(copy_address_range_state) ( Addr src, Addr dst, SizeT len );

void MC_(print_malloc_stats) ( void );
SizeT MC_(get_cmalloc_n_frees) ( void );

void* MC_(malloc)               ( ThreadId tid, SizeT n );
void* MC_(__builtin_new)        ( ThreadId tid, SizeT n );
void* MC_(__builtin_vec_new)    ( ThreadId tid, SizeT n );
void* MC_(memalign)             ( ThreadId tid, SizeT align, SizeT n );
void* MC_(calloc)               ( ThreadId tid, SizeT nmemb, SizeT size1 );
void  MC_(free)                 ( ThreadId tid, void* p );
void  MC_(__builtin_delete)     ( ThreadId tid, void* p );
void  MC_(__builtin_vec_delete) ( ThreadId tid, void* p );
void* MC_(realloc)              ( ThreadId tid, void* p, SizeT new_size );
SizeT MC_(malloc_usable_size)   ( ThreadId tid, void* p );

void MC_(handle_resizeInPlace)(ThreadId tid, Addr p,
                               SizeT oldSizeB, SizeT newSizeB, SizeT rzB);



Int MC_(get_otrack_shadow_offset) ( Int offset, Int szB );
IRType MC_(get_otrack_reg_array_equiv_int_type) ( IRRegArray* arr );


#define MC_OKIND_UNKNOWN  0  
#define MC_OKIND_HEAP     1  
#define MC_OKIND_STACK    2  
#define MC_OKIND_USER     3  




#ifdef MC_PROFILE_MEMORY
#  define N_PROF_EVENTS 500

UInt   MC_(event_ctr)[N_PROF_EVENTS];
HChar* MC_(event_ctr_name)[N_PROF_EVENTS];

#  define PROF_EVENT(ev, name)                                \
   do { tl_assert((ev) >= 0 && (ev) < N_PROF_EVENTS);         \
           \
                    \
        if (MC_(event_ctr_name)[ev])                         \
           tl_assert(name == MC_(event_ctr_name)[ev]);       \
        MC_(event_ctr)[ev]++;                                \
        MC_(event_ctr_name)[ev] = (name);                    \
   } while (False);

#else

#  define PROF_EVENT(ev, name) 

#endif   



#define SM_SIZE 65536            
#define SM_MASK (SM_SIZE-1)      

#define V_BIT_DEFINED         0
#define V_BIT_UNDEFINED       1

#define V_BITS8_DEFINED       0
#define V_BITS8_UNDEFINED     0xFF

#define V_BITS16_DEFINED      0
#define V_BITS16_UNDEFINED    0xFFFF

#define V_BITS32_DEFINED      0
#define V_BITS32_UNDEFINED    0xFFFFFFFF

#define V_BITS64_DEFINED      0ULL
#define V_BITS64_UNDEFINED    0xFFFFFFFFFFFFFFFFULL



typedef 
   enum { 
      
      
      Reachable    =0,  
      Possible     =1,  
                        
      IndirectLeak =2,  
                        
      Unreached    =3,  
                        
  }
  Reachedness;

#define R2S(r) (1 << (r))
#define RiS(r,s) ((s) & R2S(r))
UInt MC_(all_Reachedness)(void);

extern SizeT MC_(bytes_leaked);
extern SizeT MC_(bytes_indirect);
extern SizeT MC_(bytes_dubious);
extern SizeT MC_(bytes_reachable);
extern SizeT MC_(bytes_suppressed);

extern SizeT MC_(blocks_leaked);
extern SizeT MC_(blocks_indirect);
extern SizeT MC_(blocks_dubious);
extern SizeT MC_(blocks_reachable);
extern SizeT MC_(blocks_suppressed);

typedef
   enum {
      LC_Off,
      LC_Summary,
      LC_Full,
   }
   LeakCheckMode;

typedef
   enum {
      LCD_Any,       
      LCD_Increased, 
      LCD_Changed,   
                     
   }
   LeakCheckDeltaMode;

typedef
   struct _LossRecordKey {
      Reachedness  state;        
      ExeContext*  allocated_at; 
   } 
   LossRecordKey;

typedef
   struct _LossRecord {
      LossRecordKey key;  
      SizeT szB;          
      SizeT indirect_szB; 
      UInt  num_blocks;   
      SizeT old_szB;          
      SizeT old_indirect_szB; 
      UInt  old_num_blocks;   
   }
   LossRecord;

typedef
   struct _LeakCheckParams {
      LeakCheckMode mode;
      UInt show_leak_kinds;
      UInt errors_for_leak_kinds;
      UInt heuristics;
      LeakCheckDeltaMode deltamode;
      UInt max_loss_records_output; 
      Bool requested_by_monitor_command; 
   }
   LeakCheckParams;

void MC_(detect_memory_leaks) ( ThreadId tid, LeakCheckParams * lcp);

extern UInt MC_(leak_search_gen);

extern LeakCheckDeltaMode MC_(detect_memory_leaks_last_delta_mode);

Bool MC_(print_block_list) ( UInt loss_record_nr);

void MC_(who_points_at) ( Addr address, SizeT szB);

extern HChar * MC_(snprintf_delta) (HChar * buf, Int size, 
                                    SizeT current_val, SizeT old_val, 
                                    LeakCheckDeltaMode delta_mode);


Bool MC_(is_valid_aligned_word)     ( Addr a );
Bool MC_(is_within_valid_secondary) ( Addr a );

void MC_(pp_LossRecord)(UInt n_this_record, UInt n_total_records,
                        LossRecord* l);
                          


extern Bool MC_(any_value_errors);

Bool MC_(eq_Error)           ( VgRes res, const Error* e1, const Error* e2 );
void MC_(before_pp_Error)    ( const Error* err );
void MC_(pp_Error)           ( const Error* err );
UInt MC_(update_Error_extra) ( const Error* err );

Bool MC_(is_recognised_suppression) ( const HChar* name, Supp* su );

Bool MC_(read_extra_suppression_info) ( Int fd, HChar** buf,
                                        SizeT* nBuf, Int* lineno, Supp *su );

Bool MC_(error_matches_suppression) ( const Error* err, const Supp* su );

SizeT MC_(get_extra_suppression_info) ( const Error* err,
                                        HChar* buf, Int nBuf );
SizeT MC_(print_extra_suppression_use) ( const Supp* su,
                                         HChar* buf, Int nBuf );
void MC_(update_extra_suppression_use) ( const Error* err, const Supp* su );

const HChar* MC_(get_error_name) ( const Error* err );

void MC_(record_address_error) ( ThreadId tid, Addr a, Int szB,
                                 Bool isWrite );
void MC_(record_cond_error)    ( ThreadId tid, UInt otag );
void MC_(record_value_error)   ( ThreadId tid, Int szB, UInt otag );
void MC_(record_jump_error)    ( ThreadId tid, Addr a );

void MC_(record_free_error)            ( ThreadId tid, Addr a ); 
void MC_(record_illegal_mempool_error) ( ThreadId tid, Addr a );
void MC_(record_freemismatch_error)    ( ThreadId tid, MC_Chunk* mc );

void MC_(record_overlap_error)  ( ThreadId tid, const HChar* function,
                                  Addr src, Addr dst, SizeT szB );
void MC_(record_core_mem_error) ( ThreadId tid, const HChar* msg );
void MC_(record_regparam_error) ( ThreadId tid, const HChar* msg, UInt otag );
void MC_(record_memparam_error) ( ThreadId tid, Addr a, 
                                  Bool isAddrErr, const HChar* msg, UInt otag );
void MC_(record_user_error)     ( ThreadId tid, Addr a,
                                  Bool isAddrErr, UInt otag );

Bool MC_(record_leak_error)     ( ThreadId tid,
                                  UInt n_this_record,
                                  UInt n_total_records,
                                  LossRecord* lossRecord,
                                  Bool print_record,
                                  Bool count_error );

Bool MC_(record_fishy_value_error)  ( ThreadId tid, const HChar* function,
                                      const HChar *argument_name, SizeT value );

extern const HChar* MC_(parse_leak_kinds_tokens);

void MC_(pp_describe_addr) (Addr a);

Bool MC_(in_ignored_range) ( Addr a );



typedef
   struct {
      Addr        start;
      SizeT       size;
      ExeContext* where;
      HChar*      desc;
   } 
   CGenBlock;

void MC_(get_ClientBlock_array)( CGenBlock** blocks,
                                 UWord* nBlocks );



extern Bool MC_(clo_partial_loads_ok);

extern Long MC_(clo_freelist_vol);

extern Long MC_(clo_freelist_big_blocks);

extern LeakCheckMode MC_(clo_leak_check);

extern VgRes MC_(clo_leak_resolution);

extern UInt MC_(clo_show_leak_kinds);

extern UInt MC_(clo_errors_for_leak_kinds);

typedef 
   enum {
      LchNone                =0,
      
      LchStdString           =1,
      
      
      LchLength64            =2,
      
      
      
      
      LchNewArray            =3,
      
      
      
      LchMultipleInheritance =4,
      
      
  }
  LeakCheckHeuristic;

#define N_LEAK_CHECK_HEURISTICS 5

#define H2S(h) (1 << (h))
#define HiS(h,s) ((s) & H2S(h))

extern UInt MC_(clo_leak_check_heuristics);

extern Bool MC_(clo_workaround_gcc296_bugs);

extern Int MC_(clo_malloc_fill);
extern Int MC_(clo_free_fill);

typedef
   enum {                 
      KS_none,            
      KS_alloc,           
      KS_free,            
      KS_alloc_then_free, 
      KS_alloc_and_free,  
   }
   KeepStacktraces;
extern KeepStacktraces MC_(clo_keep_stacktraces);

extern Int MC_(clo_mc_level);

extern Bool MC_(clo_show_mismatched_frees);




VG_REGPARM(2) void MC_(helperc_value_checkN_fail_w_o) ( HWord, UWord );
VG_REGPARM(1) void MC_(helperc_value_check8_fail_w_o) ( UWord );
VG_REGPARM(1) void MC_(helperc_value_check4_fail_w_o) ( UWord );
VG_REGPARM(1) void MC_(helperc_value_check1_fail_w_o) ( UWord );
VG_REGPARM(1) void MC_(helperc_value_check0_fail_w_o) ( UWord );

VG_REGPARM(1) void MC_(helperc_value_checkN_fail_no_o) ( HWord );
VG_REGPARM(0) void MC_(helperc_value_check8_fail_no_o) ( void );
VG_REGPARM(0) void MC_(helperc_value_check4_fail_no_o) ( void );
VG_REGPARM(0) void MC_(helperc_value_check1_fail_no_o) ( void );
VG_REGPARM(0) void MC_(helperc_value_check0_fail_no_o) ( void );

VG_REGPARM(1) void MC_(helperc_STOREV64be) ( Addr, ULong );
VG_REGPARM(1) void MC_(helperc_STOREV64le) ( Addr, ULong );
VG_REGPARM(2) void MC_(helperc_STOREV32be) ( Addr, UWord );
VG_REGPARM(2) void MC_(helperc_STOREV32le) ( Addr, UWord );
VG_REGPARM(2) void MC_(helperc_STOREV16be) ( Addr, UWord );
VG_REGPARM(2) void MC_(helperc_STOREV16le) ( Addr, UWord );
VG_REGPARM(2) void MC_(helperc_STOREV8)    ( Addr, UWord );

VG_REGPARM(2) void  MC_(helperc_LOADV256be) ( V256*, Addr );
VG_REGPARM(2) void  MC_(helperc_LOADV256le) ( V256*, Addr );
VG_REGPARM(2) void  MC_(helperc_LOADV128be) ( V128*, Addr );
VG_REGPARM(2) void  MC_(helperc_LOADV128le) ( V128*, Addr );
VG_REGPARM(1) ULong MC_(helperc_LOADV64be)  ( Addr );
VG_REGPARM(1) ULong MC_(helperc_LOADV64le)  ( Addr );
VG_REGPARM(1) UWord MC_(helperc_LOADV32be)  ( Addr );
VG_REGPARM(1) UWord MC_(helperc_LOADV32le)  ( Addr );
VG_REGPARM(1) UWord MC_(helperc_LOADV16be)  ( Addr );
VG_REGPARM(1) UWord MC_(helperc_LOADV16le)  ( Addr );
VG_REGPARM(1) UWord MC_(helperc_LOADV8)     ( Addr );

void MC_(helperc_MAKE_STACK_UNINIT) ( Addr base, UWord len,
                                                 Addr nia );

VG_REGPARM(2) void  MC_(helperc_b_store1) ( Addr a, UWord d32 );
VG_REGPARM(2) void  MC_(helperc_b_store2) ( Addr a, UWord d32 );
VG_REGPARM(2) void  MC_(helperc_b_store4) ( Addr a, UWord d32 );
VG_REGPARM(2) void  MC_(helperc_b_store8) ( Addr a, UWord d32 );
VG_REGPARM(2) void  MC_(helperc_b_store16)( Addr a, UWord d32 );
VG_REGPARM(2) void  MC_(helperc_b_store32)( Addr a, UWord d32 );
VG_REGPARM(1) UWord MC_(helperc_b_load1) ( Addr a );
VG_REGPARM(1) UWord MC_(helperc_b_load2) ( Addr a );
VG_REGPARM(1) UWord MC_(helperc_b_load4) ( Addr a );
VG_REGPARM(1) UWord MC_(helperc_b_load8) ( Addr a );
VG_REGPARM(1) UWord MC_(helperc_b_load16)( Addr a );
VG_REGPARM(1) UWord MC_(helperc_b_load32)( Addr a );

IRSB* MC_(instrument) ( VgCallbackClosure* closure,
                        IRSB* bb_in, 
                        const VexGuestLayout* layout, 
                        const VexGuestExtents* vge,
                        const VexArchInfo* archinfo_host,
                        IRType gWordTy, IRType hWordTy );

IRSB* MC_(final_tidy) ( IRSB* );

#endif 

