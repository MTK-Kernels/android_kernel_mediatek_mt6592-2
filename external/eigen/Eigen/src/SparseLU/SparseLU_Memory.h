// Copyright (C) 2012 Désiré Nuentsa-Wakam <desire.nuentsa_wakam@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

/* 
 
 * NOTE: This file is the modified version of [s,d,c,z]memory.c files in SuperLU 
 
 * -- SuperLU routine (version 3.1) --
 * Univ. of California Berkeley, Xerox Palo Alto Research Center,
 * and Lawrence Berkeley National Lab.
 * August 1, 2008
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

#ifndef EIGEN_SPARSELU_MEMORY
#define EIGEN_SPARSELU_MEMORY

namespace Eigen {
namespace internal {
  
enum { LUNoMarker = 3 };
enum {emptyIdxLU = -1};
template<typename Index>
inline Index LUnumTempV(Index& m, Index& w, Index& t, Index& b)
{
  return (std::max)(m, (t+b)*w);
}

template< typename Scalar, typename Index>
inline Index LUTempSpace(Index&m, Index& w)
{
  return (2*w + 4 + LUNoMarker) * m * sizeof(Index) + (w + 1) * m * sizeof(Scalar);
}




template <typename Scalar, typename Index>
template <typename VectorType>
Index  SparseLUImpl<Scalar,Index>::expand(VectorType& vec, Index& length, Index nbElts, Index keep_prev, Index& num_expansions) 
{
  
  float alpha = 1.5; 
  Index new_len; 
  
  if(num_expansions == 0 || keep_prev) 
    new_len = length ; 
  else 
    new_len = (std::max)(length+1,Index(alpha * length));
  
  VectorType old_vec; 
  if (nbElts > 0 )
    old_vec = vec.segment(0,nbElts); 
  
  
#ifdef EIGEN_EXCEPTIONS
  try
#endif
  {
    vec.resize(new_len); 
  }
#ifdef EIGEN_EXCEPTIONS
  catch(std::bad_alloc& )
#else
  if(!vec.size())
#endif
  {
    if (!num_expansions)
    {
      
      
      return -1;
    }
    if (keep_prev)
    {
      
      return new_len;
    }
    else 
    {
      
      Index tries = 0; 
      do 
      {
        alpha = (alpha + 1)/2;
        new_len = (std::max)(length+1,Index(alpha * length));
#ifdef EIGEN_EXCEPTIONS
        try
#endif
        {
          vec.resize(new_len); 
        }
#ifdef EIGEN_EXCEPTIONS
        catch(std::bad_alloc& )
#else
        if (!vec.size())
#endif
        {
          tries += 1; 
          if ( tries > 10) return new_len; 
        }
      } while (!vec.size());
    }
  }
  
  if (nbElts > 0)
    vec.segment(0, nbElts) = old_vec;   
   
  
  length  = new_len;
  if(num_expansions) ++num_expansions;
  return 0; 
}

template <typename Scalar, typename Index>
Index SparseLUImpl<Scalar,Index>::memInit(Index m, Index n, Index annz, Index lwork, Index fillratio, Index panel_size,  GlobalLU_t& glu)
{
  Index& num_expansions = glu.num_expansions; 
  num_expansions = 0;
  glu.nzumax = glu.nzlumax = (std::min)(fillratio * annz / n, m) * n; 
  glu.nzlmax = (std::max)(Index(4), fillratio) * annz / 4; 
  
  Index tempSpace;
  tempSpace = (2*panel_size + 4 + LUNoMarker) * m * sizeof(Index) + (panel_size + 1) * m * sizeof(Scalar);
  if (lwork == emptyIdxLU) 
  {
    Index estimated_size;
    estimated_size = (5 * n + 5) * sizeof(Index)  + tempSpace
                    + (glu.nzlmax + glu.nzumax) * sizeof(Index) + (glu.nzlumax+glu.nzumax) *  sizeof(Scalar) + n; 
    return estimated_size;
  }
  
  
  
  
  glu.xsup.resize(n+1);
  glu.supno.resize(n+1);
  glu.xlsub.resize(n+1);
  glu.xlusup.resize(n+1);
  glu.xusub.resize(n+1);

  
  do 
  {
    if(     (expand<ScalarVector>(glu.lusup, glu.nzlumax, 0, 0, num_expansions)<0)
        ||  (expand<ScalarVector>(glu.ucol,  glu.nzumax,  0, 0, num_expansions)<0)
        ||  (expand<IndexVector> (glu.lsub,  glu.nzlmax,  0, 0, num_expansions)<0)
        ||  (expand<IndexVector> (glu.usub,  glu.nzumax,  0, 1, num_expansions)<0) )
    {
      
      glu.nzlumax /= 2;
      glu.nzumax /= 2;
      glu.nzlmax /= 2;
      if (glu.nzlumax < annz ) return glu.nzlumax; 
    }
  } while (!glu.lusup.size() || !glu.ucol.size() || !glu.lsub.size() || !glu.usub.size());
  
  ++num_expansions;
  return 0;
  
} 

template <typename Scalar, typename Index>
template <typename VectorType>
Index SparseLUImpl<Scalar,Index>::memXpand(VectorType& vec, Index& maxlen, Index nbElts, MemType memtype, Index& num_expansions)
{
  Index failed_size; 
  if (memtype == USUB)
     failed_size = this->expand<VectorType>(vec, maxlen, nbElts, 1, num_expansions);
  else
    failed_size = this->expand<VectorType>(vec, maxlen, nbElts, 0, num_expansions);

  if (failed_size)
    return failed_size; 
  
  return 0 ;  
}

} 

} 
#endif 
