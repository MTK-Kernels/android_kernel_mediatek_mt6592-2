// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed


namespace Eigen { 

template<typename Other,
         int OtherRows=Other::RowsAtCompileTime,
         int OtherCols=Other::ColsAtCompileTime>
struct ei_quaternion_assign_impl;


template<typename _Scalar> struct ei_traits<Quaternion<_Scalar> >
{
  typedef _Scalar Scalar;
};

template<typename _Scalar>
class Quaternion : public RotationBase<Quaternion<_Scalar>,3>
{
  typedef RotationBase<Quaternion<_Scalar>,3> Base;

public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(_Scalar,4)

  using Base::operator*;

  
  typedef _Scalar Scalar;

  
  typedef Matrix<Scalar, 4, 1> Coefficients;
  
  typedef Matrix<Scalar,3,1> Vector3;
  
  typedef Matrix<Scalar,3,3> Matrix3;
  
  typedef AngleAxis<Scalar> AngleAxisType;

  
  inline Scalar x() const { return m_coeffs.coeff(0); }
  
  inline Scalar y() const { return m_coeffs.coeff(1); }
  
  inline Scalar z() const { return m_coeffs.coeff(2); }
  
  inline Scalar w() const { return m_coeffs.coeff(3); }

  
  inline Scalar& x() { return m_coeffs.coeffRef(0); }
  
  inline Scalar& y() { return m_coeffs.coeffRef(1); }
  
  inline Scalar& z() { return m_coeffs.coeffRef(2); }
  
  inline Scalar& w() { return m_coeffs.coeffRef(3); }

  
  inline const Block<const Coefficients,3,1> vec() const { return m_coeffs.template start<3>(); }

  
  inline Block<Coefficients,3,1> vec() { return m_coeffs.template start<3>(); }

  
  inline const Coefficients& coeffs() const { return m_coeffs; }

  
  inline Coefficients& coeffs() { return m_coeffs; }

  
  inline Quaternion() {}

  inline Quaternion(Scalar w, Scalar x, Scalar y, Scalar z)
  { m_coeffs << x, y, z, w; }

  
  inline Quaternion(const Quaternion& other) { m_coeffs = other.m_coeffs; }

  
  explicit inline Quaternion(const AngleAxisType& aa) { *this = aa; }

  template<typename Derived>
  explicit inline Quaternion(const MatrixBase<Derived>& other) { *this = other; }

  Quaternion& operator=(const Quaternion& other);
  Quaternion& operator=(const AngleAxisType& aa);
  template<typename Derived>
  Quaternion& operator=(const MatrixBase<Derived>& m);

  static inline Quaternion Identity() { return Quaternion(1, 0, 0, 0); }

  inline Quaternion& setIdentity() { m_coeffs << 0, 0, 0, 1; return *this; }

  inline Scalar squaredNorm() const { return m_coeffs.squaredNorm(); }

  inline Scalar norm() const { return m_coeffs.norm(); }

  inline void normalize() { m_coeffs.normalize(); }
  inline Quaternion normalized() const { return Quaternion(m_coeffs.normalized()); }

  inline Scalar eigen2_dot(const Quaternion& other) const { return m_coeffs.eigen2_dot(other.m_coeffs); }

  inline Scalar angularDistance(const Quaternion& other) const;

  Matrix3 toRotationMatrix(void) const;

  template<typename Derived1, typename Derived2>
  Quaternion& setFromTwoVectors(const MatrixBase<Derived1>& a, const MatrixBase<Derived2>& b);

  inline Quaternion operator* (const Quaternion& q) const;
  inline Quaternion& operator*= (const Quaternion& q);

  Quaternion inverse(void) const;
  Quaternion conjugate(void) const;

  Quaternion slerp(Scalar t, const Quaternion& other) const;

  template<typename Derived>
  Vector3 operator* (const MatrixBase<Derived>& vec) const;

  template<typename NewScalarType>
  inline typename internal::cast_return_type<Quaternion,Quaternion<NewScalarType> >::type cast() const
  { return typename internal::cast_return_type<Quaternion,Quaternion<NewScalarType> >::type(*this); }

  
  template<typename OtherScalarType>
  inline explicit Quaternion(const Quaternion<OtherScalarType>& other)
  { m_coeffs = other.coeffs().template cast<Scalar>(); }

