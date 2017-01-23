template<typename OtherDerived>
EIGEN_STRONG_INLINE const EIGEN_CWISE_PRODUCT_RETURN_TYPE(Derived,OtherDerived)
operator*(const EIGEN_CURRENT_STORAGE_BASE_CLASS<OtherDerived> &other) const
{
  return EIGEN_CWISE_PRODUCT_RETURN_TYPE(Derived,OtherDerived)(derived(), other.derived());
}

template<typename OtherDerived>
EIGEN_STRONG_INLINE const CwiseBinaryOp<internal::scalar_quotient_op<Scalar>, const Derived, const OtherDerived>
operator/(const EIGEN_CURRENT_STORAGE_BASE_CLASS<OtherDerived> &other) const
{
  return CwiseBinaryOp<internal::scalar_quotient_op<Scalar>, const Derived, const OtherDerived>(derived(), other.derived());
}

EIGEN_MAKE_CWISE_BINARY_OP(min,internal::scalar_min_op)

EIGEN_STRONG_INLINE const CwiseBinaryOp<internal::scalar_min_op<Scalar>, const Derived,
                                        const CwiseNullaryOp<internal::scalar_constant_op<Scalar>, PlainObject> >
#ifdef EIGEN_PARSED_BY_DOXYGEN
min
#else
(min)
#endif
(const Scalar &other) const
{
  return (min)(Derived::PlainObject::Constant(rows(), cols(), other));
}

EIGEN_MAKE_CWISE_BINARY_OP(max,internal::scalar_max_op)

EIGEN_STRONG_INLINE const CwiseBinaryOp<internal::scalar_max_op<Scalar>, const Derived,
                                        const CwiseNullaryOp<internal::scalar_constant_op<Scalar>, PlainObject> >
#ifdef EIGEN_PARSED_BY_DOXYGEN
max
#else
(max)
#endif
(const Scalar &other) const
{
  return (max)(Derived::PlainObject::Constant(rows(), cols(), other));
}

EIGEN_MAKE_CWISE_BINARY_OP(operator<,std::less)

EIGEN_MAKE_CWISE_BINARY_OP(operator<=,std::less_equal)

EIGEN_MAKE_CWISE_BINARY_OP(operator>,std::greater)

EIGEN_MAKE_CWISE_BINARY_OP(operator>=,std::greater_equal)

EIGEN_MAKE_CWISE_BINARY_OP(operator==,std::equal_to)

EIGEN_MAKE_CWISE_BINARY_OP(operator!=,std::not_equal_to)


inline const CwiseUnaryOp<internal::scalar_add_op<Scalar>, const Derived>
operator+(const Scalar& scalar) const
{
  return CwiseUnaryOp<internal::scalar_add_op<Scalar>, const Derived>(derived(), internal::scalar_add_op<Scalar>(scalar));
}

friend inline const CwiseUnaryOp<internal::scalar_add_op<Scalar>, const Derived>
operator+(const Scalar& scalar,const EIGEN_CURRENT_STORAGE_BASE_CLASS<Derived>& other)
{
  return other + scalar;
}

inline const CwiseUnaryOp<internal::scalar_add_op<Scalar>, const Derived>
operator-(const Scalar& scalar) const
{
  return *this + (-scalar);
}

friend inline const CwiseUnaryOp<internal::scalar_add_op<Scalar>, const CwiseUnaryOp<internal::scalar_opposite_op<Scalar>, const Derived> >
operator-(const Scalar& scalar,const EIGEN_CURRENT_STORAGE_BASE_CLASS<Derived>& other)
{
  return (-other) + scalar;
}

template<typename OtherDerived>
inline const CwiseBinaryOp<internal::scalar_boolean_and_op, const Derived, const OtherDerived>
operator&&(const EIGEN_CURRENT_STORAGE_BASE_CLASS<OtherDerived> &other) const
{
  EIGEN_STATIC_ASSERT((internal::is_same<bool,Scalar>::value && internal::is_same<bool,typename OtherDerived::Scalar>::value),
                      THIS_METHOD_IS_ONLY_FOR_EXPRESSIONS_OF_BOOL);
  return CwiseBinaryOp<internal::scalar_boolean_and_op, const Derived, const OtherDerived>(derived(),other.derived());
}

template<typename OtherDerived>
inline const CwiseBinaryOp<internal::scalar_boolean_or_op, const Derived, const OtherDerived>
operator||(const EIGEN_CURRENT_STORAGE_BASE_CLASS<OtherDerived> &other) const
{
  EIGEN_STATIC_ASSERT((internal::is_same<bool,Scalar>::value && internal::is_same<bool,typename OtherDerived::Scalar>::value),
                      THIS_METHOD_IS_ONLY_FOR_EXPRESSIONS_OF_BOOL);
  return CwiseBinaryOp<internal::scalar_boolean_or_op, const Derived, const OtherDerived>(derived(),other.derived());
}
