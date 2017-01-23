

/*
   This file is part of Helgrind, a Valgrind tool for detecting errors
   in threaded programs.

   Copyright (C) 2007-2013 OpenWorks LLP
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

#ifndef __HG_WORDSET_H
#define __HG_WORDSET_H


typedef  struct _WordSetU  WordSetU;  

typedef  UInt              WordSet;   

WordSetU* HG_(newWordSetU) ( void* (*alloc_nofail)( const HChar*, SizeT ),
                             const HChar* cc,
                             void  (*dealloc)(void*),
                             Word  cacheSize );

void HG_(deleteWordSetU) ( WordSetU* );

UWord HG_(cardinalityWSU) ( WordSetU* );

void HG_(ppWSUstats) ( WordSetU* wsu, const HChar* name );



WordSet HG_(emptyWS)        ( WordSetU* );
WordSet HG_(addToWS)        ( WordSetU*, WordSet, UWord );
WordSet HG_(delFromWS)      ( WordSetU*, WordSet, UWord );
WordSet HG_(unionWS)        ( WordSetU*, WordSet, WordSet );
WordSet HG_(intersectWS)    ( WordSetU*, WordSet, WordSet );
WordSet HG_(minusWS)        ( WordSetU*, WordSet, WordSet );
Bool    HG_(isEmptyWS)      ( WordSetU*, WordSet );
Bool    HG_(isSingletonWS)  ( WordSetU*, WordSet, UWord );
UWord   HG_(anyElementOfWS) ( WordSetU*, WordSet );
UWord   HG_(cardinalityWS)  ( WordSetU*, WordSet );
Bool    HG_(elemWS)         ( WordSetU*, WordSet, UWord );
WordSet HG_(doubletonWS)    ( WordSetU*, UWord, UWord );
WordSet HG_(singletonWS)    ( WordSetU*, UWord );
WordSet HG_(isSubsetOf)     ( WordSetU*, WordSet, WordSet );

Bool    HG_(plausibleWS)    ( WordSetU*, WordSet );


Bool    HG_(saneWS_SLOW)    ( WordSetU*, WordSet );

void    HG_(ppWS)           ( WordSetU*, WordSet );

void    HG_(getPayloadWS)   ( UWord** words, UWord* nWords, 
                             WordSetU*, WordSet );

void    HG_(dieWS)          ( WordSetU*, WordSet );




#endif 

