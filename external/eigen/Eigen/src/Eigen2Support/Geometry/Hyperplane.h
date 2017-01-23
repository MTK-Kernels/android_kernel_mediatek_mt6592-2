// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
// Copyright (C) 2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed


namespace Eigen { 

template <typename _Scalar, int _AmbientDim>
class Hyperplane
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(_Scalar,_AmbientDim==Dynamic ? Dynamic : _AmbientDim+1)
  enum { AmbientDimAtCompileTime = _AmbientDim };
  typedef _Scalar Scalar;
  typedef typename NumTraits<Scalar>::Real RealScalar;
  typedef Matrix<Scalar,AmbientDimAtCompileTime,1> VectorType;
  typedef Matrix<Scalar,int(AmbientDimAtCompileTime)==Dynamic
                        ? Dynamic
                        : int(AmbientDimAtCompileTime)+1,1> Coefficients;
  typedef Block<Coefficients,AmbientDimAtCompileTime,1> NormalReturnType;

  
  inline Hyperplane() {}

  inline explicit Hyperplane(int _dim) : m_coeffs(_dim+1) {}

  inline Hyperplane(const VectorType& n, const VectorType& e)
    : m_coeffs(n.size()+1)
  {
    normal() = n;
    offset() = -e.eigen2_dot(n);
  }

  inline Hyperplane(const VectorType& n, Scalar d)
    : m_coeffs(n.size()+1)
  {
    normal() = n;
    offset() = d;
  }

  static inline Hyperplane Through(const VectorType& p0, const VectorType& p1)
  {
    Hyperplane result(p0.size());
    result.normal() = (p1 - p0).unitOrthogonal();
    result.offset() = -result.normal().eigen2_dot(p0);
    return result;
  }

  static inline Hyperplane Through(const VectorType& p0, const VectorType& p1, const VectorType& p2)
  {
    EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(VectorType, 3)
    Hyperplane result(p0.size());
    result.normal() = (p2 - p0).cross(p1 - p0).normalized();
    result.offset() = -result.normal().eigen2_dot(p0);
    return result;
  }

  
  explicit Hyperplane(const ParametrizedLine<Scalar, AmbientDimAtCompileTime>& parametrized)
  {
    normal() = parametrized.direction().unitOrthogonal();
    offset() = -normal().eigen2_dot(parametrized.origin());
  }

  ~Hyperplane() {}

  
  inline int dim() const { return int(AmbientDimAtCompileTime)==Dynamic ? m_coeffs.size()-1 : int(AmbientDimAtCompileTime); }

  
  void normalize(void)
  {
    m_coeffs /= normal().norm();
  }

  inline Scalar signedDistance(const VectorType& p) const { return p.eigen2_dot(normal()) + offset(); }

  inline Scalar absDistance(const VectorType& p) const { return ei_abs(signedDistance(p)); }

  inline VectorType projection(const VectorType& p) const { return p - signedDistance(p) * normal(); }

  inline const NormalReturnType normal() const { return NormalReturnType(*const_cast<Coefficients*>(&m_coeffs),0,0,dim(),1); }

  inline NormalReturnType normal() { return NormalReturnType(m_coeffs,0,0,dim(),1); }

  inline const Scalar& offset() const { return m_coeffs.coeff(dim()); }

  inline Scalar& offset() { return m_coeffs(dim()); }

  inline const Coefficients& coeffs() const { return m_coeffs; }

  inline Coefficients& coeffs() { return m_coeffs; }

  VectorType intersection(const Hyperplane& other)
  {
    EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(VectorType, 2)
    Scalar det = coeffs().coeff(0) * other.coeffs().coeff(1) - coeffs().coeff(1) * other.coeffs().coeff(0);
    
    
    if(ei_isMuchSmallerThan(det, Scalar(1)))
    {   
        if(ei_abs(coeffs().coeff(1))>ei_abs(coeffs().coeff(0)))
            return VectorType(coeffs().coeff(1), -coeffs().coeff(2)/coeffs().coeff(1)-coeffs().coeff(0));
        else
            return VectorType(-coeffs().coeff(2)/coeffs().coeff(0)-coeffs().coeff(1), coeffs().coeff(0));
    }
    else
    {   
        Scalar invdet = Scalar(1) / det;
        return VectorType(invdet*(coeffs().coeff(1)*other.coeffs().coeff(2)-other.coeffs().coeff(1)*coeffs().coeff(2)),
                          invdet*(other.coeffs().coeff(0)*coeffs().coeff(2)-coeffs().coeff(0)*other.coeffs().coeff(2)));
    }
  }

  template<typename XprType>
  inline Hyperplane& transform(const MatrixBase<XprType>& mat, TransformTraits traits = Affine)
  {
    if (traits==Affine)
      normal() = mat.inverse().transpose() * normal();
    else if (traits==Isometry)
      normal() = mat * normal();
    else
    {
      ei_assert("invalid traits value in Hyperplane::transform()");
    }
    return *this;
  }

  inline Hyperplane& transform(const Transform<Scalar,AmbientDimAtCompileTime>& t,
                                TransformTraits traits = Affine)
  {
    transform(t.linear(), traits);
    offset() -= t.translation().eigen2_dot(normal());
    return *this;
  }

  template<typename NewScalarType>
  inline typename internal::cast_return_type<Hyperplane,
           Hyperplane<NewScalarType,AmbientDimAtCompileTime> >::type cast() const
  {
    return typename internal::cast_return_type<Hyperplane,
                    Hyperplane<NewScalarType,AmbientDimAtCompileTime> >::type(*this);
  }

  
  template<typename OtherScalarType>
  inline explicit Hyperplane(const Hyperplane<OtherScalarType,AmbientDimAtCompileTime>& other)
  { m_coeffs = other.coeffs().template cast<Scalar>(); }

  bool isApprox(const Hyperplane& other, typename NumTraits<Scalar>::Real prec = precision<Scalar>()) const
  { return m_coeffs.isApprox(other.m_coeffs, prec); }

protected:

  Coefficients m_coeffs;
};

} 
