

/*
   This file is part of Helgrind, a Valgrind tool for detecting errors
   in threaded programs.

   Copyright (C) 2007-2013 OpenWorks LLP
      info@open-works.co.uk

   Copyright (C) 2007-2013 Apple, Inc.

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

   Neither the names of the U.S. Department of Energy nor the
   University of California nor the names of its contributors may be
   used to endorse or promote products derived from this software
   without prior written permission.
*/

#include "pub_tool_basics.h"
#include "pub_tool_gdbserver.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_threadstate.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_hashtable.h"
#include "pub_tool_replacemalloc.h"
#include "pub_tool_machine.h"
#include "pub_tool_options.h"
#include "pub_tool_xarray.h"
#include "pub_tool_stacktrace.h"
#include "pub_tool_wordfm.h"
#include "pub_tool_debuginfo.h" 
#include "pub_tool_redir.h"     
#include "pub_tool_vki.h"       
#include "pub_tool_libcproc.h"  
#include "pub_tool_aspacemgr.h" 
#include "pub_tool_poolalloc.h"
#include "pub_tool_addrinfo.h"

#include "hg_basics.h"
#include "hg_wordset.h"
#include "hg_addrdescr.h"
#include "hg_lock_n_thread.h"
#include "hg_errors.h"

#include "libhb.h"

#include "helgrind.h"












#define SHOW_EVENTS 0


static void all__sanity_check ( const HChar* who ); 

#define HG_CLI__DEFAULT_MALLOC_REDZONE_SZB 16 

#define SHOW_DATA_STRUCTURES 0






static Thread* admin_threads = NULL;
Thread* get_admin_threads ( void ) { return admin_threads; }

static Lock* admin_locks = NULL;

static Thread** map_threads = NULL; 

static WordFM* map_locks = NULL; 

static WordSetU* univ_lsets = NULL; 
static WordSetU* univ_laog  = NULL; 
static Int next_gc_univ_laog = 1;

WordSetU* HG_(get_univ_lsets) ( void ) { return univ_lsets; }

Lock* HG_(get_admin_locks) ( void ) { return admin_locks; }



static UWord stats__lockN_acquires = 0;
static UWord stats__lockN_releases = 0;

static
ThreadId map_threads_maybe_reverse_lookup_SLOW ( Thread* thr ); 


static Thread* mk_Thread ( Thr* hbthr ) {
   static Int indx      = 1;
   Thread* thread       = HG_(zalloc)( "hg.mk_Thread.1", sizeof(Thread) );
   thread->locksetA     = HG_(emptyWS)( univ_lsets );
   thread->locksetW     = HG_(emptyWS)( univ_lsets );
   thread->magic        = Thread_MAGIC;
   thread->hbthr        = hbthr;
   thread->coretid      = VG_INVALID_THREADID;
   thread->created_at   = NULL;
   thread->announced    = False;
   thread->errmsg_index = indx++;
   thread->admin        = admin_threads;
   admin_threads        = thread;
   return thread;
}

static Lock* mk_LockN ( LockKind kind, Addr guestaddr ) {
   static ULong unique = 0;
   Lock* lock             = HG_(zalloc)( "hg.mk_Lock.1", sizeof(Lock) );
   
   if (admin_locks)
      admin_locks->admin_prev = lock;
   lock->admin_next       = admin_locks;
   lock->admin_prev       = NULL;
   admin_locks            = lock;
   
   lock->unique           = unique++;
   lock->magic            = LockN_MAGIC;
   lock->appeared_at      = NULL;
   lock->acquired_at      = NULL;
   lock->hbso             = libhb_so_alloc();
   lock->guestaddr        = guestaddr;
   lock->kind             = kind;
   lock->heldW            = False;
   lock->heldBy           = NULL;
   tl_assert(HG_(is_sane_LockN)(lock));
   return lock;
}

static void del_LockN ( Lock* lk ) 
{
   tl_assert(HG_(is_sane_LockN)(lk));
   tl_assert(lk->hbso);
   libhb_so_dealloc(lk->hbso);
   if (lk->heldBy)
      VG_(deleteBag)( lk->heldBy );
   
   if (lk == admin_locks) {
      tl_assert(lk->admin_prev == NULL);
      if (lk->admin_next)
         lk->admin_next->admin_prev = NULL;
      admin_locks = lk->admin_next;
   }
   else {
      tl_assert(lk->admin_prev != NULL);
      lk->admin_prev->admin_next = lk->admin_next;
      if (lk->admin_next)
         lk->admin_next->admin_prev = lk->admin_prev;
   }
   
   VG_(memset)(lk, 0xAA, sizeof(*lk));
   HG_(free)(lk);
}

static void lockN_acquire_writer ( Lock* lk, Thread* thr ) 
{
   tl_assert(HG_(is_sane_LockN)(lk));
   tl_assert(HG_(is_sane_Thread)(thr));

   stats__lockN_acquires++;

   
   if (lk->acquired_at == NULL) {
      ThreadId tid;
      tl_assert(lk->heldBy == NULL);
      tid = map_threads_maybe_reverse_lookup_SLOW(thr);
      lk->acquired_at
         = VG_(record_ExeContext)(tid, 0);
   } else {
      tl_assert(lk->heldBy != NULL);
   }
   

   switch (lk->kind) {
      case LK_nonRec:
      case_LK_nonRec:
         tl_assert(lk->heldBy == NULL); 
         tl_assert(!lk->heldW);
         lk->heldW  = True;
         lk->heldBy = VG_(newBag)( HG_(zalloc), "hg.lNaw.1", HG_(free) );
         VG_(addToBag)( lk->heldBy, (UWord)thr );
         break;
      case LK_mbRec:
         if (lk->heldBy == NULL)
            goto case_LK_nonRec;
         
         tl_assert(lk->heldW);
         
         tl_assert(VG_(sizeUniqueBag(lk->heldBy)) == 1);
         
         tl_assert(VG_(elemBag)(lk->heldBy, (UWord)thr)
                   == VG_(sizeTotalBag)(lk->heldBy));
         VG_(addToBag)(lk->heldBy, (UWord)thr);
         break;
      case LK_rdwr:
         tl_assert(lk->heldBy == NULL && !lk->heldW); 
         goto case_LK_nonRec;
      default: 
         tl_assert(0);
  }
  tl_assert(HG_(is_sane_LockN)(lk));
}

static void lockN_acquire_reader ( Lock* lk, Thread* thr )
{
   tl_assert(HG_(is_sane_LockN)(lk));
   tl_assert(HG_(is_sane_Thread)(thr));
   
   tl_assert(lk->kind == LK_rdwr);
   
   tl_assert(lk->heldBy == NULL 
             || (lk->heldBy != NULL && !lk->heldW));

   stats__lockN_acquires++;

   
   if (lk->acquired_at == NULL) {
      ThreadId tid;
      tl_assert(lk->heldBy == NULL);
      tid = map_threads_maybe_reverse_lookup_SLOW(thr);
      lk->acquired_at
         = VG_(record_ExeContext)(tid, 0);
   } else {
      tl_assert(lk->heldBy != NULL);
   }
   

   if (lk->heldBy) {
      VG_(addToBag)(lk->heldBy, (UWord)thr);
   } else {
      lk->heldW  = False;
      lk->heldBy = VG_(newBag)( HG_(zalloc), "hg.lNar.1", HG_(free) );
      VG_(addToBag)( lk->heldBy, (UWord)thr );
   }
   tl_assert(!lk->heldW);
   tl_assert(HG_(is_sane_LockN)(lk));
}


static void lockN_release ( Lock* lk, Thread* thr )
{
   Bool b;
   tl_assert(HG_(is_sane_LockN)(lk));
   tl_assert(HG_(is_sane_Thread)(thr));
   
   tl_assert(lk->heldBy);
   stats__lockN_releases++;
   
   b = VG_(delFromBag)(lk->heldBy, (UWord)thr);
   
   tl_assert(b);
   
   tl_assert(lk->acquired_at);
   if (VG_(isEmptyBag)(lk->heldBy)) {
      VG_(deleteBag)(lk->heldBy);
      lk->heldBy      = NULL;
      lk->heldW       = False;
      lk->acquired_at = NULL;
   }
   tl_assert(HG_(is_sane_LockN)(lk));
}

static void remove_Lock_from_locksets_of_all_owning_Threads( Lock* lk )
{
   Thread* thr;
   if (!lk->heldBy) {
      tl_assert(!lk->heldW);
      return;
   }
   
   VG_(initIterBag)( lk->heldBy );
   while (VG_(nextIterBag)( lk->heldBy, (UWord*)&thr, NULL )) {
      tl_assert(HG_(is_sane_Thread)(thr));
      tl_assert(HG_(elemWS)( univ_lsets,
                             thr->locksetA, (UWord)lk ));
      thr->locksetA
         = HG_(delFromWS)( univ_lsets, thr->locksetA, (UWord)lk );

      if (lk->heldW) {
         tl_assert(HG_(elemWS)( univ_lsets,
                                thr->locksetW, (UWord)lk ));
         thr->locksetW
            = HG_(delFromWS)( univ_lsets, thr->locksetW, (UWord)lk );
      }
   }
   VG_(doneIterBag)( lk->heldBy );
}



#define PP_THREADS      (1<<1)
#define PP_LOCKS        (1<<2)
#define PP_ALL (PP_THREADS | PP_LOCKS)


static const Int sHOW_ADMIN = 0;

static void space ( Int n )
{
   Int  i;
   HChar spaces[128+1];
   tl_assert(n >= 0 && n < 128);
   if (n == 0)
      return;
   for (i = 0; i < n; i++)
      spaces[i] = ' ';
   spaces[i] = 0;
   tl_assert(i < 128+1);
   VG_(printf)("%s", spaces);
}

static void pp_Thread ( Int d, Thread* t )
{
   space(d+0); VG_(printf)("Thread %p {\n", t);
   if (sHOW_ADMIN) {
   space(d+3); VG_(printf)("admin    %p\n",   t->admin);
   space(d+3); VG_(printf)("magic    0x%x\n", (UInt)t->magic);
   }
   space(d+3); VG_(printf)("locksetA %d\n",   (Int)t->locksetA);
   space(d+3); VG_(printf)("locksetW %d\n",   (Int)t->locksetW);
   space(d+0); VG_(printf)("}\n");
}

static void pp_admin_threads ( Int d )
{
   Int     i, n;
   Thread* t;
   for (n = 0, t = admin_threads;  t;  n++, t = t->admin) {
      
   }
   space(d); VG_(printf)("admin_threads (%d records) {\n", n);
   for (i = 0, t = admin_threads;  t;  i++, t = t->admin) {
      if (0) {
         space(n); 
         VG_(printf)("admin_threads record %d of %d:\n", i, n);
      }
      pp_Thread(d+3, t);
   }
   space(d); VG_(printf)("}\n");
}

static void pp_map_threads ( Int d )
{
   Int i, n = 0;
   space(d); VG_(printf)("map_threads ");
   for (i = 0; i < VG_N_THREADS; i++) {
      if (map_threads[i] != NULL)
         n++;
   }
   VG_(printf)("(%d entries) {\n", n);
   for (i = 0; i < VG_N_THREADS; i++) {
      if (map_threads[i] == NULL)
         continue;
      space(d+3);
      VG_(printf)("coretid %d -> Thread %p\n", i, map_threads[i]);
   }
   space(d); VG_(printf)("}\n");
}

static const HChar* show_LockKind ( LockKind lkk ) {
   switch (lkk) {
      case LK_mbRec:  return "mbRec";
      case LK_nonRec: return "nonRec";
      case LK_rdwr:   return "rdwr";
      default:        tl_assert(0);
   }
}

static void pp_Lock ( Int d, Lock* lk,
                      Bool show_lock_addrdescr,
                      Bool show_internal_data)
{
   space(d+0); 
   if (show_internal_data)
      VG_(printf)("Lock %p (ga %#lx) {\n", lk, lk->guestaddr);
   else
      VG_(printf)("Lock ga %#lx {\n", lk->guestaddr);
   if (!show_lock_addrdescr 
       || !HG_(get_and_pp_addrdescr) ((Addr) lk->guestaddr))
      VG_(printf)("\n");
      
   if (sHOW_ADMIN) {
      space(d+3); VG_(printf)("admin_n  %p\n",   lk->admin_next);
      space(d+3); VG_(printf)("admin_p  %p\n",   lk->admin_prev);
      space(d+3); VG_(printf)("magic    0x%x\n", (UInt)lk->magic);
   }
   if (show_internal_data) {
      space(d+3); VG_(printf)("unique %llu\n", lk->unique);
   }
   space(d+3); VG_(printf)("kind   %s\n", show_LockKind(lk->kind));
   if (show_internal_data) {
      space(d+3); VG_(printf)("heldW  %s\n", lk->heldW ? "yes" : "no");
   }
   if (show_internal_data) {
      space(d+3); VG_(printf)("heldBy %p", lk->heldBy);
   }
   if (lk->heldBy) {
      Thread* thr;
      UWord   count;
      VG_(printf)(" { ");
      VG_(initIterBag)( lk->heldBy );
      while (VG_(nextIterBag)( lk->heldBy, (UWord*)&thr, &count )) {
         if (show_internal_data)
            VG_(printf)("%lu:%p ", count, thr);
         else {
            VG_(printf)("%c%lu:thread #%d ",
                        lk->heldW ? 'W' : 'R',
                        count, thr->errmsg_index);
            if (thr->coretid == VG_INVALID_THREADID) 
               VG_(printf)("tid (exited) ");
            else
               VG_(printf)("tid %d ", thr->coretid);

         }
      }
      VG_(doneIterBag)( lk->heldBy );
      VG_(printf)("}\n");
   }
   space(d+0); VG_(printf)("}\n");
}

static void pp_admin_locks ( Int d )
{
   Int   i, n;
   Lock* lk;
   for (n = 0, lk = admin_locks;  lk;  n++, lk = lk->admin_next) {
      
   }
   space(d); VG_(printf)("admin_locks (%d records) {\n", n);
   for (i = 0, lk = admin_locks;  lk;  i++, lk = lk->admin_next) {
      if (0) {
         space(n); 
         VG_(printf)("admin_locks record %d of %d:\n", i, n);
      }
      pp_Lock(d+3, lk,
              False ,
              True );
   }
   space(d); VG_(printf)("}\n");
}

static void pp_map_locks ( Int d)
{
   void* gla;
   Lock* lk;
   space(d); VG_(printf)("map_locks (%d entries) {\n",
                         (Int)VG_(sizeFM)( map_locks ));
   VG_(initIterFM)( map_locks );
   while (VG_(nextIterFM)( map_locks, (UWord*)&gla,
                                      (UWord*)&lk )) {
      space(d+3);
      VG_(printf)("guest %p -> Lock %p\n", gla, lk);
   }
   VG_(doneIterFM)( map_locks );
   space(d); VG_(printf)("}\n");
}

static void pp_everything ( Int flags, const HChar* caller )
{
   Int d = 0;
   VG_(printf)("\n");
   VG_(printf)("All_Data_Structures (caller = \"%s\") {\n", caller);
   if (flags & PP_THREADS) {
      VG_(printf)("\n");
      pp_admin_threads(d+3);
      VG_(printf)("\n");
      pp_map_threads(d+3);
   }
   if (flags & PP_LOCKS) {
      VG_(printf)("\n");
      pp_admin_locks(d+3);
      VG_(printf)("\n");
      pp_map_locks(d+3);
   }

   VG_(printf)("\n");
   VG_(printf)("}\n");
   VG_(printf)("\n");
}

#undef SHOW_ADMIN



static void initialise_data_structures ( Thr* hbthr_root )
{
   Thread*   thr;
   WordSetID wsid;

   
   tl_assert(admin_threads == NULL);
   tl_assert(admin_locks == NULL);

   tl_assert(map_threads == NULL);
   map_threads = HG_(zalloc)( "hg.ids.1", VG_N_THREADS * sizeof(Thread*) );

   tl_assert(sizeof(Addr) == sizeof(UWord));
   tl_assert(map_locks == NULL);
   map_locks = VG_(newFM)( HG_(zalloc), "hg.ids.2", HG_(free), 
                           NULL);

   tl_assert(univ_lsets == NULL);
   univ_lsets = HG_(newWordSetU)( HG_(zalloc), "hg.ids.4", HG_(free),
                                  8 );
   tl_assert(univ_lsets != NULL);
   wsid = HG_(emptyWS)(univ_lsets);
   tl_assert(wsid == 0);

   tl_assert(univ_laog == NULL);
   if (HG_(clo_track_lockorders)) {
      univ_laog = HG_(newWordSetU)( HG_(zalloc), "hg.ids.5 (univ_laog)",
                                    HG_(free), 24 );
      tl_assert(univ_laog != NULL);
   }

   
   

   
   thr = mk_Thread(hbthr_root);
   thr->coretid = 1; 
   tl_assert( libhb_get_Thr_hgthread(hbthr_root) == NULL );
   libhb_set_Thr_hgthread(hbthr_root, thr);

   
   tl_assert(HG_(is_sane_ThreadId)(thr->coretid));
   tl_assert(thr->coretid != VG_INVALID_THREADID);

   map_threads[thr->coretid] = thr;

   tl_assert(VG_INVALID_THREADID == 0);

   all__sanity_check("initialise_data_structures");
}



static Thread* map_threads_maybe_lookup ( ThreadId coretid )
{
   Thread* thr;
   tl_assert( HG_(is_sane_ThreadId)(coretid) );
   thr = map_threads[coretid];
   return thr;
}

static inline Thread* map_threads_lookup ( ThreadId coretid )
{
   Thread* thr;
   tl_assert( HG_(is_sane_ThreadId)(coretid) );
   thr = map_threads[coretid];
   tl_assert(thr);
   return thr;
}

static ThreadId map_threads_maybe_reverse_lookup_SLOW ( Thread* thr )
{
   ThreadId tid;
   tl_assert(HG_(is_sane_Thread)(thr));
   
   tl_assert(VG_INVALID_THREADID >= 0 && VG_INVALID_THREADID < VG_N_THREADS);
   tl_assert(map_threads[VG_INVALID_THREADID] == NULL);
   tid = thr->coretid;
   tl_assert(HG_(is_sane_ThreadId)(tid));
   return tid;
}

static ThreadId map_threads_reverse_lookup_SLOW ( Thread* thr )
{
   ThreadId tid = map_threads_maybe_reverse_lookup_SLOW( thr );
   tl_assert(tid != VG_INVALID_THREADID);
   tl_assert(map_threads[tid]);
   tl_assert(map_threads[tid]->coretid == tid);
   return tid;
}

static void map_threads_delete ( ThreadId coretid )
{
   Thread* thr;
   tl_assert(coretid != 0);
   tl_assert( HG_(is_sane_ThreadId)(coretid) );
   thr = map_threads[coretid];
   tl_assert(thr);
   map_threads[coretid] = NULL;
}



