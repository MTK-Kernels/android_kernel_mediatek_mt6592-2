// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_ARRAY_CWISE_OPERATORS_H
#define EIGEN_ARRAY_CWISE_OPERATORS_H

namespace Eigen { 



template<typename ExpressionType>
EIGEN_STRONG_INLINE const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_abs_op)
Cwise<ExpressionType>::abs() const
{
  return _expression();
}

template<typename ExpressionType>
EIGEN_STRONG_INLINE const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_abs2_op)
Cwise<ExpressionType>::abs2() const
{
  return _expression();
}

template<typename ExpressionType>
inline const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_exp_op)
Cwise<ExpressionType>::exp() const
{
  return _expression();
}

template<typename ExpressionType>
inline const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_log_op)
Cwise<ExpressionType>::log() const
{
  return _expression();
}

template<typename ExpressionType>
template<typename OtherDerived>
EIGEN_STRONG_INLINE const EIGEN_CWISE_PRODUCT_RETURN_TYPE(ExpressionType,OtherDerived)
Cwise<ExpressionType>::operator*(const MatrixBase<OtherDerived> &other) const
{
  return EIGEN_CWISE_PRODUCT_RETURN_TYPE(ExpressionType,OtherDerived)(_expression(), other.derived());
}

template<typename ExpressionType>
template<typename OtherDerived>
EIGEN_STRONG_INLINE const EIGEN_CWISE_BINOP_RETURN_TYPE(internal::scalar_quotient_op)
Cwise<ExpressionType>::operator/(const MatrixBase<OtherDerived> &other) const
{
  return EIGEN_CWISE_BINOP_RETURN_TYPE(internal::scalar_quotient_op)(_expression(), other.derived());
}

template<typename ExpressionType>
template<typename OtherDerived>
inline ExpressionType& Cwise<ExpressionType>::operator*=(const MatrixBase<OtherDerived> &other)
{
  return m_matrix.const_cast_derived() = *this * other;
}

template<typename ExpressionType>
template<typename OtherDerived>
inline ExpressionType& Cwise<ExpressionType>::operator/=(const MatrixBase<OtherDerived> &other)
{
  return m_matrix.const_cast_derived() = *this / other;
}



template<typename ExpressionType>
inline const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_sqrt_op)
Cwise<ExpressionType>::sqrt() const
{
  return _expression();
}

template<typename ExpressionType>
inline const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_cos_op)
Cwise<ExpressionType>::cos() const
{
  return _expression();
}


template<typename ExpressionType>
inline const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_sin_op)
Cwise<ExpressionType>::sin() const
{
  return _expression();
}


template<typename ExpressionType>
inline const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_pow_op)
Cwise<ExpressionType>::pow(const Scalar& exponent) const
{
  return EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_pow_op)(_expression(), internal::scalar_pow_op<Scalar>(exponent));
}


template<typename ExpressionType>
inline const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_inverse_op)
Cwise<ExpressionType>::inverse() const
{
  return _expression();
}

template<typename ExpressionType>
inline const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_square_op)
Cwise<ExpressionType>::square() const
{
  return _expression();
}

template<typename ExpressionType>
inline const EIGEN_CWISE_UNOP_RETURN_TYPE(internal::scalar_cube_op)
Cwise<ExpressionType>::cube() const
{
  return _expression();
}



template<typename ExpressionType>
template<typename OtherDerived>
inline const EIGEN_CWISE_BINOP_RETURN_TYPE(std::less)
Cwise<ExpressionType>::operator<(const MatrixBase<OtherDerived> &other) const
{
  return EIGEN_CWISE_BINOP_RETURN_TYPE(std::less)(_expression(), other.derived());
}

template<typename ExpressionType>
template<typename OtherDerived>
inline const EIGEN_CWISE_BINOP_RETURN_TYPE(std::less_equal)
Cwise<ExpressionType>::operator<=(const MatrixBase<OtherDerived> &other) const
{
  return EIGEN_CWISE_BINOP_RETURN_TYPE(std::less_equal)(_expression(), other.derived());
}

