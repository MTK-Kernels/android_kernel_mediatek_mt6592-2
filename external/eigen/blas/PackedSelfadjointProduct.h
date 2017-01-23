// Copyright (C) 2012 Chen-Pang He <jdh8@ms63.hinet.net>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_SELFADJOINT_PACKED_PRODUCT_H
#define EIGEN_SELFADJOINT_PACKED_PRODUCT_H

namespace internal {

template<typename Scalar, typename Index, int StorageOrder, int UpLo, bool ConjLhs, bool ConjRhs>
struct selfadjoint_packed_rank1_update;

template<typename Scalar, typename Index, int UpLo, bool ConjLhs, bool ConjRhs>
struct selfadjoint_packed_rank1_update<Scalar,Index,ColMajor,UpLo,ConjLhs,ConjRhs>
{
  typedef typename NumTraits<Scalar>::Real RealScalar;
  static void run(Index size, Scalar* mat, const Scalar* vec, RealScalar alpha)
  {
    typedef Map<const Matrix<Scalar,Dynamic,1> > OtherMap;
    typedef typename conj_expr_if<ConjLhs,OtherMap>::type ConjRhsType;
    conj_if<ConjRhs> cj;

    for (Index i=0; i<size; ++i)
    {
      Map<Matrix<Scalar,Dynamic,1> >(mat, UpLo==Lower ? size-i : (i+1)) += alpha * cj(vec[i]) * ConjRhsType(OtherMap(vec+(UpLo==Lower ? i : 0), UpLo==Lower ? size-i : (i+1)));
      
      mat[UpLo==Lower ? 0 : i] = numext::real(mat[UpLo==Lower ? 0 : i]);
      mat += UpLo==Lower ? size-i : (i+1);
    }
  }
};

template<typename Scalar, typename Index, int UpLo, bool ConjLhs, bool ConjRhs>
struct selfadjoint_packed_rank1_update<Scalar,Index,RowMajor,UpLo,ConjLhs,ConjRhs>
{
  typedef typename NumTraits<Scalar>::Real RealScalar;
  static void run(Index size, Scalar* mat, const Scalar* vec, RealScalar alpha)
  {
    selfadjoint_packed_rank1_update<Scalar,Index,ColMajor,UpLo==Lower?Upper:Lower,ConjRhs,ConjLhs>::run(size,mat,vec,alpha);
  }
};

} 

#endif 
