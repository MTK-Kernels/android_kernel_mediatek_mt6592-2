

/*
   This file is part of Helgrind, a Valgrind tool for detecting errors
   in threaded programs.

   Copyright (C) 2007-2013 OpenWorks Ltd
      info@open-works.co.uk

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

#ifndef __HG_BASICS_H
#define __HG_BASICS_H



#define HG_(str) VGAPPEND(vgHelgrind_,str)

void*  HG_(zalloc) ( const HChar* cc, SizeT n );
void   HG_(free)   ( void* p );
HChar* HG_(strdup) ( const HChar* cc, const HChar* s );

static inline Bool HG_(is_sane_ThreadId) ( ThreadId coretid ) {
   return coretid >= 0 && coretid < VG_N_THREADS;
}



#define SCE_THREADS  (1<<0)  
#define SCE_LOCKS    (1<<1)  
#define SCE_BIGRANGE (1<<2)  
#define SCE_ACCESS   (1<<3)  
#define SCE_LAOG     (1<<4)  

#define SCE_BIGRANGE_T 256  


extern Bool HG_(clo_track_lockorders);

extern Bool HG_(clo_cmp_race_err_addrs);

extern UWord HG_(clo_history_level);

extern UWord HG_(clo_conflict_cache_size);

extern Word HG_(clo_sanity_flags);

/* Treat heap frees as if the memory was written immediately prior to
   the free.  This shakes out races in which memory is referenced by
   one thread, and freed by another, and there's no observable
   synchronisation event to guarantee that the reference happens
   before the free. */
extern Bool HG_(clo_free_is_write);

extern UWord HG_(clo_vts_pruning);

extern Bool HG_(clo_check_stack_refs); 

#endif 