template<typename ExpressionType>
template<typename OtherDerived>
inline const EIGEN_CWISE_BINOP_RETURN_TYPE(std::greater)
Cwise<ExpressionType>::operator>(const MatrixBase<OtherDerived> &other) const
{
  return EIGEN_CWISE_BINOP_RETURN_TYPE(std::greater)(_expression(), other.derived());
}

template<typename ExpressionType>
template<typename OtherDerived>
inline const EIGEN_CWISE_BINOP_RETURN_TYPE(std::greater_equal)
Cwise<ExpressionType>::operator>=(const MatrixBase<OtherDerived> &other) const
{
  return EIGEN_CWISE_BINOP_RETURN_TYPE(std::greater_equal)(_expression(), other.derived());
}

template<typename ExpressionType>
template<typename OtherDerived>
inline const EIGEN_CWISE_BINOP_RETURN_TYPE(std::equal_to)
Cwise<ExpressionType>::operator==(const MatrixBase<OtherDerived> &other) const
{
  return EIGEN_CWISE_BINOP_RETURN_TYPE(std::equal_to)(_expression(), other.derived());
}

template<typename ExpressionType>
template<typename OtherDerived>
inline const EIGEN_CWISE_BINOP_RETURN_TYPE(std::not_equal_to)
Cwise<ExpressionType>::operator!=(const MatrixBase<OtherDerived> &other) const
{
  return EIGEN_CWISE_BINOP_RETURN_TYPE(std::not_equal_to)(_expression(), other.derived());
}


template<typename ExpressionType>
inline const EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::less)
Cwise<ExpressionType>::operator<(Scalar s) const
{
  return EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::less)(_expression(),
            typename ExpressionType::ConstantReturnType(_expression().rows(), _expression().cols(), s));
}

template<typename ExpressionType>
inline const EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::less_equal)
Cwise<ExpressionType>::operator<=(Scalar s) const
{
  return EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::less_equal)(_expression(),
            typename ExpressionType::ConstantReturnType(_expression().rows(), _expression().cols(), s));
}

template<typename ExpressionType>
inline const EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::greater)
Cwise<ExpressionType>::operator>(Scalar s) const
{
  return EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::greater)(_expression(),
            typename ExpressionType::ConstantReturnType(_expression().rows(), _expression().cols(), s));
}

template<typename ExpressionType>
inline const EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::greater_equal)
Cwise<ExpressionType>::operator>=(Scalar s) const
{
  return EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::greater_equal)(_expression(),
            typename ExpressionType::ConstantReturnType(_expression().rows(), _expression().cols(), s));
}

template<typename ExpressionType>
inline const EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::equal_to)
Cwise<ExpressionType>::operator==(Scalar s) const
{
  return EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::equal_to)(_expression(),
            typename ExpressionType::ConstantReturnType(_expression().rows(), _expression().cols(), s));
}

template<typename ExpressionType>
inline const EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::not_equal_to)
Cwise<ExpressionType>::operator!=(Scalar s) const
{
  return EIGEN_CWISE_COMP_TO_SCALAR_RETURN_TYPE(std::not_equal_to)(_expression(),
            typename ExpressionType::ConstantReturnType(_expression().rows(), _expression().cols(), s));
}


template<typename ExpressionType>
inline const typename Cwise<ExpressionType>::ScalarAddReturnType
Cwise<ExpressionType>::operator+(const Scalar& scalar) const
{
  return typename Cwise<ExpressionType>::ScalarAddReturnType(m_matrix, internal::scalar_add_op<Scalar>(scalar));
}

template<typename ExpressionType>
inline ExpressionType& Cwise<ExpressionType>::operator+=(const Scalar& scalar)
{
  return m_matrix.const_cast_derived() = *this + scalar;
}

template<typename ExpressionType>
inline const typename Cwise<ExpressionType>::ScalarAddReturnType
Cwise<ExpressionType>::operator-(const Scalar& scalar) const
{
  return *this + (-scalar);
}

template<typename ExpressionType>
inline ExpressionType& Cwise<ExpressionType>::operator-=(const Scalar& scalar)
{
  return m_matrix.const_cast_derived() = *this - scalar;
}

} 

#endif 
