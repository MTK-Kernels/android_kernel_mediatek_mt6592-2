// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed


namespace Eigen { 

template<typename _Scalar, int _Dim>
class Scaling
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(_Scalar,_Dim)
  
  enum { Dim = _Dim };
  
  typedef _Scalar Scalar;
  
  typedef Matrix<Scalar,Dim,1> VectorType;
  
  typedef Matrix<Scalar,Dim,Dim> LinearMatrixType;
  
  typedef Translation<Scalar,Dim> TranslationType;
  
  typedef Transform<Scalar,Dim> TransformType;

protected:

  VectorType m_coeffs;

public:

  
  Scaling() {}
  
  explicit inline Scaling(const Scalar& s) { m_coeffs.setConstant(s); }
  
  inline Scaling(const Scalar& sx, const Scalar& sy)
  {
    ei_assert(Dim==2);
    m_coeffs.x() = sx;
    m_coeffs.y() = sy;
  }
  
  inline Scaling(const Scalar& sx, const Scalar& sy, const Scalar& sz)
  {
    ei_assert(Dim==3);
    m_coeffs.x() = sx;
    m_coeffs.y() = sy;
    m_coeffs.z() = sz;
  }
  
  explicit inline Scaling(const VectorType& coeffs) : m_coeffs(coeffs) {}

  const VectorType& coeffs() const { return m_coeffs; }
  VectorType& coeffs() { return m_coeffs; }

  
  inline Scaling operator* (const Scaling& other) const
  { return Scaling(coeffs().cwise() * other.coeffs()); }

  
  inline TransformType operator* (const TranslationType& t) const;

  
  inline TransformType operator* (const TransformType& t) const;

  
  
  inline LinearMatrixType operator* (const LinearMatrixType& other) const
  { return coeffs().asDiagonal() * other; }

  
  
  friend inline LinearMatrixType operator* (const LinearMatrixType& other, const Scaling& s)
  { return other * s.coeffs().asDiagonal(); }

  template<typename Derived>
  inline LinearMatrixType operator*(const RotationBase<Derived,Dim>& r) const
  { return *this * r.toRotationMatrix(); }

  
  inline VectorType operator* (const VectorType& other) const
  { return coeffs().asDiagonal() * other; }

  
  inline Scaling inverse() const
  { return Scaling(coeffs().cwise().inverse()); }

  inline Scaling& operator=(const Scaling& other)
  {
    m_coeffs = other.m_coeffs;
    return *this;
  }

  template<typename NewScalarType>
  inline typename internal::cast_return_type<Scaling,Scaling<NewScalarType,Dim> >::type cast() const
  { return typename internal::cast_return_type<Scaling,Scaling<NewScalarType,Dim> >::type(*this); }

  
  template<typename OtherScalarType>
  inline explicit Scaling(const Scaling<OtherScalarType,Dim>& other)
  { m_coeffs = other.coeffs().template cast<Scalar>(); }

  bool isApprox(const Scaling& other, typename NumTraits<Scalar>::Real prec = precision<Scalar>()) const
  { return m_coeffs.isApprox(other.m_coeffs, prec); }

};

typedef Scaling<float, 2> Scaling2f;
typedef Scaling<double,2> Scaling2d;
typedef Scaling<float, 3> Scaling3f;
typedef Scaling<double,3> Scaling3d;

template<typename Scalar, int Dim>
inline typename Scaling<Scalar,Dim>::TransformType
Scaling<Scalar,Dim>::operator* (const TranslationType& t) const
{
  TransformType res;
  res.matrix().setZero();
  res.linear().diagonal() = coeffs();
  res.translation() = m_coeffs.cwise() * t.vector();
  res(Dim,Dim) = Scalar(1);
  return res;
}

template<typename Scalar, int Dim>
inline typename Scaling<Scalar,Dim>::TransformType
Scaling<Scalar,Dim>::operator* (const TransformType& t) const
{
  TransformType res = t;
  res.prescale(m_coeffs);
  return res;
}

} 
