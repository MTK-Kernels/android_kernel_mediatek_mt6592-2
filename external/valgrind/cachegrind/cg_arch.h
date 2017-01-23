

/*
   This file is part of Cachegrind, a Valgrind tool for cache
   profiling programs.

   Copyright (C) 2002-2013 Nicholas Nethercote
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

#ifndef __CG_ARCH_H
#define __CG_ARCH_H

typedef struct {
   Int size;       
   Int assoc;
   Int line_size;  
} cache_t;

#define MIN_LINE_SIZE         16

#define UNDEFINED_CACHE     { -1, -1, -1 }

Bool VG_(str_clo_cache_opt)(const HChar *arg,
                            cache_t* clo_I1c,
                            cache_t* clo_D1c,
                            cache_t* clo_LLc);

void VG_(post_clo_init_configure_caches)(cache_t* I1c,
                                         cache_t* D1c,
                                         cache_t* LLc,
                                         cache_t* clo_I1c,
                                         cache_t* clo_D1c,
                                         cache_t* clo_LLc);

void VG_(print_cache_clo_opts)(void);

#endif   

