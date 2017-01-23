// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_TRANSLATION_H
#define EIGEN_TRANSLATION_H

namespace Eigen { 

template<typename _Scalar, int _Dim>
class Translation
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(_Scalar,_Dim)
  
  enum { Dim = _Dim };
  
  typedef _Scalar Scalar;
  
  typedef Matrix<Scalar,Dim,1> VectorType;
  
  typedef Matrix<Scalar,Dim,Dim> LinearMatrixType;
  
  typedef Transform<Scalar,Dim,Affine> AffineTransformType;
  
  typedef Transform<Scalar,Dim,Isometry> IsometryTransformType;

protected:

  VectorType m_coeffs;

public:

  
  Translation() {}
  
  inline Translation(const Scalar& sx, const Scalar& sy)
  {
    eigen_assert(Dim==2);
    m_coeffs.x() = sx;
    m_coeffs.y() = sy;
  }
  
  inline Translation(const Scalar& sx, const Scalar& sy, const Scalar& sz)
  {
    eigen_assert(Dim==3);
    m_coeffs.x() = sx;
    m_coeffs.y() = sy;
    m_coeffs.z() = sz;
  }
  
  explicit inline Translation(const VectorType& vector) : m_coeffs(vector) {}

  
  inline Scalar x() const { return m_coeffs.x(); }
  
  inline Scalar y() const { return m_coeffs.y(); }
  
  inline Scalar z() const { return m_coeffs.z(); }

  
  inline Scalar& x() { return m_coeffs.x(); }
  
  inline Scalar& y() { return m_coeffs.y(); }
  
  inline Scalar& z() { return m_coeffs.z(); }

  const VectorType& vector() const { return m_coeffs; }
  VectorType& vector() { return m_coeffs; }

  const VectorType& translation() const { return m_coeffs; }
  VectorType& translation() { return m_coeffs; }

  
  inline Translation operator* (const Translation& other) const
  { return Translation(m_coeffs + other.m_coeffs); }

  
  inline AffineTransformType operator* (const UniformScaling<Scalar>& other) const;

  
  template<typename OtherDerived>
  inline AffineTransformType operator* (const EigenBase<OtherDerived>& linear) const;

  
  template<typename Derived>
  inline IsometryTransformType operator*(const RotationBase<Derived,Dim>& r) const
  { return *this * IsometryTransformType(r); }

  
  
  template<typename OtherDerived> friend
  inline AffineTransformType operator*(const EigenBase<OtherDerived>& linear, const Translation& t)
  {
    AffineTransformType res;
    res.matrix().setZero();
    res.linear() = linear.derived();
    res.translation() = linear.derived() * t.m_coeffs;
    res.matrix().row(Dim).setZero();
    res(Dim,Dim) = Scalar(1);
    return res;
  }

  
  template<int Mode, int Options>
  inline Transform<Scalar,Dim,Mode> operator* (const Transform<Scalar,Dim,Mode,Options>& t) const
  {
    Transform<Scalar,Dim,Mode> res = t;
    res.pretranslate(m_coeffs);
    return res;
  }

  
  inline VectorType operator* (const VectorType& other) const
  { return m_coeffs + other; }

  
  Translation inverse() const { return Translation(-m_coeffs); }

  Translation& operator=(const Translation& other)
  {
    m_coeffs = other.m_coeffs;
    return *this;
  }

  static const Translation Identity() { return Translation(VectorType::Zero()); }

  template<typename NewScalarType>
  inline typename internal::cast_return_type<Translation,Translation<NewScalarType,Dim> >::type cast() const
  { return typename internal::cast_return_type<Translation,Translation<NewScalarType,Dim> >::type(*this); }

  
  template<typename OtherScalarType>
  inline explicit Translation(const Translation<OtherScalarType,Dim>& other)
  { m_coeffs = other.vector().template cast<Scalar>(); }

  bool isApprox(const Translation& other, typename NumTraits<Scalar>::Real prec = NumTraits<Scalar>::dummy_precision()) const
  { return m_coeffs.isApprox(other.m_coeffs, prec); }

};

typedef Translation<float, 2> Translation2f;
typedef Translation<double,2> Translation2d;
typedef Translation<float, 3> Translation3f;
typedef Translation<double,3> Translation3d;

template<typename Scalar, int Dim>
inline typename Translation<Scalar,Dim>::AffineTransformType
Translation<Scalar,Dim>::operator* (const UniformScaling<Scalar>& other) const
{
  AffineTransformType res;
  res.matrix().setZero();
  res.linear().diagonal().fill(other.factor());
  res.translation() = m_coeffs;
  res(Dim,Dim) = Scalar(1);
  return res;
}

template<typename Scalar, int Dim>
template<typename OtherDerived>
inline typename Translation<Scalar,Dim>::AffineTransformType
Translation<Scalar,Dim>::operator* (const EigenBase<OtherDerived>& linear) const
{
  AffineTransformType res;
  res.matrix().setZero();
  res.linear() = linear.derived();
  res.translation() = m_coeffs;
  res.matrix().row(Dim).setZero();
  res(Dim,Dim) = Scalar(1);
  return res;
}

} 

#endif 