static 
Lock* map_locks_lookup_or_create ( LockKind lkk, Addr ga, ThreadId tid )
{
   Bool  found;
   Lock* oldlock = NULL;
   tl_assert(HG_(is_sane_ThreadId)(tid));
   found = VG_(lookupFM)( map_locks, 
                          NULL, (UWord*)&oldlock, (UWord)ga );
   if (!found) {
      Lock* lock = mk_LockN(lkk, ga);
      lock->appeared_at = VG_(record_ExeContext)( tid, 0 );
      tl_assert(HG_(is_sane_LockN)(lock));
      VG_(addToFM)( map_locks, (UWord)ga, (UWord)lock );
      tl_assert(oldlock == NULL);
      return lock;
   } else {
      tl_assert(oldlock != NULL);
      tl_assert(HG_(is_sane_LockN)(oldlock));
      tl_assert(oldlock->guestaddr == ga);
      return oldlock;
   }
}

static Lock* map_locks_maybe_lookup ( Addr ga )
{
   Bool  found;
   Lock* lk = NULL;
   found = VG_(lookupFM)( map_locks, NULL, (UWord*)&lk, (UWord)ga );
   tl_assert(found  ?  lk != NULL  :  lk == NULL);
   return lk;
}

static void map_locks_delete ( Addr ga )
{
   Addr  ga2 = 0;
   Lock* lk  = NULL;
   VG_(delFromFM)( map_locks,
                   (UWord*)&ga2, (UWord*)&lk, (UWord)ga );
   tl_assert(lk != NULL);
   tl_assert(ga2 == ga);
}




static UWord stats__sanity_checks = 0;

static void laog__sanity_check ( const HChar* who ); 



static Bool thread_is_a_holder_of_Lock ( Thread* thr, Lock* lk )
{
   if (lk->heldBy)
      return VG_(elemBag)( lk->heldBy, (UWord)thr ) > 0;
   else
      return False;
}

__attribute__((noinline))
static void threads__sanity_check ( const HChar* who )
{
#define BAD(_str) do { how = (_str); goto bad; } while (0)
   const HChar* how = "no error";
   Thread*   thr;
   WordSetID wsA, wsW;
   UWord*    ls_words;
   UWord     ls_size, i;
   Lock*     lk;
   for (thr = admin_threads; thr; thr = thr->admin) {
      if (!HG_(is_sane_Thread)(thr)) BAD("1");
      wsA = thr->locksetA;
      wsW = thr->locksetW;
      
      if (!HG_(isSubsetOf)( univ_lsets, wsW, wsA )) BAD("7");
      HG_(getPayloadWS)( &ls_words, &ls_size, univ_lsets, wsA );
      for (i = 0; i < ls_size; i++) {
         lk = (Lock*)ls_words[i];
         
         if (!HG_(is_sane_LockN)(lk)) BAD("2");
         
         
         if (!thread_is_a_holder_of_Lock(thr,lk)) BAD("3");
      }
   }
   return;
  bad:
   VG_(printf)("threads__sanity_check: who=\"%s\", bad=\"%s\"\n", who, how);
   tl_assert(0);
#undef BAD
}


__attribute__((noinline))
static void locks__sanity_check ( const HChar* who )
{
#define BAD(_str) do { how = (_str); goto bad; } while (0)
   const HChar* how = "no error";
   Addr      gla;
   Lock*     lk;
   Int       i;
   
   for (i = 0, lk = admin_locks;  lk;  i++, lk = lk->admin_next)
      ;
   if (i != VG_(sizeFM)(map_locks)) BAD("1");
   
   
   VG_(initIterFM)( map_locks );
   while (VG_(nextIterFM)( map_locks,
                           (UWord*)&gla, (UWord*)&lk )) {
      if (lk->guestaddr != gla) BAD("2");
   }
   VG_(doneIterFM)( map_locks );
   
   for (lk = admin_locks; lk; lk = lk->admin_next) {
      
      
      if (!HG_(is_sane_LockN)(lk)) BAD("3");
      
      if (lk != map_locks_maybe_lookup(lk->guestaddr)) BAD("4");
      
      
      if (lk->heldBy) {
         Thread* thr;
         UWord   count;
         VG_(initIterBag)( lk->heldBy );
         while (VG_(nextIterBag)( lk->heldBy, 
                                  (UWord*)&thr, &count )) {
            
            tl_assert(count >= 1);
            tl_assert(HG_(is_sane_Thread)(thr));
            if (!HG_(elemWS)(univ_lsets, thr->locksetA, (UWord)lk)) 
               BAD("6");
            
            if (lk->heldW 
                && !HG_(elemWS)(univ_lsets, thr->locksetW, (UWord)lk)) 
               BAD("7");
            if ((!lk->heldW)
                && HG_(elemWS)(univ_lsets, thr->locksetW, (UWord)lk)) 
               BAD("8");
         }
         VG_(doneIterBag)( lk->heldBy );
      } else {
         
         if (lk->heldW) BAD("9"); 
         
         
      }
   }

   return;
  bad:
   VG_(printf)("locks__sanity_check: who=\"%s\", bad=\"%s\"\n", who, how);
   tl_assert(0);
#undef BAD
}


static void all_except_Locks__sanity_check ( const HChar* who ) {
   stats__sanity_checks++;
   if (0) VG_(printf)("all_except_Locks__sanity_check(%s)\n", who);
   threads__sanity_check(who);
   if (HG_(clo_track_lockorders))
      laog__sanity_check(who);
}
static void all__sanity_check ( const HChar* who ) {
   all_except_Locks__sanity_check(who);
   locks__sanity_check(who);
}



static void laog__pre_thread_acquires_lock ( Thread*, Lock* ); 
static inline Thread* get_current_Thread ( void ); 
__attribute__((noinline))
static void laog__handle_one_lock_deletion ( Lock* lk ); 


static void shadow_mem_scopy_range ( Thread* thr,
                                     Addr src, Addr dst, SizeT len )
{
   Thr*     hbthr = thr->hbthr;
   tl_assert(hbthr);
   libhb_copy_shadow_state( hbthr, src, dst, len );
}

static void shadow_mem_cread_range ( Thread* thr, Addr a, SizeT len )
{
   Thr*     hbthr = thr->hbthr;
   tl_assert(hbthr);
   LIBHB_CREAD_N(hbthr, a, len);
}

static void shadow_mem_cwrite_range ( Thread* thr, Addr a, SizeT len ) {
   Thr*     hbthr = thr->hbthr;
   tl_assert(hbthr);
   LIBHB_CWRITE_N(hbthr, a, len);
}

static void shadow_mem_make_New ( Thread* thr, Addr a, SizeT len )
{
   libhb_srange_new( thr->hbthr, a, len );
}

static void shadow_mem_make_NoAccess_NoFX ( Thread* thr, Addr aIN, SizeT len )
{
   if (0 && len > 500)
      VG_(printf)("make NoAccess_NoFX ( %#lx, %ld )\n", aIN, len );
   
   libhb_srange_noaccess_NoFX( thr->hbthr, aIN, len );
}

static void shadow_mem_make_NoAccess_AHAE ( Thread* thr, Addr aIN, SizeT len )
{
   if (0 && len > 500)
      VG_(printf)("make NoAccess_AHAE ( %#lx, %ld )\n", aIN, len );
   
   libhb_srange_noaccess_AHAE( thr->hbthr, aIN, len );
}

static void shadow_mem_make_Untracked ( Thread* thr, Addr aIN, SizeT len )
{
   if (0 && len > 500)
      VG_(printf)("make Untracked ( %#lx, %ld )\n", aIN, len );
   libhb_srange_untrack( thr->hbthr, aIN, len );
}






static 
void evhH__post_thread_w_acquires_lock ( Thread* thr, 
                                         LockKind lkk, Addr lock_ga )
{
   Lock* lk; 


   tl_assert(HG_(is_sane_Thread)(thr));
   lk = map_locks_lookup_or_create( 
           lkk, lock_ga, map_threads_reverse_lookup_SLOW(thr) );
   tl_assert( HG_(is_sane_LockN)(lk) );

   
   tl_assert(thr->hbthr);
   tl_assert(lk->hbso);

   if (lk->heldBy == NULL) {
      
      tl_assert(!lk->heldW);
      lockN_acquire_writer( lk, thr );
      
      libhb_so_recv( thr->hbthr, lk->hbso, True );
      goto noerror;
   }

   tl_assert(lk->heldBy);
   if (!lk->heldW) {
      HG_(record_error_Misc)(
         thr, "Bug in libpthread: write lock "
              "granted on rwlock which is currently rd-held");
      goto error;
   }

   tl_assert(VG_(sizeUniqueBag)(lk->heldBy) == 1); 

   if (thr != (Thread*)VG_(anyElementOfBag)(lk->heldBy)) {
      HG_(record_error_Misc)(
         thr, "Bug in libpthread: write lock "
              "granted on mutex/rwlock which is currently "
              "wr-held by a different thread");
      goto error;
   }

   if (lk->kind != LK_mbRec) {
      HG_(record_error_Misc)(
         thr, "Bug in libpthread: recursive write lock "
              "granted on mutex/wrlock which does not "
              "support recursion");
      goto error;
   }

   
   lockN_acquire_writer( lk, thr );
   libhb_so_recv( thr->hbthr, lk->hbso, True );
   goto noerror;

  noerror:
   if (HG_(clo_track_lockorders)) {
      laog__pre_thread_acquires_lock( thr, lk );
   }
   
   thr->locksetA = HG_(addToWS)( univ_lsets, thr->locksetA, (UWord)lk );
   thr->locksetW = HG_(addToWS)( univ_lsets, thr->locksetW, (UWord)lk );
   

  error:
   tl_assert(HG_(is_sane_LockN)(lk));
}


static 
void evhH__post_thread_r_acquires_lock ( Thread* thr, 
                                         LockKind lkk, Addr lock_ga )
{
   Lock* lk; 


   tl_assert(HG_(is_sane_Thread)(thr));
   tl_assert(lkk == LK_rdwr);
   lk = map_locks_lookup_or_create( 
           lkk, lock_ga, map_threads_reverse_lookup_SLOW(thr) );
   tl_assert( HG_(is_sane_LockN)(lk) );

   
   tl_assert(thr->hbthr);
   tl_assert(lk->hbso);

   if (lk->heldBy == NULL) {
      
      tl_assert(!lk->heldW);
      lockN_acquire_reader( lk, thr );
      
      libhb_so_recv( thr->hbthr, lk->hbso, False );
      goto noerror;
   }

   tl_assert(lk->heldBy);
   if (lk->heldW) {
      HG_(record_error_Misc)( thr, "Bug in libpthread: read lock "
                                   "granted on rwlock which is "
                                   "currently wr-held");
      goto error;
   }

   lockN_acquire_reader( lk, thr );
   libhb_so_recv( thr->hbthr, lk->hbso, False );
   goto noerror;

  noerror:
   if (HG_(clo_track_lockorders)) {
      laog__pre_thread_acquires_lock( thr, lk );
   }
   
   thr->locksetA = HG_(addToWS)( univ_lsets, thr->locksetA, (UWord)lk );
   
   

  error:
   tl_assert(HG_(is_sane_LockN)(lk));
}


static 
void evhH__pre_thread_releases_lock ( Thread* thr,
                                      Addr lock_ga, Bool isRDWR )
{
   Lock* lock;
   Word  n;
   Bool  was_heldW;


   tl_assert(HG_(is_sane_Thread)(thr));
   lock = map_locks_maybe_lookup( lock_ga );

   if (!lock) {
      HG_(record_error_UnlockBogus)( thr, lock_ga );
      return;
   }

   tl_assert(lock->guestaddr == lock_ga);
   tl_assert(HG_(is_sane_LockN)(lock));

   if (isRDWR && lock->kind != LK_rdwr) {
      HG_(record_error_Misc)( thr, "pthread_rwlock_unlock with a "
                                   "pthread_mutex_t* argument " );
   }
   if ((!isRDWR) && lock->kind == LK_rdwr) {
      HG_(record_error_Misc)( thr, "pthread_mutex_unlock with a "
                                   "pthread_rwlock_t* argument " );
   }

   if (!lock->heldBy) {
      tl_assert(!lock->heldW);
      HG_(record_error_UnlockUnlocked)( thr, lock );
      tl_assert(!HG_(elemWS)( univ_lsets, thr->locksetA, (UWord)lock ));
      tl_assert(!HG_(elemWS)( univ_lsets, thr->locksetW, (UWord)lock ));
      goto error;
   }

   
   tl_assert(lock->heldBy);
   was_heldW = lock->heldW;

   n = VG_(elemBag)( lock->heldBy, (UWord)thr );
   tl_assert(n >= 0);
   if (n == 0) {
      Thread* realOwner = (Thread*)VG_(anyElementOfBag)( lock->heldBy );
      tl_assert(HG_(is_sane_Thread)(realOwner));
      tl_assert(realOwner != thr);
      tl_assert(!HG_(elemWS)( univ_lsets, thr->locksetA, (UWord)lock ));
      tl_assert(!HG_(elemWS)( univ_lsets, thr->locksetW, (UWord)lock ));
      HG_(record_error_UnlockForeign)( thr, realOwner, lock );
      goto error;
   }

   
   tl_assert(n >= 1);

   lockN_release( lock, thr );

   n--;
   tl_assert(n >= 0);

   if (n > 0) {
      tl_assert(lock->heldBy);
      tl_assert(n == VG_(elemBag)( lock->heldBy, (UWord)thr )); 
      tl_assert(lock->kind == LK_mbRec
                || (lock->kind == LK_rdwr && !lock->heldW));
      tl_assert(HG_(elemWS)( univ_lsets, thr->locksetA, (UWord)lock ));
      if (lock->heldW)
         tl_assert(HG_(elemWS)( univ_lsets, thr->locksetW, (UWord)lock ));
      else
         tl_assert(!HG_(elemWS)( univ_lsets, thr->locksetW, (UWord)lock ));
   } else {
      if (lock->kind == LK_rdwr && lock->heldBy) {
         
         tl_assert(lock->heldW == False);
      } else {
         
         tl_assert(!lock->heldBy);
         tl_assert(lock->heldW == False);
      }
      
      
      
      
      thr->locksetA
         = HG_(delFromWS)( univ_lsets, thr->locksetA, (UWord)lock );
      thr->locksetW
         = HG_(delFromWS)( univ_lsets, thr->locksetW, (UWord)lock );
      
      tl_assert(thr->hbthr);
      tl_assert(lock->hbso);
      libhb_so_send( thr->hbthr, lock->hbso, was_heldW );
   }
   

  error:
   tl_assert(HG_(is_sane_LockN)(lock));
}




static Thread *current_Thread      = NULL,
              *current_Thread_prev = NULL;

static void evh__start_client_code ( ThreadId tid, ULong nDisp ) {
   if (0) VG_(printf)("start %d %llu\n", (Int)tid, nDisp);
   tl_assert(current_Thread == NULL);
   current_Thread = map_threads_lookup( tid );
   tl_assert(current_Thread != NULL);
   if (current_Thread != current_Thread_prev) {
      libhb_Thr_resumes( current_Thread->hbthr );
      current_Thread_prev = current_Thread;
   }
}
static void evh__stop_client_code ( ThreadId tid, ULong nDisp ) {
   if (0) VG_(printf)(" stop %d %llu\n", (Int)tid, nDisp);
   tl_assert(current_Thread != NULL);
   current_Thread = NULL;
   libhb_maybe_GC();
}
static inline Thread* get_current_Thread_in_C_C ( void ) {
   return current_Thread;
}
static inline Thread* get_current_Thread ( void ) {
   ThreadId coretid;
   Thread*  thr;
   thr = get_current_Thread_in_C_C();
   if (LIKELY(thr))
      return thr;
   
   coretid = VG_(get_running_tid)();
   if (coretid == VG_INVALID_THREADID)
      coretid = 1; 
   thr = map_threads_lookup( coretid );
   return thr;
}

static
void evh__new_mem ( Addr a, SizeT len ) {
   if (SHOW_EVENTS >= 2)
      VG_(printf)("evh__new_mem(%p, %lu)\n", (void*)a, len );
   shadow_mem_make_New( get_current_Thread(), a, len );
   if (len >= SCE_BIGRANGE_T && (HG_(clo_sanity_flags) & SCE_BIGRANGE))
      all__sanity_check("evh__new_mem-post");
}

static
void evh__new_mem_stack ( Addr a, SizeT len ) {
   if (SHOW_EVENTS >= 2)
      VG_(printf)("evh__new_mem_stack(%p, %lu)\n", (void*)a, len );
   shadow_mem_make_New( get_current_Thread(),
                        -VG_STACK_REDZONE_SZB + a, len );
   if (len >= SCE_BIGRANGE_T && (HG_(clo_sanity_flags) & SCE_BIGRANGE))
      all__sanity_check("evh__new_mem_stack-post");
}

static
void evh__new_mem_w_tid ( Addr a, SizeT len, ThreadId tid ) {
   if (SHOW_EVENTS >= 2)
      VG_(printf)("evh__new_mem_w_tid(%p, %lu)\n", (void*)a, len );
   shadow_mem_make_New( get_current_Thread(), a, len );
   if (len >= SCE_BIGRANGE_T && (HG_(clo_sanity_flags) & SCE_BIGRANGE))
      all__sanity_check("evh__new_mem_w_tid-post");
}

static
void evh__new_mem_w_perms ( Addr a, SizeT len, 
                            Bool rr, Bool ww, Bool xx, ULong di_handle ) {
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__new_mem_w_perms(%p, %lu, %d,%d,%d)\n",
                  (void*)a, len, (Int)rr, (Int)ww, (Int)xx );
   if (rr || ww || xx)
      shadow_mem_make_New( get_current_Thread(), a, len );
   if (len >= SCE_BIGRANGE_T && (HG_(clo_sanity_flags) & SCE_BIGRANGE))
      all__sanity_check("evh__new_mem_w_perms-post");
}

static
void evh__set_perms ( Addr a, SizeT len,
                      Bool rr, Bool ww, Bool xx ) {
   
   
   
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__set_perms(%p, %lu, r=%d w=%d x=%d)\n",
                  (void*)a, len, (Int)rr, (Int)ww, (Int)xx );
   if (!(rr || ww))
      shadow_mem_make_NoAccess_AHAE( get_current_Thread(), a, len );
   if (len >= SCE_BIGRANGE_T && (HG_(clo_sanity_flags) & SCE_BIGRANGE))
      all__sanity_check("evh__set_perms-post");
}

static
void evh__die_mem ( Addr a, SizeT len ) {
   
   if (SHOW_EVENTS >= 2)
      VG_(printf)("evh__die_mem(%p, %lu)\n", (void*)a, len );
   shadow_mem_make_NoAccess_NoFX( get_current_Thread(), a, len );
   if (len >= SCE_BIGRANGE_T && (HG_(clo_sanity_flags) & SCE_BIGRANGE))
      all__sanity_check("evh__die_mem-post");
}

static
void evh__die_mem_munmap ( Addr a, SizeT len ) {
   
   
   
   
   
   
   
   
   if (SHOW_EVENTS >= 2)
      VG_(printf)("evh__die_mem_munmap(%p, %lu)\n", (void*)a, len );
   shadow_mem_make_NoAccess_AHAE( get_current_Thread(), a, len );
}

