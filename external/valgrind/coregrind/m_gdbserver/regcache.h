/* Register support routines for the remote server for GDB.
   Copyright (C) 2001, 2002, 2012 Free Software Foundation, Inc.

   This file is part of GDB.
   It has been modified to integrate it in valgrind

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#ifndef REGCACHE_H
#define REGCACHE_H

#include "pub_core_basics.h"    

struct inferior_list_entry;


void *new_register_cache (void);


void free_register_cache (void *regcache);


void regcache_invalidate_one (struct inferior_list_entry *);
void regcache_invalidate (void);


void registers_to_string (char *buf);


void registers_from_string (const char *buf);


int registers_length (void);


struct reg *find_register_by_number (int n);

int register_size (int n);

int find_regno (const char *name);

extern const char **gdbserver_expedite_regs;

void supply_register (int n, const void *buf, Bool *mod);

void supply_register_from_string (int n, const char *buf, Bool *mod);

void supply_register_by_name (const char *name, const void *buf, Bool *mod);

void collect_register (int n, void *buf);

void collect_register_as_string (int n, char *buf);

void collect_register_by_name (const char *name, void *buf);

#endif 
