// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_ANGLEAXIS_H
#define EIGEN_ANGLEAXIS_H

namespace Eigen { 


namespace internal {
template<typename _Scalar> struct traits<AngleAxis<_Scalar> >
{
  typedef _Scalar Scalar;
};
}

template<typename _Scalar>
class AngleAxis : public RotationBase<AngleAxis<_Scalar>,3>
{
  typedef RotationBase<AngleAxis<_Scalar>,3> Base;

public:

  using Base::operator*;

  enum { Dim = 3 };
  
  typedef _Scalar Scalar;
  typedef Matrix<Scalar,3,3> Matrix3;
  typedef Matrix<Scalar,3,1> Vector3;
  typedef Quaternion<Scalar> QuaternionType;

protected:

  Vector3 m_axis;
  Scalar m_angle;

public:

  
  AngleAxis() {}
  template<typename Derived>
  inline AngleAxis(const Scalar& angle, const MatrixBase<Derived>& axis) : m_axis(axis), m_angle(angle) {}
  
  template<typename QuatDerived> inline explicit AngleAxis(const QuaternionBase<QuatDerived>& q) { *this = q; }
  
  template<typename Derived>
  inline explicit AngleAxis(const MatrixBase<Derived>& m) { *this = m; }

  Scalar angle() const { return m_angle; }
  Scalar& angle() { return m_angle; }

  const Vector3& axis() const { return m_axis; }
  Vector3& axis() { return m_axis; }

  
  inline QuaternionType operator* (const AngleAxis& other) const
  { return QuaternionType(*this) * QuaternionType(other); }

  
  inline QuaternionType operator* (const QuaternionType& other) const
  { return QuaternionType(*this) * other; }

  
  friend inline QuaternionType operator* (const QuaternionType& a, const AngleAxis& b)
  { return a * QuaternionType(b); }

  
  AngleAxis inverse() const
  { return AngleAxis(-m_angle, m_axis); }

  template<class QuatDerived>
  AngleAxis& operator=(const QuaternionBase<QuatDerived>& q);
  template<typename Derived>
  AngleAxis& operator=(const MatrixBase<Derived>& m);

  template<typename Derived>
  AngleAxis& fromRotationMatrix(const MatrixBase<Derived>& m);
  Matrix3 toRotationMatrix(void) const;

  template<typename NewScalarType>
  inline typename internal::cast_return_type<AngleAxis,AngleAxis<NewScalarType> >::type cast() const
  { return typename internal::cast_return_type<AngleAxis,AngleAxis<NewScalarType> >::type(*this); }

  
  template<typename OtherScalarType>
  inline explicit AngleAxis(const AngleAxis<OtherScalarType>& other)
  {
    m_axis = other.axis().template cast<Scalar>();
    m_angle = Scalar(other.angle());
  }

  static inline const AngleAxis Identity() { return AngleAxis(0, Vector3::UnitX()); }

  bool isApprox(const AngleAxis& other, const typename NumTraits<Scalar>::Real& prec = NumTraits<Scalar>::dummy_precision()) const
  { return m_axis.isApprox(other.m_axis, prec) && internal::isApprox(m_angle,other.m_angle, prec); }
};

typedef AngleAxis<float> AngleAxisf;
typedef AngleAxis<double> AngleAxisd;

template<typename Scalar>
template<typename QuatDerived>
AngleAxis<Scalar>& AngleAxis<Scalar>::operator=(const QuaternionBase<QuatDerived>& q)
{
  using std::acos;
  using std::min;
  using std::max;
  using std::sqrt;
  Scalar n2 = q.vec().squaredNorm();
  if (n2 < NumTraits<Scalar>::dummy_precision()*NumTraits<Scalar>::dummy_precision())
  {
    m_angle = 0;
    m_axis << 1, 0, 0;
  }
  else
  {
    m_angle = Scalar(2)*acos((min)((max)(Scalar(-1),q.w()),Scalar(1)));
    m_axis = q.vec() / sqrt(n2);
  }
  return *this;
}

template<typename Scalar>
template<typename Derived>
AngleAxis<Scalar>& AngleAxis<Scalar>::operator=(const MatrixBase<Derived>& mat)
{
  
  
  return *this = QuaternionType(mat);
}

template<typename Scalar>
template<typename Derived>
AngleAxis<Scalar>& AngleAxis<Scalar>::fromRotationMatrix(const MatrixBase<Derived>& mat)
{
  return *this = QuaternionType(mat);
}

template<typename Scalar>
typename AngleAxis<Scalar>::Matrix3
AngleAxis<Scalar>::toRotationMatrix(void) const
{
  using std::sin;
  using std::cos;
  Matrix3 res;
  Vector3 sin_axis  = sin(m_angle) * m_axis;
  Scalar c = cos(m_angle);
  Vector3 cos1_axis = (Scalar(1)-c) * m_axis;

  Scalar tmp;
  tmp = cos1_axis.x() * m_axis.y();
  res.coeffRef(0,1) = tmp - sin_axis.z();
  res.coeffRef(1,0) = tmp + sin_axis.z();

  tmp = cos1_axis.x() * m_axis.z();
  res.coeffRef(0,2) = tmp + sin_axis.y();
  res.coeffRef(2,0) = tmp - sin_axis.y();

  tmp = cos1_axis.y() * m_axis.z();
  res.coeffRef(1,2) = tmp - sin_axis.x();
  res.coeffRef(2,1) = tmp + sin_axis.x();

  res.diagonal() = (cos1_axis.cwiseProduct(m_axis)).array() + c;

  return res;
}

} 

#endif 