static
void evh__untrack_mem ( Addr a, SizeT len ) {
   
   if (SHOW_EVENTS >= 2)
      VG_(printf)("evh__untrack_mem(%p, %lu)\n", (void*)a, len );
   shadow_mem_make_Untracked( get_current_Thread(), a, len );
   if (len >= SCE_BIGRANGE_T && (HG_(clo_sanity_flags) & SCE_BIGRANGE))
      all__sanity_check("evh__untrack_mem-post");
}

static
void evh__copy_mem ( Addr src, Addr dst, SizeT len ) {
   if (SHOW_EVENTS >= 2)
      VG_(printf)("evh__copy_mem(%p, %p, %lu)\n", (void*)src, (void*)dst, len );
   shadow_mem_scopy_range( get_current_Thread(), src, dst, len );
   if (len >= SCE_BIGRANGE_T && (HG_(clo_sanity_flags) & SCE_BIGRANGE))
      all__sanity_check("evh__copy_mem-post");
}

static
void evh__pre_thread_ll_create ( ThreadId parent, ThreadId child )
{
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__pre_thread_ll_create(p=%d, c=%d)\n",
                  (Int)parent, (Int)child );

   if (parent != VG_INVALID_THREADID) {
      Thread* thr_p;
      Thread* thr_c;
      Thr*    hbthr_p;
      Thr*    hbthr_c;

      tl_assert(HG_(is_sane_ThreadId)(parent));
      tl_assert(HG_(is_sane_ThreadId)(child));
      tl_assert(parent != child);

      thr_p = map_threads_maybe_lookup( parent );
      thr_c = map_threads_maybe_lookup( child );

      tl_assert(thr_p != NULL);
      tl_assert(thr_c == NULL);

      hbthr_p = thr_p->hbthr;
      tl_assert(hbthr_p != NULL);
      tl_assert( libhb_get_Thr_hgthread(hbthr_p) == thr_p );

      hbthr_c = libhb_create ( hbthr_p );

      
      
      thr_c = mk_Thread( hbthr_c );
      tl_assert( libhb_get_Thr_hgthread(hbthr_c) == NULL );
      libhb_set_Thr_hgthread(hbthr_c, thr_c);

      
      map_threads[child] = thr_c;
      tl_assert(thr_c->coretid == VG_INVALID_THREADID);
      thr_c->coretid = child;

      { Word first_ip_delta = 0;
#       if defined(VGP_amd64_linux) || defined(VGP_x86_linux)
        first_ip_delta = -3;
#       elif defined(VGP_arm64_linux) || defined(VGP_arm_linux)
        first_ip_delta = -1;
#       endif
        thr_c->created_at = VG_(record_ExeContext)(parent, first_ip_delta);
      }
   }

   if (HG_(clo_sanity_flags) & SCE_THREADS)
      all__sanity_check("evh__pre_thread_create-post");
}

static
void evh__pre_thread_ll_exit ( ThreadId quit_tid )
{
   Int     nHeld;
   Thread* thr_q;
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__pre_thread_ll_exit(thr=%d)\n",
                  (Int)quit_tid );

   tl_assert(HG_(is_sane_ThreadId)(quit_tid));
   thr_q = map_threads_maybe_lookup( quit_tid );
   tl_assert(thr_q != NULL);

   
   nHeld = HG_(cardinalityWS)( univ_lsets, thr_q->locksetA );
   tl_assert(nHeld >= 0);
   if (nHeld > 0) {
      HChar buf[80];
      VG_(sprintf)(buf, "Exiting thread still holds %d lock%s",
                        nHeld, nHeld > 1 ? "s" : "");
      HG_(record_error_Misc)( thr_q, buf );
   }

   tl_assert(thr_q->hbthr);
   libhb_async_exit(thr_q->hbthr);
   tl_assert(thr_q->coretid == quit_tid);
   thr_q->coretid = VG_INVALID_THREADID;
   map_threads_delete( quit_tid );

   if (HG_(clo_sanity_flags) & SCE_THREADS)
      all__sanity_check("evh__pre_thread_ll_exit-post");
}

static
void evh__atfork_child ( ThreadId tid )
{
   UInt    i;
   Thread* thr;
   
   thr = map_threads_maybe_lookup( 0 );
   tl_assert(!thr);
   
   for (i = 1; i < VG_N_THREADS; i++) {
      if (i == tid)
         continue;
      thr = map_threads_maybe_lookup(i);
      if (!thr)
         continue;
      tl_assert(thr->hbthr);
      libhb_async_exit(thr->hbthr);
      tl_assert(thr->coretid == i);
      thr->coretid = VG_INVALID_THREADID;
      map_threads_delete(i);
   }
}

static
void generate_quitter_stayer_dependence (Thr* hbthr_q, Thr* hbthr_s)
{
   SO*      so;
   so = libhb_so_alloc();
   tl_assert(so);
   libhb_so_send(hbthr_q, so, True);
   libhb_so_recv(hbthr_s, so, True);
   libhb_so_dealloc(so);

   libhb_joinedwith_done(hbthr_q);
}


static
void evh__HG_PTHREAD_JOIN_POST ( ThreadId stay_tid, Thread* quit_thr )
{
   Thread*  thr_s;
   Thread*  thr_q;
   Thr*     hbthr_s;
   Thr*     hbthr_q;

   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__post_thread_join(stayer=%d, quitter=%p)\n",
                  (Int)stay_tid, quit_thr );

   tl_assert(HG_(is_sane_ThreadId)(stay_tid));

   thr_s = map_threads_maybe_lookup( stay_tid );
   thr_q = quit_thr;
   tl_assert(thr_s != NULL);
   tl_assert(thr_q != NULL);
   tl_assert(thr_s != thr_q);

   hbthr_s = thr_s->hbthr;
   hbthr_q = thr_q->hbthr;
   tl_assert(hbthr_s != hbthr_q);
   tl_assert( libhb_get_Thr_hgthread(hbthr_s) == thr_s );
   tl_assert( libhb_get_Thr_hgthread(hbthr_q) == thr_q );

   generate_quitter_stayer_dependence (hbthr_q, hbthr_s);


   tl_assert( map_threads_maybe_reverse_lookup_SLOW(thr_q)
              == VG_INVALID_THREADID);

   if (HG_(clo_sanity_flags) & SCE_THREADS)
      all__sanity_check("evh__post_thread_join-post");
}

static
void evh__pre_mem_read ( CorePart part, ThreadId tid, const HChar* s, 
                         Addr a, SizeT size) {
   if (SHOW_EVENTS >= 2
       || (SHOW_EVENTS >= 1 && size != 1))
      VG_(printf)("evh__pre_mem_read(ctid=%d, \"%s\", %p, %lu)\n", 
                  (Int)tid, s, (void*)a, size );
   shadow_mem_cread_range( map_threads_lookup(tid), a, size);
   if (size >= SCE_BIGRANGE_T && (HG_(clo_sanity_flags) & SCE_BIGRANGE))
      all__sanity_check("evh__pre_mem_read-post");
}

static
void evh__pre_mem_read_asciiz ( CorePart part, ThreadId tid,
                                const HChar* s, Addr a ) {
   Int len;
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__pre_mem_asciiz(ctid=%d, \"%s\", %p)\n", 
                  (Int)tid, s, (void*)a );
   
   
   
   
   if (!VG_(am_is_valid_for_client) (a, 1, VKI_PROT_READ))
      return;
   len = VG_(strlen)( (HChar*) a );
   shadow_mem_cread_range( map_threads_lookup(tid), a, len+1 );
   if (len >= SCE_BIGRANGE_T && (HG_(clo_sanity_flags) & SCE_BIGRANGE))
      all__sanity_check("evh__pre_mem_read_asciiz-post");
}

static
void evh__pre_mem_write ( CorePart part, ThreadId tid, const HChar* s,
                          Addr a, SizeT size ) {
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__pre_mem_write(ctid=%d, \"%s\", %p, %lu)\n", 
                  (Int)tid, s, (void*)a, size );
   shadow_mem_cwrite_range( map_threads_lookup(tid), a, size);
   if (size >= SCE_BIGRANGE_T && (HG_(clo_sanity_flags) & SCE_BIGRANGE))
      all__sanity_check("evh__pre_mem_write-post");
}

static
void evh__new_mem_heap ( Addr a, SizeT len, Bool is_inited ) {
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__new_mem_heap(%p, %lu, inited=%d)\n", 
                  (void*)a, len, (Int)is_inited );
   
   shadow_mem_make_New(get_current_Thread(), a, len);
   if (len >= SCE_BIGRANGE_T && (HG_(clo_sanity_flags) & SCE_BIGRANGE))
      all__sanity_check("evh__pre_mem_read-post");
}

static
void evh__die_mem_heap ( Addr a, SizeT len ) {
   Thread* thr;
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__die_mem_heap(%p, %lu)\n", (void*)a, len );
   thr = get_current_Thread();
   tl_assert(thr);
   if (HG_(clo_free_is_write)) {
      /* Treat frees as if the memory was written immediately prior to
         the free.  This shakes out more races, specifically, cases
         where memory is referenced by one thread, and freed by
         another, and there's no observable synchronisation event to
         guarantee that the reference happens before the free. */
      shadow_mem_cwrite_range(thr, a, len);
   }
   shadow_mem_make_NoAccess_AHAE( thr, a, len );

   if (len >= SCE_BIGRANGE_T && (HG_(clo_sanity_flags) & SCE_BIGRANGE))
      all__sanity_check("evh__pre_mem_read-post");
}


static VG_REGPARM(1)
void evh__mem_help_cread_1(Addr a) {
   Thread*  thr = get_current_Thread_in_C_C();
   Thr*     hbthr = thr->hbthr;
   LIBHB_CREAD_1(hbthr, a);
}

static VG_REGPARM(1)
void evh__mem_help_cread_2(Addr a) {
   Thread*  thr = get_current_Thread_in_C_C();
   Thr*     hbthr = thr->hbthr;
   LIBHB_CREAD_2(hbthr, a);
}

static VG_REGPARM(1)
void evh__mem_help_cread_4(Addr a) {
   Thread*  thr = get_current_Thread_in_C_C();
   Thr*     hbthr = thr->hbthr;
   LIBHB_CREAD_4(hbthr, a);
}

static VG_REGPARM(1)
void evh__mem_help_cread_8(Addr a) {
   Thread*  thr = get_current_Thread_in_C_C();
   Thr*     hbthr = thr->hbthr;
   LIBHB_CREAD_8(hbthr, a);
}

static VG_REGPARM(2)
void evh__mem_help_cread_N(Addr a, SizeT size) {
   Thread*  thr = get_current_Thread_in_C_C();
   Thr*     hbthr = thr->hbthr;
   LIBHB_CREAD_N(hbthr, a, size);
}

static VG_REGPARM(1)
void evh__mem_help_cwrite_1(Addr a) {
   Thread*  thr = get_current_Thread_in_C_C();
   Thr*     hbthr = thr->hbthr;
   LIBHB_CWRITE_1(hbthr, a);
}

static VG_REGPARM(1)
void evh__mem_help_cwrite_2(Addr a) {
   Thread*  thr = get_current_Thread_in_C_C();
   Thr*     hbthr = thr->hbthr;
   LIBHB_CWRITE_2(hbthr, a);
}

static VG_REGPARM(1)
void evh__mem_help_cwrite_4(Addr a) {
   Thread*  thr = get_current_Thread_in_C_C();
   Thr*     hbthr = thr->hbthr;
   LIBHB_CWRITE_4(hbthr, a);
}

static VG_REGPARM(1)
void evh__mem_help_cwrite_8(Addr a) {
   Thread*  thr = get_current_Thread_in_C_C();
   Thr*     hbthr = thr->hbthr;
   LIBHB_CWRITE_8(hbthr, a);
}

static VG_REGPARM(2)
void evh__mem_help_cwrite_N(Addr a, SizeT size) {
   Thread*  thr = get_current_Thread_in_C_C();
   Thr*     hbthr = thr->hbthr;
   LIBHB_CWRITE_N(hbthr, a, size);
}



static
void evh__HG_PTHREAD_MUTEX_INIT_POST( ThreadId tid, 
                                      void* mutex, Word mbRec )
{
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__hg_PTHREAD_MUTEX_INIT_POST(ctid=%d, mbRec=%ld, %p)\n", 
                  (Int)tid, mbRec, (void*)mutex );
   tl_assert(mbRec == 0 || mbRec == 1);
   map_locks_lookup_or_create( mbRec ? LK_mbRec : LK_nonRec,
                               (Addr)mutex, tid );
   if (HG_(clo_sanity_flags) & SCE_LOCKS)
      all__sanity_check("evh__hg_PTHREAD_MUTEX_INIT_POST");
}

static
void evh__HG_PTHREAD_MUTEX_DESTROY_PRE( ThreadId tid, void* mutex,
                                        Bool mutex_is_init )
{
   Thread* thr;
   Lock*   lk;
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__hg_PTHREAD_MUTEX_DESTROY_PRE"
                  "(ctid=%d, %p, isInit=%d)\n", 
                  (Int)tid, (void*)mutex, (Int)mutex_is_init );

   thr = map_threads_maybe_lookup( tid );
   
   tl_assert( HG_(is_sane_Thread)(thr) );

   lk = map_locks_maybe_lookup( (Addr)mutex );

   if (lk == NULL && mutex_is_init) {
      goto out;
   }

   if (lk == NULL || (lk->kind != LK_nonRec && lk->kind != LK_mbRec)) {
      HG_(record_error_Misc)(
         thr, "pthread_mutex_destroy with invalid argument" );
   }

   if (lk) {
      tl_assert( HG_(is_sane_LockN)(lk) );
      tl_assert( lk->guestaddr == (Addr)mutex );
      if (lk->heldBy) {
         
         HG_(record_error_Misc)(
            thr, "pthread_mutex_destroy of a locked mutex" );
         
         remove_Lock_from_locksets_of_all_owning_Threads( lk );
         VG_(deleteBag)( lk->heldBy );
         lk->heldBy = NULL;
         lk->heldW = False;
         lk->acquired_at = NULL;
      }
      tl_assert( !lk->heldBy );
      tl_assert( HG_(is_sane_LockN)(lk) );
      
      if (HG_(clo_track_lockorders))
         laog__handle_one_lock_deletion(lk);
      map_locks_delete( lk->guestaddr );
      del_LockN( lk );
   }

  out:
   if (HG_(clo_sanity_flags) & SCE_LOCKS)
      all__sanity_check("evh__hg_PTHREAD_MUTEX_DESTROY_PRE");
}

static void evh__HG_PTHREAD_MUTEX_LOCK_PRE ( ThreadId tid,
                                             void* mutex, Word isTryLock )
{
   
   
   Thread* thr;
   Lock*   lk;
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__hg_PTHREAD_MUTEX_LOCK_PRE(ctid=%d, mutex=%p)\n", 
                  (Int)tid, (void*)mutex );

   tl_assert(isTryLock == 0 || isTryLock == 1);
   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   lk = map_locks_maybe_lookup( (Addr)mutex );

   if (lk && (lk->kind == LK_rdwr)) {
      HG_(record_error_Misc)( thr, "pthread_mutex_lock with a "
                                   "pthread_rwlock_t* argument " );
   }

   if ( lk 
        && isTryLock == 0
        && (lk->kind == LK_nonRec || lk->kind == LK_rdwr)
        && lk->heldBy
        && lk->heldW
        && VG_(elemBag)( lk->heldBy, (UWord)thr ) > 0 ) {
      const HChar* errstr = "Attempt to re-lock a "
                            "non-recursive lock I already hold";
      const HChar* auxstr = "Lock was previously acquired";
      if (lk->acquired_at) {
         HG_(record_error_Misc_w_aux)( thr, errstr, auxstr, lk->acquired_at );
      } else {
         HG_(record_error_Misc)( thr, errstr );
      }
   }
}

static void evh__HG_PTHREAD_MUTEX_LOCK_POST ( ThreadId tid, void* mutex )
{
   
   Thread* thr;
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__HG_PTHREAD_MUTEX_LOCK_POST(ctid=%d, mutex=%p)\n", 
                  (Int)tid, (void*)mutex );

   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   evhH__post_thread_w_acquires_lock( 
      thr, 
      LK_mbRec, 
      (Addr)mutex
   );
}

static void evh__HG_PTHREAD_MUTEX_UNLOCK_PRE ( ThreadId tid, void* mutex )
{
   
   Thread* thr;
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__HG_PTHREAD_MUTEX_UNLOCK_PRE(ctid=%d, mutex=%p)\n", 
                  (Int)tid, (void*)mutex );

   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   evhH__pre_thread_releases_lock( thr, (Addr)mutex, False );
}

static void evh__HG_PTHREAD_MUTEX_UNLOCK_POST ( ThreadId tid, void* mutex )
{
   
   Thread* thr;
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__hg_PTHREAD_MUTEX_UNLOCK_POST(ctid=%d, mutex=%p)\n", 
                  (Int)tid, (void*)mutex );
   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   
}




static void evh__HG_PTHREAD_SPIN_INIT_OR_UNLOCK_PRE( ThreadId tid, 
                                                     void* slock )
{
   Thread* thr;
   Lock*   lk;

   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__hg_PTHREAD_SPIN_INIT_OR_UNLOCK_PRE"
                  "(ctid=%d, slock=%p)\n", 
                  (Int)tid, (void*)slock );

   thr = map_threads_maybe_lookup( tid );
   ;
   tl_assert( HG_(is_sane_Thread)(thr) );

   lk = map_locks_maybe_lookup( (Addr)slock );
   if (lk && lk->heldBy) {
      evhH__pre_thread_releases_lock( thr, (Addr)slock,
                                           False );
   }
}

static void evh__HG_PTHREAD_SPIN_INIT_OR_UNLOCK_POST( ThreadId tid, 
                                                      void* slock )
{
   Lock* lk;

   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__hg_PTHREAD_SPIN_INIT_OR_UNLOCK_POST"
                  "(ctid=%d, slock=%p)\n", 
                  (Int)tid, (void*)slock );

   lk = map_locks_maybe_lookup( (Addr)slock );
   if (!lk) {
      map_locks_lookup_or_create( LK_nonRec, (Addr)slock, tid );
   }
}

static void evh__HG_PTHREAD_SPIN_LOCK_PRE( ThreadId tid, 
                                           void* slock, Word isTryLock )
{
   evh__HG_PTHREAD_MUTEX_LOCK_PRE( tid, slock, isTryLock );
}

static void evh__HG_PTHREAD_SPIN_LOCK_POST( ThreadId tid, 
                                            void* slock )
{
   evh__HG_PTHREAD_MUTEX_LOCK_POST( tid, slock );
}

static void evh__HG_PTHREAD_SPIN_DESTROY_PRE( ThreadId tid, 
                                              void* slock )
{
   evh__HG_PTHREAD_MUTEX_DESTROY_PRE( tid, slock, 0 );
}




typedef
   struct { 
      SO*   so;       
      void* mx_ga;    
      UWord nWaiters; 
   }
   CVInfo;


static WordFM* map_cond_to_CVInfo = NULL;

