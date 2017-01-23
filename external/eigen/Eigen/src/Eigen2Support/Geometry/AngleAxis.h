// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed


namespace Eigen { 


template<typename _Scalar> struct ei_traits<AngleAxis<_Scalar> >
{
  typedef _Scalar Scalar;
};

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
  inline AngleAxis(Scalar angle, const MatrixBase<Derived>& axis) : m_axis(axis), m_angle(angle) {}
  
  inline AngleAxis(const QuaternionType& q) { *this = q; }
  
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

  
  inline Matrix3 operator* (const Matrix3& other) const
  { return toRotationMatrix() * other; }

  
  inline friend Matrix3 operator* (const Matrix3& a, const AngleAxis& b)
  { return a * b.toRotationMatrix(); }

  
  inline Vector3 operator* (const Vector3& other) const
  { return toRotationMatrix() * other; }

  
  AngleAxis inverse() const
  { return AngleAxis(-m_angle, m_axis); }

  AngleAxis& operator=(const QuaternionType& q);
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

  bool isApprox(const AngleAxis& other, typename NumTraits<Scalar>::Real prec = precision<Scalar>()) const
  { return m_axis.isApprox(other.m_axis, prec) && ei_isApprox(m_angle,other.m_angle, prec); }
};

typedef AngleAxis<float> AngleAxisf;
typedef AngleAxis<double> AngleAxisd;

template<typename Scalar>
AngleAxis<Scalar>& AngleAxis<Scalar>::operator=(const QuaternionType& q)
{
  Scalar n2 = q.vec().squaredNorm();
  if (n2 < precision<Scalar>()*precision<Scalar>())
  {
    m_angle = 0;
    m_axis << 1, 0, 0;
  }
  else
  {
    m_angle = 2*std::acos(q.w());
    m_axis = q.vec() / ei_sqrt(n2);
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
typename AngleAxis<Scalar>::Matrix3
AngleAxis<Scalar>::toRotationMatrix(void) const
{
  Matrix3 res;
  Vector3 sin_axis  = ei_sin(m_angle) * m_axis;
  Scalar c = ei_cos(m_angle);
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

  res.diagonal() = (cos1_axis.cwise() * m_axis).cwise() + c;

  return res;
}

} 
