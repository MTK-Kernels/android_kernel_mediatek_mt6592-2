// Copyright (C) 2006-2008, 2010 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_DOT_H
#define EIGEN_DOT_H

namespace Eigen { 

namespace internal {

template<typename T, typename U,
         bool NeedToTranspose = T::IsVectorAtCompileTime
                && U::IsVectorAtCompileTime
                && ((int(T::RowsAtCompileTime) == 1 && int(U::ColsAtCompileTime) == 1)
                      |  
                         
                    (int(T::ColsAtCompileTime) == 1 && int(U::RowsAtCompileTime) == 1))
>
struct dot_nocheck
{
  typedef typename scalar_product_traits<typename traits<T>::Scalar,typename traits<U>::Scalar>::ReturnType ResScalar;
  static inline ResScalar run(const MatrixBase<T>& a, const MatrixBase<U>& b)
  {
    return a.template binaryExpr<scalar_conj_product_op<typename traits<T>::Scalar,typename traits<U>::Scalar> >(b).sum();
  }
};

template<typename T, typename U>
struct dot_nocheck<T, U, true>
{
  typedef typename scalar_product_traits<typename traits<T>::Scalar,typename traits<U>::Scalar>::ReturnType ResScalar;
  static inline ResScalar run(const MatrixBase<T>& a, const MatrixBase<U>& b)
  {
    return a.transpose().template binaryExpr<scalar_conj_product_op<typename traits<T>::Scalar,typename traits<U>::Scalar> >(b).sum();
  }
};

} 

template<typename Derived>
template<typename OtherDerived>
typename internal::scalar_product_traits<typename internal::traits<Derived>::Scalar,typename internal::traits<OtherDerived>::Scalar>::ReturnType
MatrixBase<Derived>::dot(const MatrixBase<OtherDerived>& other) const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(OtherDerived)
  EIGEN_STATIC_ASSERT_SAME_VECTOR_SIZE(Derived,OtherDerived)
  typedef internal::scalar_conj_product_op<Scalar,typename OtherDerived::Scalar> func;
  EIGEN_CHECK_BINARY_COMPATIBILIY(func,Scalar,typename OtherDerived::Scalar);

  eigen_assert(size() == other.size());

  return internal::dot_nocheck<Derived,OtherDerived>::run(*this, other);
}

#ifdef EIGEN2_SUPPORT
template<typename Derived>
template<typename OtherDerived>
typename internal::traits<Derived>::Scalar
MatrixBase<Derived>::eigen2_dot(const MatrixBase<OtherDerived>& other) const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(OtherDerived)
  EIGEN_STATIC_ASSERT_SAME_VECTOR_SIZE(Derived,OtherDerived)
  EIGEN_STATIC_ASSERT((internal::is_same<Scalar, typename OtherDerived::Scalar>::value),
    YOU_MIXED_DIFFERENT_NUMERIC_TYPES__YOU_NEED_TO_USE_THE_CAST_METHOD_OF_MATRIXBASE_TO_CAST_NUMERIC_TYPES_EXPLICITLY)

  eigen_assert(size() == other.size());

  return internal::dot_nocheck<OtherDerived,Derived>::run(other,*this);
}
#endif



template<typename Derived>
EIGEN_STRONG_INLINE typename NumTraits<typename internal::traits<Derived>::Scalar>::Real MatrixBase<Derived>::squaredNorm() const
{
  return numext::real((*this).cwiseAbs2().sum());
}

template<typename Derived>
inline typename NumTraits<typename internal::traits<Derived>::Scalar>::Real MatrixBase<Derived>::norm() const
{
  using std::sqrt;
  return sqrt(squaredNorm());
}

template<typename Derived>
inline const typename MatrixBase<Derived>::PlainObject
MatrixBase<Derived>::normalized() const
{
  typedef typename internal::nested<Derived>::type Nested;
  typedef typename internal::remove_reference<Nested>::type _Nested;
  _Nested n(derived());
  return n / n.norm();
}

template<typename Derived>
inline void MatrixBase<Derived>::normalize()
{
  *this /= norm();
}


namespace internal {

template<typename Derived, int p>
struct lpNorm_selector
{
  typedef typename NumTraits<typename traits<Derived>::Scalar>::Real RealScalar;
  static inline RealScalar run(const MatrixBase<Derived>& m)
  {
    using std::pow;
    return pow(m.cwiseAbs().array().pow(p).sum(), RealScalar(1)/p);
  }
};

template<typename Derived>
struct lpNorm_selector<Derived, 1>
{
  static inline typename NumTraits<typename traits<Derived>::Scalar>::Real run(const MatrixBase<Derived>& m)
  {
    return m.cwiseAbs().sum();
  }
};

template<typename Derived>
struct lpNorm_selector<Derived, 2>
{
  static inline typename NumTraits<typename traits<Derived>::Scalar>::Real run(const MatrixBase<Derived>& m)
  {
    return m.norm();
  }
};

template<typename Derived>
struct lpNorm_selector<Derived, Infinity>
{
  static inline typename NumTraits<typename traits<Derived>::Scalar>::Real run(const MatrixBase<Derived>& m)
  {
    return m.cwiseAbs().maxCoeff();
  }
};

} 

template<typename Derived>
template<int p>
inline typename NumTraits<typename internal::traits<Derived>::Scalar>::Real
MatrixBase<Derived>::lpNorm() const
{
  return internal::lpNorm_selector<Derived, p>::run(*this);
}


template<typename Derived>
template<typename OtherDerived>
bool MatrixBase<Derived>::isOrthogonal
(const MatrixBase<OtherDerived>& other, const RealScalar& prec) const
{
  typename internal::nested<Derived,2>::type nested(derived());
  typename internal::nested<OtherDerived,2>::type otherNested(other.derived());
  return numext::abs2(nested.dot(otherNested)) <= prec * prec * nested.squaredNorm() * otherNested.squaredNorm();
}

template<typename Derived>
bool MatrixBase<Derived>::isUnitary(const RealScalar& prec) const
{
  typename Derived::Nested nested(derived());
  for(Index i = 0; i < cols(); ++i)
  {
    if(!internal::isApprox(nested.col(i).squaredNorm(), static_cast<RealScalar>(1), prec))
      return false;
    for(Index j = 0; j < i; ++j)
      if(!internal::isMuchSmallerThan(nested.col(i).dot(nested.col(j)), static_cast<Scalar>(1), prec))
        return false;
  }
  return true;
}

} 

#endif 