static void map_cond_to_CVInfo_INIT ( void ) {
   if (UNLIKELY(map_cond_to_CVInfo == NULL)) {
      map_cond_to_CVInfo = VG_(newFM)( HG_(zalloc),
                                       "hg.mctCI.1", HG_(free), NULL );
   }
}

static CVInfo* map_cond_to_CVInfo_lookup_or_alloc ( void* cond ) {
   UWord key, val;
   map_cond_to_CVInfo_INIT();
   if (VG_(lookupFM)( map_cond_to_CVInfo, &key, &val, (UWord)cond )) {
      tl_assert(key == (UWord)cond);
      return (CVInfo*)val;
   } else {
      SO*     so  = libhb_so_alloc();
      CVInfo* cvi = HG_(zalloc)("hg.mctCloa.1", sizeof(CVInfo));
      cvi->so     = so;
      cvi->mx_ga  = 0;
      VG_(addToFM)( map_cond_to_CVInfo, (UWord)cond, (UWord)cvi );
      return cvi;
   }
}

static CVInfo* map_cond_to_CVInfo_lookup_NO_alloc ( void* cond ) {
   UWord key, val;
   map_cond_to_CVInfo_INIT();
   if (VG_(lookupFM)( map_cond_to_CVInfo, &key, &val, (UWord)cond )) {
      tl_assert(key == (UWord)cond);
      return (CVInfo*)val;
   } else {
      return NULL;
   }
}

static void map_cond_to_CVInfo_delete ( ThreadId tid,
                                        void* cond, Bool cond_is_init ) {
   Thread*   thr;
   UWord keyW, valW;

   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   map_cond_to_CVInfo_INIT();
   if (VG_(lookupFM)( map_cond_to_CVInfo, &keyW, &valW, (UWord)cond )) {
      CVInfo* cvi = (CVInfo*)valW;
      tl_assert(keyW == (UWord)cond);
      tl_assert(cvi);
      tl_assert(cvi->so);
      if (cvi->nWaiters > 0) {
         HG_(record_error_Misc)(
            thr, "pthread_cond_destroy:"
                 " destruction of condition variable being waited upon");
         return;
      }
      if (!VG_(delFromFM)( map_cond_to_CVInfo, &keyW, &valW, (UWord)cond ))
         tl_assert(0); 
      libhb_so_dealloc(cvi->so);
      cvi->mx_ga = 0;
      HG_(free)(cvi);
   } else {
      if (!cond_is_init) {
         HG_(record_error_Misc)(
            thr, "pthread_cond_destroy: destruction of unknown cond var");
      }
   }
}

static void evh__HG_PTHREAD_COND_SIGNAL_PRE ( ThreadId tid, void* cond )
{
   Thread*   thr;
   CVInfo*   cvi;
   

   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__HG_PTHREAD_COND_SIGNAL_PRE(ctid=%d, cond=%p)\n", 
                  (Int)tid, (void*)cond );

   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   cvi = map_cond_to_CVInfo_lookup_or_alloc( cond );
   tl_assert(cvi);
   tl_assert(cvi->so);

   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   if (1) {
      Lock* lk = NULL;
      if (cvi->mx_ga != 0) {
         lk = map_locks_maybe_lookup( (Addr)cvi->mx_ga );
      }
      
      if (lk) {
         if (lk->kind == LK_rdwr) {
            HG_(record_error_Misc)(thr,
               "pthread_cond_{signal,broadcast}: associated lock is a rwlock");
         }
         if (lk->heldBy == NULL) {
            HG_(record_error_Misc)(thr,
               "pthread_cond_{signal,broadcast}: dubious: "
               "associated lock is not held by any thread");
         }
         if (lk->heldBy != NULL && 0 == VG_(elemBag)(lk->heldBy, (UWord)thr)) {
            HG_(record_error_Misc)(thr,
               "pthread_cond_{signal,broadcast}: "
               "associated lock is not held by calling thread");
         }
      } else {
         
         
         
         
         
         
         
         
      }
   }

   libhb_so_send( thr->hbthr, cvi->so, True );
}

static Bool evh__HG_PTHREAD_COND_WAIT_PRE ( ThreadId tid,
                                            void* cond, void* mutex )
{
   Thread* thr;
   Lock*   lk;
   Bool    lk_valid = True;
   CVInfo* cvi;

   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__hg_PTHREAD_COND_WAIT_PRE"
                  "(ctid=%d, cond=%p, mutex=%p)\n", 
                  (Int)tid, (void*)cond, (void*)mutex );

   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   lk = map_locks_maybe_lookup( (Addr)mutex );

   if (lk == NULL) {
      lk_valid = False;
      HG_(record_error_Misc)( 
         thr, 
         "pthread_cond_{timed}wait called with invalid mutex" );
   } else {
      tl_assert( HG_(is_sane_LockN)(lk) );
      if (lk->kind == LK_rdwr) {
         lk_valid = False;
         HG_(record_error_Misc)(
            thr, "pthread_cond_{timed}wait called with mutex "
                 "of type pthread_rwlock_t*" );
      } else
         if (lk->heldBy == NULL) {
         lk_valid = False;
         HG_(record_error_Misc)( 
            thr, "pthread_cond_{timed}wait called with un-held mutex");
      } else
      if (lk->heldBy != NULL
          && VG_(elemBag)( lk->heldBy, (UWord)thr ) == 0) {
         lk_valid = False;
         HG_(record_error_Misc)(
            thr, "pthread_cond_{timed}wait called with mutex "
                 "held by a different thread" );
      }
   }

   
   cvi = map_cond_to_CVInfo_lookup_or_alloc(cond);
   tl_assert(cvi);
   tl_assert(cvi->so);
   if (cvi->nWaiters == 0) {
      
      cvi->mx_ga = mutex;
   }
   else 
   if (cvi->mx_ga != mutex) {
      HG_(record_error_Misc)(
         thr, "pthread_cond_{timed}wait: cond is associated "
              "with a different mutex");
   }
   cvi->nWaiters++;

   return lk_valid;
}

static void evh__HG_PTHREAD_COND_WAIT_POST ( ThreadId tid,
                                             void* cond, void* mutex,
                                             Bool timeout)
{
   Thread* thr;
   CVInfo* cvi;

   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__HG_PTHREAD_COND_WAIT_POST"
                  "(ctid=%d, cond=%p, mutex=%p)\n, timeout=%d",
                  (Int)tid, (void*)cond, (void*)mutex, (Int)timeout );

   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   

   cvi = map_cond_to_CVInfo_lookup_NO_alloc( cond );
   if (!cvi) {
      HG_(record_error_Misc)(thr, "condition variable has been destroyed while"
                             " being waited upon");
      return;
   }

   tl_assert(cvi);
   tl_assert(cvi->so);
   tl_assert(cvi->nWaiters > 0);

   if (!timeout && !libhb_so_everSent(cvi->so)) {
      HG_(record_error_Misc)( thr, "Bug in libpthread: pthread_cond_wait "
                                   "succeeded"
                                   " without prior pthread_cond_post");
   }

   
   libhb_so_recv( thr->hbthr, cvi->so, True );

   cvi->nWaiters--;
}

static void evh__HG_PTHREAD_COND_INIT_POST ( ThreadId tid,
                                             void* cond, void* cond_attr )
{
   CVInfo* cvi;

   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__HG_PTHREAD_COND_INIT_POST"
                  "(ctid=%d, cond=%p, cond_attr=%p)\n", 
                  (Int)tid, (void*)cond, (void*) cond_attr );

   cvi = map_cond_to_CVInfo_lookup_or_alloc( cond );
   tl_assert (cvi);
   tl_assert (cvi->so);
}


static void evh__HG_PTHREAD_COND_DESTROY_PRE ( ThreadId tid,
                                               void* cond, Bool cond_is_init )
{
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__HG_PTHREAD_COND_DESTROY_PRE"
                  "(ctid=%d, cond=%p, cond_is_init=%d)\n", 
                  (Int)tid, (void*)cond, (Int)cond_is_init );

   map_cond_to_CVInfo_delete( tid, cond, cond_is_init );
}



static
void evh__HG_PTHREAD_RWLOCK_INIT_POST( ThreadId tid, void* rwl )
{
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__hg_PTHREAD_RWLOCK_INIT_POST(ctid=%d, %p)\n", 
                  (Int)tid, (void*)rwl );
   map_locks_lookup_or_create( LK_rdwr, (Addr)rwl, tid );
   if (HG_(clo_sanity_flags) & SCE_LOCKS)
      all__sanity_check("evh__hg_PTHREAD_RWLOCK_INIT_POST");
}

static
void evh__HG_PTHREAD_RWLOCK_DESTROY_PRE( ThreadId tid, void* rwl )
{
   Thread* thr;
   Lock*   lk;
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__hg_PTHREAD_RWLOCK_DESTROY_PRE(ctid=%d, %p)\n", 
                  (Int)tid, (void*)rwl );

   thr = map_threads_maybe_lookup( tid );
   
   tl_assert( HG_(is_sane_Thread)(thr) );

   lk = map_locks_maybe_lookup( (Addr)rwl );

   if (lk == NULL || lk->kind != LK_rdwr) {
      HG_(record_error_Misc)(
         thr, "pthread_rwlock_destroy with invalid argument" );
   }

   if (lk) {
      tl_assert( HG_(is_sane_LockN)(lk) );
      tl_assert( lk->guestaddr == (Addr)rwl );
      if (lk->heldBy) {
         
         HG_(record_error_Misc)(
            thr, "pthread_rwlock_destroy of a locked mutex" );
         
         remove_Lock_from_locksets_of_all_owning_Threads( lk );
         VG_(deleteBag)( lk->heldBy );
         lk->heldBy = NULL;
         lk->heldW = False;
         lk->acquired_at = NULL;
      }
      tl_assert( !lk->heldBy );
      tl_assert( HG_(is_sane_LockN)(lk) );
      
      if (HG_(clo_track_lockorders))
         laog__handle_one_lock_deletion(lk);
      map_locks_delete( lk->guestaddr );
      del_LockN( lk );
   }

   if (HG_(clo_sanity_flags) & SCE_LOCKS)
      all__sanity_check("evh__hg_PTHREAD_RWLOCK_DESTROY_PRE");
}

static 
void evh__HG_PTHREAD_RWLOCK_LOCK_PRE ( ThreadId tid,
                                       void* rwl,
                                       Word isW, Word isTryLock )
{
   
   
   Thread* thr;
   Lock*   lk;
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__hg_PTHREAD_RWLOCK_LOCK_PRE(ctid=%d, isW=%d, %p)\n", 
                  (Int)tid, (Int)isW, (void*)rwl );

   tl_assert(isW == 0 || isW == 1); 
   tl_assert(isTryLock == 0 || isTryLock == 1); 
   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   lk = map_locks_maybe_lookup( (Addr)rwl );
   if ( lk 
        && (lk->kind == LK_nonRec || lk->kind == LK_mbRec) ) {
      
      HG_(record_error_Misc)( 
         thr, "pthread_rwlock_{rd,rw}lock with a "
              "pthread_mutex_t* argument " );
   }
}

static 
void evh__HG_PTHREAD_RWLOCK_LOCK_POST ( ThreadId tid, void* rwl, Word isW )
{
   
   Thread* thr;
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__hg_PTHREAD_RWLOCK_LOCK_POST(ctid=%d, isW=%d, %p)\n", 
                  (Int)tid, (Int)isW, (void*)rwl );

   tl_assert(isW == 0 || isW == 1); 
   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   (isW ? evhH__post_thread_w_acquires_lock 
        : evhH__post_thread_r_acquires_lock)( 
      thr, 
      LK_rdwr, 
      (Addr)rwl
   );
}

static void evh__HG_PTHREAD_RWLOCK_UNLOCK_PRE ( ThreadId tid, void* rwl )
{
   
   Thread* thr;
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__HG_PTHREAD_RWLOCK_UNLOCK_PRE(ctid=%d, rwl=%p)\n", 
                  (Int)tid, (void*)rwl );

   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   evhH__pre_thread_releases_lock( thr, (Addr)rwl, True );
}

static void evh__HG_PTHREAD_RWLOCK_UNLOCK_POST ( ThreadId tid, void* rwl )
{
   
   Thread* thr;
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__hg_PTHREAD_RWLOCK_UNLOCK_POST(ctid=%d, rwl=%p)\n", 
                  (Int)tid, (void*)rwl );
   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   
}





static WordFM* map_sem_to_SO_stack = NULL;

static void map_sem_to_SO_stack_INIT ( void ) {
   if (map_sem_to_SO_stack == NULL) {
      map_sem_to_SO_stack = VG_(newFM)( HG_(zalloc), "hg.mstSs.1",
                                        HG_(free), NULL );
   }
}

static void push_SO_for_sem ( void* sem, SO* so ) {
   UWord   keyW;
   XArray* xa;
   tl_assert(so);
   map_sem_to_SO_stack_INIT();
   if (VG_(lookupFM)( map_sem_to_SO_stack, 
                      &keyW, (UWord*)&xa, (UWord)sem )) {
      tl_assert(keyW == (UWord)sem);
      tl_assert(xa);
      VG_(addToXA)( xa, &so );
   } else {
     xa = VG_(newXA)( HG_(zalloc), "hg.pSfs.1", HG_(free), sizeof(SO*) );
      VG_(addToXA)( xa, &so );
      VG_(addToFM)( map_sem_to_SO_stack, (UWord)sem, (UWord)xa );
   }
}

static SO* mb_pop_SO_for_sem ( void* sem ) {
   UWord    keyW;
   XArray*  xa;
   SO* so;
   map_sem_to_SO_stack_INIT();
   if (VG_(lookupFM)( map_sem_to_SO_stack, 
                      &keyW, (UWord*)&xa, (UWord)sem )) {
      
      Word sz; 
      tl_assert(keyW == (UWord)sem);
      sz = VG_(sizeXA)( xa );
      tl_assert(sz >= 0);
      if (sz == 0)
         return NULL; 
      so = *(SO**)VG_(indexXA)( xa, sz-1 );
      tl_assert(so);
      VG_(dropTailXA)( xa, 1 );
      return so;
   } else {
      
      return NULL;
   }
}

static void evh__HG_POSIX_SEM_DESTROY_PRE ( ThreadId tid, void* sem )
{
   UWord keyW, valW;
   SO*   so;

   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__HG_POSIX_SEM_DESTROY_PRE(ctid=%d, sem=%p)\n", 
                  (Int)tid, (void*)sem );

   map_sem_to_SO_stack_INIT();

   while (1) {
      so = mb_pop_SO_for_sem( sem );
      if (!so) break;
      libhb_so_dealloc(so);
   }

   if (VG_(delFromFM)( map_sem_to_SO_stack, &keyW, &valW, (UWord)sem )) {
      XArray* xa = (XArray*)valW;
      tl_assert(keyW == (UWord)sem);
      tl_assert(xa);
      tl_assert(VG_(sizeXA)(xa) == 0); 
      VG_(deleteXA)(xa);
   }
}

static 
void evh__HG_POSIX_SEM_INIT_POST ( ThreadId tid, void* sem, UWord value )
{
   SO*     so;
   Thread* thr;

   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__HG_POSIX_SEM_INIT_POST(ctid=%d, sem=%p, value=%lu)\n", 
                  (Int)tid, (void*)sem, value );

   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   while (1) {
      so = mb_pop_SO_for_sem( sem );
      if (!so) break;
      libhb_so_dealloc(so);
   }

   if (value > 10000) {
      HG_(record_error_Misc)(
         thr, "sem_init: initial value exceeds 10000; using 10000" );
      value = 10000;
   }

   for (; value > 0; value--) {
      Thr* hbthr = thr->hbthr;
      tl_assert(hbthr);

      so = libhb_so_alloc();
      libhb_so_send( hbthr, so, True );
      push_SO_for_sem( sem, so );
   }
}

static void evh__HG_POSIX_SEM_POST_PRE ( ThreadId tid, void* sem )
{

   Thread* thr;
   SO*     so;
   Thr*    hbthr;

   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__HG_POSIX_SEM_POST_PRE(ctid=%d, sem=%p)\n", 
                  (Int)tid, (void*)sem );

   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   

   hbthr = thr->hbthr;
   tl_assert(hbthr);

   so = libhb_so_alloc();
   libhb_so_send( hbthr, so, True );
   push_SO_for_sem( sem, so );
}

static void evh__HG_POSIX_SEM_WAIT_POST ( ThreadId tid, void* sem )
{

   Thread* thr;
   SO*     so;
   Thr*    hbthr;

   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__HG_POSIX_SEM_WAIT_POST(ctid=%d, sem=%p)\n", 
                  (Int)tid, (void*)sem );

   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   

   so = mb_pop_SO_for_sem( sem );

   if (so) {
      hbthr = thr->hbthr;
      tl_assert(hbthr);

      libhb_so_recv( hbthr, so, True );
      libhb_so_dealloc(so);
   } else {
      HG_(record_error_Misc)(
         thr, "Bug in libpthread: sem_wait succeeded on"
              " semaphore without prior sem_post");
   }
}



typedef
   struct {
      Bool    initted; 
      Bool    resizable; 
      UWord   size;    
      XArray* waiting; 
   }
   Bar;

static Bar* new_Bar ( void ) {
   Bar* bar = HG_(zalloc)( "hg.nB.1 (new_Bar)", sizeof(Bar) );
   
   tl_assert(bar->initted == False);
   return bar;
}

static void delete_Bar ( Bar* bar ) {
   tl_assert(bar);
   if (bar->waiting)
      VG_(deleteXA)(bar->waiting);
   HG_(free)(bar);
}


static WordFM* map_barrier_to_Bar = NULL;

static void map_barrier_to_Bar_INIT ( void ) {
   if (UNLIKELY(map_barrier_to_Bar == NULL)) {
      map_barrier_to_Bar = VG_(newFM)( HG_(zalloc),
                                       "hg.mbtBI.1", HG_(free), NULL );
   }
}

static Bar* map_barrier_to_Bar_lookup_or_alloc ( void* barrier ) {
   UWord key, val;
   map_barrier_to_Bar_INIT();
   if (VG_(lookupFM)( map_barrier_to_Bar, &key, &val, (UWord)barrier )) {
      tl_assert(key == (UWord)barrier);
      return (Bar*)val;
   } else {
      Bar* bar = new_Bar();
      VG_(addToFM)( map_barrier_to_Bar, (UWord)barrier, (UWord)bar );
      return bar;
   }
}

static void map_barrier_to_Bar_delete ( void* barrier ) {
   UWord keyW, valW;
   map_barrier_to_Bar_INIT();
   if (VG_(delFromFM)( map_barrier_to_Bar, &keyW, &valW, (UWord)barrier )) {
      Bar* bar = (Bar*)valW;
      tl_assert(keyW == (UWord)barrier);
      delete_Bar(bar);
   }
}


