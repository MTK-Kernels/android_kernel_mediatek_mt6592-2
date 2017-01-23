// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed


template<typename OtherDerived>
EIGEN_STRONG_INLINE const EIGEN_CWISE_PRODUCT_RETURN_TYPE(Derived,OtherDerived)
cwiseProduct(const EIGEN_CURRENT_STORAGE_BASE_CLASS<OtherDerived> &other) const
{
  return EIGEN_CWISE_PRODUCT_RETURN_TYPE(Derived,OtherDerived)(derived(), other.derived());
}

template<typename OtherDerived>
inline const CwiseBinaryOp<std::equal_to<Scalar>, const Derived, const OtherDerived>
cwiseEqual(const EIGEN_CURRENT_STORAGE_BASE_CLASS<OtherDerived> &other) const
{
  return CwiseBinaryOp<std::equal_to<Scalar>, const Derived, const OtherDerived>(derived(), other.derived());
}

template<typename OtherDerived>
inline const CwiseBinaryOp<std::not_equal_to<Scalar>, const Derived, const OtherDerived>
cwiseNotEqual(const EIGEN_CURRENT_STORAGE_BASE_CLASS<OtherDerived> &other) const
{
  return CwiseBinaryOp<std::not_equal_to<Scalar>, const Derived, const OtherDerived>(derived(), other.derived());
}

template<typename OtherDerived>
EIGEN_STRONG_INLINE const CwiseBinaryOp<internal::scalar_min_op<Scalar>, const Derived, const OtherDerived>
cwiseMin(const EIGEN_CURRENT_STORAGE_BASE_CLASS<OtherDerived> &other) const
{
  return CwiseBinaryOp<internal::scalar_min_op<Scalar>, const Derived, const OtherDerived>(derived(), other.derived());
}

EIGEN_STRONG_INLINE const CwiseBinaryOp<internal::scalar_min_op<Scalar>, const Derived, const ConstantReturnType>
cwiseMin(const Scalar &other) const
{
  return cwiseMin(Derived::Constant(rows(), cols(), other));
}

template<typename OtherDerived>
EIGEN_STRONG_INLINE const CwiseBinaryOp<internal::scalar_max_op<Scalar>, const Derived, const OtherDerived>
cwiseMax(const EIGEN_CURRENT_STORAGE_BASE_CLASS<OtherDerived> &other) const
{
  return CwiseBinaryOp<internal::scalar_max_op<Scalar>, const Derived, const OtherDerived>(derived(), other.derived());
}

EIGEN_STRONG_INLINE const CwiseBinaryOp<internal::scalar_max_op<Scalar>, const Derived, const ConstantReturnType>
cwiseMax(const Scalar &other) const
{
  return cwiseMax(Derived::Constant(rows(), cols(), other));
}


template<typename OtherDerived>
EIGEN_STRONG_INLINE const CwiseBinaryOp<internal::scalar_quotient_op<Scalar>, const Derived, const OtherDerived>
cwiseQuotient(const EIGEN_CURRENT_STORAGE_BASE_CLASS<OtherDerived> &other) const
{
  return CwiseBinaryOp<internal::scalar_quotient_op<Scalar>, const Derived, const OtherDerived>(derived(), other.derived());
}
