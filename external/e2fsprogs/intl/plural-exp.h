/* Expression parsing and evaluation for plural form selection.
   Copyright (C) 2000-2003 Free Software Foundation, Inc.
   Written by Ulrich Drepper <drepper@cygnus.com>, 2000.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU Library General Public License as published
   by the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.  */

#ifndef _PLURAL_EXP_H
#define _PLURAL_EXP_H

#ifndef internal_function
# define internal_function
#endif

#ifndef attribute_hidden
# define attribute_hidden
#endif


struct expression
{
  int nargs;			
  enum operator
  {
    
    var,			
    num,			
    
    lnot,			
    
    mult,			
    divide,			
    module,			
    plus,			
    minus,			
    less_than,			
    greater_than,		
    less_or_equal,		
    greater_or_equal,		
    equal,			
    not_equal,			
    land,			
    lor,			
    
    qmop			
  } operation;
  union
  {
    unsigned long int num;	
    struct expression *args[3];	
  } val;
};

struct parse_args
{
  const char *cp;
  struct expression *res;
};


#ifdef _LIBC
# define FREE_EXPRESSION __gettext_free_exp
# define PLURAL_PARSE __gettextparse
# define GERMANIC_PLURAL __gettext_germanic_plural
# define EXTRACT_PLURAL_EXPRESSION __gettext_extract_plural
#elif defined (IN_LIBINTL)
# define FREE_EXPRESSION libintl_gettext_free_exp
# define PLURAL_PARSE libintl_gettextparse
# define GERMANIC_PLURAL libintl_gettext_germanic_plural
# define EXTRACT_PLURAL_EXPRESSION libintl_gettext_extract_plural
#else
# define FREE_EXPRESSION free_plural_expression
# define PLURAL_PARSE parse_plural_expression
# define GERMANIC_PLURAL germanic_plural
# define EXTRACT_PLURAL_EXPRESSION extract_plural_expression
#endif

extern void FREE_EXPRESSION (struct expression *exp)
     internal_function;
extern int PLURAL_PARSE (void *arg);
extern struct expression GERMANIC_PLURAL attribute_hidden;
extern void EXTRACT_PLURAL_EXPRESSION (const char *nullentry,
				       struct expression **pluralp,
				       unsigned long int *npluralsp)
     internal_function;

#if !defined (_LIBC) && !defined (IN_LIBINTL)
extern unsigned long int plural_eval (struct expression *pexp,
				      unsigned long int n);
#endif

#endif 