static void evh__HG_PTHREAD_BARRIER_INIT_PRE ( ThreadId tid,
                                               void* barrier,
                                               UWord count,
                                               UWord resizable )
{
   Thread* thr;
   Bar*    bar;

   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__HG_PTHREAD_BARRIER_INIT_PRE"
                  "(tid=%d, barrier=%p, count=%lu, resizable=%lu)\n", 
                  (Int)tid, (void*)barrier, count, resizable );

   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   if (count == 0) {
      HG_(record_error_Misc)(
         thr, "pthread_barrier_init: 'count' argument is zero"
      );
   }

   if (resizable != 0 && resizable != 1) {
      HG_(record_error_Misc)(
         thr, "pthread_barrier_init: invalid 'resizable' argument"
      );
   }

   bar = map_barrier_to_Bar_lookup_or_alloc(barrier);
   tl_assert(bar);

   if (bar->initted) {
      HG_(record_error_Misc)(
         thr, "pthread_barrier_init: barrier is already initialised"
      );
   }

   if (bar->waiting && VG_(sizeXA)(bar->waiting) > 0) {
      tl_assert(bar->initted);
      HG_(record_error_Misc)(
         thr, "pthread_barrier_init: threads are waiting at barrier"
      );
      VG_(dropTailXA)(bar->waiting, VG_(sizeXA)(bar->waiting));
   }
   if (!bar->waiting) {
      bar->waiting = VG_(newXA)( HG_(zalloc), "hg.eHPBIP.1", HG_(free),
                                 sizeof(Thread*) );
   }

   tl_assert(VG_(sizeXA)(bar->waiting) == 0);
   bar->initted   = True;
   bar->resizable = resizable == 1 ? True : False;
   bar->size      = count;
}


static void evh__HG_PTHREAD_BARRIER_DESTROY_PRE ( ThreadId tid,
                                                  void* barrier )
{
   Thread* thr;
   Bar*    bar;

   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__HG_PTHREAD_BARRIER_DESTROY_PRE"
                  "(tid=%d, barrier=%p)\n", 
                  (Int)tid, (void*)barrier );

   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   bar = map_barrier_to_Bar_lookup_or_alloc(barrier);
   tl_assert(bar);

   if (!bar->initted) {
      HG_(record_error_Misc)(
         thr, "pthread_barrier_destroy: barrier was never initialised"
      );
   }

   if (bar->initted && bar->waiting && VG_(sizeXA)(bar->waiting) > 0) {
      HG_(record_error_Misc)(
         thr, "pthread_barrier_destroy: threads are waiting at barrier"
      );
   }

   map_barrier_to_Bar_delete( barrier );
}


static void do_barrier_cross_sync_and_empty ( Bar* bar )
{
   
   UWord i;
   SO*   so = libhb_so_alloc();

   tl_assert(bar->waiting);
   tl_assert(VG_(sizeXA)(bar->waiting) == bar->size);

   
   for (i = 0; i < bar->size; i++) {
      Thread* t = *(Thread**)VG_(indexXA)(bar->waiting, i);
      Thr* hbthr = t->hbthr;
      libhb_so_send( hbthr, so, False );
   }
   
   for (i = 0; i < bar->size; i++) {
      Thread* t = *(Thread**)VG_(indexXA)(bar->waiting, i);
      Thr* hbthr = t->hbthr;
      libhb_so_recv( hbthr, so, True );
   }

   
   VG_(dropTailXA)(bar->waiting, VG_(sizeXA)(bar->waiting));

   libhb_so_dealloc(so);
}


static void evh__HG_PTHREAD_BARRIER_WAIT_PRE ( ThreadId tid,
                                               void* barrier )
{
   Thread* thr;
   Bar*    bar;
   UWord   present;

   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__HG_PTHREAD_BARRIER_WAIT_PRE"
                  "(tid=%d, barrier=%p)\n", 
                  (Int)tid, (void*)barrier );

   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   bar = map_barrier_to_Bar_lookup_or_alloc(barrier);
   tl_assert(bar);

   if (!bar->initted) {
      HG_(record_error_Misc)(
         thr, "pthread_barrier_wait: barrier is uninitialised"
      );
      return; 
   }

   
   tl_assert(bar->size > 0);
   tl_assert(bar->waiting);

   VG_(addToXA)( bar->waiting, &thr );

   
   present = VG_(sizeXA)(bar->waiting);
   tl_assert(present > 0 && present <= bar->size);

   if (present < bar->size)
      return;

   do_barrier_cross_sync_and_empty(bar);
}


static void evh__HG_PTHREAD_BARRIER_RESIZE_PRE ( ThreadId tid,
                                                 void* barrier,
                                                 UWord newcount )
{
   Thread* thr;
   Bar*    bar;
   UWord   present;

   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__HG_PTHREAD_BARRIER_RESIZE_PRE"
                  "(tid=%d, barrier=%p, newcount=%lu)\n", 
                  (Int)tid, (void*)barrier, newcount );

   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   bar = map_barrier_to_Bar_lookup_or_alloc(barrier);
   tl_assert(bar);

   if (!bar->initted) {
      HG_(record_error_Misc)(
         thr, "pthread_barrier_resize: barrier is uninitialised"
      );
      return; 
   }

   if (!bar->resizable) {
      HG_(record_error_Misc)(
         thr, "pthread_barrier_resize: barrier is may not be resized"
      );
      return; 
   }

   if (newcount == 0) {
      HG_(record_error_Misc)(
         thr, "pthread_barrier_resize: 'newcount' argument is zero"
      );
      return; 
   }

   
   tl_assert(bar->size > 0);
   tl_assert(bar->waiting);
   
   tl_assert(newcount > 0);

   if (newcount >= bar->size) {
      bar->size = newcount;
   } else {
      present = VG_(sizeXA)(bar->waiting);
      tl_assert(present >= 0 && present <= bar->size);
      if (newcount <= present) {
         bar->size = present; 
         do_barrier_cross_sync_and_empty(bar);
      }
      bar->size = newcount;
   }
}






static WordFM* map_usertag_to_SO = NULL;

static void map_usertag_to_SO_INIT ( void ) {
   if (UNLIKELY(map_usertag_to_SO == NULL)) {
      map_usertag_to_SO = VG_(newFM)( HG_(zalloc),
                                      "hg.mutS.1", HG_(free), NULL );
   }
}

static SO* map_usertag_to_SO_lookup_or_alloc ( UWord usertag ) {
   UWord key, val;
   map_usertag_to_SO_INIT();
   if (VG_(lookupFM)( map_usertag_to_SO, &key, &val, usertag )) {
      tl_assert(key == (UWord)usertag);
      return (SO*)val;
   } else {
      SO* so = libhb_so_alloc();
      VG_(addToFM)( map_usertag_to_SO, usertag, (UWord)so );
      return so;
   }
}

static void map_usertag_to_SO_delete ( UWord usertag ) {
   UWord keyW, valW;
   map_usertag_to_SO_INIT();
   if (VG_(delFromFM)( map_usertag_to_SO, &keyW, &valW, usertag )) {
      SO* so = (SO*)valW;
      tl_assert(keyW == usertag);
      tl_assert(so);
      libhb_so_dealloc(so);
   }
}


static
void evh__HG_USERSO_SEND_PRE ( ThreadId tid, UWord usertag )
{
   Thread* thr;
   SO*     so;

   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__HG_USERSO_SEND_PRE(ctid=%d, usertag=%#lx)\n", 
                  (Int)tid, usertag );

   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   so = map_usertag_to_SO_lookup_or_alloc( usertag );
   tl_assert(so);

   libhb_so_send( thr->hbthr, so, False );
}

static
void evh__HG_USERSO_RECV_POST ( ThreadId tid, UWord usertag )
{
   Thread* thr;
   SO*     so;

   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__HG_USERSO_RECV_POST(ctid=%d, usertag=%#lx)\n", 
                  (Int)tid, usertag );

   thr = map_threads_maybe_lookup( tid );
   tl_assert(thr); 

   so = map_usertag_to_SO_lookup_or_alloc( usertag );
   tl_assert(so);

   libhb_so_recv( thr->hbthr, so, True );
}

static
void evh__HG_USERSO_FORGET_ALL ( ThreadId tid, UWord usertag )
{
   if (SHOW_EVENTS >= 1)
      VG_(printf)("evh__HG_USERSO_FORGET_ALL(ctid=%d, usertag=%#lx)\n", 
                  (Int)tid, usertag );

   map_usertag_to_SO_delete( usertag );
}




typedef
   struct {
      WordSetID inns; 
      WordSetID outs; 
   }
   LAOGLinks;

static WordFM* laog = NULL; 

typedef
   struct {
      Addr        src_ga; 
      Addr        dst_ga; 
      ExeContext* src_ec; 
      ExeContext* dst_ec; 
   }
   LAOGLinkExposition;

static Word cmp_LAOGLinkExposition ( UWord llx1W, UWord llx2W ) {
   
   LAOGLinkExposition* llx1 = (LAOGLinkExposition*)llx1W;
   LAOGLinkExposition* llx2 = (LAOGLinkExposition*)llx2W;
   if (llx1->src_ga < llx2->src_ga) return -1;
   if (llx1->src_ga > llx2->src_ga) return  1;
   if (llx1->dst_ga < llx2->dst_ga) return -1;
   if (llx1->dst_ga > llx2->dst_ga) return  1;
   return 0;
}

static WordFM* laog_exposition = NULL; 


__attribute__((noinline))
static void laog__init ( void )
{
   tl_assert(!laog);
   tl_assert(!laog_exposition);
   tl_assert(HG_(clo_track_lockorders));

   laog = VG_(newFM)( HG_(zalloc), "hg.laog__init.1", 
                      HG_(free), NULL );

   laog_exposition = VG_(newFM)( HG_(zalloc), "hg.laog__init.2", HG_(free), 
                                 cmp_LAOGLinkExposition );
}

static void laog__show ( const HChar* who ) {
   UWord i, ws_size;
   UWord* ws_words;
   Lock* me;
   LAOGLinks* links;
   VG_(printf)("laog (requested by %s) {\n", who);
   VG_(initIterFM)( laog );
   me = NULL;
   links = NULL;
   while (VG_(nextIterFM)( laog, (UWord*)&me,
                                 (UWord*)&links )) {
      tl_assert(me);
      tl_assert(links);
      VG_(printf)("   node %p:\n", me);
      HG_(getPayloadWS)( &ws_words, &ws_size, univ_laog, links->inns );
      for (i = 0; i < ws_size; i++)
         VG_(printf)("      inn %#lx\n", ws_words[i] );
      HG_(getPayloadWS)( &ws_words, &ws_size, univ_laog, links->outs );
      for (i = 0; i < ws_size; i++)
         VG_(printf)("      out %#lx\n", ws_words[i] );
      me = NULL;
      links = NULL;
   }
   VG_(doneIterFM)( laog );
   VG_(printf)("}\n");
}

static void univ_laog_do_GC ( void ) {
   Word i;
   LAOGLinks* links;
   Word seen = 0;
   Int prev_next_gc_univ_laog = next_gc_univ_laog;
   const UWord univ_laog_cardinality = HG_(cardinalityWSU)( univ_laog);

   Bool *univ_laog_seen = HG_(zalloc) ( "hg.gc_univ_laog.1",
                                        (Int) univ_laog_cardinality 
                                        * sizeof(Bool) );
   

   VG_(initIterFM)( laog );
   links = NULL;
   while (VG_(nextIterFM)( laog, NULL, (UWord*)&links )) {
      tl_assert(links);
      tl_assert(links->inns >= 0 && links->inns < univ_laog_cardinality);
      univ_laog_seen[links->inns] = True;
      tl_assert(links->outs >= 0 && links->outs < univ_laog_cardinality);
      univ_laog_seen[links->outs] = True;
      links = NULL;
   }
   VG_(doneIterFM)( laog );

   for (i = 0; i < (Int)univ_laog_cardinality; i++) {
      if (univ_laog_seen[i])
         seen++;
      else
         HG_(dieWS) ( univ_laog, (WordSet)i );
   }

   HG_(free) (univ_laog_seen);

   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   next_gc_univ_laog = prev_next_gc_univ_laog + 1;

   if (VG_(clo_stats))
      VG_(message)
         (Vg_DebugMsg,
          "univ_laog_do_GC cardinality entered %d exit %d next gc at %d\n",
          (Int)univ_laog_cardinality, (Int)seen, next_gc_univ_laog);
}


__attribute__((noinline))
static void laog__add_edge ( Lock* src, Lock* dst ) {
   UWord      keyW;
   LAOGLinks* links;
   Bool       presentF, presentR;
   if (0) VG_(printf)("laog__add_edge %p %p\n", src, dst);

   presentF = presentR = False;

   
   keyW  = 0;
   links = NULL;
   if (VG_(lookupFM)( laog, &keyW, (UWord*)&links, (UWord)src )) {
      WordSetID outs_new;
      tl_assert(links);
      tl_assert(keyW == (UWord)src);
      outs_new = HG_(addToWS)( univ_laog, links->outs, (UWord)dst );
      presentF = outs_new == links->outs;
      links->outs = outs_new;
   } else {
      links = HG_(zalloc)("hg.lae.1", sizeof(LAOGLinks));
      links->inns = HG_(emptyWS)( univ_laog );
      links->outs = HG_(singletonWS)( univ_laog, (UWord)dst );
      VG_(addToFM)( laog, (UWord)src, (UWord)links );
   }
   
   keyW  = 0;
   links = NULL;
   if (VG_(lookupFM)( laog, &keyW, (UWord*)&links, (UWord)dst )) {
      WordSetID inns_new;
      tl_assert(links);
      tl_assert(keyW == (UWord)dst);
      inns_new = HG_(addToWS)( univ_laog, links->inns, (UWord)src );
      presentR = inns_new == links->inns;
      links->inns = inns_new;
   } else {
      links = HG_(zalloc)("hg.lae.2", sizeof(LAOGLinks));
      links->inns = HG_(singletonWS)( univ_laog, (UWord)src );
      links->outs = HG_(emptyWS)( univ_laog );
      VG_(addToFM)( laog, (UWord)dst, (UWord)links );
   }

   tl_assert( (presentF && presentR) || (!presentF && !presentR) );

   if (!presentF && src->acquired_at && dst->acquired_at) {
      LAOGLinkExposition expo;
      if (0) VG_(printf)("acquire edge %#lx %#lx\n",
                         src->guestaddr, dst->guestaddr);
      expo.src_ga = src->guestaddr;
      expo.dst_ga = dst->guestaddr;
      expo.src_ec = NULL;
      expo.dst_ec = NULL;
      tl_assert(laog_exposition);
      if (VG_(lookupFM)( laog_exposition, NULL, NULL, (UWord)&expo )) {
         
      } else {
         LAOGLinkExposition* expo2 = HG_(zalloc)("hg.lae.3", 
                                               sizeof(LAOGLinkExposition));
         expo2->src_ga = src->guestaddr;
         expo2->dst_ga = dst->guestaddr;
         expo2->src_ec = src->acquired_at;
         expo2->dst_ec = dst->acquired_at;
         VG_(addToFM)( laog_exposition, (UWord)expo2, (UWord)NULL );
      }
   }

   if (HG_(cardinalityWSU) (univ_laog) >= next_gc_univ_laog)
      univ_laog_do_GC();
}

__attribute__((noinline))
static void laog__del_edge ( Lock* src, Lock* dst ) {
   UWord      keyW;
   LAOGLinks* links;
   if (0) VG_(printf)("laog__del_edge enter %p %p\n", src, dst);
   
   keyW  = 0;
   links = NULL;
   if (VG_(lookupFM)( laog, &keyW, (UWord*)&links, (UWord)src )) {
      tl_assert(links);
      tl_assert(keyW == (UWord)src);
      links->outs = HG_(delFromWS)( univ_laog, links->outs, (UWord)dst );
   }
   
   keyW  = 0;
   links = NULL;
   if (VG_(lookupFM)( laog, &keyW, (UWord*)&links, (UWord)dst )) {
      tl_assert(links);
      tl_assert(keyW == (UWord)dst);
      links->inns = HG_(delFromWS)( univ_laog, links->inns, (UWord)src );
   }

   
   {
      LAOGLinkExposition *fm_expo;
      
      LAOGLinkExposition expo;
      expo.src_ga = src->guestaddr;
      expo.dst_ga = dst->guestaddr;
      expo.src_ec = NULL;
      expo.dst_ec = NULL;

      if (VG_(delFromFM) (laog_exposition, 
                          (UWord*)&fm_expo, NULL, (UWord)&expo )) {
         HG_(free) (fm_expo);
      }
   }

   
   if (HG_(cardinalityWSU) (univ_laog) >= next_gc_univ_laog)
      univ_laog_do_GC();
   if (0) VG_(printf)("laog__del_edge exit\n");
}

__attribute__((noinline))
static WordSetID  laog__succs ( Lock* lk ) {
   UWord      keyW;
   LAOGLinks* links;
   keyW  = 0;
   links = NULL;
   if (VG_(lookupFM)( laog, &keyW, (UWord*)&links, (UWord)lk )) {
      tl_assert(links);
      tl_assert(keyW == (UWord)lk);
      return links->outs;
   } else {
      return HG_(emptyWS)( univ_laog );
   }
}

__attribute__((noinline))
static WordSetID  laog__preds ( Lock* lk ) {
   UWord      keyW;
   LAOGLinks* links;
   keyW  = 0;
   links = NULL;
   if (VG_(lookupFM)( laog, &keyW, (UWord*)&links, (UWord)lk )) {
      tl_assert(links);
      tl_assert(keyW == (UWord)lk);
      return links->inns;
   } else {
      return HG_(emptyWS)( univ_laog );
   }
}

__attribute__((noinline))
static void laog__sanity_check ( const HChar* who ) {
   UWord i, ws_size;
   UWord* ws_words;
   Lock* me;
   LAOGLinks* links;
   VG_(initIterFM)( laog );
   me = NULL;
   links = NULL;
   if (0) VG_(printf)("laog sanity check\n");
   while (VG_(nextIterFM)( laog, (UWord*)&me,
                                 (UWord*)&links )) {
      tl_assert(me);
      tl_assert(links);
      HG_(getPayloadWS)( &ws_words, &ws_size, univ_laog, links->inns );
      for (i = 0; i < ws_size; i++) {
         if ( ! HG_(elemWS)( univ_laog, 
                             laog__succs( (Lock*)ws_words[i] ), 
                             (UWord)me ))
            goto bad;
      }
      HG_(getPayloadWS)( &ws_words, &ws_size, univ_laog, links->outs );
      for (i = 0; i < ws_size; i++) {
         if ( ! HG_(elemWS)( univ_laog, 
                             laog__preds( (Lock*)ws_words[i] ), 
                             (UWord)me ))
            goto bad;
      }
      me = NULL;
      links = NULL;
   }
   VG_(doneIterFM)( laog );
   return;

  bad:
   VG_(printf)("laog__sanity_check(%s) FAILED\n", who);
   laog__show(who);
   tl_assert(0);
}

