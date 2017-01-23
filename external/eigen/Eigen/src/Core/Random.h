// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_RANDOM_H
#define EIGEN_RANDOM_H

namespace Eigen { 

namespace internal {

template<typename Scalar> struct scalar_random_op {
  EIGEN_EMPTY_STRUCT_CTOR(scalar_random_op)
  template<typename Index>
  inline const Scalar operator() (Index, Index = 0) const { return random<Scalar>(); }
};

template<typename Scalar>
struct functor_traits<scalar_random_op<Scalar> >
{ enum { Cost = 5 * NumTraits<Scalar>::MulCost, PacketAccess = false, IsRepeatable = false }; };

} 

template<typename Derived>
inline const CwiseNullaryOp<internal::scalar_random_op<typename internal::traits<Derived>::Scalar>, Derived>
DenseBase<Derived>::Random(Index rows, Index cols)
{
  return NullaryExpr(rows, cols, internal::scalar_random_op<Scalar>());
}

template<typename Derived>
inline const CwiseNullaryOp<internal::scalar_random_op<typename internal::traits<Derived>::Scalar>, Derived>
DenseBase<Derived>::Random(Index size)
{
  return NullaryExpr(size, internal::scalar_random_op<Scalar>());
}

template<typename Derived>
inline const CwiseNullaryOp<internal::scalar_random_op<typename internal::traits<Derived>::Scalar>, Derived>
DenseBase<Derived>::Random()
{
  return NullaryExpr(RowsAtCompileTime, ColsAtCompileTime, internal::scalar_random_op<Scalar>());
}

template<typename Derived>
inline Derived& DenseBase<Derived>::setRandom()
{
  return *this = Random(rows(), cols());
}

template<typename Derived>
EIGEN_STRONG_INLINE Derived&
PlainObjectBase<Derived>::setRandom(Index newSize)
{
  resize(newSize);
  return setRandom();
}

template<typename Derived>
EIGEN_STRONG_INLINE Derived&
PlainObjectBase<Derived>::setRandom(Index nbRows, Index nbCols)
{
  resize(nbRows, nbCols);
  return setRandom();
}

} 

#endif 
