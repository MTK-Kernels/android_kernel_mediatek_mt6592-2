// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed


#ifndef EIGEN_PARSED_BY_DOXYGEN

typedef CwiseUnaryOp<internal::scalar_multiple_op<Scalar>, const Derived> ScalarMultipleReturnType;
typedef CwiseUnaryOp<internal::scalar_quotient1_op<Scalar>, const Derived> ScalarQuotient1ReturnType;
typedef typename internal::conditional<NumTraits<Scalar>::IsComplex,
                    const CwiseUnaryOp<internal::scalar_conjugate_op<Scalar>, const Derived>,
                    const Derived&
                  >::type ConjugateReturnType;
typedef typename internal::conditional<NumTraits<Scalar>::IsComplex,
                    const CwiseUnaryOp<internal::scalar_real_op<Scalar>, const Derived>,
                    const Derived&
                  >::type RealReturnType;
typedef typename internal::conditional<NumTraits<Scalar>::IsComplex,
                    CwiseUnaryView<internal::scalar_real_ref_op<Scalar>, Derived>,
                    Derived&
                  >::type NonConstRealReturnType;
typedef CwiseUnaryOp<internal::scalar_imag_op<Scalar>, const Derived> ImagReturnType;
typedef CwiseUnaryView<internal::scalar_imag_ref_op<Scalar>, Derived> NonConstImagReturnType;

#endif 

inline const CwiseUnaryOp<internal::scalar_opposite_op<typename internal::traits<Derived>::Scalar>, const Derived>
operator-() const { return derived(); }


inline const ScalarMultipleReturnType
operator*(const Scalar& scalar) const
{
  return CwiseUnaryOp<internal::scalar_multiple_op<Scalar>, const Derived>
    (derived(), internal::scalar_multiple_op<Scalar>(scalar));
}

#ifdef EIGEN_PARSED_BY_DOXYGEN
const ScalarMultipleReturnType operator*(const RealScalar& scalar) const;
#endif

inline const CwiseUnaryOp<internal::scalar_quotient1_op<typename internal::traits<Derived>::Scalar>, const Derived>
operator/(const Scalar& scalar) const
{
  return CwiseUnaryOp<internal::scalar_quotient1_op<Scalar>, const Derived>
    (derived(), internal::scalar_quotient1_op<Scalar>(scalar));
}

inline const CwiseUnaryOp<internal::scalar_multiple2_op<Scalar,std::complex<Scalar> >, const Derived>
operator*(const std::complex<Scalar>& scalar) const
{
  return CwiseUnaryOp<internal::scalar_multiple2_op<Scalar,std::complex<Scalar> >, const Derived>
    (*static_cast<const Derived*>(this), internal::scalar_multiple2_op<Scalar,std::complex<Scalar> >(scalar));
}

inline friend const ScalarMultipleReturnType
operator*(const Scalar& scalar, const StorageBaseType& matrix)
{ return matrix*scalar; }

inline friend const CwiseUnaryOp<internal::scalar_multiple2_op<Scalar,std::complex<Scalar> >, const Derived>
operator*(const std::complex<Scalar>& scalar, const StorageBaseType& matrix)
{ return matrix*scalar; }

template<typename NewType>
typename internal::cast_return_type<Derived,const CwiseUnaryOp<internal::scalar_cast_op<typename internal::traits<Derived>::Scalar, NewType>, const Derived> >::type
cast() const
{
  return derived();
}

inline ConjugateReturnType
conjugate() const
{
  return ConjugateReturnType(derived());
}

inline RealReturnType
real() const { return derived(); }

inline const ImagReturnType
imag() const { return derived(); }

template<typename CustomUnaryOp>
inline const CwiseUnaryOp<CustomUnaryOp, const Derived>
unaryExpr(const CustomUnaryOp& func = CustomUnaryOp()) const
{
  return CwiseUnaryOp<CustomUnaryOp, const Derived>(derived(), func);
}

template<typename CustomViewOp>
inline const CwiseUnaryView<CustomViewOp, const Derived>
unaryViewExpr(const CustomViewOp& func = CustomViewOp()) const
{
  return CwiseUnaryView<CustomViewOp, const Derived>(derived(), func);
}

inline NonConstRealReturnType
real() { return derived(); }

inline NonConstImagReturnType
imag() { return derived(); }