__attribute__((noinline))
static
Lock* laog__do_dfs_from_to ( Lock* src, WordSetID dsts  )
{
   Lock*     ret;
   Word      ssz;
   XArray*   stack;   
   WordFM*   visited; 
   Lock*     here;
   WordSetID succs;
   UWord     succs_size, i;
   UWord*    succs_words;
   

   if (HG_(isEmptyWS)( univ_lsets, dsts ))
      return NULL;

   ret     = NULL;
   stack   = VG_(newXA)( HG_(zalloc), "hg.lddft.1", HG_(free), sizeof(Lock*) );
   visited = VG_(newFM)( HG_(zalloc), "hg.lddft.2", HG_(free), NULL );

   (void) VG_(addToXA)( stack, &src );

   while (True) {

      ssz = VG_(sizeXA)( stack );

      if (ssz == 0) { ret = NULL; break; }

      here = *(Lock**) VG_(indexXA)( stack, ssz-1 );
      VG_(dropTailXA)( stack, 1 );

      if (HG_(elemWS)( univ_lsets, dsts, (UWord)here )) { ret = here; break; }

      if (VG_(lookupFM)( visited, NULL, NULL, (UWord)here ))
         continue;

      VG_(addToFM)( visited, (UWord)here, 0 );

      succs = laog__succs( here );
      HG_(getPayloadWS)( &succs_words, &succs_size, univ_laog, succs );
      for (i = 0; i < succs_size; i++)
         (void) VG_(addToXA)( stack, &succs_words[i] );
   }

   VG_(deleteFM)( visited, NULL, NULL );
   VG_(deleteXA)( stack );
   return ret;
}


__attribute__((noinline))
static void laog__pre_thread_acquires_lock ( 
               Thread* thr, 
               Lock*   lk
            )
{
   UWord*   ls_words;
   UWord    ls_size, i;
   Lock*    other;

   
   if (HG_(elemWS)( univ_lsets, thr->locksetA, (UWord)lk ))
      return;

   other = laog__do_dfs_from_to(lk, thr->locksetA);
   if (other) {
      LAOGLinkExposition key, *found;
      key.src_ga = lk->guestaddr;
      key.dst_ga = other->guestaddr;
      key.src_ec = NULL;
      key.dst_ec = NULL;
      found = NULL;
      if (VG_(lookupFM)( laog_exposition,
                         (UWord*)&found, NULL, (UWord)&key )) {
         tl_assert(found != &key);
         tl_assert(found->src_ga == key.src_ga);
         tl_assert(found->dst_ga == key.dst_ga);
         tl_assert(found->src_ec);
         tl_assert(found->dst_ec);
         HG_(record_error_LockOrder)( 
            thr, lk, other,
                 found->src_ec, found->dst_ec, other->acquired_at );
      } else {
         
                    
         HG_(record_error_LockOrder)(
            thr, lk, other,
                 NULL, NULL, other->acquired_at );
      }
   }

   tl_assert(lk->acquired_at);
   HG_(getPayloadWS)( &ls_words, &ls_size, univ_lsets, thr->locksetA );
   for (i = 0; i < ls_size; i++) {
      Lock* old = (Lock*)ls_words[i];
      tl_assert(old->acquired_at);
      laog__add_edge( old, lk );
   }

   if (HG_(clo_sanity_flags) & SCE_LAOG)
      all_except_Locks__sanity_check("laog__pre_thread_acquires_lock-post");
}

static UWord* UWordV_dup(UWord* words, Word words_size)
{
   UInt i;

   if (words_size == 0)
      return NULL;

   UWord *dup = HG_(zalloc) ("hg.dup.1", (SizeT) words_size * sizeof(UWord));

   for (i = 0; i < words_size; i++)
      dup[i] = words[i];

   return dup;
}


__attribute__((noinline))
static void laog__handle_one_lock_deletion ( Lock* lk )
{
   WordSetID preds, succs;
   UWord preds_size, succs_size, i, j;
   UWord *preds_words, *succs_words;

   preds = laog__preds( lk );
   succs = laog__succs( lk );

   
   
   HG_(getPayloadWS)( &preds_words, &preds_size, univ_laog, preds );
   preds_words = UWordV_dup(preds_words, preds_size);

   HG_(getPayloadWS)( &succs_words, &succs_size, univ_laog, succs );
   succs_words = UWordV_dup(succs_words, succs_size);

   for (i = 0; i < preds_size; i++)
      laog__del_edge( (Lock*)preds_words[i], lk );

   for (j = 0; j < succs_size; j++)
      laog__del_edge( lk, (Lock*)succs_words[j] );

   for (i = 0; i < preds_size; i++) {
      for (j = 0; j < succs_size; j++) {
         if (preds_words[i] != succs_words[j]) {
            laog__add_edge( (Lock*)preds_words[i], (Lock*)succs_words[j] );
         }
      }
   }

   if (preds_words)
      HG_(free) (preds_words);
   if (succs_words)
      HG_(free) (succs_words);

   
   {
      LAOGLinks *links;
      Lock* linked_lk;

      if (VG_(delFromFM) (laog, 
                          (UWord*)&linked_lk, (UWord*)&links, (UWord)lk)) {
         tl_assert (linked_lk == lk);
         HG_(free) (links);
      }
   }
   
}




typedef
   struct {
      void*       next;    
      Addr        payload; 
      SizeT       szB;     
      ExeContext* where;   
      Thread*     thr;     
   }
   MallocMeta;

static VgHashTable *hg_mallocmeta_table = NULL;

static PoolAlloc *MallocMeta_poolalloc = NULL;

static MallocMeta* new_MallocMeta ( void ) {
   MallocMeta* md = VG_(allocEltPA) (MallocMeta_poolalloc);
   VG_(memset)(md, 0, sizeof(MallocMeta));
   return md;
}
static void delete_MallocMeta ( MallocMeta* md ) {
   VG_(freeEltPA)(MallocMeta_poolalloc, md);
}



static
void* handle_alloc ( ThreadId tid, 
                     SizeT szB, SizeT alignB, Bool is_zeroed )
{
   Addr        p;
   MallocMeta* md;

   tl_assert( ((SSizeT)szB) >= 0 );
   p = (Addr)VG_(cli_malloc)(alignB, szB);
   if (!p) {
      return NULL;
   }
   if (is_zeroed)
      VG_(memset)((void*)p, 0, szB);

   md = new_MallocMeta();
   md->payload = p;
   md->szB     = szB;
   md->where   = VG_(record_ExeContext)( tid, 0 );
   md->thr     = map_threads_lookup( tid );

   VG_(HT_add_node)( hg_mallocmeta_table, (VgHashNode*)md );

   
   evh__new_mem_heap( p, szB, is_zeroed );

   return (void*)p;
}

static void* hg_cli__malloc ( ThreadId tid, SizeT n ) {
   if (((SSizeT)n) < 0) return NULL;
   return handle_alloc ( tid, n, VG_(clo_alignment),
                         False );
}
static void* hg_cli____builtin_new ( ThreadId tid, SizeT n ) {
   if (((SSizeT)n) < 0) return NULL;
   return handle_alloc ( tid, n, VG_(clo_alignment),
                         False );
}
static void* hg_cli____builtin_vec_new ( ThreadId tid, SizeT n ) {
   if (((SSizeT)n) < 0) return NULL;
   return handle_alloc ( tid, n, VG_(clo_alignment), 
                         False );
}
static void* hg_cli__memalign ( ThreadId tid, SizeT align, SizeT n ) {
   if (((SSizeT)n) < 0) return NULL;
   return handle_alloc ( tid, n, align, 
                         False );
}
static void* hg_cli__calloc ( ThreadId tid, SizeT nmemb, SizeT size1 ) {
   if ( ((SSizeT)nmemb) < 0 || ((SSizeT)size1) < 0 ) return NULL;
   return handle_alloc ( tid, nmemb*size1, VG_(clo_alignment),
                         True );
}



static void handle_free ( ThreadId tid, void* p )
{
   MallocMeta *md, *old_md;
   SizeT      szB;

   
   md = (MallocMeta*) VG_(HT_lookup)( hg_mallocmeta_table, (UWord)p );
   if (!md)
      return; 

   tl_assert(md->payload == (Addr)p);
   szB = md->szB;

   
   old_md = (MallocMeta*)
            VG_(HT_remove)( hg_mallocmeta_table, (UWord)p );
   tl_assert(old_md); 
   tl_assert(old_md == md);
   tl_assert(old_md->payload == (Addr)p);

   VG_(cli_free)((void*)old_md->payload);
   delete_MallocMeta(old_md);

   
   evh__die_mem_heap( (Addr)p, szB );
}

static void hg_cli__free ( ThreadId tid, void* p ) {
   handle_free(tid, p);
}
static void hg_cli____builtin_delete ( ThreadId tid, void* p ) {
   handle_free(tid, p);
}
static void hg_cli____builtin_vec_delete ( ThreadId tid, void* p ) {
   handle_free(tid, p);
}


static void* hg_cli__realloc ( ThreadId tid, void* payloadV, SizeT new_size )
{
   MallocMeta *md, *md_new, *md_tmp;
   SizeT      i;

   Addr payload = (Addr)payloadV;

   if (((SSizeT)new_size) < 0) return NULL;

   md = (MallocMeta*) VG_(HT_lookup)( hg_mallocmeta_table, (UWord)payload );
   if (!md)
      return NULL; 
  
   tl_assert(md->payload == payload);

   if (md->szB == new_size) {
      
      md->where = VG_(record_ExeContext)(tid, 0);
      return payloadV;
   }

   if (md->szB > new_size) {
      
      md->szB   = new_size;
      md->where = VG_(record_ExeContext)(tid, 0);
      evh__die_mem_heap( md->payload + new_size, md->szB - new_size );
      return payloadV;
   }

    {
      
      Addr p_new = (Addr)VG_(cli_malloc)(VG_(clo_alignment), new_size);

      
      
      
      evh__copy_mem( payload, p_new, md->szB );
      evh__new_mem_heap ( p_new + md->szB, new_size - md->szB,
                          False );
      evh__die_mem_heap( payload, md->szB );

      
      for (i = 0; i < md->szB; i++)
         ((UChar*)p_new)[i] = ((UChar*)payload)[i];

      md_new = new_MallocMeta();
      *md_new = *md;

      md_tmp = VG_(HT_remove)( hg_mallocmeta_table, payload );
      tl_assert(md_tmp);
      tl_assert(md_tmp == md);

      VG_(cli_free)((void*)md->payload);
      delete_MallocMeta(md);

      
      md_new->where   = VG_(record_ExeContext)( tid, 0 );
      md_new->szB     = new_size;
      md_new->payload = p_new;
      md_new->thr     = map_threads_lookup( tid );

      
      VG_(HT_add_node)( hg_mallocmeta_table, (VgHashNode*)md_new );

      return (void*)p_new;
   }  
}

static SizeT hg_cli_malloc_usable_size ( ThreadId tid, void* p )
{
   MallocMeta *md = VG_(HT_lookup)( hg_mallocmeta_table, (UWord)p );

   
   
   return ( md ? md->szB : 0 );
}



static inline Bool addr_is_in_MM_Chunk( MallocMeta* mm, Addr a )
{
  if (UNLIKELY(mm->szB == 0 && a == mm->payload))
     return True;
  
  if (LIKELY(a < mm->payload)) return False;
  if (LIKELY(a >= mm->payload + mm->szB)) return False;
  return True;
}

Bool HG_(mm_find_containing_block)( ExeContext** where,
                                    UInt*        tnr,
                                    Addr*        payload,
                                    SizeT*       szB,
                                    Addr                data_addr )
{
   MallocMeta* mm;
   Int i;
   const Int n_fast_check_words = 16;

   for (i = 0; i < n_fast_check_words; i++) {
      mm = VG_(HT_lookup)( hg_mallocmeta_table,
                           data_addr - (UWord)(UInt)i * sizeof(UWord) );
      if (UNLIKELY(mm && addr_is_in_MM_Chunk(mm, data_addr)))
         goto found;
   }

   VG_(HT_ResetIter)(hg_mallocmeta_table);
   while ( (mm = VG_(HT_Next)(hg_mallocmeta_table)) ) {
      if (UNLIKELY(addr_is_in_MM_Chunk(mm, data_addr)))
         goto found;
   }

   
   return False;
   

  found:
   tl_assert(mm);
   tl_assert(addr_is_in_MM_Chunk(mm, data_addr));
   if (where)   *where   = mm->where;
   if (tnr)     *tnr     = mm->thr->errmsg_index;
   if (payload) *payload = mm->payload;
   if (szB)     *szB     = mm->szB;
   return True;
}



#define unop(_op, _arg1)         IRExpr_Unop((_op),(_arg1))
#define binop(_op, _arg1, _arg2) IRExpr_Binop((_op),(_arg1),(_arg2))
#define mkexpr(_tmp)             IRExpr_RdTmp((_tmp))
#define mkU32(_n)                IRExpr_Const(IRConst_U32(_n))
#define mkU64(_n)                IRExpr_Const(IRConst_U64(_n))
#define assign(_t, _e)           IRStmt_WrTmp((_t), (_e))

static IRExpr* mk_And1 ( IRSB* sbOut, IRExpr* arg1, IRExpr* arg2 )
{
   tl_assert(arg1 && arg2);
   tl_assert(isIRAtom(arg1));
   tl_assert(isIRAtom(arg2));
   IRTemp wide1 = newIRTemp(sbOut->tyenv, Ity_I32);
   IRTemp wide2 = newIRTemp(sbOut->tyenv, Ity_I32);
   IRTemp anded = newIRTemp(sbOut->tyenv, Ity_I32);
   IRTemp res   = newIRTemp(sbOut->tyenv, Ity_I1);
   addStmtToIRSB(sbOut, assign(wide1, unop(Iop_1Uto32, arg1)));
   addStmtToIRSB(sbOut, assign(wide2, unop(Iop_1Uto32, arg2)));
   addStmtToIRSB(sbOut, assign(anded, binop(Iop_And32, mkexpr(wide1),
                                                       mkexpr(wide2))));
   addStmtToIRSB(sbOut, assign(res, unop(Iop_32to1, mkexpr(anded))));
   return mkexpr(res);
}

static void instrument_mem_access ( IRSB*   sbOut, 
                                    IRExpr* addr,
                                    Int     szB,
                                    Bool    isStore,
                                    Int     hWordTy_szB,
                                    Int     goff_sp,
                                    IRExpr* guard ) 
{
   IRType   tyAddr   = Ity_INVALID;
   const HChar* hName    = NULL;
   void*    hAddr    = NULL;
   Int      regparms = 0;
   IRExpr** argv     = NULL;
   IRDirty* di       = NULL;

   
   
   const Int THRESH = 4096 * 4; 
   const Int rz_szB = VG_STACK_REDZONE_SZB;

   tl_assert(isIRAtom(addr));
   tl_assert(hWordTy_szB == 4 || hWordTy_szB == 8);

   tyAddr = typeOfIRExpr( sbOut->tyenv, addr );
   tl_assert(tyAddr == Ity_I32 || tyAddr == Ity_I64);

   
   regparms = 1; 
   if (isStore) {
      switch (szB) {
         case 1:
            hName = "evh__mem_help_cwrite_1";
            hAddr = &evh__mem_help_cwrite_1;
            argv = mkIRExprVec_1( addr );
            break;
         case 2:
            hName = "evh__mem_help_cwrite_2";
            hAddr = &evh__mem_help_cwrite_2;
            argv = mkIRExprVec_1( addr );
            break;
         case 4:
            hName = "evh__mem_help_cwrite_4";
            hAddr = &evh__mem_help_cwrite_4;
            argv = mkIRExprVec_1( addr );
            break;
         case 8:
            hName = "evh__mem_help_cwrite_8";
            hAddr = &evh__mem_help_cwrite_8;
            argv = mkIRExprVec_1( addr );
            break;
         default:
            tl_assert(szB > 8 && szB <= 512); 
            regparms = 2;
            hName = "evh__mem_help_cwrite_N";
            hAddr = &evh__mem_help_cwrite_N;
            argv = mkIRExprVec_2( addr, mkIRExpr_HWord( szB ));
            break;
      }
   } else {
      switch (szB) {
         case 1:
            hName = "evh__mem_help_cread_1";
            hAddr = &evh__mem_help_cread_1;
            argv = mkIRExprVec_1( addr );
            break;
         case 2:
            hName = "evh__mem_help_cread_2";
            hAddr = &evh__mem_help_cread_2;
            argv = mkIRExprVec_1( addr );
            break;
         case 4:
            hName = "evh__mem_help_cread_4";
            hAddr = &evh__mem_help_cread_4;
            argv = mkIRExprVec_1( addr );
            break;
         case 8:
            hName = "evh__mem_help_cread_8";
            hAddr = &evh__mem_help_cread_8;
            argv = mkIRExprVec_1( addr );
            break;
         default: 
            tl_assert(szB > 8 && szB <= 512); 
            regparms = 2;
            hName = "evh__mem_help_cread_N";
            hAddr = &evh__mem_help_cread_N;
            argv = mkIRExprVec_2( addr, mkIRExpr_HWord( szB ));
            break;
      }
   }

   
   tl_assert(hName);
   tl_assert(hAddr);
   tl_assert(argv);
   di = unsafeIRDirty_0_N( regparms,
                           hName, VG_(fnptr_to_fnentry)( hAddr ),
                           argv );

   if (! HG_(clo_check_stack_refs)) {
      IRTemp sp = newIRTemp(sbOut->tyenv, tyAddr);
      addStmtToIRSB( sbOut, assign(sp, IRExpr_Get(goff_sp, tyAddr)));

      
      IRTemp addr_minus_sp = newIRTemp(sbOut->tyenv, tyAddr);
      addStmtToIRSB(
         sbOut,
         assign(addr_minus_sp,
                tyAddr == Ity_I32
                   ? binop(Iop_Sub32, addr, mkexpr(sp))
                   : binop(Iop_Sub64, addr, mkexpr(sp)))
      );

      
      IRTemp diff = newIRTemp(sbOut->tyenv, tyAddr);
      addStmtToIRSB(
         sbOut,
         assign(diff,
                tyAddr == Ity_I32 
                   ? binop(Iop_Add32, mkexpr(addr_minus_sp), mkU32(rz_szB))
                   : binop(Iop_Add64, mkexpr(addr_minus_sp), mkU64(rz_szB)))
      );

      
      IRTemp guardA = newIRTemp(sbOut->tyenv, Ity_I1);
      addStmtToIRSB(
         sbOut,
         assign(guardA,
                tyAddr == Ity_I32 
                   ? binop(Iop_CmpLT32U, mkU32(THRESH), mkexpr(diff))
                   : binop(Iop_CmpLT64U, mkU64(THRESH), mkexpr(diff)))
      );
      di->guard = mkexpr(guardA);
   }

   if (guard) {
      di->guard = mk_And1(sbOut, di->guard, guard);
   }

   
   addStmtToIRSB( sbOut, IRStmt_Dirty(di) );
}