  bool isApprox(const Quaternion& other, typename NumTraits<Scalar>::Real prec = precision<Scalar>()) const
  { return m_coeffs.isApprox(other.m_coeffs, prec); }

protected:
  Coefficients m_coeffs;
};

typedef Quaternion<float> Quaternionf;
typedef Quaternion<double> Quaterniond;

template<typename Scalar> inline Quaternion<Scalar>
ei_quaternion_product(const Quaternion<Scalar>& a, const Quaternion<Scalar>& b)
{
  return Quaternion<Scalar>
  (
    a.w() * b.w() - a.x() * b.x() - a.y() * b.y() - a.z() * b.z(),
    a.w() * b.x() + a.x() * b.w() + a.y() * b.z() - a.z() * b.y(),
    a.w() * b.y() + a.y() * b.w() + a.z() * b.x() - a.x() * b.z(),
    a.w() * b.z() + a.z() * b.w() + a.x() * b.y() - a.y() * b.x()
  );
}

template <typename Scalar>
inline Quaternion<Scalar> Quaternion<Scalar>::operator* (const Quaternion& other) const
{
  return ei_quaternion_product(*this,other);
}

template <typename Scalar>
inline Quaternion<Scalar>& Quaternion<Scalar>::operator*= (const Quaternion& other)
{
  return (*this = *this * other);
}

template <typename Scalar>
template<typename Derived>
inline typename Quaternion<Scalar>::Vector3
Quaternion<Scalar>::operator* (const MatrixBase<Derived>& v) const
{
    
    
    
    
    
    Vector3 uv;
    uv = 2 * this->vec().cross(v);
    return v + this->w() * uv + this->vec().cross(uv);
}

template<typename Scalar>
inline Quaternion<Scalar>& Quaternion<Scalar>::operator=(const Quaternion& other)
{
  m_coeffs = other.m_coeffs;
  return *this;
}

template<typename Scalar>
inline Quaternion<Scalar>& Quaternion<Scalar>::operator=(const AngleAxisType& aa)
{
  Scalar ha = Scalar(0.5)*aa.angle(); 
  this->w() = ei_cos(ha);
  this->vec() = ei_sin(ha) * aa.axis();
  return *this;
}

template<typename Scalar>
template<typename Derived>
inline Quaternion<Scalar>& Quaternion<Scalar>::operator=(const MatrixBase<Derived>& xpr)
{
  ei_quaternion_assign_impl<Derived>::run(*this, xpr.derived());
  return *this;
}

template<typename Scalar>
inline typename Quaternion<Scalar>::Matrix3
Quaternion<Scalar>::toRotationMatrix(void) const
{
  
  
  
  
  Matrix3 res;

  const Scalar tx  = Scalar(2)*this->x();
  const Scalar ty  = Scalar(2)*this->y();
  const Scalar tz  = Scalar(2)*this->z();
  const Scalar twx = tx*this->w();
  const Scalar twy = ty*this->w();
  const Scalar twz = tz*this->w();
  const Scalar txx = tx*this->x();
  const Scalar txy = ty*this->x();
  const Scalar txz = tz*this->x();
  const Scalar tyy = ty*this->y();
  const Scalar tyz = tz*this->y();
  const Scalar tzz = tz*this->z();

  res.coeffRef(0,0) = Scalar(1)-(tyy+tzz);
  res.coeffRef(0,1) = txy-twz;
  res.coeffRef(0,2) = txz+twy;
  res.coeffRef(1,0) = txy+twz;
  res.coeffRef(1,1) = Scalar(1)-(txx+tzz);
  res.coeffRef(1,2) = tyz-twx;
  res.coeffRef(2,0) = txz-twy;
  res.coeffRef(2,1) = tyz+twx;
  res.coeffRef(2,2) = Scalar(1)-(txx+tyy);

  return res;
}

template<typename Scalar>
template<typename Derived1, typename Derived2>
inline Quaternion<Scalar>& Quaternion<Scalar>::setFromTwoVectors(const MatrixBase<Derived1>& a, const MatrixBase<Derived2>& b)
{
  Vector3 v0 = a.normalized();
  Vector3 v1 = b.normalized();
  Scalar c = v0.eigen2_dot(v1);

  
  if (ei_isApprox(c,Scalar(1)))
  {
    
    this->w() = 1; this->vec().setZero();
    return *this;
  }
  
  if (ei_isApprox(c,Scalar(-1)))
  {
    this->vec() = v0.unitOrthogonal();
    this->w() = 0;
    return *this;
  }

  Vector3 axis = v0.cross(v1);
  Scalar s = ei_sqrt((Scalar(1)+c)*Scalar(2));
  Scalar invs = Scalar(1)/s;
  this->vec() = axis * invs;
  this->w() = s * Scalar(0.5);

  return *this;
}

template <typename Scalar>
inline Quaternion<Scalar> Quaternion<Scalar>::inverse() const
{
  
  Scalar n2 = this->squaredNorm();
  if (n2 > 0)
    return Quaternion(conjugate().coeffs() / n2);
  else
  {
    
    return Quaternion(Coefficients::Zero());
  }
}

template <typename Scalar>
inline Quaternion<Scalar> Quaternion<Scalar>::conjugate() const
{
  return Quaternion(this->w(),-this->x(),-this->y(),-this->z());
}

template <typename Scalar>
inline Scalar Quaternion<Scalar>::angularDistance(const Quaternion& other) const
{
  double d = ei_abs(this->eigen2_dot(other));
  if (d>=1.0)
    return 0;
  return Scalar(2) * std::acos(d);
}

template <typename Scalar>
Quaternion<Scalar> Quaternion<Scalar>::slerp(Scalar t, const Quaternion& other) const
{
  static const Scalar one = Scalar(1) - machine_epsilon<Scalar>();
  Scalar d = this->eigen2_dot(other);
  Scalar absD = ei_abs(d);

  Scalar scale0;
  Scalar scale1;

  if (absD>=one)
  {
    scale0 = Scalar(1) - t;
    scale1 = t;
  }
  else
  {
    
    Scalar theta = std::acos(absD);
    Scalar sinTheta = ei_sin(theta);

    scale0 = ei_sin( ( Scalar(1) - t ) * theta) / sinTheta;
    scale1 = ei_sin( ( t * theta) ) / sinTheta;
    if (d<0)
      scale1 = -scale1;
  }

  return Quaternion<Scalar>(scale0 * coeffs() + scale1 * other.coeffs());
}

template<typename Other>
struct ei_quaternion_assign_impl<Other,3,3>
{
  typedef typename Other::Scalar Scalar;
  static inline void run(Quaternion<Scalar>& q, const Other& mat)
  {
    
    
    Scalar t = mat.trace();
    if (t > 0)
    {
      t = ei_sqrt(t + Scalar(1.0));
      q.w() = Scalar(0.5)*t;
      t = Scalar(0.5)/t;
      q.x() = (mat.coeff(2,1) - mat.coeff(1,2)) * t;
      q.y() = (mat.coeff(0,2) - mat.coeff(2,0)) * t;
      q.z() = (mat.coeff(1,0) - mat.coeff(0,1)) * t;
    }
    else
    {
      int i = 0;
      if (mat.coeff(1,1) > mat.coeff(0,0))
        i = 1;
      if (mat.coeff(2,2) > mat.coeff(i,i))
        i = 2;
      int j = (i+1)%3;
      int k = (j+1)%3;

      t = ei_sqrt(mat.coeff(i,i)-mat.coeff(j,j)-mat.coeff(k,k) + Scalar(1.0));
      q.coeffs().coeffRef(i) = Scalar(0.5) * t;
      t = Scalar(0.5)/t;
      q.w() = (mat.coeff(k,j)-mat.coeff(j,k))*t;
      q.coeffs().coeffRef(j) = (mat.coeff(j,i)+mat.coeff(i,j))*t;
      q.coeffs().coeffRef(k) = (mat.coeff(k,i)+mat.coeff(i,k))*t;
    }
  }
};

template<typename Other>
struct ei_quaternion_assign_impl<Other,4,1>
{
  typedef typename Other::Scalar Scalar;
  static inline void run(Quaternion<Scalar>& q, const Other& vec)
  {
    q.coeffs() = vec;
  }
};

} 
