// Copyright (C) 2012 Chen-Pang He <jdh8@ms63.hinet.net>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_GENERAL_RANK1UPDATE_H
#define EIGEN_GENERAL_RANK1UPDATE_H

namespace internal {

template<typename Scalar, typename Index, int StorageOrder, bool ConjLhs, bool ConjRhs>
struct general_rank1_update;

template<typename Scalar, typename Index, bool ConjLhs, bool ConjRhs>
struct general_rank1_update<Scalar,Index,ColMajor,ConjLhs,ConjRhs>
{
  static void run(Index rows, Index cols, Scalar* mat, Index stride, const Scalar* u, const Scalar* v, Scalar alpha)
  {
    typedef Map<const Matrix<Scalar,Dynamic,1> > OtherMap;
    typedef typename conj_expr_if<ConjLhs,OtherMap>::type ConjRhsType;
    conj_if<ConjRhs> cj;

    for (Index i=0; i<cols; ++i)
      Map<Matrix<Scalar,Dynamic,1> >(mat+stride*i,rows) += alpha * cj(v[i]) * ConjRhsType(OtherMap(u,rows));
  }
};

template<typename Scalar, typename Index, bool ConjLhs, bool ConjRhs>
struct general_rank1_update<Scalar,Index,RowMajor,ConjLhs,ConjRhs>
{
  static void run(Index rows, Index cols, Scalar* mat, Index stride, const Scalar* u, const Scalar* v, Scalar alpha)
  {
    general_rank1_update<Scalar,Index,ColMajor,ConjRhs,ConjRhs>::run(rows,cols,mat,stride,u,v,alpha);
  }
};

} 

#endif 
