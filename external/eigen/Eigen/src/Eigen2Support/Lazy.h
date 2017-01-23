// Copyright (C) 2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_LAZY_H
#define EIGEN_LAZY_H

namespace Eigen { 

template<typename Derived>
template<unsigned int Added>
inline const Flagged<Derived, Added, 0>
MatrixBase<Derived>::marked() const
{
  return derived();
}

template<typename Derived>
inline const Flagged<Derived, 0, EvalBeforeAssigningBit>
MatrixBase<Derived>::lazy() const
{
  return derived();
}


template<typename Derived>
template<typename ProductDerived, typename Lhs, typename Rhs>
Derived& MatrixBase<Derived>::operator+=(const Flagged<ProductBase<ProductDerived, Lhs,Rhs>, 0,
                                                       EvalBeforeAssigningBit>& other)
{
  other._expression().derived().addTo(derived()); return derived();
}

template<typename Derived>
template<typename ProductDerived, typename Lhs, typename Rhs>
Derived& MatrixBase<Derived>::operator-=(const Flagged<ProductBase<ProductDerived, Lhs,Rhs>, 0,
                                                       EvalBeforeAssigningBit>& other)
{
  other._expression().derived().subTo(derived()); return derived();
}

} 

#endif 