static Bool is_in_dynamic_linker_shared_object( Addr ga )
{
   DebugInfo* dinfo;
   const HChar* soname;
   if (0) return False;

   dinfo = VG_(find_DebugInfo)( ga );
   if (!dinfo) return False;

   soname = VG_(DebugInfo_get_soname)(dinfo);
   tl_assert(soname);
   if (0) VG_(printf)("%s\n", soname);

#  if defined(VGO_linux)
   if (VG_STREQ(soname, VG_U_LD_LINUX_SO_3))        return True;
   if (VG_STREQ(soname, VG_U_LD_LINUX_SO_2))        return True;
   if (VG_STREQ(soname, VG_U_LD_LINUX_X86_64_SO_2)) return True;
   if (VG_STREQ(soname, VG_U_LD64_SO_1))            return True;
   if (VG_STREQ(soname, VG_U_LD64_SO_2))            return True;
   if (VG_STREQ(soname, VG_U_LD_SO_1))              return True;
   if (VG_STREQ(soname, VG_U_LD_LINUX_AARCH64_SO_1)) return True;
   if (VG_STREQ(soname, VG_U_LD_LINUX_ARMHF_SO_3))  return True;
#  elif defined(VGO_darwin)
   if (VG_STREQ(soname, VG_U_DYLD)) return True;
#  else
#    error "Unsupported OS"
#  endif
   return False;
}

static
IRSB* hg_instrument ( VgCallbackClosure* closure,
                      IRSB* bbIn,
                      const VexGuestLayout* layout,
                      const VexGuestExtents* vge,
                      const VexArchInfo* archinfo_host,
                      IRType gWordTy, IRType hWordTy )
{
   Int     i;
   IRSB*   bbOut;
   Addr    cia; 
   IRStmt* st;
   Bool    inLDSO = False;
   Addr    inLDSOmask4K = 1; 

   const Int goff_sp = layout->offset_SP;

   if (gWordTy != hWordTy) {
      
      VG_(tool_panic)("host/guest word size mismatch");
   }

   if (VKI_PAGE_SIZE < 4096 || VG_(log2)(VKI_PAGE_SIZE) == -1) {
      VG_(tool_panic)("implausible or too-small VKI_PAGE_SIZE");
   }

   
   bbOut           = emptyIRSB();
   bbOut->tyenv    = deepCopyIRTypeEnv(bbIn->tyenv);
   bbOut->next     = deepCopyIRExpr(bbIn->next);
   bbOut->jumpkind = bbIn->jumpkind;
   bbOut->offsIP   = bbIn->offsIP;

   
   i = 0;
   while (i < bbIn->stmts_used && bbIn->stmts[i]->tag != Ist_IMark) {
      addStmtToIRSB( bbOut, bbIn->stmts[i] );
      i++;
   }

   
   tl_assert(bbIn->stmts_used > 0);
   tl_assert(i < bbIn->stmts_used);
   st = bbIn->stmts[i];
   tl_assert(Ist_IMark == st->tag);
   cia = st->Ist.IMark.addr;
   st = NULL;

   for (; i < bbIn->stmts_used; i++) {
      st = bbIn->stmts[i];
      tl_assert(st);
      tl_assert(isFlatIRStmt(st));
      switch (st->tag) {
         case Ist_NoOp:
         case Ist_AbiHint:
         case Ist_Put:
         case Ist_PutI:
         case Ist_Exit:
            
            break;

         case Ist_IMark:
            
            cia = st->Ist.IMark.addr;
            if ((cia & ~(Addr)0xFFF) != inLDSOmask4K) {
               if (0) VG_(printf)("NEW %#lx\n", cia);
               inLDSOmask4K = cia & ~(Addr)0xFFF;
               inLDSO = is_in_dynamic_linker_shared_object(cia);
            } else {
               if (0) VG_(printf)("old %#lx\n", cia);
            }
            break;

         case Ist_MBE:
            switch (st->Ist.MBE.event) {
               case Imbe_Fence:
               case Imbe_CancelReservation:
                  break; 
               default:
                  goto unhandled;
            }
            break;

         case Ist_CAS: {
            IRCAS* cas    = st->Ist.CAS.details;
            Bool   isDCAS = cas->oldHi != IRTemp_INVALID;
            if (isDCAS) {
               tl_assert(cas->expdHi);
               tl_assert(cas->dataHi);
            } else {
               tl_assert(!cas->expdHi);
               tl_assert(!cas->dataHi);
            }
            
            if (!inLDSO) {
               instrument_mem_access(
                  bbOut,
                  cas->addr,
                  (isDCAS ? 2 : 1)
                     * sizeofIRType(typeOfIRExpr(bbIn->tyenv, cas->dataLo)),
                  False,
                  sizeofIRType(hWordTy), goff_sp,
                  NULL
               );
            }
            break;
         }

         case Ist_LLSC: {
            IRType dataTy;
            if (st->Ist.LLSC.storedata == NULL) {
               
               dataTy = typeOfIRTemp(bbIn->tyenv, st->Ist.LLSC.result);
               if (!inLDSO) {
                  instrument_mem_access(
                     bbOut,
                     st->Ist.LLSC.addr,
                     sizeofIRType(dataTy),
                     False,
                     sizeofIRType(hWordTy), goff_sp,
                     NULL
                  );
               }
            } else {
               
               
            }
            break;
         }

         case Ist_Store:
            if (!inLDSO) {
               instrument_mem_access( 
                  bbOut, 
                  st->Ist.Store.addr, 
                  sizeofIRType(typeOfIRExpr(bbIn->tyenv, st->Ist.Store.data)),
                  True,
                  sizeofIRType(hWordTy), goff_sp,
                  NULL
               );
            }
            break;

         case Ist_StoreG: {
            IRStoreG* sg   = st->Ist.StoreG.details;
            IRExpr*   data = sg->data;
            IRExpr*   addr = sg->addr;
            IRType    type = typeOfIRExpr(bbIn->tyenv, data);
            tl_assert(type != Ity_INVALID);
            instrument_mem_access( bbOut, addr, sizeofIRType(type),
                                   True,
                                   sizeofIRType(hWordTy),
                                   goff_sp, sg->guard );
            break;
         }

         case Ist_LoadG: {
            IRLoadG* lg       = st->Ist.LoadG.details;
            IRType   type     = Ity_INVALID; 
            IRType   typeWide = Ity_INVALID; 
            IRExpr*  addr     = lg->addr;
            typeOfIRLoadGOp(lg->cvt, &typeWide, &type);
            tl_assert(type != Ity_INVALID);
            instrument_mem_access( bbOut, addr, sizeofIRType(type),
                                   False,
                                   sizeofIRType(hWordTy),
                                   goff_sp, lg->guard );
            break;
         }

         case Ist_WrTmp: {
            IRExpr* data = st->Ist.WrTmp.data;
            if (data->tag == Iex_Load) {
               if (!inLDSO) {
                  instrument_mem_access(
                     bbOut,
                     data->Iex.Load.addr,
                     sizeofIRType(data->Iex.Load.ty),
                     False,
                     sizeofIRType(hWordTy), goff_sp,
                     NULL
                  );
               }
            }
            break;
         }

         case Ist_Dirty: {
            Int      dataSize;
            IRDirty* d = st->Ist.Dirty.details;
            if (d->mFx != Ifx_None) {
               tl_assert(d->mAddr != NULL);
               tl_assert(d->mSize != 0);
               dataSize = d->mSize;
               if (d->mFx == Ifx_Read || d->mFx == Ifx_Modify) {
                  if (!inLDSO) {
                     instrument_mem_access( 
                        bbOut, d->mAddr, dataSize, False,
                        sizeofIRType(hWordTy), goff_sp, NULL
                     );
                  }
               }
               if (d->mFx == Ifx_Write || d->mFx == Ifx_Modify) {
                  if (!inLDSO) {
                     instrument_mem_access( 
                        bbOut, d->mAddr, dataSize, True,
                        sizeofIRType(hWordTy), goff_sp, NULL
                     );
                  }
               }
            } else {
               tl_assert(d->mAddr == NULL);
               tl_assert(d->mSize == 0);
            }
            break;
         }

         default:
         unhandled:
            ppIRStmt(st);
            tl_assert(0);

      } 

      addStmtToIRSB( bbOut, st );
   } 

   return bbOut;
}

#undef binop
#undef mkexpr
#undef mkU32
#undef mkU64
#undef assign



static WordFM* map_pthread_t_to_Thread = NULL; 

static void map_pthread_t_to_Thread_INIT ( void ) {
   if (UNLIKELY(map_pthread_t_to_Thread == NULL)) {
      map_pthread_t_to_Thread = VG_(newFM)( HG_(zalloc), "hg.mpttT.1", 
                                            HG_(free), NULL );
   }
}

typedef
   struct { 
      void* dependent; 
      void* master;    
      Word  master_level; 
      Thread* hg_dependent; 
   }
   GNAT_dmml;
static XArray* gnat_dmmls;   
static void gnat_dmmls_INIT (void)
{
   if (UNLIKELY(gnat_dmmls == NULL)) {
      gnat_dmmls = VG_(newXA) (HG_(zalloc), "hg.gnat_md.1",
                               HG_(free),
                               sizeof(GNAT_dmml) );
   }
}
static void print_monitor_help ( void )
{
   VG_(gdb_printf) 
      (
"\n"
"helgrind monitor commands:\n"
"  info locks [lock_addr]  : show status of lock at addr lock_addr\n"
"           with no lock_addr, show status of all locks\n"
"  accesshistory <addr> [<len>]   : show access history recorded\n"
"                     for <len> (or 1) bytes at <addr>\n"
"\n");
}

static Bool handle_gdb_monitor_command (ThreadId tid, HChar *req)
{
   HChar* wcmd;
   HChar s[VG_(strlen(req))]; 
   HChar *ssaveptr;
   Int   kwdid;

   VG_(strcpy) (s, req);

   wcmd = VG_(strtok_r) (s, " ", &ssaveptr);
   switch (VG_(keyword_id) 
           ("help info accesshistory", 
            wcmd, kwd_report_duplicated_matches)) {
   case -2: 
      return True;
   case -1: 
      return False;
   case  0: 
      print_monitor_help();
      return True;
   case  1: 
      wcmd = VG_(strtok_r) (NULL, " ", &ssaveptr);
      switch (kwdid = VG_(keyword_id) 
              ("locks",
               wcmd, kwd_report_all)) {
      case -2:
      case -1: 
         break;
      case 0: 
         {
            const HChar* wa;
            Addr lk_addr = 0;
            Bool lk_shown = False;
            Bool all_locks = True;
            Int i;
            Lock* lk;

            wa = VG_(strtok_r) (NULL, " ", &ssaveptr);
            if (wa != NULL) {
               if (VG_(parse_Addr) (&wa, &lk_addr) )
                  all_locks = False;
               else {
                  VG_(gdb_printf) ("missing or malformed address\n");
               }
            }
            for (i = 0, lk = admin_locks;  lk;  i++, lk = lk->admin_next) {
               if (all_locks || lk_addr == lk->guestaddr) {
                  pp_Lock(0, lk,
                          True ,
                          False );
                  lk_shown = True;
               }
            }
            if (i == 0)
               VG_(gdb_printf) ("no locks\n");
            if (!all_locks && !lk_shown)
               VG_(gdb_printf) ("lock with address %p not found\n",
                                (void*)lk_addr);
         }
         break;
      default:
         tl_assert(0);
      }
      return True;

   case  2: 
      {
         Addr address;
         SizeT szB = 1;
         if (VG_(strtok_get_address_and_size) (&address, &szB, &ssaveptr)) {
            if (szB >= 1) 
               libhb_event_map_access_history (address, szB, HG_(print_access));
            else
               VG_(gdb_printf) ("len must be >=1\n");
         }
         return True;
      }

   default: 
      tl_assert(0);
      return False;
   }
}

static 
Bool hg_handle_client_request ( ThreadId tid, UWord* args, UWord* ret)
{
   if (!VG_IS_TOOL_USERREQ('H','G',args[0])
       && VG_USERREQ__GDB_MONITOR_COMMAND   != args[0])
      return False;


   
   *ret = 0;

   switch (args[0]) {

      

      case VG_USERREQ__HG_CLEAN_MEMORY:
         if (0) VG_(printf)("VG_USERREQ__HG_CLEAN_MEMORY(%#lx,%ld)\n",
                            args[1], args[2]);
         if (args[2] > 0) { 
            evh__die_mem(args[1], args[2]);
            
            evh__new_mem(args[1], args[2]);
         }
         break;

      case _VG_USERREQ__HG_CLEAN_MEMORY_HEAPBLOCK: {
         Addr  payload = 0;
         SizeT pszB = 0;
         if (0) VG_(printf)("VG_USERREQ__HG_CLEAN_MEMORY_HEAPBLOCK(%#lx)\n",
                            args[1]);
         if (HG_(mm_find_containing_block)(NULL, NULL, 
                                           &payload, &pszB, args[1])) {
            if (pszB > 0) {
               evh__die_mem(payload, pszB);
               evh__new_mem(payload, pszB);
            }
            *ret = pszB;
         } else {
            *ret = (UWord)-1;
         }
         break;
      }

      case _VG_USERREQ__HG_ARANGE_MAKE_UNTRACKED:
         if (0) VG_(printf)("HG_ARANGE_MAKE_UNTRACKED(%#lx,%ld)\n",
                            args[1], args[2]);
         if (args[2] > 0) { 
            evh__untrack_mem(args[1], args[2]);
         }
         break;

      case _VG_USERREQ__HG_ARANGE_MAKE_TRACKED:
         if (0) VG_(printf)("HG_ARANGE_MAKE_TRACKED(%#lx,%ld)\n",
                            args[1], args[2]);
         if (args[2] > 0) { 
            evh__new_mem(args[1], args[2]);
         }
         break;

      case _VG_USERREQ__HG_GET_ABITS:
         if (0) VG_(printf)("HG_GET_ABITS(%#lx,%#lx,%ld)\n",
                            args[1], args[2], args[3]);
         UChar *zzabit = (UChar *) args[2];
         if (zzabit == NULL 
             || VG_(am_is_valid_for_client)((Addr)zzabit, (SizeT)args[3],
                                            VKI_PROT_READ|VKI_PROT_WRITE))
            *ret = (UWord) libhb_srange_get_abits ((Addr)   args[1],
                                                   (UChar*) args[2],
                                                   (SizeT)  args[3]);
         else
            *ret = -1;
         break;

      

      case _VG_USERREQ__HG_SET_MY_PTHREAD_T: {
         Thread* my_thr = NULL;
         if (0)
         VG_(printf)("SET_MY_PTHREAD_T (tid %d): pthread_t = %p\n", (Int)tid,
                     (void*)args[1]);
         map_pthread_t_to_Thread_INIT();
         my_thr = map_threads_maybe_lookup( tid );
         tl_assert(my_thr != NULL);
         if (0)
         VG_(printf)("XXXX: bind pthread_t %p to Thread* %p\n",
                     (void*)args[1], (void*)my_thr );
         VG_(addToFM)( map_pthread_t_to_Thread, (UWord)args[1], (UWord)my_thr );
         break;
      }

      case _VG_USERREQ__HG_PTH_API_ERROR: {
         Thread* my_thr = NULL;
         map_pthread_t_to_Thread_INIT();
         my_thr = map_threads_maybe_lookup( tid );
         tl_assert(my_thr); 
         HG_(record_error_PthAPIerror)(
            my_thr, (HChar*)args[1], (UWord)args[2], (HChar*)args[3] );
         break;
      }

      case _VG_USERREQ__HG_PTHREAD_JOIN_POST: {
         Thread* thr_q = NULL; 
         Bool    found = False;
         if (0)
         VG_(printf)("NOTIFY_JOIN_COMPLETE (tid %d): quitter = %p\n", (Int)tid,
                     (void*)args[1]);
         map_pthread_t_to_Thread_INIT();
         found = VG_(lookupFM)( map_pthread_t_to_Thread, 
                                NULL, (UWord*)&thr_q, (UWord)args[1] );
         
         tl_assert(found);
         if (found) {
            if (0)
            VG_(printf)(".................... quitter Thread* = %p\n", 
                        thr_q);
            evh__HG_PTHREAD_JOIN_POST( tid, thr_q );
         }
         break;
      }

      
      case _VG_USERREQ__HG_GNAT_MASTER_HOOK: {
         GNAT_dmml dmml;
         dmml.dependent = (void*)args[1];
         dmml.master = (void*)args[2];
         dmml.master_level = (Word)args[3];
         dmml.hg_dependent = map_threads_maybe_lookup( tid );
         tl_assert(dmml.hg_dependent);

         if (0)
         VG_(printf)("HG_GNAT_MASTER_HOOK (tid %d): "
                     "dependent = %p master = %p master_level = %ld"
                     " dependent Thread* = %p\n",
                     (Int)tid, dmml.dependent, dmml.master, dmml.master_level,
                     dmml.hg_dependent);
         gnat_dmmls_INIT();
         VG_(addToXA) (gnat_dmmls, &dmml);
         break;
      }

      case _VG_USERREQ__HG_GNAT_MASTER_COMPLETED_HOOK: {
         Word n;
         const Thread *stayer = map_threads_maybe_lookup( tid );
         const void *master = (void*)args[1];
         const Word master_level = (Word) args[2];
         tl_assert(stayer);

         if (0)
         VG_(printf)("HG_GNAT_MASTER_COMPLETED_HOOK (tid %d): "
                     "self_id = %p master_level = %ld Thread* = %p\n",
                     (Int)tid, master, master_level, stayer);

         gnat_dmmls_INIT();
         for (n = VG_(sizeXA) (gnat_dmmls) - 1; n >= 0; n--) {
            GNAT_dmml *dmml = (GNAT_dmml*) VG_(indexXA)(gnat_dmmls, n);
            if (dmml->master == master
                && dmml->master_level == master_level) {
               if (0)
               VG_(printf)("quitter %p dependency to stayer %p\n",
                           dmml->hg_dependent->hbthr,  stayer->hbthr);
               tl_assert(dmml->hg_dependent->hbthr != stayer->hbthr);
               generate_quitter_stayer_dependence (dmml->hg_dependent->hbthr,
                                                   stayer->hbthr);
               VG_(removeIndexXA) (gnat_dmmls, n);
            }
         }
         break;
      }

      case _VG_USERREQ__HG_PTHREAD_MUTEX_INIT_POST:
         evh__HG_PTHREAD_MUTEX_INIT_POST( tid, (void*)args[1], args[2] );
         break;

      
      case _VG_USERREQ__HG_PTHREAD_MUTEX_DESTROY_PRE:
         evh__HG_PTHREAD_MUTEX_DESTROY_PRE( tid, (void*)args[1], args[2] != 0 );
         break;

      case _VG_USERREQ__HG_PTHREAD_MUTEX_UNLOCK_PRE:   
         evh__HG_PTHREAD_MUTEX_UNLOCK_PRE( tid, (void*)args[1] );
         break;

      case _VG_USERREQ__HG_PTHREAD_MUTEX_UNLOCK_POST:  
         evh__HG_PTHREAD_MUTEX_UNLOCK_POST( tid, (void*)args[1] );
         break;

      case _VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_PRE:     
         evh__HG_PTHREAD_MUTEX_LOCK_PRE( tid, (void*)args[1], args[2] );
         break;

      case _VG_USERREQ__HG_PTHREAD_MUTEX_LOCK_POST:    
         evh__HG_PTHREAD_MUTEX_LOCK_POST( tid, (void*)args[1] );
         break;

      case _VG_USERREQ__HG_PTHREAD_COND_SIGNAL_PRE:
      case _VG_USERREQ__HG_PTHREAD_COND_BROADCAST_PRE:
         evh__HG_PTHREAD_COND_SIGNAL_PRE( tid, (void*)args[1] );
         break;

      case _VG_USERREQ__HG_PTHREAD_COND_WAIT_PRE: {
         Bool mutex_is_valid
            = evh__HG_PTHREAD_COND_WAIT_PRE( tid, (void*)args[1], 
                                                  (void*)args[2] );
         *ret = mutex_is_valid ? 1 : 0;
         break;
      }

      case _VG_USERREQ__HG_PTHREAD_COND_INIT_POST:
         evh__HG_PTHREAD_COND_INIT_POST( tid,
                                         (void*)args[1], (void*)args[2] );
	 break;

      
      case _VG_USERREQ__HG_PTHREAD_COND_DESTROY_PRE:
         evh__HG_PTHREAD_COND_DESTROY_PRE( tid, (void*)args[1], args[2] != 0 );
         break;

      case _VG_USERREQ__HG_PTHREAD_COND_WAIT_POST:
         evh__HG_PTHREAD_COND_WAIT_POST( tid,
                                         (void*)args[1], (void*)args[2],
                                         (Bool)args[3] );
         break;

      case _VG_USERREQ__HG_PTHREAD_RWLOCK_INIT_POST:
         evh__HG_PTHREAD_RWLOCK_INIT_POST( tid, (void*)args[1] );
         break;

      case _VG_USERREQ__HG_PTHREAD_RWLOCK_DESTROY_PRE:
         evh__HG_PTHREAD_RWLOCK_DESTROY_PRE( tid, (void*)args[1] );
         break;

      
      case _VG_USERREQ__HG_PTHREAD_RWLOCK_LOCK_PRE:
         evh__HG_PTHREAD_RWLOCK_LOCK_PRE( tid, (void*)args[1],
                                               args[2], args[3] );
         break;

      
      case _VG_USERREQ__HG_PTHREAD_RWLOCK_LOCK_POST:
         evh__HG_PTHREAD_RWLOCK_LOCK_POST( tid, (void*)args[1], args[2] );
         break;

      case _VG_USERREQ__HG_PTHREAD_RWLOCK_UNLOCK_PRE:
         evh__HG_PTHREAD_RWLOCK_UNLOCK_PRE( tid, (void*)args[1] );
         break;

      case _VG_USERREQ__HG_PTHREAD_RWLOCK_UNLOCK_POST:
         evh__HG_PTHREAD_RWLOCK_UNLOCK_POST( tid, (void*)args[1] );
         break;

      case _VG_USERREQ__HG_POSIX_SEM_INIT_POST: 
         evh__HG_POSIX_SEM_INIT_POST( tid, (void*)args[1], args[2] );
         break;

      case _VG_USERREQ__HG_POSIX_SEM_DESTROY_PRE: 
         evh__HG_POSIX_SEM_DESTROY_PRE( tid, (void*)args[1] );
         break;

      case _VG_USERREQ__HG_POSIX_SEM_POST_PRE: 
         evh__HG_POSIX_SEM_POST_PRE( tid, (void*)args[1] );
         break;

      case _VG_USERREQ__HG_POSIX_SEM_WAIT_POST: 
         evh__HG_POSIX_SEM_WAIT_POST( tid, (void*)args[1] );
         break;

      case _VG_USERREQ__HG_PTHREAD_BARRIER_INIT_PRE:
         
         evh__HG_PTHREAD_BARRIER_INIT_PRE( tid, (void*)args[1],
                                                args[2], args[3] );
         break;

      case _VG_USERREQ__HG_PTHREAD_BARRIER_RESIZE_PRE:
         
         evh__HG_PTHREAD_BARRIER_RESIZE_PRE ( tid, (void*)args[1],
                                              args[2] );
         break;

      case _VG_USERREQ__HG_PTHREAD_BARRIER_WAIT_PRE:
         
         evh__HG_PTHREAD_BARRIER_WAIT_PRE( tid, (void*)args[1] );
         break;

      case _VG_USERREQ__HG_PTHREAD_BARRIER_DESTROY_PRE:
         
         evh__HG_PTHREAD_BARRIER_DESTROY_PRE( tid, (void*)args[1] );
         break;

      case _VG_USERREQ__HG_PTHREAD_SPIN_INIT_OR_UNLOCK_PRE:
         
         evh__HG_PTHREAD_SPIN_INIT_OR_UNLOCK_PRE( tid, (void*)args[1] );
         break;

      case _VG_USERREQ__HG_PTHREAD_SPIN_INIT_OR_UNLOCK_POST:
         
         evh__HG_PTHREAD_SPIN_INIT_OR_UNLOCK_POST( tid, (void*)args[1] );
         break;

      case _VG_USERREQ__HG_PTHREAD_SPIN_LOCK_PRE:
         
         evh__HG_PTHREAD_SPIN_LOCK_PRE( tid, (void*)args[1], args[2] );
         break;

      case _VG_USERREQ__HG_PTHREAD_SPIN_LOCK_POST:
         
         evh__HG_PTHREAD_SPIN_LOCK_POST( tid, (void*)args[1] );
         break;

      case _VG_USERREQ__HG_PTHREAD_SPIN_DESTROY_PRE:
         
         evh__HG_PTHREAD_SPIN_DESTROY_PRE( tid, (void*)args[1] );
         break;

      case _VG_USERREQ__HG_CLIENTREQ_UNIMP: {
         
         HChar*  who = (HChar*)args[1];
         HChar   buf[50 + 50];
         Thread* thr = map_threads_maybe_lookup( tid );
         tl_assert( thr ); 
         tl_assert( who );
         tl_assert( VG_(strlen)(who) <= 50 );
         VG_(sprintf)(buf, "Unimplemented client request macro \"%s\"", who );
         
         HG_(record_error_Misc)( thr, buf );
         break;
      }

      case _VG_USERREQ__HG_USERSO_SEND_PRE:
         
         evh__HG_USERSO_SEND_PRE( tid, args[1] );
         break;

      case _VG_USERREQ__HG_USERSO_RECV_POST:
         
         evh__HG_USERSO_RECV_POST( tid, args[1] );
         break;

      case _VG_USERREQ__HG_USERSO_FORGET_ALL:
         
         evh__HG_USERSO_FORGET_ALL( tid, args[1] );
         break;

      case VG_USERREQ__GDB_MONITOR_COMMAND: {
         Bool handled = handle_gdb_monitor_command (tid, (HChar*)args[1]);
         if (handled)
            *ret = 1;
         else
            *ret = 0;
         return handled;
      }

      default:
         
         tl_assert2(0, "unhandled Helgrind client request 0x%lx",
                       args[0]);
   }

   return True;
}



