

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

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

#include "pub_core_basics.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcbase.h"    
#include "pub_core_seqmatch.h"    


Bool VG_(generic_match) ( 
        Bool matchAll,
        const void* patt,  SizeT szbPatt,  UWord nPatt,  UWord ixPatt,
        const void* input, SizeT szbInput, UWord nInput, UWord ixInput,
        Bool (*pIsStar)(const void*),
        Bool (*pIsQuery)(const void*),
        Bool (*pattEQinp)(const void*,const void*,void*,UWord),
        void* inputCompleter, Bool (*haveInputInpC)(void*,UWord)
     )
{
   /* This is the spec, written in my favourite formal specification
      language.  It specifies non-greedy matching of '*'s.

      ma ('*':ps) (i:is) = ma ps (i:is) || ma ('*':ps) is
      ma ('*':ps) []     = ma ps []

      ma ('?':ps) (i:is) = ma ps is
      ma ('?':ps) []     = False

      ma (p:ps)   (i:is) = p == i && ma ps is

      ma (p:ps)   []     = False
      ma []       (i:is) = False -- m-all, True for m-prefix
      ma []       []     = True
   */
   Bool  havePatt, haveInput;
   const HChar *currPatt, *currInput;
  tailcall:
   vg_assert(nPatt >= 0 && nPatt  < 1000000); 
   vg_assert(inputCompleter
             || (nInput >= 0  && nInput < 1000000)); 
   vg_assert(ixPatt >= 0  && ixPatt <= nPatt);
   vg_assert(ixInput >= 0 && (inputCompleter || ixInput <= nInput));

   havePatt  = ixPatt < nPatt;
   haveInput = inputCompleter ? 
      (*haveInputInpC)(inputCompleter, ixInput)
      : ixInput < nInput;

   currPatt  = havePatt  ? ((const HChar*)patt) + szbPatt * ixPatt    : NULL;
   currInput = haveInput && !inputCompleter ? 
      ((const HChar*)input) + szbInput * ixInput : NULL;

   
   
   
   
   
   
   
   
   
   
   
   
   
   
   if (havePatt && pIsStar(currPatt)) {
      if (haveInput) {
         
         
         
         if (VG_(generic_match)( matchAll,
                                 patt, szbPatt, nPatt,  ixPatt+1,
                                 input,szbInput,nInput, ixInput+0,
                                 pIsStar,pIsQuery,pattEQinp,
                                 inputCompleter,haveInputInpC) ) {
            return True;
         }
         
         ixInput++; goto tailcall;
      } else {
         
         ixPatt++; goto tailcall;
      }
   }

   
   
   
   
   if (havePatt && pIsQuery(currPatt)) {
      if (haveInput) {
         ixPatt++; ixInput++; goto tailcall;
      } else {
         return False;
      }
   }

   
   
   
   if (havePatt && haveInput) {
      if (!pattEQinp(currPatt,currInput,inputCompleter,ixInput)) return False;
      ixPatt++; ixInput++; goto tailcall;
   }

   
   
   if (havePatt && !haveInput) return False;

   
   
   
   
   
   
   
   if (!havePatt && haveInput) {
      return matchAll ? False 
                      : True; 
   }

   
   
   if (!havePatt && !haveInput) return True;

   
   vg_assert(0);
}


static Bool charIsStar  ( const void* pV ) { return *(const HChar*)pV == '*'; }
static Bool charIsQuery ( const void* pV ) { return *(const HChar*)pV == '?'; }
static Bool char_p_EQ_i ( const void* pV, const void* cV,
                          void* null_completer, UWord ixcV ) {
   HChar p = *(const HChar*)pV;
   HChar c = *(const HChar*)cV;
   vg_assert(p != '*' && p != '?');
   return p == c;
}
Bool VG_(string_match) ( const HChar* patt, const HChar* input )
{
   return VG_(generic_match)(
             True,
             patt,  sizeof(HChar), VG_(strlen)(patt), 0,
             input, sizeof(HChar), VG_(strlen)(input), 0,
             charIsStar, charIsQuery, char_p_EQ_i,
             NULL, NULL
          );
}



