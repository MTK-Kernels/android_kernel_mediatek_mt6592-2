// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed


EIGEN_STRONG_INLINE const CwiseUnaryOp<internal::scalar_abs_op<Scalar>, const Derived>
cwiseAbs() const { return derived(); }

EIGEN_STRONG_INLINE const CwiseUnaryOp<internal::scalar_abs2_op<Scalar>, const Derived>
cwiseAbs2() const { return derived(); }

inline const CwiseUnaryOp<internal::scalar_sqrt_op<Scalar>, const Derived>
cwiseSqrt() const { return derived(); }

inline const CwiseUnaryOp<internal::scalar_inverse_op<Scalar>, const Derived>
cwiseInverse() const { return derived(); }

inline const CwiseUnaryOp<std::binder1st<std::equal_to<Scalar> >, const Derived>
cwiseEqual(const Scalar& s) const
{
  return CwiseUnaryOp<std::binder1st<std::equal_to<Scalar> >,const Derived>
          (derived(), std::bind1st(std::equal_to<Scalar>(), s));
}
