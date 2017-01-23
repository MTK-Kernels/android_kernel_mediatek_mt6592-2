

EIGEN_STRONG_INLINE const CwiseUnaryOp<internal::scalar_abs_op<Scalar>, const Derived>
abs() const
{
  return derived();
}

EIGEN_STRONG_INLINE const CwiseUnaryOp<internal::scalar_abs2_op<Scalar>, const Derived>
abs2() const
{
  return derived();
}

inline const CwiseUnaryOp<internal::scalar_exp_op<Scalar>, const Derived>
exp() const
{
  return derived();
}

inline const CwiseUnaryOp<internal::scalar_log_op<Scalar>, const Derived>
log() const
{
  return derived();
}

inline const CwiseUnaryOp<internal::scalar_sqrt_op<Scalar>, const Derived>
sqrt() const
{
  return derived();
}

inline const CwiseUnaryOp<internal::scalar_cos_op<Scalar>, const Derived>
cos() const
{
  return derived();
}


inline const CwiseUnaryOp<internal::scalar_sin_op<Scalar>, const Derived>
sin() const
{
  return derived();
}

inline const CwiseUnaryOp<internal::scalar_acos_op<Scalar>, const Derived>
acos() const
{
  return derived();
}

inline const CwiseUnaryOp<internal::scalar_asin_op<Scalar>, const Derived>
asin() const
{
  return derived();
}

inline const CwiseUnaryOp<internal::scalar_tan_op<Scalar>, Derived>
tan() const
{
  return derived();
}


inline const CwiseUnaryOp<internal::scalar_pow_op<Scalar>, const Derived>
pow(const Scalar& exponent) const
{
  return CwiseUnaryOp<internal::scalar_pow_op<Scalar>, const Derived>
          (derived(), internal::scalar_pow_op<Scalar>(exponent));
}


inline const CwiseUnaryOp<internal::scalar_inverse_op<Scalar>, const Derived>
inverse() const
{
  return derived();
}

inline const CwiseUnaryOp<internal::scalar_square_op<Scalar>, const Derived>
square() const
{
  return derived();
}

inline const CwiseUnaryOp<internal::scalar_cube_op<Scalar>, const Derived>
cube() const
{
  return derived();
}

#define EIGEN_MAKE_SCALAR_CWISE_UNARY_OP(METHOD_NAME,FUNCTOR) \
  inline const CwiseUnaryOp<std::binder2nd<FUNCTOR<Scalar> >, const Derived> \
  METHOD_NAME(const Scalar& s) const { \
    return CwiseUnaryOp<std::binder2nd<FUNCTOR<Scalar> >, const Derived> \
            (derived(), std::bind2nd(FUNCTOR<Scalar>(), s)); \
  }

EIGEN_MAKE_SCALAR_CWISE_UNARY_OP(operator==,  std::equal_to)
EIGEN_MAKE_SCALAR_CWISE_UNARY_OP(operator!=,  std::not_equal_to)
EIGEN_MAKE_SCALAR_CWISE_UNARY_OP(operator<,   std::less)
EIGEN_MAKE_SCALAR_CWISE_UNARY_OP(operator<=,  std::less_equal)
EIGEN_MAKE_SCALAR_CWISE_UNARY_OP(operator>,   std::greater)
EIGEN_MAKE_SCALAR_CWISE_UNARY_OP(operator>=,  std::greater_equal)


