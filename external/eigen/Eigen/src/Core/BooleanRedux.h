// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_ALLANDANY_H
#define EIGEN_ALLANDANY_H

namespace Eigen { 

namespace internal {

template<typename Derived, int UnrollCount>
struct all_unroller
{
  enum {
    col = (UnrollCount-1) / Derived::RowsAtCompileTime,
    row = (UnrollCount-1) % Derived::RowsAtCompileTime
  };

  static inline bool run(const Derived &mat)
  {
    return all_unroller<Derived, UnrollCount-1>::run(mat) && mat.coeff(row, col);
  }
};

template<typename Derived>
struct all_unroller<Derived, 0>
{
  static inline bool run(const Derived &) { return true; }
};

template<typename Derived>
struct all_unroller<Derived, Dynamic>
{
  static inline bool run(const Derived &) { return false; }
};

template<typename Derived, int UnrollCount>
struct any_unroller
{
  enum {
    col = (UnrollCount-1) / Derived::RowsAtCompileTime,
    row = (UnrollCount-1) % Derived::RowsAtCompileTime
  };

  static inline bool run(const Derived &mat)
  {
    return any_unroller<Derived, UnrollCount-1>::run(mat) || mat.coeff(row, col);
  }
};

template<typename Derived>
struct any_unroller<Derived, 0>
{
  static inline bool run(const Derived & ) { return false; }
};

template<typename Derived>
struct any_unroller<Derived, Dynamic>
{
  static inline bool run(const Derived &) { return false; }
};

} 

template<typename Derived>
inline bool DenseBase<Derived>::all() const
{
  enum {
    unroll = SizeAtCompileTime != Dynamic
          && CoeffReadCost != Dynamic
          && NumTraits<Scalar>::AddCost != Dynamic
          && SizeAtCompileTime * (CoeffReadCost + NumTraits<Scalar>::AddCost) <= EIGEN_UNROLLING_LIMIT
  };
  if(unroll)
    return internal::all_unroller<Derived, unroll ? int(SizeAtCompileTime) : Dynamic>::run(derived());
  else
  {
    for(Index j = 0; j < cols(); ++j)
      for(Index i = 0; i < rows(); ++i)
        if (!coeff(i, j)) return false;
    return true;
  }
}

template<typename Derived>
inline bool DenseBase<Derived>::any() const
{
  enum {
    unroll = SizeAtCompileTime != Dynamic
          && CoeffReadCost != Dynamic
          && NumTraits<Scalar>::AddCost != Dynamic
          && SizeAtCompileTime * (CoeffReadCost + NumTraits<Scalar>::AddCost) <= EIGEN_UNROLLING_LIMIT
  };
  if(unroll)
    return internal::any_unroller<Derived, unroll ? int(SizeAtCompileTime) : Dynamic>::run(derived());
  else
  {
    for(Index j = 0; j < cols(); ++j)
      for(Index i = 0; i < rows(); ++i)
        if (coeff(i, j)) return true;
    return false;
  }
}

template<typename Derived>
inline typename DenseBase<Derived>::Index DenseBase<Derived>::count() const
{
  return derived().template cast<bool>().template cast<Index>().sum();
}

template<typename Derived>
inline bool DenseBase<Derived>::hasNaN() const
{
  return !((derived().array()==derived().array()).all());
}

template<typename Derived>
inline bool DenseBase<Derived>::allFinite() const
{
  return !((derived()-derived()).hasNaN());
}
    
} 

#endif 
