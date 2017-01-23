/* Internal demangler interface for g++ V3 ABI.
   Copyright (C) 2003, 2004, 2005, 2006, 2007, 2010
   Free Software Foundation, Inc.
   Written by Ian Lance Taylor <ian@wasabisystems.com>.

   This file is part of the libiberty library, which is part of GCC.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   In addition to the permissions in the GNU General Public License, the
   Free Software Foundation gives you unlimited permission to link the
   compiled version of this file into combinations with other programs,
   and to distribute those combinations without any restriction coming
   from the use of this file.  (The General Public License restrictions
   do apply in other respects; for example, they cover modification of
   the file, and distribution when not linked into a combined
   executable.)

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA. 
*/



struct demangle_operator_info
{
  
  const char *code;
  
  const char *name;
  
  int len;
  
  int args;
};


enum d_builtin_type_print
{
  
  D_PRINT_DEFAULT,
  
  D_PRINT_INT,
  
  D_PRINT_UNSIGNED,
  
  D_PRINT_LONG,
  
  D_PRINT_UNSIGNED_LONG,
  
  D_PRINT_LONG_LONG,
  
  D_PRINT_UNSIGNED_LONG_LONG,
  
  D_PRINT_BOOL,
  
  D_PRINT_FLOAT,
  
  D_PRINT_VOID
};


struct demangle_builtin_type_info
{
  
  const char *name;
  
  int len;
  
  const char *java_name;
  
  int java_len;
  
  enum d_builtin_type_print print;
};


struct d_info
{
  
  const char *s;
  
  const char *send;
  
  int options;
  
  const char *n;
  
  struct demangle_component *comps;
  
  int next_comp;
  
  int num_comps;
  
  struct demangle_component **subs;
  
  int next_sub;
  
  int num_subs;
  int did_subs;
  
  struct demangle_component *last_name;
  int expansion;
  
  int is_expression;
  int is_conversion;
};

#define d_peek_char(di) (*((di)->n))
#define d_peek_next_char(di) ((di)->n[1])
#define d_advance(di, i) ((di)->n += (i))
#define d_check_char(di, c) (d_peek_char(di) == c ? ((di)->n++, 1) : 0)
#define d_next_char(di) (d_peek_char(di) == '\0' ? '\0' : *((di)->n++))
#define d_str(di) ((di)->n)

#ifdef IN_GLIBCPP_V3
#define CP_STATIC_IF_GLIBCPP_V3 static
#else
#define CP_STATIC_IF_GLIBCPP_V3 extern
#endif

#ifndef IN_GLIBCPP_V3
extern const struct demangle_operator_info cplus_demangle_operators[];
#endif

#define D_BUILTIN_TYPE_COUNT (33)

CP_STATIC_IF_GLIBCPP_V3
const struct demangle_builtin_type_info
cplus_demangle_builtin_types[D_BUILTIN_TYPE_COUNT];

CP_STATIC_IF_GLIBCPP_V3
struct demangle_component *
cplus_demangle_mangled_name (struct d_info *, int);

CP_STATIC_IF_GLIBCPP_V3
struct demangle_component *
cplus_demangle_type (struct d_info *);

extern void
cplus_demangle_init_info (const char *, int, size_t, struct d_info *);

#undef CP_STATIC_IF_GLIBCPP_V3
