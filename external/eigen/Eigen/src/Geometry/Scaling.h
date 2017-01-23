// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_SCALING_H
#define EIGEN_SCALING_H

namespace Eigen { 

template<typename _Scalar>
class UniformScaling
{
public:
  
  typedef _Scalar Scalar;

protected:

  Scalar m_factor;

public:

  
  UniformScaling() {}
  
  explicit inline UniformScaling(const Scalar& s) : m_factor(s) {}

  inline const Scalar& factor() const { return m_factor; }
  inline Scalar& factor() { return m_factor; }

  
  inline UniformScaling operator* (const UniformScaling& other) const
  { return UniformScaling(m_factor * other.factor()); }

  
  template<int Dim>
  inline Transform<Scalar,Dim,Affine> operator* (const Translation<Scalar,Dim>& t) const;

  
  template<int Dim, int Mode, int Options>
  inline Transform<Scalar,Dim,(int(Mode)==int(Isometry)?Affine:Mode)> operator* (const Transform<Scalar,Dim, Mode, Options>& t) const
  {
   Transform<Scalar,Dim,(int(Mode)==int(Isometry)?Affine:Mode)> res = t;
   res.prescale(factor());
   return res;
}

  
  
  template<typename Derived>
  inline typename internal::plain_matrix_type<Derived>::type operator* (const MatrixBase<Derived>& other) const
  { return other * m_factor; }

  template<typename Derived,int Dim>
  inline Matrix<Scalar,Dim,Dim> operator*(const RotationBase<Derived,Dim>& r) const
  { return r.toRotationMatrix() * m_factor; }

  
  inline UniformScaling inverse() const
  { return UniformScaling(Scalar(1)/m_factor); }

  template<typename NewScalarType>
  inline UniformScaling<NewScalarType> cast() const
  { return UniformScaling<NewScalarType>(NewScalarType(m_factor)); }

  
  template<typename OtherScalarType>
  inline explicit UniformScaling(const UniformScaling<OtherScalarType>& other)
  { m_factor = Scalar(other.factor()); }

  bool isApprox(const UniformScaling& other, const typename NumTraits<Scalar>::Real& prec = NumTraits<Scalar>::dummy_precision()) const
  { return internal::isApprox(m_factor, other.factor(), prec); }

};

template<typename Derived> typename MatrixBase<Derived>::ScalarMultipleReturnType
MatrixBase<Derived>::operator*(const UniformScaling<Scalar>& s) const
{ return derived() * s.factor(); }

static inline UniformScaling<float> Scaling(float s) { return UniformScaling<float>(s); }
static inline UniformScaling<double> Scaling(double s) { return UniformScaling<double>(s); }
template<typename RealScalar>
static inline UniformScaling<std::complex<RealScalar> > Scaling(const std::complex<RealScalar>& s)
{ return UniformScaling<std::complex<RealScalar> >(s); }

template<typename Scalar>
static inline DiagonalMatrix<Scalar,2> Scaling(const Scalar& sx, const Scalar& sy)
{ return DiagonalMatrix<Scalar,2>(sx, sy); }
template<typename Scalar>
static inline DiagonalMatrix<Scalar,3> Scaling(const Scalar& sx, const Scalar& sy, const Scalar& sz)
{ return DiagonalMatrix<Scalar,3>(sx, sy, sz); }

template<typename Derived>
static inline const DiagonalWrapper<const Derived> Scaling(const MatrixBase<Derived>& coeffs)
{ return coeffs.asDiagonal(); }

typedef DiagonalMatrix<float, 2> AlignedScaling2f;
typedef DiagonalMatrix<double,2> AlignedScaling2d;
typedef DiagonalMatrix<float, 3> AlignedScaling3f;
typedef DiagonalMatrix<double,3> AlignedScaling3d;

template<typename Scalar>
template<int Dim>
inline Transform<Scalar,Dim,Affine>
UniformScaling<Scalar>::operator* (const Translation<Scalar,Dim>& t) const
{
  Transform<Scalar,Dim,Affine> res;
  res.matrix().setZero();
  res.linear().diagonal().fill(factor());
  res.translation() = factor() * t.vector();
  res(Dim,Dim) = Scalar(1);
  return res;
}

} 

#endif 
