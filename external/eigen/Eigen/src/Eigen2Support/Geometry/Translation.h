// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed


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
  
  typedef Scaling<Scalar,Dim> ScalingType;
  
  typedef Transform<Scalar,Dim> TransformType;

protected:

  VectorType m_coeffs;

public:

  
  Translation() {}
  
  inline Translation(const Scalar& sx, const Scalar& sy)
  {
    ei_assert(Dim==2);
    m_coeffs.x() = sx;
    m_coeffs.y() = sy;
  }
  
  inline Translation(const Scalar& sx, const Scalar& sy, const Scalar& sz)
  {
    ei_assert(Dim==3);
    m_coeffs.x() = sx;
    m_coeffs.y() = sy;
    m_coeffs.z() = sz;
  }
  
  explicit inline Translation(const VectorType& vector) : m_coeffs(vector) {}

  const VectorType& vector() const { return m_coeffs; }
  VectorType& vector() { return m_coeffs; }

  
  inline Translation operator* (const Translation& other) const
  { return Translation(m_coeffs + other.m_coeffs); }

  
  inline TransformType operator* (const ScalingType& other) const;

  
  inline TransformType operator* (const LinearMatrixType& linear) const;

  template<typename Derived>
  inline TransformType operator*(const RotationBase<Derived,Dim>& r) const
  { return *this * r.toRotationMatrix(); }

  
  
  friend inline TransformType operator* (const LinearMatrixType& linear, const Translation& t)
  {
    TransformType res;
    res.matrix().setZero();
    res.linear() = linear;
    res.translation() = linear * t.m_coeffs;
    res.matrix().row(Dim).setZero();
    res(Dim,Dim) = Scalar(1);
    return res;
  }

  
  inline TransformType operator* (const TransformType& t) const;

  
  inline VectorType operator* (const VectorType& other) const
  { return m_coeffs + other; }

  
  Translation inverse() const { return Translation(-m_coeffs); }

  Translation& operator=(const Translation& other)
  {
    m_coeffs = other.m_coeffs;
    return *this;
  }

  template<typename NewScalarType>
  inline typename internal::cast_return_type<Translation,Translation<NewScalarType,Dim> >::type cast() const
  { return typename internal::cast_return_type<Translation,Translation<NewScalarType,Dim> >::type(*this); }

  
  template<typename OtherScalarType>
  inline explicit Translation(const Translation<OtherScalarType,Dim>& other)
  { m_coeffs = other.vector().template cast<Scalar>(); }

  bool isApprox(const Translation& other, typename NumTraits<Scalar>::Real prec = precision<Scalar>()) const
  { return m_coeffs.isApprox(other.m_coeffs, prec); }

};

typedef Translation<float, 2> Translation2f;
typedef Translation<double,2> Translation2d;
typedef Translation<float, 3> Translation3f;
typedef Translation<double,3> Translation3d;


template<typename Scalar, int Dim>
inline typename Translation<Scalar,Dim>::TransformType
Translation<Scalar,Dim>::operator* (const ScalingType& other) const
{
  TransformType res;
  res.matrix().setZero();
  res.linear().diagonal() = other.coeffs();
  res.translation() = m_coeffs;
  res(Dim,Dim) = Scalar(1);
  return res;
}

template<typename Scalar, int Dim>
inline typename Translation<Scalar,Dim>::TransformType
Translation<Scalar,Dim>::operator* (const LinearMatrixType& linear) const
{
  TransformType res;
  res.matrix().setZero();
  res.linear() = linear;
  res.translation() = m_coeffs;
  res.matrix().row(Dim).setZero();
  res(Dim,Dim) = Scalar(1);
  return res;
}

template<typename Scalar, int Dim>
inline typename Translation<Scalar,Dim>::TransformType
Translation<Scalar,Dim>::operator* (const TransformType& t) const
{
  TransformType res = t;
  res.pretranslate(m_coeffs);
  return res;
}

} 
