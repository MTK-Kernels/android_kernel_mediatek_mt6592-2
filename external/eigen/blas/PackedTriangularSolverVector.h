// Copyright (C) 2012 Chen-Pang He <jdh8@ms63.hinet.net>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_PACKED_TRIANGULAR_SOLVER_VECTOR_H
#define EIGEN_PACKED_TRIANGULAR_SOLVER_VECTOR_H

namespace internal {

template<typename LhsScalar, typename RhsScalar, typename Index, int Side, int Mode, bool Conjugate, int StorageOrder>
struct packed_triangular_solve_vector;

template<typename LhsScalar, typename RhsScalar, typename Index, int Mode, bool Conjugate>
struct packed_triangular_solve_vector<LhsScalar, RhsScalar, Index, OnTheLeft, Mode, Conjugate, RowMajor>
{
  enum {
    IsLower = (Mode&Lower)==Lower
  };
  static void run(Index size, const LhsScalar* lhs, RhsScalar* rhs)
  {
    internal::conj_if<Conjugate> cj;
    typedef Map<const Matrix<LhsScalar,Dynamic,1> > LhsMap;
    typedef typename conj_expr_if<Conjugate,LhsMap>::type ConjLhsType;

    lhs += IsLower ? 0 : (size*(size+1)>>1)-1;
    for(Index pi=0; pi<size; ++pi)
    {
      Index i = IsLower ? pi : size-pi-1;
      Index s = IsLower ? 0 : 1;
      if (pi>0)
	rhs[i] -= (ConjLhsType(LhsMap(lhs+s,pi))
	    .cwiseProduct(Map<const Matrix<RhsScalar,Dynamic,1> >(rhs+(IsLower ? 0 : i+1),pi))).sum();
      if (!(Mode & UnitDiag))
	rhs[i] /= cj(lhs[IsLower ? i : 0]);
      IsLower ? lhs += pi+1 : lhs -= pi+2;
    }
  }
};

template<typename LhsScalar, typename RhsScalar, typename Index, int Mode, bool Conjugate>
struct packed_triangular_solve_vector<LhsScalar, RhsScalar, Index, OnTheLeft, Mode, Conjugate, ColMajor>
{
  enum {
    IsLower = (Mode&Lower)==Lower
  };
  static void run(Index size, const LhsScalar* lhs, RhsScalar* rhs)
  {
    internal::conj_if<Conjugate> cj;
    typedef Map<const Matrix<LhsScalar,Dynamic,1> > LhsMap;
    typedef typename conj_expr_if<Conjugate,LhsMap>::type ConjLhsType;

    lhs += IsLower ? 0 : size*(size-1)>>1;
    for(Index pi=0; pi<size; ++pi)
    {
      Index i = IsLower ? pi : size-pi-1;
      Index r = size - pi - 1;
      if (!(Mode & UnitDiag))
	rhs[i] /= cj(lhs[IsLower ? 0 : i]);
      if (r>0)
	Map<Matrix<RhsScalar,Dynamic,1> >(rhs+(IsLower? i+1 : 0),r) -=
	    rhs[i] * ConjLhsType(LhsMap(lhs+(IsLower? 1 : 0),r));
      IsLower ? lhs += size-pi : lhs -= r;
    }
  }
};

template<typename LhsScalar, typename RhsScalar, typename Index, int Mode, bool Conjugate, int StorageOrder>
struct packed_triangular_solve_vector<LhsScalar, RhsScalar, Index, OnTheRight, Mode, Conjugate, StorageOrder>
{
  static void run(Index size, const LhsScalar* lhs, RhsScalar* rhs)
  {
    packed_triangular_solve_vector<LhsScalar,RhsScalar,Index,OnTheLeft,
	((Mode&Upper)==Upper ? Lower : Upper) | (Mode&UnitDiag),
	Conjugate,StorageOrder==RowMajor?ColMajor:RowMajor
      >::run(size, lhs, rhs);
  }
};

} 

#endif 
