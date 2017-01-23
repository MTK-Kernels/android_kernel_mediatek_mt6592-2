

/*
   This file is part of LibHB, a library for implementing and checking
   the happens-before relationship in concurrent programs.

   Copyright (C) 2008-2013 OpenWorks Ltd
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

#ifndef __LIBHB_H
#define __LIBHB_H

 

 

Thr* libhb_init (
        void        (*get_stacktrace)( Thr*, Addr*, UWord ),
        ExeContext* (*get_EC)( Thr* )
     );

void libhb_shutdown ( Bool show_stats );

Thr* libhb_create ( Thr* parent );

void libhb_async_exit      ( Thr* exitter );
void libhb_joinedwith_done ( Thr* exitter );


SO* libhb_so_alloc ( void );

void libhb_so_dealloc ( SO* so );

void libhb_so_send ( Thr* thr, SO* so, Bool strong_send );

void libhb_so_recv ( Thr* thr, SO* so, Bool strong_recv );

Bool libhb_so_everSent ( SO* so );

#define LIBHB_CWRITE_1(_thr,_a)    zsm_sapply08_f__msmcwrite((_thr),(_a))
#define LIBHB_CWRITE_2(_thr,_a)    zsm_sapply16_f__msmcwrite((_thr),(_a))
#define LIBHB_CWRITE_4(_thr,_a)    zsm_sapply32_f__msmcwrite((_thr),(_a))
#define LIBHB_CWRITE_8(_thr,_a)    zsm_sapply64_f__msmcwrite((_thr),(_a))
#define LIBHB_CWRITE_N(_thr,_a,_n) zsm_sapplyNN_f__msmcwrite((_thr),(_a),(_n))

#define LIBHB_CREAD_1(_thr,_a)    zsm_sapply08_f__msmcread((_thr),(_a))
#define LIBHB_CREAD_2(_thr,_a)    zsm_sapply16_f__msmcread((_thr),(_a))
#define LIBHB_CREAD_4(_thr,_a)    zsm_sapply32_f__msmcread((_thr),(_a))
#define LIBHB_CREAD_8(_thr,_a)    zsm_sapply64_f__msmcread((_thr),(_a))
#define LIBHB_CREAD_N(_thr,_a,_n) zsm_sapplyNN_f__msmcread((_thr),(_a),(_n))

void zsm_sapply08_f__msmcwrite ( Thr* thr, Addr a );
void zsm_sapply16_f__msmcwrite ( Thr* thr, Addr a );
void zsm_sapply32_f__msmcwrite ( Thr* thr, Addr a );
void zsm_sapply64_f__msmcwrite ( Thr* thr, Addr a );
void zsm_sapplyNN_f__msmcwrite ( Thr* thr, Addr a, SizeT len );

void zsm_sapply08_f__msmcread ( Thr* thr, Addr a );
void zsm_sapply16_f__msmcread ( Thr* thr, Addr a );
void zsm_sapply32_f__msmcread ( Thr* thr, Addr a );
void zsm_sapply64_f__msmcread ( Thr* thr, Addr a );
void zsm_sapplyNN_f__msmcread ( Thr* thr, Addr a, SizeT len );

void libhb_Thr_resumes ( Thr* thr );

void libhb_srange_new      ( Thr*, Addr, SizeT );
void libhb_srange_untrack  ( Thr*, Addr, SizeT );
void libhb_srange_noaccess_NoFX ( Thr*, Addr, SizeT ); 
void libhb_srange_noaccess_AHAE ( Thr*, Addr, SizeT ); 

UWord libhb_srange_get_abits (Addr a, UChar *abits, SizeT len);

Thread* libhb_get_Thr_hgthread ( Thr* );
void    libhb_set_Thr_hgthread ( Thr*, Thread* );

void libhb_copy_shadow_state ( Thr* thr, Addr src, Addr dst, SizeT len );

void libhb_maybe_GC ( void );

Bool libhb_event_map_lookup ( ExeContext** resEC,
                              Thr**        resThr,
                              SizeT*       resSzB,
                              Bool*        resIsW,
                              WordSetID*   locksHeldW,
                              Thr* thr, Addr a, SizeT szB, Bool isW );

typedef void (*Access_t) (StackTrace ips, UInt n_ips,
                          Thr*  Thr_a,
                          Addr  ga,
                          SizeT SzB,
                          Bool  isW,
                          WordSetID locksHeldW );
void libhb_event_map_access_history ( Addr a, SizeT szB, Access_t fn );


WordSetU* HG_(get_univ_lsets) ( void );

Lock* HG_(get_admin_locks) ( void );

#endif 

