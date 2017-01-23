// Copyright (C) 2012 Désiré Nuentsa-Wakam <desire.nuentsa_wakam@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed


#ifndef EIGEN_LU_STRUCTS
#define EIGEN_LU_STRUCTS
namespace Eigen {
namespace internal {
  
typedef enum {LUSUP, UCOL, LSUB, USUB, LLVL, ULVL} MemType; 

template <typename IndexVector, typename ScalarVector>
struct LU_GlobalLU_t {
  typedef typename IndexVector::Scalar Index; 
  IndexVector xsup; 
  IndexVector supno; 
  ScalarVector  lusup; 
  IndexVector lsub; 
  IndexVector xlusup; 
  IndexVector xlsub; 
  Index   nzlmax; 
  Index   nzlumax; 
  ScalarVector  ucol; 
  IndexVector usub; 
  IndexVector xusub; 
  Index   nzumax; 
  Index   n; 
  Index   num_expansions; 
};

template <typename Index>
struct perfvalues {
  Index panel_size; 
  Index relax; 
                
                
  Index maxsuper; 
  Index rowblk; 
  Index colblk; 
  Index fillfactor; 
}; 

} 

} 
#endif 
