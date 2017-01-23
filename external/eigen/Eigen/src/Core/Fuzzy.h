// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_FUZZY_H
#define EIGEN_FUZZY_H

namespace Eigen { 

namespace internal
{

template<typename Derived, typename OtherDerived, bool is_integer = NumTraits<typename Derived::Scalar>::IsInteger>
struct isApprox_selector
{
  static bool run(const Derived& x, const OtherDerived& y, const typename Derived::RealScalar& prec)
  {
    using std::min;
    typename internal::nested<Derived,2>::type nested(x);
    typename internal::nested<OtherDerived,2>::type otherNested(y);
    return (nested - otherNested).cwiseAbs2().sum() <= prec * prec * (min)(nested.cwiseAbs2().sum(), otherNested.cwiseAbs2().sum());
  }
};

template<typename Derived, typename OtherDerived>
struct isApprox_selector<Derived, OtherDerived, true>
{
  static bool run(const Derived& x, const OtherDerived& y, const typename Derived::RealScalar&)
  {
    return x.matrix() == y.matrix();
  }
};

template<typename Derived, typename OtherDerived, bool is_integer = NumTraits<typename Derived::Scalar>::IsInteger>
struct isMuchSmallerThan_object_selector
{
  static bool run(const Derived& x, const OtherDerived& y, const typename Derived::RealScalar& prec)
  {
    return x.cwiseAbs2().sum() <= numext::abs2(prec) * y.cwiseAbs2().sum();
  }
};

template<typename Derived, typename OtherDerived>
struct isMuchSmallerThan_object_selector<Derived, OtherDerived, true>
{
  static bool run(const Derived& x, const OtherDerived&, const typename Derived::RealScalar&)
  {
    return x.matrix() == Derived::Zero(x.rows(), x.cols()).matrix();
  }
};

template<typename Derived, bool is_integer = NumTraits<typename Derived::Scalar>::IsInteger>
struct isMuchSmallerThan_scalar_selector
{
  static bool run(const Derived& x, const typename Derived::RealScalar& y, const typename Derived::RealScalar& prec)
  {
    return x.cwiseAbs2().sum() <= numext::abs2(prec * y);
  }
};

template<typename Derived>
struct isMuchSmallerThan_scalar_selector<Derived, true>
{
  static bool run(const Derived& x, const typename Derived::RealScalar&, const typename Derived::RealScalar&)
  {
    return x.matrix() == Derived::Zero(x.rows(), x.cols()).matrix();
  }
};

} 


template<typename Derived>
template<typename OtherDerived>
bool DenseBase<Derived>::isApprox(
  const DenseBase<OtherDerived>& other,
  const RealScalar& prec
) const
{
  return internal::isApprox_selector<Derived, OtherDerived>::run(derived(), other.derived(), prec);
}

template<typename Derived>
bool DenseBase<Derived>::isMuchSmallerThan(
  const typename NumTraits<Scalar>::Real& other,
  const RealScalar& prec
) const
{
  return internal::isMuchSmallerThan_scalar_selector<Derived>::run(derived(), other, prec);
}

template<typename Derived>
template<typename OtherDerived>
bool DenseBase<Derived>::isMuchSmallerThan(
  const DenseBase<OtherDerived>& other,
  const RealScalar& prec
) const
{
  return internal::isMuchSmallerThan_object_selector<Derived, OtherDerived>::run(derived(), other.derived(), prec);
}

} 

#endif 