static Bool hg_process_cmd_line_option ( const HChar* arg )
{
   const HChar* tmp_str;

   if      VG_BOOL_CLO(arg, "--track-lockorders",
                            HG_(clo_track_lockorders)) {}
   else if VG_BOOL_CLO(arg, "--cmp-race-err-addrs",
                            HG_(clo_cmp_race_err_addrs)) {}

   else if VG_XACT_CLO(arg, "--history-level=none",
                            HG_(clo_history_level), 0);
   else if VG_XACT_CLO(arg, "--history-level=approx",
                            HG_(clo_history_level), 1);
   else if VG_XACT_CLO(arg, "--history-level=full",
                            HG_(clo_history_level), 2);

   else if VG_BINT_CLO(arg, "--conflict-cache-size",
                       HG_(clo_conflict_cache_size), 10*1000, 150*1000*1000) {}

   
   else if VG_STR_CLO(arg, "--hg-sanity-flags", tmp_str) {
      Int j;
   
      if (6 != VG_(strlen)(tmp_str)) {
         VG_(message)(Vg_UserMsg, 
                      "--hg-sanity-flags argument must have 6 digits\n");
         return False;
      }
      for (j = 0; j < 6; j++) {
         if      ('0' == tmp_str[j]) {  }
         else if ('1' == tmp_str[j]) HG_(clo_sanity_flags) |= (1 << (6-1-j));
         else {
            VG_(message)(Vg_UserMsg, "--hg-sanity-flags argument can "
                                     "only contain 0s and 1s\n");
            return False;
         }
      }
      if (0) VG_(printf)("XXX sanity flags: 0x%lx\n", HG_(clo_sanity_flags));
   }

   else if VG_BOOL_CLO(arg, "--free-is-write",
                            HG_(clo_free_is_write)) {}

   else if VG_XACT_CLO(arg, "--vts-pruning=never",
                            HG_(clo_vts_pruning), 0);
   else if VG_XACT_CLO(arg, "--vts-pruning=auto",
                            HG_(clo_vts_pruning), 1);
   else if VG_XACT_CLO(arg, "--vts-pruning=always",
                            HG_(clo_vts_pruning), 2);

   else if VG_BOOL_CLO(arg, "--check-stack-refs",
                            HG_(clo_check_stack_refs)) {}

   else 
      return VG_(replacement_malloc_process_cmd_line_option)(arg);

   return True;
}

static void hg_print_usage ( void )
{
   VG_(printf)(
"    --free-is-write=no|yes    treat heap frees as writes [no]\n"
"    --track-lockorders=no|yes show lock ordering errors? [yes]\n"
"    --history-level=none|approx|full [full]\n"
"       full:   show both stack traces for a data race (can be very slow)\n"
"       approx: full trace for one thread, approx for the other (faster)\n"
"       none:   only show trace for one thread in a race (fastest)\n"
"    --conflict-cache-size=N   size of 'full' history cache [2000000]\n"
"    --check-stack-refs=no|yes race-check reads and writes on the\n"
"                              main stack and thread stacks? [yes]\n"
   );
}

static void hg_print_debug_usage ( void )
{
   VG_(printf)("    --cmp-race-err-addrs=no|yes  are data addresses in "
               "race errors significant? [no]\n");
   VG_(printf)("    --hg-sanity-flags=<XXXXXX>   sanity check "
               "  at events (X = 0|1) [000000]\n");
   VG_(printf)("    --hg-sanity-flags values:\n");
   VG_(printf)("       010000   after changes to "
               "lock-order-acquisition-graph\n");
   VG_(printf)("       001000   at memory accesses (NB: not currently used)\n");
   VG_(printf)("       000100   at mem permission setting for "
               "ranges >= %d bytes\n", SCE_BIGRANGE_T);
   VG_(printf)("       000010   at lock/unlock events\n");
   VG_(printf)("       000001   at thread create/join events\n");
   VG_(printf)(
"    --vts-pruning=never|auto|always [auto]\n"
"       never:   is never done (may cause big space leaks in Helgrind)\n"
"       auto:    done just often enough to keep space usage under control\n"
"       always:  done after every VTS GC (mostly just a big time waster)\n"
    );
}

static void hg_print_stats (void)
{

   if (1) {
      VG_(printf)("\n");
      HG_(ppWSUstats)( univ_lsets, "univ_lsets" );
      if (HG_(clo_track_lockorders)) {
         VG_(printf)("\n");
         HG_(ppWSUstats)( univ_laog,  "univ_laog" );
      }
   }

   
   
   
   
   
   
   
   
   
   
   

   VG_(printf)("\n");
   VG_(printf)("        locksets: %'8d unique lock sets\n",
               (Int)HG_(cardinalityWSU)( univ_lsets ));
   if (HG_(clo_track_lockorders)) {
      VG_(printf)("       univ_laog: %'8d unique lock sets\n",
                  (Int)HG_(cardinalityWSU)( univ_laog ));
   }

   
   
   

   VG_(printf)("  LockN-to-P map: %'8llu queries (%llu map size)\n",
               HG_(stats__LockN_to_P_queries),
               HG_(stats__LockN_to_P_get_map_size)() );

   VG_(printf)("client malloc-ed blocks: %'8d\n",
               VG_(HT_count_nodes)(hg_mallocmeta_table));
               
   VG_(printf)("string table map: %'8llu queries (%llu map size)\n",
               HG_(stats__string_table_queries),
               HG_(stats__string_table_get_map_size)() );
   if (HG_(clo_track_lockorders)) {
      VG_(printf)("            LAOG: %'8d map size\n",
                  (Int)(laog ? VG_(sizeFM)( laog ) : 0));
      VG_(printf)(" LAOG exposition: %'8d map size\n",
                  (Int)(laog_exposition ? VG_(sizeFM)( laog_exposition ) : 0));
   }

   VG_(printf)("           locks: %'8lu acquires, "
               "%'lu releases\n",
               stats__lockN_acquires,
               stats__lockN_releases
              );
   VG_(printf)("   sanity checks: %'8lu\n", stats__sanity_checks);

   VG_(printf)("\n");
   libhb_shutdown(True); 
}

static void hg_fini ( Int exitcode )
{
   if (VG_(clo_verbosity) == 1 && !VG_(clo_xml)) {
      VG_(message)(Vg_UserMsg, 
                   "For counts of detected and suppressed errors, "
                   "rerun with: -v\n");
   }

   if (VG_(clo_verbosity) == 1 && !VG_(clo_xml)
       && HG_(clo_history_level) >= 2) {
      VG_(umsg)( 
         "Use --history-level=approx or =none to gain increased speed, at\n" );
      VG_(umsg)(
         "the cost of reduced accuracy of conflicting-access information\n");
   }

   if (SHOW_DATA_STRUCTURES)
      pp_everything( PP_ALL, "SK_(fini)" );
   if (HG_(clo_sanity_flags))
      all__sanity_check("SK_(fini)");

   if (VG_(clo_stats))
      hg_print_stats();
}


static
void for_libhb__get_stacktrace ( Thr* hbt, Addr* frames, UWord nRequest )
{
   Thread*     thr;
   ThreadId    tid;
   UWord       nActual;
   tl_assert(hbt);
   thr = libhb_get_Thr_hgthread( hbt );
   tl_assert(thr);
   tid = map_threads_maybe_reverse_lookup_SLOW(thr);
   nActual = (UWord)VG_(get_StackTrace)( tid, frames, (UInt)nRequest,
                                         NULL, NULL, 0 );
   tl_assert(nActual <= nRequest);
   for (; nActual < nRequest; nActual++)
      frames[nActual] = 0;
}

static
ExeContext* for_libhb__get_EC ( Thr* hbt )
{
   Thread*     thr;
   ThreadId    tid;
   ExeContext* ec;
   tl_assert(hbt);
   thr = libhb_get_Thr_hgthread( hbt );
   tl_assert(thr);
   tid = map_threads_maybe_reverse_lookup_SLOW(thr);
   
   ec = VG_(record_ExeContext)( tid, 0 );
   return ec;
}


static void hg_post_clo_init ( void )
{
   Thr* hbthr_root;

   
   hbthr_root = libhb_init( for_libhb__get_stacktrace, 
                            for_libhb__get_EC );
   


   if (HG_(clo_track_lockorders))
      laog__init();

   initialise_data_structures(hbthr_root);
}

static void hg_info_location (Addr a)
{
   (void) HG_(get_and_pp_addrdescr) (a);
}

static void hg_pre_clo_init ( void )
{
   VG_(details_name)            ("Helgrind");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("a thread error detector");
   VG_(details_copyright_author)(
      "Copyright (C) 2007-2013, and GNU GPL'd, by OpenWorks LLP et al.");
   VG_(details_bug_reports_to)  (VG_BUGS_TO);
   VG_(details_avg_translation_sizeB) ( 320 );

   VG_(basic_tool_funcs)          (hg_post_clo_init,
                                   hg_instrument,
                                   hg_fini);

   VG_(needs_core_errors)         ();
   VG_(needs_tool_errors)         (HG_(eq_Error),
                                   HG_(before_pp_Error),
                                   HG_(pp_Error),
                                   False,
                                   HG_(update_extra),
                                   HG_(recognised_suppression),
                                   HG_(read_extra_suppression_info),
                                   HG_(error_matches_suppression),
                                   HG_(get_error_name),
                                   HG_(get_extra_suppression_info),
                                   HG_(print_extra_suppression_use),
                                   HG_(update_extra_suppression_use));

   VG_(needs_xml_output)          ();

   VG_(needs_command_line_options)(hg_process_cmd_line_option,
                                   hg_print_usage,
                                   hg_print_debug_usage);
   VG_(needs_client_requests)     (hg_handle_client_request);

   
   
   

   VG_(needs_print_stats) (hg_print_stats);
   VG_(needs_info_location) (hg_info_location);

   VG_(needs_malloc_replacement)  (hg_cli__malloc,
                                   hg_cli____builtin_new,
                                   hg_cli____builtin_vec_new,
                                   hg_cli__memalign,
                                   hg_cli__calloc,
                                   hg_cli__free,
                                   hg_cli____builtin_delete,
                                   hg_cli____builtin_vec_delete,
                                   hg_cli__realloc,
                                   hg_cli_malloc_usable_size,
                                   HG_CLI__DEFAULT_MALLOC_REDZONE_SZB );

   if (0) VG_(needs_var_info)(); 

   VG_(track_new_mem_startup)     ( evh__new_mem_w_perms );
   VG_(track_new_mem_stack_signal)( evh__new_mem_w_tid );
   VG_(track_new_mem_brk)         ( evh__new_mem_w_tid );
   VG_(track_new_mem_mmap)        ( evh__new_mem_w_perms );
   VG_(track_new_mem_stack)       ( evh__new_mem_stack );

   
   VG_(track_copy_mem_remap)      ( evh__copy_mem );

   VG_(track_change_mem_mprotect) ( evh__set_perms );

   VG_(track_die_mem_stack_signal)( evh__die_mem );
   VG_(track_die_mem_brk)         ( evh__die_mem_munmap );
   VG_(track_die_mem_munmap)      ( evh__die_mem_munmap );

      
   

   
   VG_(track_ban_mem_stack)       (NULL);

   VG_(track_pre_mem_read)        ( evh__pre_mem_read );
   VG_(track_pre_mem_read_asciiz) ( evh__pre_mem_read_asciiz );
   VG_(track_pre_mem_write)       ( evh__pre_mem_write );
   VG_(track_post_mem_write)      (NULL);

   

   VG_(track_pre_thread_ll_create)( evh__pre_thread_ll_create );
   VG_(track_pre_thread_ll_exit)  ( evh__pre_thread_ll_exit );

   VG_(track_start_client_code)( evh__start_client_code );
   VG_(track_stop_client_code)( evh__stop_client_code );

   tl_assert( sizeof(void*) == sizeof(struct _MallocMeta*) );
   tl_assert( sizeof(UWord) == sizeof(Addr) );
   hg_mallocmeta_table
      = VG_(HT_construct)( "hg_malloc_metadata_table" );

   MallocMeta_poolalloc = VG_(newPA) ( sizeof(MallocMeta),
                                       1000,
                                       HG_(zalloc),
                                       "hg_malloc_metadata_pool",
                                       HG_(free));

   
   VG_(atfork)(NULL, NULL, evh__atfork_child);
}

VG_DETERMINE_INTERFACE_VERSION(hg_pre_clo_init)

