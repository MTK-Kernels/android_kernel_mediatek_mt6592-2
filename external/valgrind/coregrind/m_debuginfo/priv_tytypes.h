

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2008-2013 OpenWorks LLP
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

   Neither the names of the U.S. Department of Energy nor the
   University of California nor the names of its contributors may be
   used to endorse or promote products derived from this software
   without prior written permission.
*/

#ifndef __PRIV_TYTYPES_H
#define __PRIV_TYTYPES_H

#include "pub_core_basics.h"   
#include "pub_core_xarray.h"   
#include "priv_misc.h"         

typedef
   enum {
      Te_EMPTY=10, 
      Te_INDIR,    
      Te_UNKNOWN,  
      Te_Atom,     
      Te_Field,    
      Te_Bound,    
      Te_TyBase,   
      Te_TyPtr,    
      Te_TyRef,    
      Te_TyPtrMbr, 
      Te_TyRvalRef,
      Te_TyTyDef,  
      Te_TyStOrUn, 
      Te_TyEnum,   
      Te_TyArray,  
      Te_TyFn,     
      Te_TyQual,   
      Te_TyVoid    
   }
   TyEntTag;

typedef
   struct {
      UWord    cuOff;
      TyEntTag tag;
      union {
         struct {
         } EMPTY;
         struct {
            UWord indR;
         } INDIR;
         struct {
         } UNKNOWN;
         struct {
            HChar* name; 
            Bool   valueKnown; 
            Long   value;
         } Atom;
         struct {
            HChar* name;  
            UWord  typeR; 
            union {
               UChar* loc;   
               Word offset;  
            } pos;
            Word  nLoc;  
            Bool   isStruct;
         } Field;
         struct {
            Bool knownL;
            Bool knownU;
            Long boundL;
            Long boundU;
         } Bound;
         struct {
            HChar* name; 
            Int    szB;
            UChar  enc; 
         } TyBase;
         struct {
            Int   szB;
            UWord typeR;
         } TyPorR;
         struct {
            HChar* name;  
            UWord  typeR; 
         } TyTyDef;
         struct {
            HChar*  name; 
            UWord   szB;
            UWord   typeR;
            XArray*  fieldRs;
            Bool    complete;
            Bool    isStruct;
         } TyStOrUn;
         struct {
            HChar*  name; 
            Int     szB;
            XArray*  atomRs;
         } TyEnum;
         struct {
            UWord   typeR;
            XArray*  boundRs;
         } TyArray;
         struct {
         } TyFn;
         struct {
            UChar qual; 
            UWord typeR;
         } TyQual;
         struct {
            Bool isFake; 
         } TyVoid;
      } Te;
   }
   TyEnt;

Bool ML_(TyEnt__is_type)( const TyEnt* );

void ML_(pp_TyEnt)( const TyEnt* );

void ML_(pp_TyEnts)( const XArray* tyents, const HChar* who );

void ML_(pp_TyEnt_C_ishly)( const XArray*  tyents,
                            UWord cuOff );

Word ML_(TyEnt__cmp_by_cuOff_only) ( const TyEnt* te1, const TyEnt* te2 );

Word ML_(TyEnt__cmp_by_all_except_cuOff) ( const TyEnt* te1, const TyEnt* te2 );

void ML_(TyEnt__make_EMPTY) ( TyEnt* te );


MaybeULong ML_(sizeOfType)( const XArray*  tyents,
                            UWord cuOff );

XArray*  ML_(describe_type)( PtrdiffT* residual_offset,
                                      const XArray*  tyents,
                                      UWord ty_cuOff, 
                                      PtrdiffT offset );



#define N_TYENT_INDEX_CACHE 4096

typedef
   struct {
      struct { UWord cuOff0; TyEnt* ent0; 
               UWord cuOff1; TyEnt* ent1; }
         ce[N_TYENT_INDEX_CACHE];
   }
   TyEntIndexCache;

void ML_(TyEntIndexCache__invalidate) ( TyEntIndexCache* cache );

TyEnt* ML_(TyEnts__index_by_cuOff) ( const XArray*  ents,
                                     TyEntIndexCache* cache,
                                     UWord cuOff_to_find );

#endif 

