// Copyright (C) 2012 Désiré Nuentsa-Wakam <desire.nuentsa_wakam@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

/* 
 
 * NOTE: This file is the modified version of [s,d,c,z]pruneL.c file in SuperLU 
 
 * -- SuperLU routine (version 2.0) --
 * Univ. of California Berkeley, Xerox Palo Alto Research Center,
 * and Lawrence Berkeley National Lab.
 * November 15, 1997
 *
 * Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY
 * EXPRESSED OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program for any
 * purpose, provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is
 * granted, provided the above notices are retained, and a notice that
 * the code was modified is included with the above copyright notice.
 */
#ifndef SPARSELU_PRUNEL_H
#define SPARSELU_PRUNEL_H

namespace Eigen {
namespace internal {

template <typename Scalar, typename Index>
void SparseLUImpl<Scalar,Index>::pruneL(const Index jcol, const IndexVector& perm_r, const Index pivrow, const Index nseg, const IndexVector& segrep, BlockIndexVector repfnz, IndexVector& xprune, GlobalLU_t& glu)
{
  
  Index jsupno = glu.supno(jcol); 
  Index i,irep,irep1; 
  bool movnum, do_prune = false; 
  Index kmin = 0, kmax = 0, minloc, maxloc,krow; 
  for (i = 0; i < nseg; i++)
  {
    irep = segrep(i); 
    irep1 = irep + 1; 
    do_prune = false; 
    
    
    if (repfnz(irep) == emptyIdxLU) continue; 
    
    
    
    
    if (glu.supno(irep) == glu.supno(irep1) ) continue; 
    
    
    if (glu.supno(irep) != jsupno )
    {
      if ( xprune (irep) >= glu.xlsub(irep1) )
      {
        kmin = glu.xlsub(irep);
        kmax = glu.xlsub(irep1) - 1; 
        for (krow = kmin; krow <= kmax; krow++)
        {
          if (glu.lsub(krow) == pivrow) 
          {
            do_prune = true; 
            break; 
          }
        }
      }
      
      if (do_prune) 
      {
        
        
        movnum = false; 
        if (irep == glu.xsup(glu.supno(irep)) ) 
          movnum = true; 
        
        while (kmin <= kmax)
        {
          if (perm_r(glu.lsub(kmax)) == emptyIdxLU)
            kmax--; 
          else if ( perm_r(glu.lsub(kmin)) != emptyIdxLU)
            kmin++;
          else 
          {
            
            
            std::swap(glu.lsub(kmin), glu.lsub(kmax)); 
            
            
            
            
            
            if (movnum) 
            {
              minloc = glu.xlusup(irep) + ( kmin - glu.xlsub(irep) ); 
              maxloc = glu.xlusup(irep) + ( kmax - glu.xlsub(irep) ); 
              std::swap(glu.lusup(minloc), glu.lusup(maxloc)); 
            }
            kmin++;
            kmax--;
          }
        } 
        
        xprune(irep) = kmin;  
      } 
    } 
  } 
}

} 
} 

#endif 
