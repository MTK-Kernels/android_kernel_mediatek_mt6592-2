// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed


namespace Eigen { 

template<typename _Scalar> struct ei_traits<Rotation2D<_Scalar> >
{
  typedef _Scalar Scalar;
};

template<typename _Scalar>
class Rotation2D : public RotationBase<Rotation2D<_Scalar>,2>
{
  typedef RotationBase<Rotation2D<_Scalar>,2> Base;

public:

  using Base::operator*;

  enum { Dim = 2 };
  
  typedef _Scalar Scalar;
  typedef Matrix<Scalar,2,1> Vector2;
  typedef Matrix<Scalar,2,2> Matrix2;

protected:

  Scalar m_angle;

public:

  
  inline Rotation2D(Scalar a) : m_angle(a) {}

  
  inline Scalar angle() const { return m_angle; }

  
  inline Scalar& angle() { return m_angle; }

  
  inline Rotation2D inverse() const { return -m_angle; }

  
  inline Rotation2D operator*(const Rotation2D& other) const
  { return m_angle + other.m_angle; }

  
  inline Rotation2D& operator*=(const Rotation2D& other)
  { return m_angle += other.m_angle; return *this; }

  
  Vector2 operator* (const Vector2& vec) const
  { return toRotationMatrix() * vec; }

  template<typename Derived>
  Rotation2D& fromRotationMatrix(const MatrixBase<Derived>& m);
  Matrix2 toRotationMatrix(void) const;

  inline Rotation2D slerp(Scalar t, const Rotation2D& other) const
  { return m_angle * (1-t) + other.angle() * t; }

  template<typename NewScalarType>
  inline typename internal::cast_return_type<Rotation2D,Rotation2D<NewScalarType> >::type cast() const
  { return typename internal::cast_return_type<Rotation2D,Rotation2D<NewScalarType> >::type(*this); }

  
  template<typename OtherScalarType>
  inline explicit Rotation2D(const Rotation2D<OtherScalarType>& other)
  {
    m_angle = Scalar(other.angle());
  }

  bool isApprox(const Rotation2D& other, typename NumTraits<Scalar>::Real prec = precision<Scalar>()) const
  { return ei_isApprox(m_angle,other.m_angle, prec); }
};

typedef Rotation2D<float> Rotation2Df;
typedef Rotation2D<double> Rotation2Dd;

template<typename Scalar>
template<typename Derived>
Rotation2D<Scalar>& Rotation2D<Scalar>::fromRotationMatrix(const MatrixBase<Derived>& mat)
{
  EIGEN_STATIC_ASSERT(Derived::RowsAtCompileTime==2 && Derived::ColsAtCompileTime==2,YOU_MADE_A_PROGRAMMING_MISTAKE)
  m_angle = ei_atan2(mat.coeff(1,0), mat.coeff(0,0));
  return *this;
}

template<typename Scalar>
typename Rotation2D<Scalar>::Matrix2
Rotation2D<Scalar>::toRotationMatrix(void) const
{
  Scalar sinA = ei_sin(m_angle);
  Scalar cosA = ei_cos(m_angle);
  return (Matrix2() << cosA, -sinA, sinA, cosA).finished();
}

} 
