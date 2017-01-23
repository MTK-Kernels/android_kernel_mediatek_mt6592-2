// Copyright (C) 2012 Chen-Pang He <jdh8@ms63.hinet.net>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_RANK2UPDATE_H
#define EIGEN_RANK2UPDATE_H

namespace internal {

template<typename Scalar, typename Index, int UpLo>
struct rank2_update_selector
{
  static void run(Index size, Scalar* mat, Index stride, const Scalar* u, const Scalar* v, Scalar alpha)
  {
    typedef Map<const Matrix<Scalar,Dynamic,1> > OtherMap;
    for (Index i=0; i<size; ++i)
    {
      Map<Matrix<Scalar,Dynamic,1> >(mat+stride*i+(UpLo==Lower ? i : 0), UpLo==Lower ? size-i : (i+1)) +=
      numext::conj(alpha) * numext::conj(u[i]) * OtherMap(v+(UpLo==Lower ? i : 0), UpLo==Lower ? size-i : (i+1))
                + alpha * numext::conj(v[i]) * OtherMap(u+(UpLo==Lower ? i : 0), UpLo==Lower ? size-i : (i+1));
    }
  }
};

template<typename Scalar, typename Index, int UpLo>
struct packed_rank2_update_selector
{
  static void run(Index size, Scalar* mat, const Scalar* u, const Scalar* v, Scalar alpha)
  {
    typedef Map<const Matrix<Scalar,Dynamic,1> > OtherMap;
    Index offset = 0;
    for (Index i=0; i<size; ++i)
    {
      Map<Matrix<Scalar,Dynamic,1> >(mat+offset, UpLo==Lower ? size-i : (i+1)) +=
      numext::conj(alpha) * numext::conj(u[i]) * OtherMap(v+(UpLo==Lower ? i : 0), UpLo==Lower ? size-i : (i+1))
                + alpha * numext::conj(v[i]) * OtherMap(u+(UpLo==Lower ? i : 0), UpLo==Lower ? size-i : (i+1));
      
      mat[offset+(UpLo==Lower ? 0 : i)] = numext::real(mat[offset+(UpLo==Lower ? 0 : i)]);
      offset += UpLo==Lower ? size-i : (i+1);
    }
  }
};

} 

#endif 
