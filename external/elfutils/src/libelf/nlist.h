/* Interface for nlist.
   Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifndef _NLIST_H
#define _NLIST_H 1


struct nlist
{
  char *n_name;			
  long int n_value;		
  short int n_scnum;		
  unsigned short int n_type;	
  char n_sclass;		
  char n_numaux;		
};


#ifdef __cplusplus
extern "C" {
#endif

extern int nlist (__const char *__filename, struct nlist *__nl);

#ifdef __cplusplus
}
#endif

#endif  
